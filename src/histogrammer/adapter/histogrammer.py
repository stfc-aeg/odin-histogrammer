import logging

from typing import Literal, NamedTuple
from tornado.ioloop import PeriodicCallback, IOLoop

from histogrammer.lib import XDmaHexitec
from histogrammer.lib import HexitecSaveRestore

from histogrammer.lib import (
    MASK_HIST_FORMAT_NUMBINS,
    MASK_HIST_FORMAT_RUNMODE,
    MASK_HIST_FORMAT_MAPPEDMODE)
from histogrammer.lib import MASK_CLUSTER_MODE, MASK_CLUSTER_TRIG_MODE
from histogrammer.lib import (
    MASK_CSHARE_ENB_EDGE,
    MASK_CSHARE_ENB_NEG,
    MASK_CSHARE_ENB_L_POS,
    MASK_CSHARE_DIS_SUM,
    MASK_CSHARE_DIS_ADJ)
from histogrammer.lib import DATA_PATH_SHORT_BURST_MODE

from histogrammer.lib.defines import MappedMode, NumBins, RunMode
from histogrammer.lib.defines import ClusterMode, ClusterEnable, AutoTrigMode
from histogrammer.lib.defines import Region, GlobalRegisters, ChipRegisters

from histogrammer.handler.base_handler import BaseHandler
from histogrammer.handler.UdpHandler import UdpHandler
from histogrammer.handler.AcquisitionHandler import AcquisitionHandler
from histogrammer.handler.BaselineHandler import BaselineHandler
from histogrammer.util import splitRegisterIntoValues, UsesHexitecLibrary, InternalLibException


ConnectionStatus = Literal["disconnected", "connected", "configuring", "running", "completed"]
AcquisitionMode = Literal["continuous", "timed", "count frames"]
OutputMode = Literal["UDP", "HDF5"]


class Counters(NamedTuple):
    """Data Class defining the various counters for a Run"""

    frameCount: int = 0
    """Number of frames from the detector"""

    rawHitCount: int = 0
    """Total number of Raw Hits"""

    inputTimeFrame: int = 0
    """Total number of time frames from UDP"""

    finishedTimeFrame: int = 0
    """Total number of Finished (output) time frames"""


