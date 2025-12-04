import logging

from .base_controller import BaseError, BaseController

from functools import partial
from typing import get_args, Literal
from enum import Enum

from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
from histogrammer.histogrammer import Histogrammer, ConnectionStatus, InternalLibException
from xdma_hexitec.defines import ClusterEnable, ClusterMode, AutoTrigMode

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
                "detector_frames": (lambda: self.histogrammer.frame_counters.frameCount, None),
                "raw_hits": (lambda: self.histogrammer.frame_counters.rawHitCount, None),
                "udp_frames": (lambda: self.histogrammer.frame_counters.inputTimeFrame, None),
                "complete_time_frames": (lambda: self.histogrammer.frame_counters.finishedTimeFrame, None)
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
                    "types": (None, None),
                    "auto_trig_mode": (lambda: self.enumToString(self.histogrammer.autoTrigMode),
                                       partial(self.setCluster, "autoTrigMode"),
                                       {"allowed_values": [self.enumToString(val) for val in AutoTrigMode]})

                },
                "hist_format": {
                    "bins": (None, None),
                    "run_mode": (None, None),
                    "mapped_mode": (None, None)
                },
                "thresholds": {  # set the trigger thresholds for the three available triggers
                    "main": (lambda: self.histogrammer.thres_main,
                             partial(self.setThreshold, "main")),
                    "low": (lambda: self.histogrammer.thres_low,
                            partial(self.setThreshold, "lower")),
                    "absolute": (lambda: self.histogrammer.thres_main,
                                 partial(self.setThreshold, "main"))
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
        
    def initialize(self, adapters) -> None:
        return super().initialize(adapters)

    def cleanup(self) -> None:
        logging.debug("Shutting down Histogrammer")
        self.histogrammer.stop_run()
        self.histogrammer.disconnect()

    def enumToString(self, enumVal: Enum) -> str:
        val_name = enumVal._name_
        val_name = val_name.lower()
        val_name = val_name.replace("_", " ")
        return val_name.lower().replace("_", " ")
    
    def stringToEnum(self, enumStr: str) -> str:
        return enumStr.upper().replace(" ", "_")


    def setValue(self, param, value):
        """Set the specified attribute on the Histogrammer object
        
        :param param: Name of the Attribute to set.
        :param value: Value to set the attribute to
        """
        if self.histogrammer.status == "running":
            raise ParameterTreeError("Cannot change settings while an aquisition is running")
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
            self.histogrammer.source_ip, self.histogrammer.accel_ip,
            self.histogrammer.source_port, self.histogrammer.accel_port,
            self.histogrammer.connectType
        )
        self.histogrammer.setupUdpSend(
            self.histogrammer.accel_ip, self.histogrammer.dest_ip,
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

        self.histogrammer.setTriggerThreshold(threshold, (0, 80), (0, 80), value)

    def setCluster(self, setting: Literal["clusterMode", "autoTrigMode", "clusterType"],
                   value: str):


        if setting == "clusterType":
            # TODO: Special Case cause its a set of flags. List all individually with toggles?
            val: ClusterEnable = ClusterEnable[self.stringToEnum(value)]
            self.setValue(setting, val)
            self.histogrammer.setClusterTypes(self.histogrammer.clusterType)
            return
        elif setting == "clusterMode":
            val: ClusterMode = ClusterMode[self.stringToEnum(value)]
        else:
            val: AutoTrigMode = AutoTrigMode[self.stringToEnum(value)]
        self.setValue(setting, val)
        self.histogrammer.setClusterMode(self.histogrammer.clusterMode, self.histogrammer.autoTrigMode)
