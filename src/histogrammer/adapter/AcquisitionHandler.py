import logging

from functools import partial
from typing import Literal, get_args
from dataclasses import dataclass
from datetime import datetime

from tornado.ioloop import PeriodicCallback, IOLoop

from histogrammer.lib import XDmaHexitec, CircularHdfWriter
from histogrammer.lib import CircWriterReadoutMode, CircWriterUdpTxOnlyMode, HexitecITfgMode
from histogrammer.util import UsesHexitecLibrary, AcquisitionMode, OutputMode, HexitecUnconnectedException
from histogrammer.adapter.base_handler import BaseHandler
from histogrammer.lib.defines import GlobalRegisters, TimeFrameStatus, TimeFrameMasks


runStatus = Literal["disconnected", "idle", "configuring", "running", "completed"]


@dataclass
class FrameCounts:
    frameCount: int
    rawHitCount: int
    inputTimeFrame: int
    finishedTimeFrame: int


@dataclass
class ITFGStatus:
    status: Literal["INVALID", "RUNNING", "PAUSED", "FINISHED"] = "INVALID"
    input_frame: int = 0
    output_frame: int = 0
    cycles: int = 0


class AcquisitionHandler(BaseHandler):
    """
    Object to handle running an acquisition of the Hextiec Histogrammer
    """

    def __init__(self, options: dict[str, str]):
        super().__init__(options)

        self.acqMode: AcquisitionMode = options.get("acq_mode", "count frames")
        self.outputMode: OutputMode = options.get("output_mode", "UDP")
        self.outputFile = ""

        self.runTimer = int(options.get("run_timer", 0))
        self.inpFrames = int(options.get("itfg_input", 2000000))
        self.outFrames = int(options.get("itfg_output", 20))

        self.runStatus: runStatus = "disconnected"

        self.itfg_status = ITFGStatus()
        self.frame_counters = FrameCounts(0, 0, 0, 0)

        self.hdfWriter: CircularHdfWriter = None

        self.counter_callback = PeriodicCallback(
            self.readCountersCallback, 100
        )

        self.itfg_callback = PeriodicCallback(
            self.readITFGStatusCallback, 100
        )

        self.acqTimeout = None

        self.param_tree = {
            "mode": (lambda: self.acqMode, partial(setattr, self, "acqMode"),
                     {"allowed_values": list(get_args(AcquisitionMode))}),
            "duration": (lambda: self.runTimer, partial(setattr, self, "runTimer"),
                         {"min": 0}),
            "input_frames": (lambda: self.inpFrames, partial(setattr, self, "impFrames"),
                             {"min": 0}),
            "output_frames": (lambda: self.outFrames, partial(setattr, self, "outFrames"),
                              {"min": 0}),
            "count": {
                "detector_frames": (lambda: self.frame_counters.frameCount, None),
                "raw_hits": (lambda: self.frame_counters.rawHitCount, None),
                "udp_frames": (lambda: self.frame_counters.inputTimeFrame, None),
                "complete_time_frames": (lambda: self.frame_counters.finishedTimeFrame, None)
            },
            "itfg": {
                "status": (lambda: self.itfg_status.status, None),
                "remaining_in": (
                    lambda: self.itfg_status.input_frame, None,
                    {"description":
                     "The number of input frames remaining for the current Histogram"}),
                "num_out": (lambda: self.itfg_status.output_frame, None,
                            {"description": "The Number of Histograms created"})
            },
            "output_mode": (lambda: self.outputMode, partial(setattr, self, "outputMode"),
                            {"allowed_values": list(get_args(OutputMode))}),
            "output_file": (lambda: self.outputFile, partial(setattr, self, "outputFile"))
        }

    def initialise(self, hexitec: XDmaHexitec):
        super().initialise(hexitec)
        self.runStatus = "idle"

    def cleanup(self):
        try:
            self.stopRun()
        except HexitecUnconnectedException:
            pass
        super().cleanup()

    def readCountersCallback(self):
        self.frame_counters = self.getFrameCounter()

    def readITFGStatusCallback(self):
        self.itfg_status = self.getItfgStatus()

        if self.itfg_status.status == "FINISHED":
            logging.debug("ITFG STATUS FINISHED")
            self.runStatus = "completed"
            self.itfg_callback.stop()

    @UsesHexitecLibrary()
    def getItfgStatus(self) -> ITFGStatus:
        """
        Read the Internal Time Frame Generator Status registers.

        :return ITFGStatus.input_frame: Number of input frames remaining for the current output TF
        :return ITFGStatus.output_frame: Number of frames output by the histogrammer
        :return ITFGStatus.cycles: Number of Cycles completed
        :return ITFGStatus.status: Current status of the ITFG
        """
        stat = ITFGStatus()

        stat.input_frame = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_INP_FRAME)
        stat.output_frame = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_TIME_FRAME)
        stat.cycles = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_CYCLES)

        try:
            status = self.hexitec.getGlobReg(GlobalRegisters.GLB_RD_ITFG_STATUS)
            stat.status = TimeFrameStatus(status).name
        except ValueError:
            stat.status = "INVALID"
        if stat.output_frame != self.itfg_status.output_frame:
            logging.debug("ITFG: New Output Frame %d", stat.output_frame)
        return stat

    @UsesHexitecLibrary()
    def getFrameCounter(self) -> FrameCounts:
        """
        Read the current count of frames for an in-progress run.
        Reading these values after a run has been stopped will return invalid values.

        :return FrameCounts.frameCount: Total frames from the detector
        :return FrameCounts.rawHitCount: Total number of raw hits from the detector
        :return FrameCounts.inputTimeFrame: Total number of Timeframes from the UDP input
        :return FrameCounts.finishedTimeFrame: Count of Finished Time Frames that have been output,
        or -1 if invalid
        """

        frameCount = []
        rawCount = []
        inputTimeFrame = -1
        finishedTimeFrame = -1

        for i in range(self.hexitec.getNumChips()):
            frameCount.append(self.hexitec.getGlobReg(GlobalRegisters.GLB_FRAME_COUNT0 + (2*i)))
            rawCount.append(self.hexitec.getGlobReg(GlobalRegisters.GLB_RAW_HIT_COUNT0 + (2*i)))

        frameToken = self.hexitec.getFlushedFrame()
        inputTimeFrame = self.hexitec.getInpTimeFrame(0) & TimeFrameMasks.INPUT_COUNT

        if frameToken & TimeFrameMasks.FLUSHED_VALID:
            finishedTimeFrame = frameToken & TimeFrameMasks.FLUSHED_COUNT

        return FrameCounts(frameCount[0], sum(rawCount), inputTimeFrame, finishedTimeFrame)

    @UsesHexitecLibrary()
    def setupHdfWriter(self):
        """
        Setup the Circular HDF Writer with the filename, and readout modes, then start it running.
        """
        self.runStatus = "configuring"
        readoutMode = CircWriterReadoutMode.IrqMemMapped if self.hexitec.supportsIrqs(
        ) else CircWriterReadoutMode.PolledMemMapped

        self.hdfWriter = CircularHdfWriter(self.hexitec, self.outputFile, -1, True, True, True)
        self.hdfWriter.setupReadoutMode(readoutMode, 1, CircWriterUdpTxOnlyMode.TxNormal)
        self.hdfWriter.start()

    @UsesHexitecLibrary()
    def startRun(self):
        """
        Setup the Run, configuring the histogrammer depending on run Mode
        """
        self.runStatus = "configuring"

        if self.acqMode == "count frames":
            logging.debug("Setup Internal Time Frame Generator")
            self.hexitec.iTfgSetup(HexitecITfgMode.SWFirst, 1, True,
                                   self.inpFrames, self.outFrames, 1)
            logging.debug("Using %d frames to output %d Histograms",
                          self.inpFrames*self.outFrames, self.outFrames)
        else:
            logging.debug("Disabling ITFG")
            self.hexitec.iTfgDisable()

        start = datetime.now()
        self.hexitec.clearHistAll()
        end = datetime.now()
        logging.warning("Histogram Clearing took %f seconds", (end - start).total_seconds())

        self.hexitec.enableHist()

        self.hexitec.setGlobReg(GlobalRegisters.GLB_RUN_REG, 1)
        self.runStatus = "running"

        if self.acqMode == "timed":
            logging.debug("Setting timer to end run in %d seconds", self.runTimer)
            self.acqTimeout = IOLoop.current().add_timeout(
                self.runTimer, setattr, self, "runStatus", "completed")

        if self.acqMode == "count frames":
            self.hexitec.iTfgTrigger()
            self.itfg_callback.start()
        self.counter_callback.start()

    @UsesHexitecLibrary()
    def stopRun(self):
        logging.debug("Stopping Run")

        if self.acqTimeout is not None:
            IOLoop.current().remove_timeout(self.acqTimeout)

        self.counter_callback.stop()
        self.itfg_callback.stop()

        self.frame_counters = self.getFrameCounter()
        self.hexitec.setGlobReg(GlobalRegisters.GLB_RUN_REG, 0)

        self.runStatus = "idle"

    def isHDFWriterComplete(self, lastTF: int):

        progress = self.hdfWriter.checkProgress(lastTF)

        if progress >= lastTF:
            mapOverRuns, spectraOverRuns = (
                self.hdfWriter.getMappedOverRuns(), self.hdfWriter.getSpectraOverRuns())
            if mapOverRuns or spectraOverRuns:
                logging.warning("HDF Writer detected %d Mapped overruns and %d spectra overruns",
                                mapOverRuns, spectraOverRuns)
            return True
        else:
            return False
