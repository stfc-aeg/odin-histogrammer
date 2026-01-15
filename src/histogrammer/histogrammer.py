import logging
import os
from datetime import datetime

from typing import Literal, TypeVar, NamedTuple
from tornado.ioloop import PeriodicCallback, IOLoop

from xdma_hexitec import XDmaHexitec, defines
from xdma_hexitec import HexitecUdpRxConnection, HexitecITfgMode, HexitecSaveRestore

from .base_controller import BaseError

from contextlib import redirect_stderr, redirect_stdout

from collections.abc import Callable

ConnectionStatus = Literal["disconnected", "connected", "configuring", "running", "completed"]
AcquisitionMode = Literal["continuous", "timed", "count frames"]
T = TypeVar("T")

class InternalLibException(BaseError):
    """Exception that translates a RuntimeError thrown by the Pybind11 module into a python exception"""


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

def _get_bitwise_trailing_zeros(val):
    """Method to get the number of trailing 0s on a binary value.
    Used to calculate how much to shift a masked value to return the specific value regardless of its position"""
    c = 0
    v = (val ^ (val - 1)) >> 1
    while v > 0:
        v >>= 1
        c += 1
    return c

def _splitRegisterIntoValues(reg: int, *masks: int) -> tuple[int, ...]:
    retVal: tuple[int] = ()
    for mask in masks:
        shift = _get_bitwise_trailing_zeros(mask)
        retVal = retVal + ((reg & mask) >> shift,)

    return retVal
        


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

        self.status: ConnectionStatus = "disconnected"

        # AQUISITION CONTROLS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.runTimer = 0
        """If > 0, stops a run after that many seconds"""
        self.input_frames = 2000000
        """Number of Input Frames per output Time Frame"""
        self.output_frames = 20
        """Number of Output Time Frames"""

        self.acqMode: AcquisitionMode = "count frames"
        """Define how the length of the acquisition is defined"""

        # PCI DEVICE SETTINGS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.useQdma = options.get("useqdma", "").lower() in ["true", "1", "yes"]
        self.busNum = int(options.get("bus_num", 0))
        self.devNum = int(options.get("dev_num", 0))
        self.funcNum = int(options.get("func_num", 0))

        # UDP CONFIG SETTINGS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.source_ip = options.get("source_ip", "default")
        self.dest_ip = options.get("dest_ip", "default")
        self.accel_rx_ip = options.get("accel_rx_ip", "default")
        self.accel_tx_ip = options.get("accel_tx_ip", "default")

        self.source_port = int(options.get("source_port", 0))
        self.accel_port = int(options.get("accel_port", 0))
        self.dest_port = int(options.get("dest_port", 0))

        self.connectType = HexitecUdpRxConnection.Normal
        self.numUDPThreads = 8

        # HISTOGRAM FORMAT CONFIG SETTINGS~~~~~~~~~~~~~~~~~~~~
        self.mappedMode = defines.MappedMode.OFF
        self.clusterMode = defines.ClusterMode.POSITIVE
        self.clusterType = defines.ClusterEnable.ALL
        self.autoTrigMode = defines.AutoTrigMode.AUTOTRIG_1IN16
        self.numBins = 1024  # numBins.ENG10
        self.runMode = defines.RunMode.NORMAL

        # THRESHOLD VALUES~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.thres_main = [-35, 35]
        self.thres_low = [-25, 25]
        self.thres_abs = [1, 1000]

        # BASELINE VALUES~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.baselineMask = defines.BaselineMask.FIXED
        self.baselineDiv = defines.BaselineDivide.BSUB_DIVIDE1024
        self.enableDither = False

        self.chip_select = -1  # -1 applies changes to all chips
        self.stream_select = -1  # -1 applies to all data streams

        # LINEARITY CORRECTION VALUES~~~~~~~~~~~~~~~~~~~~~~~~~
        self.lin_offset = 0.0
        self.lin_scale = 1.0

        self.inter_frame_gap = 4095

        self.frame_counters = Counters(0, 0, 0, 0)
        self.itfg_status = {
            "status": "Invalid",
            "input_frame": 0,
            "output_frame": 0,
            "cycles": 0
        }

        # CHARGE SHARING VALUES~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        self.enbEdgePos = True
        self.enbNegNeb = True
        self.enbLPos = True
        self.enbSumming = True
        self.enbAdjPosn = True

        self.counter_callback = PeriodicCallback(
            self.read_counters_callback,
            100)
        
        self.itfg_callback = PeriodicCallback(
            self.read_itfg_status_callback,
            100)


    def read_counters_callback(self):
        self.frame_counters = self.getFrameCounts()

    def read_itfg_status_callback(self):
        stat = self.getItfgStatus()
        try:
            self.itfg_status["status"] = defines.TimeFrameStatus(stat[0]).name
        except ValueError:
            self.itfg_status["status"] = "invalid"
        self.itfg_status["input_frame"] = stat[1]
        self.itfg_status["output_frame"] = stat[2]
        self.itfg_status["cycles"] = stat[3]

        if stat[0] == defines.TimeFrameStatus.FINISHED:
            self.stop_run()


        

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

            self.status = "connected"
            logging.debug("Connection to card established")
            self.initialise()
        except (RuntimeError, InternalLibException):
            self.status = "disconnected"
            self.hexitec = None
            logging.error("Unable to connected to Device. Check provided Info:")
            logging.error("BusNum: %d, DevNum: %d, FuncNum: %d", self.busNum, self.devNum, self.funcNum)
            raise InternalLibException("Unable to connect to Device. Check PCI device numbers")

    def disconnect(self):
        # additional clearup should almost certainly be done. Set the turn off run bit, for a start?
        self.hexitec = None  # garbage collection should cleanup the device
        self.status = "disconnected"

    def initialise(self):
        """Initialise various values and Lookup Tables to the initial defaults"""
        logging.debug("Initialising default values into lookup tables and registers")
        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_RUN_REG, 0)  # turn off run bit to stop any acquisition

        # initalise lookup tables
        self._run_method(self.hexitec.initRecipLUT, self.chip_select, defines.Region.REGION_EDGE_POS_RECIP, self.stream_select)
        self._run_method(self.hexitec.initRecipLUT, self.chip_select, defines.Region.REGION_NEG_NEB_RECIP, self.stream_select)
        self._run_method(self.hexitec.initRecipLUT, self.chip_select, defines.Region.REGION_L_POS_RECIP, self.stream_select)
        self._run_method(self.hexitec.initCShareLUTs, self.chip_select, self.stream_select)
        
        # init baseline Lookup Table to 0
        self._run_method(self.hexitec.setPixelLUT,
                         self.chip_select, defines.Region.REGION_BASELINE,
                         0, self.NUM_COLS,
                         0, self.NUM_ROWS, 0)
        self._run_method(self.hexitec.initPixelMask, self.chip_select)
        
        self._run_method(self.hexitec.setLinearityOne, self.chip_select, 0.0)

    def read_values(self):
        # TODO: too many magic number masks in here I reckon
        logging.debug("Reading Configuration from Hexitec System")

        # hist format
        histFormatReg = self.hexitec.getChipReg(0, defines.ChipRegisters.FORMAT)
        numBins, runMode, mappedMode = _splitRegisterIntoValues(histFormatReg, 0x7, 0x7<<3, 0x7<<8)
        
        self.mappedMode = defines.MappedMode(mappedMode)
        self.runMode = defines.RunMode(runMode)
        
        lookup = {defines.NumBins.ENG7: 2**7,
                  defines.NumBins.ENG8: 2**8,
                  defines.NumBins.ENG9: 2**9,
                  defines.NumBins.ENG10: 2**10,
                  defines.NumBins.ENG11: 2**11,
                  defines.NumBins.ENG12: 2**12,
                  defines.NumBins.ENG10LSB: 2**13}
        self.numBins = lookup.get(numBins)

        clusterReg = self.hexitec.getChipReg(0, defines.ChipRegisters.CLUSTER)
        cluster, trig = _splitRegisterIntoValues(clusterReg, 0x7, 0x3 << 8)
        self.clusterMode = defines.ClusterMode(cluster)
        self.autoTrigMode = defines.AutoTrigMode(trig)

        clustTypeReg = self.hexitec.getChipReg(0, defines.ChipRegisters.ENB_CLUSTER)
        self.clusterType = defines.ClusterEnable(clustTypeReg)

        # thresholds
        abs_thres_read = self.hexitec.readPixelLUT(0, defines.Region.REGION_ABS_THRES, 0, 1, 0, 1)[0]
        low_thres_read = self.hexitec.readPixelLUT(0, defines.Region.REGION_LTHRES, 0, 1, 0, 1)[0]
        main_thres_read = self.hexitec.readPixelLUT(0, defines.Region.REGION_MTHRES, 0, 1, 0, 1)[0]
        
        # must consider converting the uint16 value to a negative value for low and main
        neg_thres_offset = 0x2000  # seems to be the value to turn the unsigned value to signed

        self.thres_abs = [(abs_thres_read >> 16) & 0x7FFF, abs_thres_read & 0x7FFF]
        self.thres_low = [((low_thres_read >> 16) & 0x7FFF) - neg_thres_offset, low_thres_read & 0x7FFF]
        self.thres_main = [((main_thres_read >> 16) & 0x7FFF) - neg_thres_offset, main_thres_read & 0x7FFF]

        # baseline
        baselineReg = self.hexitec.getChipReg(0, defines.ChipRegisters.BASESUB)
        mask, div, dither = _splitRegisterIntoValues(baselineReg, 0xF >> 4, 0xF, 0x1 >> 12)
        self.baselineDiv = defines.BaselineDivide(div)
        self.baselineMask = defines.BaselineMask(mask)
        self.enableDither = bool(dither)

        # linearity correction
        # TODO not sure how to get these values

        # charge sharing
        cShareReg = self.hexitec.getChipReg(0, defines.ChipRegisters.CORR_A)
        self.enbEdgePos, self.enbNegNeb, self.enbLPos = tuple(bool(x) for x in 
                                                              _splitRegisterIntoValues(cShareReg, 1, 2, 4))
        self.enbSumming, self.enbAdjPosn = tuple(not x for x in _splitRegisterIntoValues(cShareReg, 0x100, 0x200))


        # UDP stuff?

        

    def start_run(self):
        """Make the histogrammer begin outputting Histograms"""
        #TODO: other setup that might have to happen prior to enabling the run?
        self.status = "configuring"

        self.loadBaseline()  # could we use python yield to resume the run starting after the looping waitLoadBaseline method completes?

        
        
    def complete_start_run(self):
        
        if self.acqMode == "count frames":
            logging.debug("Setting up Internal Time Frame Generator")
            self.hexitec.iTfgSetup(HexitecITfgMode.SWFirst, 1, True, self.input_frames, self.output_frames, 1)
            logging.debug("Total histograms to be generated: {}".format(self.input_frames*self.output_frames))
        else:
            logging.debug("Disabling ITFG")
            self.hexitec.iTfgDisable()

        start = datetime.now()
        
        self.hexitec.clearHistAll() # this is a blocking function that takes some time, I think
        end = datetime.now()
        logging.warning("Clear took {} seconds".format((end - start).total_seconds()))

        # reset the udp packet counters
        self.hexitec.udpResetCounts(False)

        # enable the histogramming by disabling any test pattern stuff
        self.hexitec.enableHist()

        # set the run reg to 1 to start producing histograms
        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_RUN_REG, 1)
        self.status = "running"

        if self.acqMode == "timed":
            IOLoop.current().call_later(self.runTimer, self.stop_run)

        if self.acqMode == "count frames":
            self.hexitec.iTfgTrigger()
            self.itfg_callback.start()
        self.counter_callback.start()

    def stop_run(self):
        logging.debug("Stopping Run")
        if self.hexitec is None or self.status != "running":
            logging.warning("Histogrammer is not running, doing nothering in stop_run")
            return
        
        # await data mover stop?
        self._run_method(self.hexitec.stopDataMoverStreamUDP, 0)
        self._run_method(self.hexitec.stopDataMoverStreamUDP, 1)

        self.counter_callback.stop()
        self.itfg_callback.stop()
        # making sure to read the current frame counts before turning off the run bit
        # as turning off the bit flushes the histograms and we lose this information
        self.frame_counters = self.getFrameCounts()
        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_RUN_REG, 0)
        self.status = "completed"

    def setBaseline(self, mask: defines.BaselineMask | None = None,
                    divide: defines.BaselineDivide | None = None,
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
        useAbsTrig = not (mask == defines.BaselineMask.FIXED)
        
        self._run_method(self.hexitec.setBaselineMode, 
                         self.chip_select, mask, divide, enableDither, useAbsTrig)

    def loadBaseline(self):
        """Loading Baseline. Will use a callback loop to wait till baseline loaded before starting a proper run.
        This function is being written here rather than using the built in one from c++ to avoid using blocking
        waitLoadBaseline() method
        """
        
        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_RUN_REG, 0)
        for chip in range(self.hexitec.getNumChips()):
            baselineReg = self.hexitec.getChipReg(chip, defines.ChipRegisters.BASESUB)
            self.hexitec.setChipReg(chip, defines.ChipRegisters.BASESUB, baselineReg & ~ defines.BaselineChipVals.LOAD)
            self.hexitec.setChipReg(chip, defines.ChipRegisters.BASESUB, baselineReg | defines.BaselineChipVals.LOAD)
        
        dataPath = self.hexitec.getGlobReg(defines.GlobalRegisters.GLB_DATA_PATH)
        dataPath = dataPath | (1<<14)  #TODO: TEMP MAGIC NUMBER, MATCHES HEXITEC_DATA_PATH_SHORT_BURST_MODE
        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_DATA_PATH, dataPath)
        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_FRAME_BURST_LENGTH, 2)
        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_RUN_REG, 1)

        IOLoop.current().add_callback(self.waitLoadBaseline, dataPath, 0)
        # wait for baseline to finish loading. ioloop of some sort

    def waitLoadBaseline(self, dataPath, loopCount): # TODO: add a timeout to avoid it getting stuck here
        """Loop waiting for the baseline to finish loading, before allowing the run to start proper"""
        mask = 0xFFFFFFFFFFFFFFF
        status = self.hexitec.getGlobReg64(defines.GlobalRegisters.GLB_LOADING_BL)
        
        timeout = 20000
        # if (status & mask) and loopCount < 1000:
        #     IOLoop.current().add_callback(self.waitLoadBaseline, dataPath, loopCount + 1)
        # else:
        if not (status & mask) or loopCount > timeout:
            # loading complete
            if loopCount > timeout:
                logging.warning("Timed out waiting for Baseline to load. This may mean data is not being sent to the Histogrammer")
            self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_RUN_REG, 0)
            dataPath = dataPath & ~ (1 << 14) #TODO: TEMP MAGIC NUMBER, MATCHES HEXITEC_DATA_PATH_SHORT_BURST_MODE
            self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_DATA_PATH, dataPath)

            self.complete_start_run()
        else:
            IOLoop.current().add_callback(self.waitLoadBaseline, dataPath, loopCount + 1)


    def setClusterMode(self, clusterMode: defines.ClusterMode | None = None, autoTrigMode: defines.AutoTrigMode | None = None):
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

    def setClusterTypes(self, clusterType: defines.ClusterEnable = None):
        """Sets the cluster pattern type(s).
        
        :param clusterType: A Flag of all cluster patterns to enable, bitwise ORd together. If this value is 0, it is overwritten to the default that enables all patterns
        """

        if clusterType is None or clusterType not in defines.ClusterEnable:
            clusterType = defines.ClusterEnable.ALL

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

    def setHistFormat(self, numBins: defines.NumBins, runMode: defines.RunMode, mappedMode: defines.MappedMode):
        """Set the Format of the Histograms. Be aware, not all combinations of numBins, runMode, and mappedMode are permitted.
        
        :param numBins: Enum value that defines the number of energy bins
        :param runMode: Enum value that defines the run mode of the histogrammer, specifying what data to include
        :param mappedMode: Enum value that defines the Mapped Mode of the histogram.

        :raises InternalLibException: If the combination of numBins and runMode is invalid
        """

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
        # 192 << 24 | 168 << 16 | 2 << 8 |

        try:
            parts = [int(x) for x in ip.split(".")]
            if not len(parts) == 4:
                raise ValueError
            return parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3]
        except ValueError:
            return 0

    def setupUdpReceive(self, srcIP: str, destIP: str,
                 srcPort: int, destPort: int,
                 connectType: HexitecUdpRxConnection):
        """Setup the UDP cores to receive data
        
        :param srcIP: IP address of the data source (Likely the Alpha Data card)
        :param destIP: IP address the data is sent to (the address of the Histogrammer module)
        :param srcPort: Port number of the data source
        :param destPort: Port number of the histogrammer
        :param connectType: The type of connection, Normal, Loopback, or FromHost
        """

        srcIP_int = self.getIntfromIP(srcIP)
        destIP_int = self.getIntfromIP(destIP)

        self.hexitec.setGlobReg(defines.GlobalRegisters.GLB_DATA_PATH, (1 << 12))  # TODO: TEMP MAGIC NUMBER, MATCHES HEXITEC_DATA_PATH_ENB_FLUSH
        
        self.hexitec.setRxEthernetLoopback(0)  # disable ethernet loopback
        self._run_method(self.hexitec.udpRxSetup,
                         srcIP_int, destIP_int,
                         srcPort, destPort,
                         connectType)
        
        for i in range(self.hexitec.getNumRxUdp()):
            if self.hexitec.getGeneration() == defines.HexitecGeneration.HexitecGenHexitec:
                self.hexitec.setRxEthernetReg(i, 0x0020, 1)  # TODO: TEMP MAGIC NUMBER, MATCHES ETHERNET_PM_TICK_REG

        # disable any datamovers that might be running
        self._run_method(self.hexitec.stopDataMoverStreamUDP, 0)
        self._run_method(self.hexitec.stopDataMoverStreamUDP, 1)
        

    def setupUdpSend(self, srcIP: str, destIP: str,
                     srcPort: int, destPort: int,
                     numThreads: int, mappedMode: defines.MappedMode):
        """Setup the UDP cores to send Histograms to a server (usually an Odin Data instance)
        
        :param srcIP: THe IP address of the Histogrammer
        :param destIP: The IP address of the destination server, to send histograms to
        :param srcPort: Port number of the Histogrammer
        :param destPort: Port number of the server. This will be the first port number if multiple threads are used
        :param numThreads: Number of UDP threads to use. Each will send to a sequential Port Number in a round robin. Must be a power 2 value
        :param mappedMode: Enum value that defines the Mapped Mode of the histogram.
        """

        srcIP_int = self.getIntfromIP(srcIP)
        destIP_int = self.getIntfromIP(destIP)
        
        if not (numThreads & (numThreads - 1) == 0 and numThreads > 0 and numThreads < (1<<8)):
            raise InternalLibException("Invalid Number of UDP RX Threads: {}. Must be power of 2".format(numThreads))


        farmBase = 0
        timeframe_start = -1  #TODO: this resets the start number of the timeframes every time. May not be the intended method
        farmMask = numThreads - 1

        if mappedMode == defines.MappedMode.INTERLEAVE:
            numThreads = numThreads * 2  # mapped interleave mode requires threads for spectra and mapped
        logging.debug("Setting up UDP Tx With The Following settings:")
        logging.debug("Source IP: {}, Source Port: {}, Dest IP: {}, DestPort: {}".format(srcIP_int, srcPort, destIP_int, destPort))
        self._run_method(self.hexitec.udpTxSetup,
                         srcIP_int, destIP_int,
                         srcPort, destPort,
                         0, numThreads,
                         True, self.inter_frame_gap, False)
        
        # trailer mode is not disabled (becasue False), but the default values are set by this method
        self.hexitec.disableDataMoverUDPTrailer(False, 0)

        autoMode = XDmaHexitec.AutonomousMode.AutoTriggerReadAndClear
        farmIndex = XDmaHexitec.FarmIndexMode.FarmIndexFromTF

        #TODO: check for no_clear/UDP_dist to modify autoMode/farmIndex

        # if mapped mode is set to allow spectra (either mappedMode OFF or mappedMode INTERLEAVE)
        if mappedMode != defines.MappedMode.ONLY:
            # setup data mover farm mode for Spectra
            logging.debug("Setting up UDP DataMover for Spectra Output")
            self._run_method(self.hexitec.startDataMoverStreamUDP, 
                             timeframe_start, XDmaHexitec.MappedView.Spectra, False,
                             True, 0, farmMask, farmBase, autoMode, farmIndex)
            farmBase = farmBase + farmMask + 1
        
        # if mapped mode is set to allow Mapped output (ONLY or INTERLEAVE)
        if mappedMode != defines.MappedMode.OFF:
            logging.debug("Setting up UDP Datamover for Mapped Output")
            self._run_method(self.hexitec.startDataMoverStreamUDP,
                             timeframe_start, XDmaHexitec.MappedView.Mapped16, False,
                             True, 1, farmMask, farmBase, autoMode, farmIndex)

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
            frameCount.append(self.hexitec.getGlobReg(defines.GlobalRegisters.GLB_FRAME_COUNT0 + (2*i)))
            rawCount.append(self.hexitec.getGlobReg(defines.GlobalRegisters.GLB_RAW_HIT_COUNT0 + (2*i)))


        frameToken = self.hexitec.getFlushedFrame()
        inputTimeFrame = self.hexitec.getInpTimeFrame(0)

        # mask to get the count from the register value
        inputTimeFrame = inputTimeFrame & defines.TimeFrameMasks.INPUT_COUNT

        # check valid bit of frameToken
        if frameToken & defines.TimeFrameMasks.FLUSHED_VALID:
            finishedTimeFrame = frameToken & defines.TimeFrameMasks.FLUSHED_COUNT

        return Counters(frameCount[0],
                        sum(rawCount),
                        inputTimeFrame,
                        finishedTimeFrame)
    
    def getItfgStatus(self):
        """Read the Time Frame Generator statuses. End an acquisition if the status reads FINISHED"""

        
        # self.hexitec.iTfgReadStatus(stat) # passing the python class as a struct pointer didnt seem to work
        # so we read the registers manually
        status = self.hexitec.getGlobReg(defines.GlobalRegisters.GLB_RD_ITFG_STATUS)
        inpFrame = self.hexitec.getGlobReg(defines.GlobalRegisters.GLB_RD_ITFG_INP_FRAME)
        timeFrame = self.hexitec.getGlobReg(defines.GlobalRegisters.GLB_RD_ITFG_TIME_FRAME)
        cycles = self.hexitec.getGlobReg(defines.GlobalRegisters.GLB_RD_ITFG_CYCLES)
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
                         defines.Region.REGION_EDGE_POS_M, filename)

    def loadCShare_mc(self, filename: str):
        
        self._run_method(self.hexitec.loadCShareAsciiMC, self.chip_select, 
                         defines.Region.REGION_EDGE_POS_M, filename)

    def loadCShare_l3(self, filename: str):
        self._run_method(self.hexitec.loadCShareAscii, self.chip_select, 
                         defines.Region.REGION_L_POS_M, filename)
        
    def load_hdf_settings(self, filename: str):
        self.hexitec.loadSettingsHdf5(filename, self.chip_select, HexitecSaveRestore.All, self.lin_scale)

    def save_hdf_settings(self, filename: str):
        self.hexitec.saveSettingsHdf5(filename, self.chip_select, HexitecSaveRestore.All)