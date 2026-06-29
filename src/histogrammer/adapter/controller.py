import logging

from .base_controller import BaseError, BaseController

from functools import partial
from typing import get_args, Literal
from enum import Enum
from os import path, listdir

from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
from histogrammer.adapter.histogrammer import Histogrammer
from histogrammer.adapter.AcquisitionHandler import runStatus
from histogrammer.util import HexitecUnconnectedException, InternalLibException
from histogrammer.lib.defines import ClusterEnable, ClusterMode, AutoTrigMode, MappedMode
from histogrammer.lib.defines import RunMode, NumBins


class HistogramException(BaseError):
    """Simple exception class to wrap lower-level exceptions."""
    pass


class HistogramController(BaseController):
    """Controller class for the Histogrammer"""

    def __init__(self, options):
        super().__init__(options)

        self.histogrammer = Histogrammer(options)

        self.config_dir = options.get("config_dir", "test/config/files")

        if not path.exists(self.config_dir):
            logging.warning("Config File Directory not found: %s", self.config_dir)
        elif not path.isdir(self.config_dir):
            logging.warning("Config File Directory path is not a Directory: %s", self.config_dir)

        # ascii config filenames
        self.fname_badPixelTrig = ""
        self.fname_badPixelOut = ""
        self.fname_gain = ""
        self.fname_linearity = ""
        self.fname_cshare_pos = ""
        self.fname_cshare_pos_l3 = ""
        self.fname_cshare_pos_mc = ""

        self.fname_hdf = ""

        self.allowed_file_names = [""]
        if path.exists(self.config_dir) and path.isdir(self.config_dir):
            self.allowed_file_names.extend([f for f in listdir(self.config_dir)
                                            if path.isfile(path.join(self.config_dir, f)) and
                                            (f.endswith(".txt") or f.endswith(".h5"))])

            # numBins allowed values dict
        self.numBins_allowed = ["4096", "2048", "1024", "512", "256", "128", "1024 10 LSB"]

        tree_device = {
            "status": (lambda: self.histogrammer.acqHandler.runStatus, None,
                       {"allowed_values": list(get_args(runStatus))}),
            "device_num": (lambda: self.histogrammer.devNum, partial(self.setValue, "devNum")),
            "connect": (lambda: self.histogrammer.hexitec is not None, self.setConnect)
        }

        tree_acquisition = self.histogrammer.acqHandler.param_tree
        tree_acquisition["run"] = (
            lambda: self.histogrammer.acqHandler.runStatus == "running", self.setRun)

        tree_baseline = self.histogrammer.baselineHandler.param_tree["baseline"]
        tree_baseline["init_baseline"] = (
            lambda: self.histogrammer.initBaseline,
            partial(self.setValue, "initBaseline"),
            {"description": "Reset the baseline subtraction before starting a new acquisiton"}
        )
        tree_thresholds = self.histogrammer.baselineHandler.param_tree["thresholds"]
        tree_thresholds["bad_pixel"] = {
            "filename": (
                lambda: self.fname_badPixelTrig,
                partial(self.setValue, "fname_badPixelTrig"),
                {"allowed_values": self.allowed_file_names}
            ),
            "load": (None, lambda _: self.loadLUTAsciiFile("badPixelTrig"))
        }

        tree = {
            "device": tree_device,
            "acquisition": tree_acquisition,
            "udp": self.histogrammer.udpHandler.param_tree,
            "config": {
                "thresholds": tree_thresholds,
                "baseline": tree_baseline,
                "hdf_filename": (lambda: self.fname_hdf, partial(self.setValue, "fname_hdf")),
                "save_hdf": (None, self.save_hdf_settings),
                "load_hdf": (None, self.load_hdf_settings),

                # cluster mode, the cluster patterns used, and the trigger mode for pixels
                "clustering": {
                    "mode": (lambda: self.histogrammer.clusterMode.name,
                             partial(self.setCluster, "clusterMode"),
                             {"allowed_values": [val.name for val in ClusterMode]}),
                    "types": {  # dict comprehension to create bool param for each flag option
                        enb.name: (partial(
                            self.getClusterType, enb),
                            partial(self.setClusterType, enb)
                        ) for enb in ClusterEnable
                    },
                    "auto_trig_mode": (lambda: self.histogrammer.autoTrigMode.name,
                                       partial(self.setCluster, "autoTrigMode"),
                                       {"allowed_values": [val.name for val in AutoTrigMode]}
                                       )

                },
                "charge_sharing": {
                    "positive_edge": (
                        lambda: self.histogrammer.enbEdgePos,
                        partial(self.SetChargeSharing, "enbEdgePos")
                    ),
                    "negative_neighbour": (
                        lambda: self.histogrammer.enbNegNeb,
                        partial(self.SetChargeSharing, "enbNegNeb")
                    ),
                    "sum_enable": (
                        lambda: self.histogrammer.enbSumming,
                        partial(self.SetChargeSharing, "enbSumming")
                    ),
                    "position_adjust": (
                        lambda: self.histogrammer.enbAdjPosn,
                        partial(self.SetChargeSharing, "enbAdjPosn")
                    ),

                    "pos_filename": (
                        lambda: self.fname_cshare_pos,
                        partial(self.setValue, "fname_cshare_pos"),
                        {"allowed_values": self.allowed_file_names}
                    ),
                    "mc_filename": (
                        lambda: self.fname_cshare_pos_mc,
                        partial(self.setValue, "fname_cshare_pos_mc"),
                        {"allowed_values": self.allowed_file_names}
                    ),
                    "l3_filename": (
                        lambda: self.fname_cshare_pos_l3,
                        partial(self.setValue, "fname_cshare_pos_l3"),
                        {"allowed_values": self.allowed_file_names}
                    ),
                    "pos_load": (None, lambda _: self.loadLUTAsciiFile("cshare_pos")),
                    "mc_load": (None, lambda _: self.loadLUTAsciiFile("cshare_mc")),
                    "l3_load": (None, lambda _: self.loadLUTAsciiFile("cshare_l3"))

                },
                "linearity_correction": {
                    "offset": (lambda: self.histogrammer.lin_offset, self.setLinOffset),
                    "scale": (
                        lambda: self.histogrammer.lin_scale,
                        partial(self.setValue, "lin_scale")
                    ),
                    "gain_filename": (
                        lambda: self.fname_gain,
                        partial(self.setValue, "fname_gain"),
                        {"allowed_values": self.allowed_file_names}
                    ),
                    "lin_filename": (
                        lambda: self.fname_linearity,
                        partial(self.setValue, "fname_linearity"),
                        {"allowed_values": self.allowed_file_names}
                    ),
                    "gain_load": (None, lambda _: self.loadLUTAsciiFile("gain")),
                    "lin_load": (None, lambda _: self.loadLUTAsciiFile("linearity"))
                },
                "hist_format": {
                    "num_bins": (lambda: self.numBins_allowed[self.histogrammer.numBins],
                                 partial(self.setHistFormat, "numBins"),
                                 {"allowed_values": self.numBins_allowed}),
                    "run_mode": (lambda: self.histogrammer.runMode.name,
                                 partial(self.setHistFormat, "runMode"),
                                 {"allowed_values": [val.name for val in RunMode]}),
                    "mapped_mode": (lambda: self.histogrammer.mappedMode.name,
                                    partial(self.setHistFormat, "mappedMode"),
                                    {"allowed_values": [val.name for val in MappedMode]}),
                    "bad_pixel_mask": {  # load file to define which pixel output to mask out
                        "filename": (
                            lambda: self.fname_badPixelOut,
                            partial(self.setValue, "fname_badPixelOut"),
                            {"allowed_values": self.allowed_file_names}
                        ),
                        "load": (None, lambda _: self.loadLUTAsciiFile("badPixelOutput"))
                    }
                }
            }
        }

        self.paramTree = ParameterTree(tree)

    def get(self, path: str, with_metadata: bool = False):
        try:
            return self.paramTree.get(path, with_metadata)
        except (ParameterTreeError, InternalLibException, HexitecUnconnectedException) as error:
            logging.error(error)
            raise HistogramException(error)

    def set(self, path: str, data) -> None:
        try:
            self.paramTree.set(path, data)
        except (ParameterTreeError, InternalLibException, HexitecUnconnectedException) as error:
            logging.error(error)
            raise HistogramException(error)
        except AttributeError as error:
            logging.error(error)
            if self.histogrammer.hexitec is None:
                raise HistogramException("Histogrammer not connected")
            else:
                raise HistogramException(error)

    def initialize(self, adapters) -> None:
        return super().initialize(adapters)

    def cleanup(self) -> None:
        logging.debug("Shutting down Histogrammer")
        self.histogrammer.disconnect()

    def setValue(self, param: str, value):
        """Set the specified attribute on the Histogrammer object

        :param param: Name of the Attribute to set.
        :param value: Value to set the attribute to
        """
        if self.histogrammer.acqHandler.runStatus == "running":
            raise ParameterTreeError("Cannot change settings while an aquisition is running")
        if not hasattr(self.histogrammer, param):
            raise ParameterTreeError(
                "Histogrammer does not have an attribute called {}".format(param))
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

    def setThreshold(self, threshold: Literal["absolute", "main", "lower"],
                     low: int, high: int):
        """Set the threshold value, after checking if the value is within the bounds.
        Cannot use paramTree METADATA for this as they are tuples, not single values
        """
        value = (low, high)
        if (low > high):
            raise ParameterTreeError(
                "Threshold value invalid as values are in the wrong order: {}".format(value))
        if threshold == "absolute":
            self.histogrammer.thres_abs = value
        elif threshold == "lower":
            self.histogrammer.thres_low = value
        else:
            self.histogrammer.thres_main = value

        self.histogrammer.setTriggerThreshold(threshold,
                                              (0, self.histogrammer.NUM_COLS),
                                              (0, self.histogrammer.NUM_ROWS), value)

    def setCluster(self, setting: Literal["clusterMode", "autoTrigMode"],
                   value: str):
        """Set the Clustering Options in the histogrammer class


        :param setting: Which of the values are being set
        :param value: the string name of the value being set.
        Will be converted into the relevent ENUM value
        """

        if setting == "clusterMode":
            val: ClusterMode = ClusterMode[value]
        else:
            val: AutoTrigMode = AutoTrigMode[value]
        self.setValue(setting, val)
        self.histogrammer.setClusterMode()

    def setHistFormat(self, setting: Literal["numBins", "mappedMode", "runMode"], value: str):
        val: NumBins | MappedMode | RunMode = 0
        if setting == "numBins":
            try:
                index = self.numBins_allowed.index(value)
            except ValueError:
                index = 0
            val = NumBins(index)
        elif setting == "mappedMode":
            val = MappedMode[value]
        elif setting == "runMode":
            val = RunMode[value]
        else:
            raise HistogramException("Hist Format Setting invalid: {}".format(setting))

        self.setValue(setting, val)

        self.histogrammer.setHistFormat()

    def getClusterType(self, flag: ClusterEnable):

        return flag in self.histogrammer.clusterType

    def setClusterType(self, flag: ClusterEnable, value: bool):

        if value:
            # OR with flag val to set flag to 1
            self.histogrammer.clusterType = self.histogrammer.clusterType | flag
        else:
            # AND with inverted flag to set bit to 0
            self.histogrammer.clusterType = self.histogrammer.clusterType & ~flag

        self.histogrammer.setClusterTypes(self.histogrammer.clusterType)

    def setLinOffset(self, offset: float):
        self.histogrammer.lin_offset = offset
        self.histogrammer.addLinearityOffset(offset)

    def loadLUTAsciiFile(self,
                         setting: Literal["badPixelTrig", "badPixelOutput", "gain", "linearity",
                                          "cshare_pos", "cshare_mc", "cshare_l3"]):
        """Load various settings from ASCII files into their respective lookup tables

        :param setting: which lookup table to load into

        """
        # TODO: check for file existence/permissions before trying to use?

        if setting == "badPixelTrig":
            logging.debug("Disabling Triggers on Bad Pixels from file: %s",
                          self.fname_badPixelTrig)
            fullpath = path.join(self.config_dir, self.fname_badPixelTrig)

            self.histogrammer.loadBadPixelTrig(fullpath)
        elif setting == "badPixelOutput":
            logging.debug("Disabling Output on Bad Pixels from file: %s", self.fname_badPixelOut)
            fullpath = path.join(self.config_dir, self.fname_badPixelOut)
            self.histogrammer.loadBadPixelOutput(fullpath)
        elif setting == "gain":
            logging.debug("Setting Gain LUT from file: %s", self.fname_gain)
            fullpath = path.join(self.config_dir, self.fname_gain)
            self.histogrammer.loadGainCorrection(fullpath)
        elif setting == "linearity":
            logging.debug("Setting Linearity LUT from file: %s", self.fname_linearity)
            fullpath = path.join(self.config_dir, self.fname_linearity)
            self.histogrammer.loadLinearityCorrection(fullpath)
        elif setting == "cshare_pos":
            logging.debug("Setting Charge Sharing Correction LUTS from file: %s",
                          self.fname_cshare_pos)
            fullpath = path.join(self.config_dir, self.fname_cshare_pos)
            self.histogrammer.loadCShare_pos(fullpath)
        elif setting == "cshare_mc":
            logging.debug("Setting Charge Sharing Correction MC LUTS from file: %s",
                          self.fname_cshare_pos_mc)
            fullpath = path.join(self.config_dir, self.fname_cshare_pos_mc)
            self.histogrammer.loadCShare_mc(fullpath)
        elif setting == "cshare_l3":
            logging.debug("Setting Charge Sharing Correction L3 LUTS from file: %s",
                          self.fname_cshare_pos_l3)
            fullpath = path.join(self.config_dir, self.fname_cshare_pos_l3)
            self.histogrammer.loadCShare_l3(fullpath)

    def SetChargeSharing(self,
                         setting: Literal["enbEdgePos", "enbNegNeb", "enbSumming", "enbAdjPosn"],
                         value: bool):
        self.setValue(setting, value)
        self.histogrammer.setCShare()

    def save_hdf_settings(self, _):
        self.histogrammer.save_hdf_settings(self.fname_hdf)

    def load_hdf_settings(self, _):
        self.histogrammer.load_hdf_settings(self.fname_hdf)
