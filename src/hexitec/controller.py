import logging
from functools import partial
from odin.adapters.parameter_tree import ParameterTree
from odin.util import run_in_executor
import concurrent.futures

from .hexitec import Hexitec, TestRunInfo



class HexitecController():

    def __init__(self, options=None) -> None:

        self.runInfo = TestRunInfo(options)

        self.run_timer = 1

        self.hexitec = Hexitec(self.runInfo, options)
        self.param_tree = {}
        self._params = {
            "connect": {
                "useQdma": (lambda: self.hexitec.useQdma, partial(self.set_value, "useQdma")),
                "busNum": (lambda: self.hexitec.busNum, partial(self.set_value, "busNum")),
                "devNum": (lambda: self.hexitec.devNum, partial(self.set_value, "devNum")),
                "funcNum": (lambda: self.hexitec.funcNum, partial(self.set_value, "funcNum")),
                "connect": (None, self.hexitec.connect)

            },
            "control": {
                "prepare_run": (None, self.hexitec.setup_run),
                "run_timer": (lambda: self.hexitec.runTimer, self.set_run_timer),
                "start_run": (None, self.do_run),
                "stop_run": (None, self.stop_run),
                "status": (lambda: self.hexitec.status, None),
                "running_flag": (lambda: self.hexitec.running_flag, None),
                "frames": {
                    "detector_frames": (lambda: self.hexitec.frameCounter, None),
                    "raw_hits": (lambda: self.hexitec.totalHits, None),
                    "udp_frames": (lambda: self.hexitec.udpTF, None),
                    "complete_time_frames": (lambda: self.hexitec.completeTimeFrame, None)
                }
            },
            "config": {
                "thresholds":
                {
                    "main": {
                        "neg": (lambda: self.hexitec.mainThres[0], partial(self.set_threshold, self.hexitec.mainThres, 0)),
                        "pos": (lambda: self.hexitec.mainThres[1], partial(self.set_threshold, self.hexitec.mainThres, 1))
                    },
                    "low": {
                        "neg": (lambda: self.hexitec.lowThres[0], partial(self.set_threshold, self.hexitec.lowThres, 0)),
                        "pos": (lambda: self.hexitec.lowThres[1], partial(self.set_threshold, self.hexitec.lowThres, 1))
                    },
                    "abs": {
                        "low": (lambda: self.hexitec.absThres[0], partial(self.set_threshold, self.hexitec.absThres, 0)),
                        "high": (lambda: self.hexitec.absThres[1], partial(self.set_threshold, self.hexitec.absThres, 1))
                    },
                    "setup": (None, self.hexitec.setup_trigger_thresholds)
                },
                "cluster": {
                    "mode": {
                        "options": (list(self.hexitec.clusterMode_options.keys()), None),
                        "selected": (partial(self.get_dropdown, self.hexitec.clusterMode_options,
                                             "clusterMode"),
                                     partial(self.set_dropdown, self.hexitec.clusterMode_options,
                                             "clusterMode"))
                    },
                    "type": {
                        "options": (list(self.hexitec.clusterType_options.keys()), None),
                        "selected": (partial(self.get_dropdown, self.hexitec.clusterType_options,
                                             "clusterType"),
                                     partial(self.set_dropdown, self.hexitec.clusterType_options,
                                             "clusterType"))
                    }
                },
                "mapped_mode": {
                    "options": (list(self.hexitec.mappedMode_options.keys()), None),
                    "selected": (partial(self.get_dropdown, self.hexitec.mappedMode_options,
                                         "mappedMode"),
                                 partial(self.set_dropdown, self.hexitec.mappedMode_options,
                                         "mappedMode"))
                },
                "baseline": {
                    "bsubmask": {
                        "options": (list(self.hexitec.bsubMask_options.keys()), None),
                        "selected": (partial(self.get_dropdown, self.hexitec.bsubMask_options,
                                             "bsubMask"),
                                     partial(self.set_dropdown, self.hexitec.bsubMask_options,
                                             "bsubMask"))
                    },
                    "bsubDivide": {
                        "options": (list(self.hexitec.bsubDivide_options.keys()), None),
                        "selected": (partial(self.get_dropdown, self.hexitec.bsubDivide_options,
                                             "bsubDivide"),
                                     partial(self.set_dropdown, self.hexitec.bsubDivide_options,
                                             "bsubDivide"))
                    }
                },
                "settings_files":
                {
                    "CShareAscii": {
                        "filename": (lambda: self.hexitec.filenames["CShareAscii"][0], partial(self.setFilename, 'CShareAscii')),
                        "activate": (lambda: self.hexitec.filenames["CShareAscii"][1], partial(self.setFilenameActive, 'CShareAscii'))
                    },
                    "CShareMC": {
                        "filename": (lambda: self.hexitec.filenames["CShareAsciiMC"][0], partial(self.setFilename, "CShareAsciiMC")),
                        "activate": (lambda: self.hexitec.filenames["CShareAsciiMC"][1], partial(self.setFilenameActive, 'CShareAsciiMC'))
                    },
                    "CShareL3": {
                        "filename": (lambda: self.hexitec.filenames["CShareAsciiL3"][0], partial(self.setFilename, "CShareAsciiL3")),
                        "activate": (lambda: self.hexitec.filenames["CShareAsciiL3"][1], partial(self.setFilenameActive, 'CShareAsciiL3'))
                    },
                    "linearityGainHDF5": {
                        "filename": (lambda: self.hexitec.filenames["linearityGainHDF5"][0], partial(self.setFilename, "linearityGainHDF5")),
                        "activate": (lambda: self.hexitec.filenames["linearityGainHDF5"][1], partial(self.setFilenameActive, 'linearityGainHDF5'))
                    },
                    "gainAscii": {
                        "filename": (lambda: self.hexitec.filenames["gainAscii"][0], partial(self.setFilename, "gainAscii")),
                        "activate": (lambda: self.hexitec.filenames["gainAscii"][1], partial(self.setFilenameActive, 'gainAscii'))
                    },
                    "linearityAscii": {
                        "filename": (lambda: self.hexitec.filenames["linearityAscii"][0], partial(self.setFilename, "linearityAscii")),
                        "activate": (lambda: self.hexitec.filenames["linearityAscii"][1], partial(self.setFilenameActive, 'linearityAscii'))
                    },
                    "hdf5_list": (lambda: self.hexitec.settings_files, None),  # this is a list so need to do something smart to change
                    "load_settings_files": (None, self.hexitec.load_settings)
                },
                "save_files":
                {
                    "saveHdf5": {
                        "filename": (lambda: self.hexitec.filenames['save_hdf'][0], partial(self.setFilename, "save_hdf")),
                        "activate": (lambda: self.hexitec.filenames["save_hdf"][1], partial(self.setFilenameActive, 'save_hdf'))
                    },
                    "saveDet": {
                        "filename": (lambda: self.hexitec.filenames['save_det'][0], partial(self.setFilename, "save_det")),
                        "activate": (lambda: self.hexitec.filenames["save_det"][1], partial(self.setFilenameActive, 'save_det'))
                    },
                    "saveAsc": {
                        "filename": (lambda: self.hexitec.filenames['save_asc'][0], partial(self.setFilename, "save_asc")),
                        "activate": (lambda: self.hexitec.filenames["save_asc"][1], partial(self.setFilenameActive, 'save_asc'))
                    },
                    "save_settings": {
                        "filename": (lambda: self.hexitec.filenames['save_settings'][0], partial(self.setFilename, "save_settings")),
                        "activate": (lambda: self.hexitec.filenames["save_settings"][1], partial(self.setFilenameActive, 'save_settings'))
                    }
                },
                "itfg":
                {
                    "input_frames": (lambda: self.hexitec.itfg_input_frames, partial(self.set_value, "itfg_input_frames")),
                    "output_frames": (lambda: self.hexitec.itfg_output_frames, partial(self.set_value, "itfg_output_frames")),
                    "cycles": (lambda: self.hexitec.itfg_cycles, partial(self.set_value, "itfg_cycles"))
                },
                "udp":
                {
                    "send_udp": {
                        "selected": (partial(self.get_dropdown, self.hexitec.sendUdp_options, "sendUdp"),
                                     partial(self.set_dropdown, self.hexitec.sendUdp_options, "sendUdp")),
                        "options": (list(self.hexitec.sendUdp_options.keys()), None)
                    },
                    "tx": (self.get_ip_from_register(self.hexitec.test_accel_tx_ip_addr), None),
                    "rx": (self.get_ip_from_register(self.hexitec.test_server_ip_addr), None)
                },
                "hist_format":
                {
                    "bins": {
                        "selected": (partial(self.get_dropdown, self.hexitec.histFormat_bins_options, "histFormat_bins"),
                                     partial(self.set_dropdown, self.hexitec.histFormat_bins_options, "histFormat_bins")),
                        "options": (list(self.hexitec.histFormat_bins_options.keys()), None)
                    },
                    "run_mode": {
                        "selected": (partial(self.get_dropdown, self.hexitec.histFormat_runMode_options, "histFormat_runMode"),
                                     partial(self.set_dropdown, self.hexitec.histFormat_runMode_options, "histFormat_runMode")),
                        "options": (list(self.hexitec.histFormat_runMode_options.keys()), None)
                    }
                }

            }
        }

        self.executor = concurrent.futures.ThreadPoolExecutor()

    def set_run_timer(self, value):
        self.hexitec.runTimer = value

    def set_value(self, param, value):
        setattr(self.hexitec, param, value)

    def setFilename(self, filename, value):
        if filename in self.hexitec.filenames.keys():
            if isinstance(self.hexitec.filenames[filename], tuple):
                self.hexitec.filenames[filename][0] = value
            else:
                self.hexitec.filenames[filename] = value

    def setFilenameActive(self, filename, value):
        if filename in self.hexitec.filenames.keys():
            self.hexitec.filenames[filename][1] = value

    def init_tree(self):
        self.param_tree = ParameterTree(self._params)

    def get_clusterMode(self):
        for key, val in self.hexitec.clusterMode_options.items():
            if self.hexitec.clusterMode == val:
                return key
        return "unknown"
    
    def set_clusterMode(self, val):
        self.hexitec.clusterMode = self.hexitec.clusterMode_options[val]

    def get_bsubMask(self):
        for key, val in self.hexitec.bsubMask_options.items():
            if self.hexitec.bsubMask == val:
                return key
        return "unknown"
    
    def get_dropdown(self, dict: dict, param):
        value = getattr(self.hexitec, param)
        for key, val in dict.items():
            if value == val:
                return key
        return "unknown"
    
    def set_dropdown(self, dict, param, value):
        setVal = dict[value]
        setattr(self.hexitec, param, setVal)

    def do_run(self, val):
        self.hexitec.status = "running"
        self.hexitec.running_flag = True
        run_in_executor(self.executor, self.hexitec.do_run, None)

    def stop_run(self, val):
        run_in_executor(self.executor, self.hexitec.stop_run, None)

    def set_threshold(self, threshold, position, value):
        threshold[position] = value

    def get_ip_from_register(self, reg):
        parts = [0, 0, 0, 0]
        parts[0] = reg >> 24 & 0xFF
        parts[1] = reg >> 16 & 0xFF
        parts[2] = reg >> 8 & 0xFF
        parts[3] = reg & 0xFF
        return "{}.{}.{}.{}".format(*parts)

