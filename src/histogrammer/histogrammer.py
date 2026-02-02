import logging
import os
from datetime import datetime

from typing import Literal, TypeVar, NamedTuple
from tornado.ioloop import PeriodicCallback, IOLoop

from xdma_hexitec import XDmaHexitec
from xdma_hexitec import HexitecSaveRestore
from xdma_hexitec.defines import MappedMode, NumBins, RunMode, ClusterMode, ClusterEnable, AutoTrigMode
from xdma_hexitec.defines import BaselineMask, BaselineDivide, BaselineChipVals
from xdma_hexitec.defines import Region, GlobalRegisters, ChipRegisters

from histogrammer.UdpHandler import UdpHandler
from histogrammer.AcquisitionHandler import AcquisitionHandler
from histogrammer.util import splitRegisterIntoValues, UsesHexitecLibrary, InternalLibException, T

from .base_controller import BaseError

from contextlib import redirect_stderr, redirect_stdout

from collections.abc import Callable

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

        # PCI DEVICE SETTINGS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.useQdma = options.get("useqdma", "").lower() in ["true", "1", "yes"]
        self.busNum = int(options.get("bus_num", 0))
        self.devNum = int(options.get("dev_num", 0))
        self.funcNum = int(options.get("func_num", 0))


        # HISTOGRAM FORMAT CONFIG SETTINGS~~~~~~~~~~~~~~~~~~~~
        self.mappedMode = MappedMode.OFF
        self.clusterMode = ClusterMode.POSITIVE
        self.clusterType = ClusterEnable.ALL
        self.autoTrigMode = AutoTrigMode.AUTOTRIG_1IN16
        self.numBins = NumBins.ENG10

        self.runMode = RunMode.NORMAL

        # THRESHOLD VALUES~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.thres_main = [-35, 35]
        self.thres_low = [-25, 25]
        self.thres_abs = [1, 1000]

        # BASELINE VALUES~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.baselineMask = BaselineMask.FIXED
        self.baselineDiv = BaselineDivide.BSUB_DIVIDE1024
        self.enableDither = False

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

            self.udpHandler.initialise(self.hexitec)
            self.acqHandler.initialise(self.hexitec)
            
            self.initialise()
        except (RuntimeError, InternalLibException):

            self.hexitec = None
            logging.error("Unable to connected to Device. Check provided Info:")
            logging.error("BusNum: %d, DevNum: %d, FuncNum: %d", self.busNum, self.devNum, self.funcNum)
            raise InternalLibException("Unable to connect to Device. Check PCI device numbers")

    def disconnect(self):
        # additional clearup should almost certainly be done. Set the turn off run bit, for a start?
        self.hexitec = None  # garbage collection should cleanup the device
        self.udpHandler.cleanup()
        self.acqHandler.cleanup()

    @UsesHexitecLibrary()
    def initialise(self):
        """Initialise various values and Lookup Tables to the initial defaults"""
        logging.debug("Initialising default values into lookup tables and registers")
        self.hexitec.setGlobReg(GlobalRegisters.GLB_RUN_REG, 0)  # turn off run bit to stop any acquisition

        # initalise lookup tables
        self.hexitec.initRecipLUT(self.chip_select, Region.REGION_EDGE_POS_RECIP, self.stream_select)
        self.hexitec.initRecipLUT(self.chip_select, Region.REGION_NEG_NEB_RECIP, self.stream_select)
        self.hexitec.initRecipLUT(self.chip_select, Region.REGION_L_POS_RECIP, self.stream_select)
        self.hexitec.initCShareLUTs(self.chip_select, self.stream_select)
        
        # init baseline Lookup Table to 0
        self.hexitec.setPixelLUT(
                         self.chip_select, Region.REGION_BASELINE,
                         0, self.NUM_COLS,
                         0, self.NUM_ROWS, 0)
        self.hexitec.initPixelMask( self.chip_select)
        
        self.hexitec.setLinearityOne( self.chip_select, 0.0)

        self.read_values()

    @UsesHexitecLibrary()
    def read_values(self):
        # TODO: too many magic number masks in here I reckon
        logging.debug("Reading Configuration from Hexitec System")

        # hist format
        histFormatReg = self.hexitec.getChipReg(0, ChipRegisters.FORMAT)
        numBins, runMode, mappedMode = splitRegisterIntoValues(histFormatReg, 0x7, 0x7<<3, 0x7<<8)
        
        self.mappedMode = MappedMode(mappedMode)
        self.runMode = RunMode(runMode)

        self.numBins = NumBins(numBins)

        clusterReg = self.hexitec.getChipReg(0, ChipRegisters.CLUSTER)
        cluster, trig = splitRegisterIntoValues(clusterReg, 0x7, 0x3 << 8)
        self.clusterMode = ClusterMode(cluster)
        self.autoTrigMode = AutoTrigMode(trig)

        clustTypeReg = self.hexitec.getChipReg(0, ChipRegisters.ENB_CLUSTER)
        self.clusterType = ClusterEnable(clustTypeReg)

        # thresholds
        abs_thres_read = self.hexitec.readPixelLUT(0, Region.REGION_ABS_THRES, 0, 1, 0, 1)[0]
        low_thres_read = self.hexitec.readPixelLUT(0, Region.REGION_LTHRES, 0, 1, 0, 1)[0]
        main_thres_read = self.hexitec.readPixelLUT(0, Region.REGION_MTHRES, 0, 1, 0, 1)[0]
        
        # must consider converting the uint16 value to a negative value for low and main
        neg_thres_offset = 0x2000  # seems to be the value to turn the unsigned value to signed

        self.thres_abs = [(abs_thres_read >> 16) & 0x7FFF, abs_thres_read & 0x7FFF]
        self.thres_low = [((low_thres_read >> 16) & 0x7FFF) - neg_thres_offset, low_thres_read & 0x7FFF]
        self.thres_main = [((main_thres_read >> 16) & 0x7FFF) - neg_thres_offset, main_thres_read & 0x7FFF]

        # baseline
        baselineReg = self.hexitec.getChipReg(0, ChipRegisters.BASESUB)
        mask, div, dither = splitRegisterIntoValues(baselineReg, 0xF >> 4, 0xF, 0x1 >> 12)
        self.baselineDiv = BaselineDivide(div)
        self.baselineMask = BaselineMask(mask)
        self.enableDither = bool(dither)

        # linearity correction
        # TODO not sure how to get these values

        # charge sharing
        cShareReg = self.hexitec.getChipReg(0, ChipRegisters.CORR_A)
        self.enbEdgePos, self.enbNegNeb, self.enbLPos = tuple(bool(x) for x in 
                                                              splitRegisterIntoValues(cShareReg, 1, 2, 4))
        self.enbSumming, self.enbAdjPosn = tuple(not x for x in splitRegisterIntoValues(cShareReg, 0x100, 0x200))

    def start_run(self):
        """Make the histogrammer begin outputting Histograms"""
        #TODO: other setup that might have to happen prior to enabling the run?
        logging.debug("Starting Run")
        if self.acqHandler.outputMode == "HDF5":
            self.acqHandler.setupHdfWriter()
            self.udpHandler.stopDataMovers()
        elif self.acqHandler.outputMode == "UDP":
            self.acqHandler.hdfWriter = None
            self.udpHandler.startDataMovers(self.mappedMode)

        self.loadBaseline()  # could we use python yield to resume the run starting after the looping waitLoadBaseline method completes?


    def complete_start_run(self):
        self.udpHandler.resetCounters()
        self.acqHandler.startRun()
        self.checkRunEndFlagsCallback.start()

    def stop_run(self):
        logging.debug("Manually Stopping Acquisition")
        self.acqHandler.runStatus = "completed"
        if self.acqHandler.outputMode == "UDP":
            self.udpHandler.stopDataMovers()
            self.acqHandler.stopRun()
        else:
            # gotta wait for hdf writer to finish. let checkRunEndFlags handle it?
            pass

        
    def checkRunEndFlags(self):
        """
        Loop to check if the run has completed, and if all required finalising steps (hdf writer, data movers)
        have finished what they need to do
        """
        if self.acqHandler.runStatus == "completed":
            logging.debug("ACQ HANDLER STAYS STAUS COMPLETE")
            # acqHandler says it's completed the acq, check for final steps
            if self.acqHandler.outputMode == "UDP" and self.acqHandler.acqMode == "count frames":
                if not self.udpHandler.areDataMoversFinished(self.acqHandler.outFrames, self.mappedMode):

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
    def setBaseline(self, mask: BaselineMask | None = None,
                    divide: BaselineDivide | None = None,
                    enableDither: bool | None = None):
        """Set the Baseline Mode
        
        :param mask: Set the Mask Mode, which controls when the error signal from the subtracted baseline is used to update the baseline estimage
        :param divide: Set the Baseline scaling applied to the error for adjusting the baseline estimate.
        :param enableDither: Enable or disable dithering (ramping bits below 0) for linearity correction
        """

        if mask is None:
            mask = self.baselineMask
        if divide is None:
            divide = self.baselineDiv
        if enableDither is None:
            enableDither = self.enableDither


        # if the mask is set to fixed, we dont use the absolute trigger
        useAbsTrig = not (mask == BaselineMask.FIXED)
        
        self.hexitec.setBaselineMode(self.chip_select, mask, divide, enableDither, useAbsTrig)

    @UsesHexitecLibrary()
    def loadBaseline(self):
        """Loading Baseline. Will use a callback loop to wait till baseline loaded before starting a proper run.
        This function is being written here rather than using the built in one from c++ to avoid using blocking
        waitLoadBaseline() method
        """
        
        self.hexitec.setGlobReg(GlobalRegisters.GLB_RUN_REG, 0)
        for chip in range(self.hexitec.getNumChips()):
            baselineReg = self.hexitec.getChipReg(chip, ChipRegisters.BASESUB)
            self.hexitec.setChipReg(chip, ChipRegisters.BASESUB, baselineReg & ~ BaselineChipVals.LOAD)
            self.hexitec.setChipReg(chip, ChipRegisters.BASESUB, baselineReg | BaselineChipVals.LOAD)
        
        dataPath = self.hexitec.getGlobReg(GlobalRegisters.GLB_DATA_PATH)
        dataPath = dataPath | (1<<14)  #TODO: TEMP MAGIC NUMBER, MATCHES HEXITEC_DATA_PATH_SHORT_BURST_MODE
        self.hexitec.setGlobReg(GlobalRegisters.GLB_DATA_PATH, dataPath)
        self.hexitec.setGlobReg(GlobalRegisters.GLB_FRAME_BURST_LENGTH, 2)
        self.hexitec.setGlobReg(GlobalRegisters.GLB_RUN_REG, 1)

        IOLoop.current().add_callback(self.waitLoadBaseline, dataPath, 0)
        # wait for baseline to finish loading. ioloop of some sort

    @UsesHexitecLibrary()
    def waitLoadBaseline(self, dataPath, loopCount): # TODO: add a timeout to avoid it getting stuck here
        """Loop waiting for the baseline to finish loading, before allowing the run to start proper"""
        mask = 0xFFFFFFFFFFFFFFF
        status = self.hexitec.getGlobReg64(GlobalRegisters.GLB_LOADING_BL)
        
        timeout = 20000
        # if (status & mask) and loopCount < 1000:
        #     IOLoop.current().add_callback(self.waitLoadBaseline, dataPath, loopCount + 1)
        # else:
        if not (status & mask) or loopCount > timeout:
            # loading complete
            if loopCount > timeout:
                logging.warning("Timed out waiting for Baseline to load. This may mean data is not being sent to the Histogrammer")
            self.hexitec.setGlobReg(GlobalRegisters.GLB_RUN_REG, 0)
            dataPath = dataPath & ~ (1 << 14) #TODO: TEMP MAGIC NUMBER, MATCHES HEXITEC_DATA_PATH_SHORT_BURST_MODE
            self.hexitec.setGlobReg(GlobalRegisters.GLB_DATA_PATH, dataPath)

            self.complete_start_run()
        else:
            IOLoop.current().add_callback(self.waitLoadBaseline, dataPath, loopCount + 1)

    @UsesHexitecLibrary()
    def setClusterMode(self, clusterMode: ClusterMode | None = None, autoTrigMode: AutoTrigMode | None = None):
        """Sets the cluster Mode and the auto triggering.
        
        :param clusterMode: The mode which describes how and which clusters are chosen.
        Defaults to using self.clusterMode
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
        
        :param clusterType: A Flag of all cluster patterns to enable, bitwise ORd together. If this value is 0, it is overwritten to the default that enables all patterns
        """

        if clusterType is None or clusterType not in ClusterEnable:
            clusterType = ClusterEnable.ALL

        self.hexitec.setClusterTypes( self.chip_select, clusterType)

    @UsesHexitecLibrary()
    def setTriggerThreshold(self, selectThreshold: Literal["absolute", "main" , "lower"],
                            cols: tuple[int, int],
                            rows: tuple[int, int],
                            threshold: tuple[int, int]):
        """Set the selected trigger threshold values for the specified rows and columns
        
        :param selectThreshold: Select which trigger threshold to set
        :param cols: The range of columns the threshold is applied
        :param rows: The range of Rows the threshold is applied
        :param threshold: The lower and upper threshold value for this trigger.
            For the Absolute threshold, this is a lower and upper value.
            For the other thresholds, this is a negative and a positive value.
        """

        if selectThreshold == "absolute":
            # setting the absolute trigger threshold
            self.hexitec.setAbsTriggerThres(
                             self.chip_select,
                             cols[0], cols[1],
                             rows[0], rows[1],
                             max(threshold),
                             min(threshold))
        elif selectThreshold == "main":
            # setting the main trigger threshold
            logging.warning("Setting Main Trigger Threshold Values. This will overwrite any Bad Pixel configuration")
            self.hexitec.setMainTriggerThres(
                             self.chip_select,
                             cols[0], cols[1],
                             rows[0], rows[1],
                             max(threshold),
                             min(threshold),
                             True)
        elif selectThreshold == "lower":
            self.hexitec.setLowerTriggerThres(
                             self.chip_select,
                             cols[0], cols[1],
                             rows[0], rows[1],
                             max(threshold),
                             min(threshold))
        else:
            raise InternalLibException("{} not a valid Threshold Option".format(selectThreshold))
    
    @UsesHexitecLibrary()
    def addLinearityOffset(self, offset: float = 0.0):
        """Adds a fixed offset to the linearity correction. Must be done after anything else
        that may change the linearity corrections, as loading new linearity will overwrite this addition
        
        :param offset: the offset to correct values by, defaults to 0
        """

        self.hexitec.linearityAddOffset(self.chip_select, offset)

    @UsesHexitecLibrary()
    def setHistFormat(self,
                      numBins: NumBins | None = None,
                      runMode: RunMode | None = None,
                      mappedMode: MappedMode | None = None):
        """Set the Format of the Histograms. Be aware, not all combinations of numBins, runMode, and mappedMode are permitted.
        
        :param numBins: Enum value that defines the number of energy bins
        :param runMode: Enum value that defines the run mode of the histogrammer, specifying what data to include
        :param mappedMode: Enum value that defines the Mapped Mode of the histogram.

        :raises InternalLibException: If the combination of numBins and runMode is invalid
        """
        numBins = numBins if numBins != None else self.numBins
        runMode = runMode if runMode != None else self.runMode
        mappedMode = mappedMode if mappedMode != None else self.mappedMode

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
        self.hexitec.loadLinearityGainAscii(self.chip_select, filename, self.lin_scale, self.lin_offset)

    @UsesHexitecLibrary()
    def loadLinearityCorrection(self, filename: str):

        self.hexitec.loadLinearityAscii(self.chip_select, filename, self.lin_scale)

    @UsesHexitecLibrary()
    def setCShare(self, enableEdgePos: bool| None = None, enableNegNeighbour: bool | None = None,
                  enableLPos: bool | None = None,
                  enableSumming: bool | None = None, enablePositonAdjustment: bool | None = None):
        """Enable/Disable the various Charge Sharing Correction options.
        
        :param enableEdgePos: Enable correction that shares positive signal with neighbours
        :param enableNegNeighbour: Enable correction that shares signal with negative neighbours
        :param enableSumming: Enable Any charge summing corrections.
        :param enablePositonAdjustment: Enable position adjustment signal correction
        """

        enableEdgePos = self.enbEdgePos if enableEdgePos is None else enableEdgePos
        enableNegNeighbour = self.enbNegNeb if enableNegNeighbour is None else enableNegNeighbour
        enableLPos = self.enbLPos if enableLPos is None else enableLPos
        enableSumming = self.enbSumming if enableSumming is None else enableSumming
        enablePositonAdjustment = self.enbAdjPosn if enablePositonAdjustment is None else enablePositonAdjustment

        self.hexitec.setCShareMode(self.chip_select, enableEdgePos, enableNegNeighbour, enableLPos, 
                                   not enableSumming, not enablePositonAdjustment)

    @UsesHexitecLibrary()
    def loadCShare_pos(self, filename: str):

        self.hexitec.loadCShareAscii(self.chip_select, Region.REGION_EDGE_POS_M, filename)

    @UsesHexitecLibrary()
    def loadCShare_mc(self, filename: str):
        
        self.hexitec.loadCShareAsciiMC(self.chip_select, Region.REGION_EDGE_POS_M, filename)

    @UsesHexitecLibrary()
    def loadCShare_l3(self, filename: str):
        self.hexitec.loadCShareAscii(self.chip_select, Region.REGION_L_POS_M, filename)

    @UsesHexitecLibrary()
    def load_hdf_settings(self, filename: str):
        self.hexitec.loadSettingsHdf5(filename, self.chip_select, HexitecSaveRestore.All, self.lin_scale)

    @UsesHexitecLibrary()
    def save_hdf_settings(self, filename: str):
        self.hexitec.saveSettingsHdf5(filename, self.chip_select, HexitecSaveRestore.All)

