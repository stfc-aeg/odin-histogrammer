import logging

from functools import partial
from typing import Literal, get_args
from dataclasses import dataclass
from datetime import datetime

from tornado.ioloop import PeriodicCallback, IOLoop

from histogrammer.lib import CircularHdfWriter
from histogrammer.lib import CircWriterReadoutMode, CircWriterUdpTxOnlyMode, HexitecITfgMode
from histogrammer.util import UsesHexitecLibrary, HexitecUnconnectedException
from histogrammer.util import AcquisitionMode, OutputMode, TriggerMode
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

        self.trigger_mode: TriggerMode = "software"
        self.num_histograms = int(options.get("itfg_output", 20))
        self.frames_per_histogram = int(options.get("itfg_input", 2000000))

        self.acqMode: AcquisitionMode = options.get("acq_mode", "count frames")
        self.outputMode: OutputMode = options.get("output_mode", "UDP")
        self.outputFile = ""

        self.runTimer = int(options.get("run_timer", 0))

        self.runStatus: runStatus = "disconnected"

        self.itfg_status = ITFGStatus()
        self.frame_counters = FrameCounts(0, 0, 0, 0)

        self.hdfWriter: CircularHdfWriter = None

        self.montiorCallback = PeriodicCallback(
            self.acqMonitoring, 100
        )

        self.acqTimeout = None

        self.param_tree = {
            "mode": (
                lambda: self.trigger_mode,
                partial(setattr, self, "trigger_mode"),
                {
                    "allowed_values": list(get_args(TriggerMode)),
                    "description": ("Define if we are using Hardware or Software triggering, to "
                                    "specify how we separate input into histograms.")
                }
            ),
            "frames_per_histogram": (
                lambda: self.frames_per_histogram,
                partial(setattr, self, "frames_per_histogram"),
                {"min": 1, "description": "Number of Raw Data frames expected per histogram"}
            ),
            "num_histograms": (
                lambda: self.num_histograms,
                partial(setattr, self, "num_histograms"),
                {"min": 1, "description": "Number of Histograms expected to be output"}
            ),
            "count": {
                "detector_frames": (
                    lambda: self.frame_counters.frameCount, None,
                    {"description": "Total frames from the detector"}
                ),
                "raw_hits": (
                    lambda: self.frame_counters.rawHitCount, None,
                    {"description": "Total number of raw hits from the detector"}
                ),
                "input_time_frames": (
                    lambda: self.frame_counters.inputTimeFrame, None,
                    {"description": "Total input Time Frames from the detector"}
                ),
                "output_time_frames": (
                    lambda: self.frame_counters.finishedTimeFrame, None,
                    {"description": "Count of completed Histograms"})
            },
            "itfg": {
                "status": (lambda: self.itfg_status.status, None),
                "remaining_in": (
                    lambda: self.itfg_status.input_frame, None,
                    {"description":
                     "The number of input frames remaining for the current Histogram"
                     }
                ),
                "num_out": (
                    lambda: self.itfg_status.output_frame, None,
                    {"description": "The Number of Histograms created"}
                )
            },
        }

    def initialise(self, hexitec):
        super().initialise(hexitec)
        self.runStatus = "idle"

    def cleanup(self):
        try:
            self.stopRun()
        except HexitecUnconnectedException:
            pass
        super().cleanup()

    def acqMonitoring(self):
        """Monitoring function, reads frame count info during an aquisition"""
        self.itfg_status = self.getItfgStatus()
        self.frame_counters = self.getFrameCounter()

        if self.trigger_mode == "software":
            if self.itfg_status.status == "FINISHED":
                # ITFG says we've finished. Good job ITFG
                logging.debug("SOFTWARE TRIGGER ACQ FINISHED")
                self.runStatus = "completed"
        else:
            if not self.frame_counters.finishedTimeFrame < self.num_histograms:
                # We've sent as many finished time frames as we said we wanted.
                logging.debug("HARDWARE TRIGGER ACQ FINISHED")
                self.runStatus = "completed"

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

        stat.input_frame = self.hexitec.getGlobReg(GlobalRegisters.RD_ITFG_INP_FRAME)
        stat.output_frame = self.hexitec.getGlobReg(GlobalRegisters.RD_ITFG_TIME_FRAME)
        stat.cycles = self.hexitec.getGlobReg(GlobalRegisters.RD_ITFG_CYCLES)

        try:
            status = self.hexitec.getGlobReg(GlobalRegisters.RD_ITFG_STATUS)
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
            frameCount.append(self.hexitec.getGlobReg(GlobalRegisters.FRAME_COUNT0 + (2*i)))
            rawCount.append(self.hexitec.getGlobReg(GlobalRegisters.RAW_HIT_COUNT0 + (2*i)))

        frameToken = self.hexitec.getFlushedFrame()
        inputTimeFrame = self.hexitec.getInpTimeFrame(0) & TimeFrameMasks.INPUT_COUNT

        if frameToken & TimeFrameMasks.FLUSHED_VALID:
            finishedTimeFrame = frameToken & TimeFrameMasks.FLUSHED_COUNT

        return FrameCounts(frameCount[0], sum(rawCount), inputTimeFrame, finishedTimeFrame)

    @UsesHexitecLibrary()
    def getLatestTimeFrame(self) -> int:
        """Get the latest Time Frame number either from flushed frames or from UDP"""
        lastTF = -1
        frameToken = self.hexitec.getFlushedFrame()
        if frameToken & TimeFrameMasks.FLUSHED_VALID:
            lastTF = frameToken & TimeFrameMasks.FLUSHED_COUNT
        else:
            lastTF = self.hexitec.getInpTimeFrame(0) & TimeFrameMasks.INPUT_COUNT

        return lastTF

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

        if self.trigger_mode == "software":
            logging.debug("Setup Internal Time Frame Generator")
            self.hexitec.iTfgSetup(HexitecITfgMode.SWFirst, 1, True,
                                   self.frames_per_histogram, self.num_histograms, 1)
            logging.debug("Using %d frames to output %d Histograms",
                          self.frames_per_histogram * self.num_histograms,
                          self.num_histograms)
        else:
            logging.debug("Disabling ITFG")
            self.hexitec.iTfgDisable()

        start = datetime.now()
        self.hexitec.clearHistAll()
        end = datetime.now()
        logging.warning("Histogram Clearing took %f seconds", (end - start).total_seconds())

        self.hexitec.enableHist()

        self.hexitec.setGlobReg(GlobalRegisters.RUN_REG, 1)
        self.runStatus = "running"

        if self.trigger_mode == "software":
            # delay iTfgTrigger by half a second, to allow baseline to settle
            IOLoop.current().call_later(.5, self.hexitec.iTfgTrigger)
        self.montiorCallback.start()

    @UsesHexitecLibrary()
    def stopRun(self):
        logging.debug("Stopping Run")

        if self.acqTimeout is not None:
            IOLoop.current().remove_timeout(self.acqTimeout)

        self.montiorCallback.stop()

        self.frame_counters = self.getFrameCounter()
        self.hexitec.setGlobReg(GlobalRegisters.RUN_REG, 0)

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