class Histogrammer:
    """
    Histogram Handling class, providing a bridge between the Adapter/Controller of Odin Control
    and the interface provided by PyBind11 to William's C++ Library
    """

    THRES_MAX = 4095
    """Maximum Threshold Value"""
    THRES_MIN = -4096
    """Minimum Threshold Value"""

    NUM_ROWS = 80
    NUM_COLS = 80

    def __init__(self, options: dict[str, str]) -> None:

        self.hexitec: XDmaHexitec = None
        self.udpHandler = UdpHandler(options)
        self.acqHandler = AcquisitionHandler(options)
        self.baselineHandler = BaselineHandler(options)

        self.handlers: list[BaseHandler] = [self.udpHandler, self.acqHandler, self.baselineHandler]

        self.initBaseline = True
        """Define if we should load the baseline at the start of an acq, or leave it as is"""

        # PCI DEVICE SETTINGS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.useQdma = options.get("useqdma", "").lower() in ["true", "1", "yes"]
        self.busNum = int(options.get("bus_num", 0))
        self.devNum = int(options.get("dev_num", 0))
        self.funcNum = int(options.get("func_num", 0))

        # HISTOGRAM FORMAT CONFIG SETTINGS~~~~~~~~~~~~~~~~~~~~
        self.mappedMode = MappedMode.OFF
        self.clusterMode = ClusterMode.POSITIVE
        self.clusterType = ClusterEnable.ALL
        self.autoTrigMode = AutoTrigMode.ONEIN16
        self.numBins = NumBins.ENG10

        self.runMode = RunMode.NORMAL

        self.chip_select = -1  # -1 applies changes to all chips
        self.stream_select = -1  # -1 applies to all data streams

        # LINEARITY CORRECTION VALUES~~~~~~~~~~~~~~~~~~~~~~~~~
        self.lin_offset = 0.0
        self.lin_scale = 1.0

        # CHARGE SHARING VALUES~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.enbEdgePos = True
        self.enbNegNeb = True
        self.enbLPos = True
        self.enbSumming = True
        self.enbAdjPosn = True

        self.checkRunEndFlagsCallback = PeriodicCallback(
            self.checkRunEndFlags, 200  # ARBITARY TIME CHOICE OF 200 MILLISEONDS
        )

    @UsesHexitecLibrary()
    def connect(self):
        try:
            self.hexitec = XDmaHexitec(self.useQdma, self.busNum, self.devNum, self.funcNum)

            logging.debug("Connection to card established")
            for handler in self.handlers:
                handler.initialise(self.hexitec)

            self.initialise()
        except (RuntimeError, InternalLibException):

            self.hexitec = None
            logging.error("Unable to connected to Device. Check provided Info:")
            logging.error("BusNum: %d, DevNum: %d, FuncNum: %d",
                          self.busNum, self.devNum, self.funcNum)
            raise InternalLibException("Unable to connect to Device. Check PCI device numbers")

    def disconnect(self):
        # additional clearup should almost certainly be done.
        # turn off run bit, for a start?
        self.hexitec = None  # garbage collection should cleanup the device
        for handler in self.handlers:
            handler.cleanup()

    @UsesHexitecLibrary()
    def initialise(self):
        """Initialise various values and Lookup Tables to the initial defaults"""
        logging.debug("Initialising default values into lookup tables and registers")
        # turn off run bit to stop any acquisition
        self.hexitec.setGlobReg(GlobalRegisters.RUN_REG, 0)

        # initalise lookup tables
        self.hexitec.initRecipLUT(self.chip_select, Region.EDGE_POS_RECIP, self.stream_select)
        self.hexitec.initRecipLUT(self.chip_select, Region.NEG_NEB_RECIP, self.stream_select)
        self.hexitec.initRecipLUT(self.chip_select, Region.L_POS_RECIP, self.stream_select)
        self.hexitec.initCShareLUTs(self.chip_select, self.stream_select)

        self.hexitec.initPixelMask(self.chip_select)

        self.hexitec.setLinearityOne(self.chip_select, 0.0)

        self.read_values()

    @UsesHexitecLibrary()
    def read_values(self):
        logging.debug("Reading Configuration from Hexitec System")

        # hist format
        histFormatReg = self.hexitec.getChipReg(0, ChipRegisters.FORMAT)
        numBins, runMode, mappedMode = splitRegisterIntoValues(histFormatReg,
                                                               MASK_HIST_FORMAT_NUMBINS,
                                                               MASK_HIST_FORMAT_RUNMODE,
                                                               MASK_HIST_FORMAT_MAPPEDMODE)

        self.mappedMode = MappedMode(mappedMode)
        self.runMode = RunMode(runMode)

        self.numBins = NumBins(numBins)

        clusterReg = self.hexitec.getChipReg(0, ChipRegisters.CLUSTER)
        cluster, trig = splitRegisterIntoValues(
            clusterReg, MASK_CLUSTER_MODE, MASK_CLUSTER_TRIG_MODE)
        self.clusterMode = ClusterMode(cluster)
        self.autoTrigMode = AutoTrigMode(trig)

        clustTypeReg = self.hexitec.getChipReg(0, ChipRegisters.ENB_CLUSTER)
        self.clusterType = ClusterEnable(clustTypeReg)

        # charge sharing
        cShareReg = self.hexitec.getChipReg(0, ChipRegisters.CORR_A)
        self.enbEdgePos, self.enbNegNeb, self.enbLPos = tuple(
            bool(x) for x in splitRegisterIntoValues(cShareReg,
                                                     MASK_CSHARE_ENB_EDGE,
                                                     MASK_CSHARE_ENB_NEG,
                                                     MASK_CSHARE_ENB_L_POS)
        )
        self.enbSumming, self.enbAdjPosn = tuple(
            not x for x in splitRegisterIntoValues(cShareReg,
                                                   MASK_CSHARE_DIS_SUM,
                                                   MASK_CSHARE_DIS_ADJ))

    def start_run(self):
        """Make the histogrammer begin outputting Histograms"""
        logging.debug("Starting Run")

        if self.acqHandler.outputMode == "HDF5":
            self.acqHandler.setupHdfWriter()
            self.udpHandler.stopDataMovers()
        elif self.acqHandler.outputMode == "UDP":
            self.acqHandler.hdfWriter = None
            self.udpHandler.startDataMovers(self.mappedMode)

        if self.initBaseline:
            self.baselineHandler.loadBaseline()
            IOLoop.current().add_callback(self.waitLoadBaseline, 20000)
        else:
            logging.debug("Skipping Baseline Initialisation")
            self.complete_start_run()

    @UsesHexitecLibrary()
    def waitLoadBaseline(self, loopcount: int):
        if self.baselineHandler.isBaselineLoaded() or loopcount < 0:
            if loopcount < 0:
                logging.warning(("Timed out waiting for Baseline to load. "
                                 "This may mean data is not being sent to the Histogrammer"))
            # baseline finished loading from data. Disable run bit,
            # reset data path to remove the "short burst mode" flag
            self.hexitec.setGlobReg(GlobalRegisters.RUN_REG, 0)
            datapath = self.hexitec.getGlobReg(GlobalRegisters.DATA_PATH)
            datapath = datapath & ~ DATA_PATH_SHORT_BURST_MODE
            self.hexitec.setGlobReg(GlobalRegisters.DATA_PATH, datapath)

            self.complete_start_run()
        else:
            IOLoop.current().add_callback(self.waitLoadBaseline, loopcount - 1)

    def complete_start_run(self):

        self.udpHandler.resetCounters()
        self.acqHandler.startRun()
        self.checkRunEndFlagsCallback.start()

    def stop_run(self):
        logging.info("Manually Stopping Acquisition")

        # only sets flag, so that we can cleanly shut down once data movers have completed
        self.acqHandler.runStatus = "completed"
        self.hexitec.iTfgDisable()

        # override the num histograms to the current latest one, so that
        # data movers etc will stop cleanly
        # self.acqHandler.num_histograms = self.acqHandler.getLatestTimeFrame()

    def checkRunEndFlags(self):
        """
        Loop to check if the run has completed, and if all required finalising
        steps (hdf writer, data movers) have finished what they need to do
        """

        if self.acqHandler.runStatus == "completed":
            logging.debug("ACQ HANDLER SAYS STATUS COMPLETE")
            # acqHandler says it's completed the acq, check for final steps
            if self.acqHandler.outputMode == "UDP":
                num_tf = self.acqHandler.num_histograms or self.acqHandler.getLatestTimeFrame()
                if not self.udpHandler.areDataMoversFinished(num_tf, self.mappedMode):
                    return
            elif self.acqHandler.outputMode == "HDF5":
                if not self.acqHandler.isHDFWriterComplete(self.hexitec.getFlushedFrame()):
                    return

            logging.debug("UDP data movers OR hdf writer have finished writing")
            self.acqHandler.hdfWriter = None
            self.udpHandler.stopDataMovers()
            self.acqHandler.stopRun()

            self.checkRunEndFlagsCallback.stop()

    @UsesHexitecLibrary()
    def setClusterMode(self,
                       clusterMode: ClusterMode | None = None,
                       autoTrigMode: AutoTrigMode | None = None):
        """Sets the cluster Mode and the auto triggering.

        :param clusterMode: The mode which describes how and which clusters are
        chosen. Defaults to using self.clusterMode
        :param autoTrigMode: Sets the frequency at which the pixels are triggered.
        Defaults to using self.autoTrigMode
        """

        if clusterMode is None:
            clusterMode = self.clusterMode
        if autoTrigMode is None:
            autoTrigMode = self.autoTrigMode

        self.hexitec.setClusterMode(self.chip_select, clusterMode, autoTrigMode)

    @UsesHexitecLibrary()
    def setClusterTypes(self, clusterType: ClusterEnable = None):
        """Sets the cluster pattern type(s).

        :param clusterType: A Flag of all cluster patterns to enable, bitwise
        ORd together. If this value is 0, it is overwritten to the default that
        enables all patterns
        """

        if clusterType is None or clusterType not in ClusterEnable:
            clusterType = ClusterEnable.ALL

        self.hexitec.setClusterTypes(self.chip_select, clusterType)

    @UsesHexitecLibrary()
    def addLinearityOffset(self, offset: float = 0.0):
        """Adds a fixed offset to the linearity correction. Must be done after
        anything else that may change the linearity corrections, as loading new
        linearity will overwrite this addition

        :param offset: the offset to correct values by, defaults to 0
        """

        self.hexitec.linearityAddOffset(self.chip_select, offset)

    @UsesHexitecLibrary()
    def setHistFormat(self,
                      numBins: NumBins | None = None,
                      runMode: RunMode | None = None,
                      mappedMode: MappedMode | None = None):
        """Set the Format of the Histograms. Be aware, not all combinations of
        numBins, runMode, and mappedMode are permitted.

        :param numBins: Enum value that defines the number of energy bins
        :param runMode: Enum value that defines the run mode of the histogrammer,
        specifying what data to include
        :param mappedMode: Enum value that defines the Mapped Mode of the histogram.

        :raises InternalLibException: If the combination of numBins and runMode is invalid
        """
        numBins = numBins if numBins is not None else self.numBins
        runMode = runMode if runMode is not None else self.runMode
        mappedMode = mappedMode if mappedMode is not None else self.mappedMode

        histFormat = (runMode << 3) | numBins
        self.hexitec.setHistFormat(self.chip_select, histFormat, mappedMode, 0)

    @UsesHexitecLibrary()
    def loadBadPixelTrig(self, filename: str):
        """Load the Bad Pixel Trigger config file into the Main Trigger LUT.
        This file defines specific pixels that should have triggering disabled

        :param filename: full path to the ascii config file
        """

        logging.debug("Loading Bad Pixel Trigger File {}".format(filename))
        self.hexitec.loadBadPixelsTrigAscii(filename)

    @UsesHexitecLibrary()
    def loadBadPixelOutput(self, filename: str):
        """Load the Bad Pixel Output config file. Masks the output of defined pixels
        from the histogram. Masked pixels can still activate triggers

        :param filename: full path to the ascii config file
        """
        self.hexitec.loadBadPixelsOutputAscii(filename)

    @UsesHexitecLibrary()
    def loadGainCorrection(self, filename: str):
        """Load Gain Correction Config File. Adds corrective value to each
        pixel, adjusting for individual noise.

        :param filename: full path to the ascii config file
        """
        self.hexitec.loadLinearityGainAscii(
            self.chip_select, filename, self.lin_scale, self.lin_offset)

    @UsesHexitecLibrary()
    def loadLinearityCorrection(self, filename: str):

        self.hexitec.loadLinearityAscii(self.chip_select, filename, self.lin_scale)

    @UsesHexitecLibrary()
    def setCShare(self):
        """Enable/Disable the various Charge Sharing Correction options."""

        self.hexitec.setCShareMode(self.chip_select, self.enbEdgePos, self.enbNegNeb, self.enbLPos,
                                   not self.enbLPos, not self.enbAdjPosn)

    @UsesHexitecLibrary()
    def loadCShare_pos(self, filename: str):

        self.hexitec.loadCShareAscii(self.chip_select, Region.EDGE_POS_M, filename)

    @UsesHexitecLibrary()
    def loadCShare_mc(self, filename: str):

        self.hexitec.loadCShareAsciiMC(self.chip_select, Region.EDGE_POS_M, filename)

    @UsesHexitecLibrary()
    def loadCShare_l3(self, filename: str):
        self.hexitec.loadCShareAscii(self.chip_select, Region.L_POS_M, filename)

    @UsesHexitecLibrary()
    def load_hdf_settings(self, filename: str):
        self.hexitec.loadSettingsHdf5(filename, self.chip_select,
                                      HexitecSaveRestore.All, self.lin_scale)

    @UsesHexitecLibrary()
    def save_hdf_settings(self, filename: str):
        self.hexitec.saveSettingsHdf5(filename, self.chip_select, HexitecSaveRestore.All)
