import logging
import os
from datetime import datetime

from typing import Literal, TypeVar, NamedTuple
from tornado.ioloop import PeriodicCallback, IOLoop

from xdma_hexitec import XDmaHexitec, CircularHdfWriter
from xdma_hexitec import CircWriterReadoutMode, CircWriterUdpTxOnlyMode
from xdma_hexitec import HexitecUdpRxConnection, HexitecITfgMode, HexitecSaveRestore
from xdma_hexitec.defines import MappedMode, NumBins, RunMode, ClusterMode, ClusterEnable, AutoTrigMode
from xdma_hexitec.defines import BaselineMask, BaselineDivide, BaselineChipVals
from xdma_hexitec.defines import Region, GlobalRegisters, ChipRegisters
from xdma_hexitec.defines import TimeFrameStatus, TimeFrameMasks, HexitecGeneration

from histogrammer.UdpHandler import UdpHandler
from histogrammer.AcquisitionHandler import AcquisitionHandler
from histogrammer.util import splitRegisterIntoValues, InternalLibException, T

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
        

    def _run_method(self, method: Callable[..., T], *args, **kwargs) -> T:
        """Run the provided Pybind11 Method, redirecting all stdout to devnull and converting
        RuntimeErrors into InternalLibExceptions

        TODO: Rather than just redirect to devnull, provide means to redirect to logger or other file

        :param method: the method to run wrapped in the redirection handler
        :param args:   the positional arguments to pass to the method
        :param kwargs: the keyword arguments to pass to the method

        :returns: The return value of the Method. Most methods return None, but some return status codes or requested values

        :raises InternalLibException: If the method raises an error, it is converted into a InternalLibException
        :raises InternalLibException: Raised if the method cannot be run, either due to the current run state or the device not being connected
        """

        if self.hexitec is None and method.__name__ != "XDmaHexitec":
            # can't run a method on a class not yet initalised (unless that method is the initalisation in question)
            raise InternalLibException("Cannot run method {}: Histogrammer not Initialised".format(method.__name__))
        try:
            with redirect_stdout(open(os.devnull, "w")), redirect_stderr(open(os.devnull, "w")):
                return method(*args, **kwargs)
        except RuntimeError as e:
            logging.error(e)
            raise InternalLibException("{}".format(e))

    def connect(self):
        try:
            self.hexitec = self._run_method(XDmaHexitec, self.useQdma, self.busNum, self.devNum, self.funcNum)

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

    def initialise(self):
        """Initialise various values and Lookup Tables to the initial defaults"""
        logging.debug("Initialising default values into lookup tables and registers")
        self.hexitec.setGlobReg(GlobalRegisters.GLB_RUN_REG, 0)  # turn off run bit to stop any acquisition

        # initalise lookup tables
        self._run_method(self.hexitec.initRecipLUT, self.chip_select, Region.REGION_EDGE_POS_RECIP, self.stream_select)
        self._run_method(self.hexitec.initRecipLUT, self.chip_select, Region.REGION_NEG_NEB_RECIP, self.stream_select)
        self._run_method(self.hexitec.initRecipLUT, self.chip_select, Region.REGION_L_POS_RECIP, self.stream_select)
        self._run_method(self.hexitec.initCShareLUTs, self.chip_select, self.stream_select)
        
        # init baseline Lookup Table to 0
        self._run_method(self.hexitec.setPixelLUT,
                         self.chip_select, Region.REGION_BASELINE,
                         0, self.NUM_COLS,
                         0, self.NUM_ROWS, 0)
        self._run_method(self.hexitec.initPixelMask, self.chip_select)
        
        self._run_method(self.hexitec.setLinearityOne, self.chip_select, 0.0)

        self.read_values()

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


        # UDP stuff?

        

    def start_run(self):
        """Make the histogrammer begin outputting Histograms"""
        #TODO: other setup that might have to happen prior to enabling the run?

        if self.acqHandler.outputMode == "HDF5":
            self.acqHandler.setupHdfWriter()
            self.udpHandler.stopDataMovers()
        elif self.acqHandler.outputMode == "UDP":
            self.acqHandler.hdfWriter = None
            self.udpHandler.startDataMovers(self.mappedMode)

        self.loadBaseline()  # could we use python yield to resume the run starting after the looping waitLoadBaseline method completes?


    def complete_start_run(self):
        self.udpHandler.resetCounters()
        self.acqHandler.setupRun()

    def stop_run(self):
        self.udpHandler.stopDataMovers()
        self.acqHandler.stop_run()
        

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
        
        self._run_method(self.hexitec.setBaselineMode, 
                         self.chip_select, mask, divide, enableDither, useAbsTrig)

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

        self._run_method(self.hexitec.setClusterMode, self.chip_select, clusterMode, autoTrigMode)

    def setClusterTypes(self, clusterType: ClusterEnable = None):
        """Sets the cluster pattern type(s).
        
        :param clusterType: A Flag of all cluster patterns to enable, bitwise ORd together. If this value is 0, it is overwritten to the default that enables all patterns
        """

        if clusterType is None or clusterType not in ClusterEnable:
            clusterType = ClusterEnable.ALL

        self._run_method(self.hexitec.setClusterTypes, self.chip_select, clusterType)

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
            self._run_method(self.hexitec.setAbsTriggerThres,
                             self.chip_select,
                             cols[0], cols[1],
                             rows[0], rows[1],
                             max(threshold),
                             min(threshold))
        elif selectThreshold == "main":
            # setting the main trigger threshold
            logging.warning("Setting Main Trigger Threshold Values. This will overwrite any Bad Pixel configuration")
            self._run_method(self.hexitec.setMainTriggerThres,
                             self.chip_select,
                             cols[0], cols[1],
                             rows[0], rows[1],
                             max(threshold),
                             min(threshold),
                             True)
        elif selectThreshold == "lower":
            self._run_method(self.hexitec.setLowerTriggerThres,
                             self.chip_select,
                             cols[0], cols[1],
                             rows[0], rows[1],
                             max(threshold),
                             min(threshold))
        else:
            raise InternalLibException("{} not a valid Threshold Option".format(selectThreshold))
    
    def addLinearityOffset(self, offset: float = 0.0):
        """Adds a fixed offset to the linearity correction. Must be done after anything else
        that may change the linearity corrections, as loading new linearity will overwrite this addition
        
        :param offset: the offset to correct values by, defaults to 0
        """

        self._run_method(self.hexitec.linearityAddOffset,
                         self.chip_select,
                         offset)

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
        self._run_method(self.hexitec.setHistFormat,
                         self.chip_select,
                         histFormat, mappedMode)


    def getIntfromIP(self, ip: str) -> int:
        """Turn an IP address string (eg 192.168.0.0) into the required 32 Bit Integer value
        
        :param ip: the IP address to convert, in the standard dotted decimal notation

        :return: The IP address provided as a 32 bit number.
                 If the supplied address is invalid for any reason, returns a 0 so the histogrammer
                 uses the default IP addresses
        """

        try:
            parts = [int(x) for x in ip.split(".")]
            if not len(parts) == 4:
                raise ValueError
            return parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3]
        except ValueError:
            return 0


    def getFrameCounts(self) -> Counters:
        """Return the current count of frames for an in-progress run.
        Reading these values after a run has been stopped will return invalid values.

        :return counters.frameCount: Total frames from the detector
        :return counters.rawHitCount: Total number of raw hits from the detector
        :return counters.inputTimeFrame: Total number of Timeframes from the UDP input
        :return counters.finishedTimeFrame: Count of Finished Time Frames that have been output, or -1 if invalid
        """


        frameCount = []
        rawCount = []
        inputTimeFrame = -1
        finishedTimeFrame = -1


        # self.hexitec.getDiagnosticCounters(frameCount, rawCount)
        # passing an array pointer does not appear to work. Thankfully, all the getDiagnositcCounters
        # method does is the for loop below

        for i in range(self.hexitec.getNumChips()):
            frameCount.append(self.hexitec.getGlobReg(GlobalRegisters.GLB_FRAME_COUNT0 + (2*i)))
            rawCount.append(self.hexitec.getGlobReg(GlobalRegisters.GLB_RAW_HIT_COUNT0 + (2*i)))


        frameToken = self.hexitec.getFlushedFrame()
        inputTimeFrame = self.hexitec.getInpTimeFrame(0)

        # mask to get the count from the register value
        inputTimeFrame = inputTimeFrame & TimeFrameMasks.INPUT_COUNT

        # check valid bit of frameToken
        if frameToken & TimeFrameMasks.FLUSHED_VALID:
            finishedTimeFrame = frameToken & TimeFrameMasks.FLUSHED_COUNT

        return Counters(frameCount[0],
                        sum(rawCount),
                        inputTimeFrame,
                        finishedTimeFrame)
    
    def getItfgStatus(self):
        """Read the Time Frame Generator statuses. End an acquisition if the status reads FINISHED"""

        
        # self.hexitec.iTfgReadStatus(stat) # passing the python class as a struct pointer didnt seem to work
        # so we read the registers manually
        status = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_STATUS)
        inpFrame = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_INP_FRAME)
        timeFrame = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_TIME_FRAME)
        cycles = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_CYCLES)
        # logging.debug("{} {} {} {}".format(status, inpFrame, timeFrame, cycles))
        return (status, inpFrame, timeFrame, cycles)
    

    def loadBadPixelTrig(self, filename: str):
        """Load the Bad Pixel Trigger config file into the Main Trigger LUT.
        This file defines specific pixels that should have triggering disabled
        
        :param filename: full path to the ascii config file
        """


        logging.debug("Loading Bad Pixel Trigger File {}".format(filename))
        self._run_method(self.hexitec.loadBadPixelsTrigAscii, filename)
        
    def loadBadPixelOutput(self, filename: str):
        """Load the Bad Pixel Output config file. Masks the output of defined pixels
        from the histogram. Masked pixels can still activate triggers
        
        :param filename: full path to the ascii config file
        """
        self._run_method(self.hexitec.loadBadPixelsOutputAscii, filename)

    def loadGainCorrection(self, filename: str):
        """
        
        :param filename: full path to the ascii config file
        """
        self._run_method(self.hexitec.loadLinearityGainAscii, self.chip_select, 
                         filename, self.lin_scale, self.lin_offset)
        
    def loadLinearityCorrection(self, filename: str):

        self._run_method(self.hexitec.loadLinearityAscii, self.chip_select, filename, self.lin_scale)

    # def badPixelTrig(self, row: int, col: int, enable: bool):

    def setCShare(self, enableEdgePos: bool| None = None, enableNegNeighbour: bool | None = None,
                  enableLPos: bool | None = None,
                  enableSumming: bool | None = None, enablePositonAdjustment: bool | None = None):
        """Enable/Disable the various Charge Sharing Correction options.
        
        :param enableEdgePos: Enable correction that shares positive signal with neighbours
        :param enableNegNeighbour: Enable correction that shares signal with negative neighbours
        :param enableSumming: Enable Any charge summing corrections.
        :param enablePositonAdjustment: Enable position adjustment signal correction
        """

        if enableEdgePos is None:
            enableEdgePos = self.enbEdgePos
        if enableNegNeighbour is None:
            enableNegNeighbour = self.enbNegNeb
        if enableLPos is None:
            enableLPos = self.enbLPos
        if enableSumming is None:
            enableSumming = self.enbSumming
        if enablePositonAdjustment is None:
            enablePositonAdjustment = self.enbAdjPosn

        self._run_method(self.hexitec.setCShareMode, self.chip_select,
                         enableEdgePos, enableNegNeighbour, enableLPos,
                         not enableSumming, not enablePositonAdjustment)

    def loadCShare_pos(self, filename: str):

        self._run_method(self.hexitec.loadCShareAscii, self.chip_select, 
                         Region.REGION_EDGE_POS_M, filename)

    def loadCShare_mc(self, filename: str):
        
        self._run_method(self.hexitec.loadCShareAsciiMC, self.chip_select, 
                         Region.REGION_EDGE_POS_M, filename)

    def loadCShare_l3(self, filename: str):
        self._run_method(self.hexitec.loadCShareAscii, self.chip_select, 
                         Region.REGION_L_POS_M, filename)
        
    def load_hdf_settings(self, filename: str):
        self.hexitec.loadSettingsHdf5(filename, self.chip_select, HexitecSaveRestore.All, self.lin_scale)

    def save_hdf_settings(self, filename: str):
        self.hexitec.saveSettingsHdf5(filename, self.chip_select, HexitecSaveRestore.All)

