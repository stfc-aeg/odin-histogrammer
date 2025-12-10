import logging

from .base_controller import BaseError, BaseController

from functools import partial
from typing import get_args, Literal, Type
from enum import Enum

from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
from histogrammer.histogrammer import Histogrammer, ConnectionStatus, InternalLibException
from xdma_hexitec.defines import ClusterEnable, ClusterMode, AutoTrigMode, MappedMode, RunMode, NumBins
from xdma_hexitec.defines import BaselineDivide, BaselineMask

class HistogramException(BaseError):
    """Simple exception class to wrap lower-level exceptions."""
    pass


class HistogramController(BaseController):
    """Controller class for the Histogrammer"""

    def __init__(self, options):
        super().__init__(options)

        self.histogrammer = Histogrammer(options)


        tree = {
            "device": {
                "status": (lambda: self.histogrammer.status, None,
                           {"allowed_values": list(get_args(ConnectionStatus))}),
                "device_num": (lambda: self.histogrammer.devNum, partial(self.setValue, "devNum")),
                "connect": (lambda: self.histogrammer.status != "disconnected", self.setConnect)
            },
            "acquisition": {
                "run": (lambda: self.histogrammer.status == "running", self.setRun),
                "timer": (lambda: self.histogrammer.runTimer, partial(self.setValue, "runTimer")),
                "input_frames": (lambda: self.histogrammer.input_frames, partial(self.setValue, "input_frames")),
                "output_frames": (lambda: self.histogrammer.output_frames, partial(self.setValue, "output_frames")),
                "count": {
                    "detector_frames": (lambda: self.histogrammer.frame_counters.frameCount, None),
                    "raw_hits": (lambda: self.histogrammer.frame_counters.rawHitCount, None),
                    "udp_frames": (lambda: self.histogrammer.frame_counters.inputTimeFrame, None),
                    "complete_time_frames": (lambda: self.histogrammer.frame_counters.finishedTimeFrame, None)
                }
            },
            "udp": {
                "setup": (None, lambda x: self.setupUDP()),
                "udp_threads": (self.histogrammer.numUDPThreads, partial(self.setValue, "numUDPThreads"),
                                {"allowed_values": [2**x for x in range(9)]}),
                "source": {
                    "ip": (lambda: self.histogrammer.source_ip, partial(self.setValue, "source_ip")),
                    "port": (lambda: self.histogrammer.source_port, partial(self.setValue, "source_port"))
                },
                "accelerator": {
                    "rx_ip": (lambda: self.histogrammer.accel_rx_ip, partial(self.setValue, "accel_rx_ip")),
                    "tx_ip": (lambda: self.histogrammer.accel_tx_ip, partial(self.setValue, "accel_tx_ip")),
                    "port": (lambda: self.histogrammer.accel_port, partial(self.setValue, "accel_port"))
                },
                "destination": {
                    "ip": (lambda: self.histogrammer.dest_ip, partial(self.setValue, "dest_ip")),
                    "port": (lambda: self.histogrammer.dest_port, partial(self.setValue, "dest_port"))
                }
            },
            "config": {
                "clustering": {  # cluster mode, the cluster patterns used, and the trigger mode for pixels
                    "mode": (lambda: self.enumToString(self.histogrammer.clusterMode),
                             partial(self.setCluster, "clusterMode"),
                             {"allowed_values": [self.enumToString(val) for val in ClusterMode]}),
                    "types": {  # dict comprehension to create bool param for each flag option
                        self.enumToString(enb): (partial(self.getClusterType, enb), partial(self.setClusterType, enb))
                        for enb in ClusterEnable
                    },
                    "auto_trig_mode": (lambda: self.enumToString(self.histogrammer.autoTrigMode),
                                       partial(self.setCluster, "autoTrigMode"),
                                       {"allowed_values": [self.enumToString(val) for val in AutoTrigMode]})

                },
                "hist_format": {
                    "num_bins": (lambda: self.histogrammer.numBins,
                                 partial(self.setHistFormat, "numBins"),
                                 {"allowed_values": [2**x for x in range(7, 13)]}),
                    "run_mode": (lambda: self.enumToString(self.histogrammer.runMode),
                                 partial(self.setHistFormat, "runMode"),
                                 {"allowed_values": [self.enumToString(val) for val in RunMode]}),
                    "mapped_mode": (lambda: self.enumToString(self.histogrammer.mappedMode),
                                    partial(self.setHistFormat, "mappedMode"),
                                    {"allowed_values": [self.enumToString(val) for val in MappedMode]})
                },
                "thresholds": {  # set the trigger thresholds for the three available triggers
                    "main": (lambda: self.histogrammer.thres_main,
                             partial(self.setThreshold, "main")),
                    "low": (lambda: self.histogrammer.thres_low,
                            partial(self.setThreshold, "lower")),
                    "absolute": (lambda: self.histogrammer.thres_main,
                                 partial(self.setThreshold, "main"))
                },
                "baseline": {
                    "mask": (lambda: self.enumToString(self.histogrammer.baselineMask),
                             partial(self.setBaselineMode, "baselineMask"),
                             {"allowed_values": [self.enumToString(val) for val in BaselineMask]}),
                    "divide": (lambda: int("".join(filter(str.isdigit, self.histogrammer.baselineDiv.name))),
                             partial(self.setBaselineMode, "baselineDiv"),
                             {"allowed_values": [int("".join(filter(str.isdigit, val.name))) for val in BaselineDivide]}),
                    "dither":(lambda: self.histogrammer.enableDither,
                              partial(self.setBaselineMode, "enableDither"))

                }
            }
        }

        self.paramTree = ParameterTree(tree)

    def get(self, path: str, with_metadata: bool = False):
        try:
            return self.paramTree.get(path, with_metadata)
        except (ParameterTreeError, InternalLibException) as error:
            logging.error(error)
            raise HistogramException(error)
        
    def set(self, path: str, data) -> None:
        try:
            self.paramTree.set(path, data)
        except (ParameterTreeError, InternalLibException) as error:
            logging.error(error)
            raise HistogramException(error)
        except AttributeError as error:
            logging.error(error)
            if self.histogrammer.hexitec is None or self.histogrammer.status == "disconnected":
                raise HistogramException("Histogrammer not connected")
            else:
                raise HistogramException(error)
        
    def initialize(self, adapters) -> None:
        return super().initialize(adapters)

    def cleanup(self) -> None:
        logging.debug("Shutting down Histogrammer")
        self.histogrammer.stop_run()
        self.histogrammer.disconnect()

    def enumToString(self, enumVal: Enum) -> str:
        val_name = enumVal._name_
        return val_name.lower().replace("_", " ")
    
    def stringToEnum(self, enumStr: str) -> str:
        return enumStr.upper().replace(" ", "_")


    def setValue(self, param: str, value):
        """Set the specified attribute on the Histogrammer object
        
        :param param: Name of the Attribute to set.
        :param value: Value to set the attribute to
        """
        if self.histogrammer.status == "running":
            raise ParameterTreeError("Cannot change settings while an aquisition is running")
        if not hasattr(self.histogrammer, param):
            raise ParameterTreeError("Histogrammer does not have an attribute called {}".format(param))
        setattr(self.histogrammer, param, value)

    def setConnect(self, connect: bool):
        if connect:
            self.histogrammer.connect()
        else:
            self.histogrammer.disconnect()

    def setRun(self, run: bool):
        if run:
            self.histogrammer.start_run()
        else:
            self.histogrammer.stop_run()

    def setupUDP(self):
        self.histogrammer.setupUdpReceive(
            self.histogrammer.source_ip, self.histogrammer.accel_rx_ip,
            self.histogrammer.source_port, self.histogrammer.accel_port,
            self.histogrammer.connectType
        )
        self.histogrammer.setupUdpSend(
            self.histogrammer.accel_tx_ip, self.histogrammer.dest_ip,
            self.histogrammer.accel_port, self.histogrammer.dest_port,
            self.histogrammer.numUDPThreads, self.histogrammer.mappedMode 
        )

    def setThreshold(self, threshold: Literal["absolute", "main" , "lower"],
                     value: tuple[int, int]):
        """Set the threshold value, after checking if the value is within the bounds.
        Cannot use paramTree METADATA for this as they are tuples, not single values
        """
        if threshold == "absolute":
            if value[0] < 0 or value[0] > value[1] or value[1] > self.histogrammer.THRES_MAX:
                raise ParameterTreeError(
                    "Absolute Threshold values must be positive and below the max {}: {} is invalid".format(
                        self.histogrammer.THRES_MAX, value))
            self.histogrammer.thres_abs = value
        elif threshold == "lower":
            if not (self.histogrammer.THRES_MIN <= value[0] <= 0) or not (0 <= value[1] <= self.histogrammer.THRES_MAX):
                raise ParameterTreeError(
                    "Lower Threshold values must be a Negative and Postiive value, and between the min/max values of {} to {}: {} is invalid".format(
                        self.histogrammer.THRES_MIN, self.histogrammer.THRES_MAX, value
                    ))
            self.histogrammer.thres_low = value
        else:
            if not (self.histogrammer.THRES_MIN <= value[0] <= 0) or not (0 <= value[1] <= self.histogrammer.THRES_MAX):
                raise ParameterTreeError(
                    "Main Threshold values must be a Negative and Postiive value, and between the min/max values of {} to {}: {} is invalid".format(
                        self.histogrammer.THRES_MIN, self.histogrammer.THRES_MAX, value
                    ))
            self.histogrammer.thres_main = value

        self.histogrammer.setTriggerThreshold(threshold,
                                              (0, self.histogrammer.NUM_COLS),
                                              (0, self.histogrammer.NUM_ROWS), value)

    def setCluster(self, setting: Literal["clusterMode", "autoTrigMode"],
                   value: str):
        """Set the Clustering Options in the histogrammer class
        

        :param setting: Which of the values are being set
        :param value: the string name of the value being set. Will be converted into the relevent ENUM value
        """

        if setting == "clusterMode":
            val: ClusterMode = ClusterMode[self.stringToEnum(value)]
        else:
            val: AutoTrigMode = AutoTrigMode[self.stringToEnum(value)]
        self.setValue(setting, val)
        self.histogrammer.setClusterMode()

    def setHistFormat(self, setting: Literal["numBins", "mappedMode", "runMode"], value: str | int):

        if setting == "numBins":
            lookup = {128: NumBins.ENG7,
                      256: NumBins.ENG8,
                      512: NumBins.ENG9,
                      1024: NumBins.ENG10,
                      2048: NumBins.ENG11,
                      4096: NumBins.ENG12}
            val: NumBins = lookup.get(value, NumBins.ENG10LSB)
        elif setting == "mappedMode":
            val = MappedMode[self.stringToEnum(value)]
        else:
            val = RunMode[self.stringToEnum(value)]
        
        self.setValue(setting, val)

        self.histogrammer.setHistFormat(self.histogrammer.numBins, self.histogrammer.runMode, self.histogrammer.mappedMode)


    def getClusterType(self, flag: ClusterEnable):

        return flag in self.histogrammer.clusterType
    
    def setClusterType(self, flag: ClusterEnable, value: bool):

        if value:
            self.histogrammer.clusterType = self.histogrammer.clusterType | flag  # OR with flag val to set flag to 1
        else:
            self.histogrammer.clusterType = self.histogrammer.clusterType & ~flag  # AND with inverted flag to set bit to 0

        self.histogrammer.setClusterTypes(self.histogrammer.clusterType)

    def setBaselineMode(self, setting: Literal["enableDither", "baselineDiv", "baselineMask"], value: str | bool | int):

        if setting == "baselineMask":
            val: BaselineMask = BaselineMask[self.stringToEnum(value)]
        elif setting == "baselineDiv":
            val: BaselineDivide = BaselineDivide["BSUB_DIVIDE{}".format(value)]
        else:
            val: bool = value

        self.setValue(setting, val)

        self.histogrammer.setBaseline()
        



