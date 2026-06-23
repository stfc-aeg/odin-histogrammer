import logging

from typing import Literal

from histogrammer.lib.defines import BaselineMask, BaselineDivide, BaselineChipVals
from histogrammer.lib.defines import Region, GlobalRegisters, ChipRegisters
from histogrammer.util import UsesHexitecLibrary, splitRegisterIntoValues

from histogrammer.lib import MASK_BSUB_DITHER, MASK_BSUB_DIV, MASK_BSUB_MODE
from histogrammer.lib import GET_THRES_POS, GET_THRES_NEG, DATA_PATH_SHORT_BURST_MODE

from histogrammer.adapter.base_handler import BaseHandler


class BaselineHandler(BaseHandler):
    """
    Object to handle the setup and control of the baseline subtraction options
    """

    # chip selection to -1, to apply to all chips
    chip_select = -1
    num_rows = 80
    num_cols = 80

    THRES_MAX = 4095
    """Maximum Threshold Value"""
    THRES_MIN = -4096
    """Minimum Threshold Value"""

    def __init__(self, options: dict[str, str]):
        super().__init__(options)

        self.divide = BaselineDivide.DIVIDE1024
        self.mask = BaselineMask.FIXED
        self.enableDither = False

        self.thres_main = [-35, 35]
        self.thres_low = [-25, 25]
        self.thres_abs = [1, 1000]

        self.param_tree = {
            "baseline": {
                "mask": (lambda: self.mask.name, self.setMask,
                         {"description": "Define when the baseline is updated",
                          "allowed_values": [val.name for val in BaselineMask]
                          }
                         ),
                "divide": (
                    lambda: int("".join(filter(str.isdigit, self.divide.name))),
                    self.setDivide,
                    {"description": "Define the Scaling Factor for the baseline feedback",
                     "allowed_values": [
                         int("".join(filter(str.isdigit, val.name))) for val in BaselineDivide]
                     }
                ),
                "dither": (lambda: self.enableDither, self.setDither,
                           {"description": "Enable Dithering on baseline correction"})
            },
            "thresholds": {
                "main": {
                    "neg": (
                        lambda: self.thres_main[0],
                        lambda val: self.setThreshold("main", [val, self.thres_main[1]]),
                        {
                            "description": (
                                "Main Trigger negative threshold. "
                                "A pixel value below this might trigger baseline adjustment"
                            ),
                            "min": self.THRES_MIN, "max": 0
                        }
                    ),
                    "pos": (
                        lambda: self.thres_main[1],
                        lambda val: self.setThreshold("main", [self.thres_main[0], val]),
                        {
                            "description": (
                                "Main Trigger positive threshold. "
                                "A pixel value above this might trigger baseline adjustment"
                            ),
                            "min": 0, "max": self.THRES_MAX
                        }
                    )
                },
                "low": {
                    "neg": (
                        lambda: self.thres_low[0],
                        lambda val: self.setThreshold("lower", [val, self.thres_low[1]]),
                        {
                            "description": (
                                "Lower Trigger negative threshold. "
                                "A pixel value below this might trigger baseline adjustment"
                            ),
                            "min": self.THRES_MIN, "max": 0
                        }
                    ),
                    "pos": (
                        lambda: self.thres_low[1],
                        lambda val: self.setThreshold("lower", [self.thres_low[0], val]),
                        {
                            "description": (
                                "Lower Trigger positive threshold. "
                                "A pixel value above this might trigger baseline adjustment"
                            ),
                            "min": 0, "max": self.THRES_MAX
                        }
                    ),
                },
                "absolute": {
                    "low": (
                        lambda: self.thres_abs[0],
                        lambda val: self.setThreshold("main", [val, self.thres_abs[1]]),
                        {
                            "description": (
                                "Absolute Trigger lower threshold. "
                                "A pixel value below this might trigger baseline adjustment"
                            ),
                            "min": 0, "max": self.THRES_MAX
                        }
                    ),
                    "high": (
                        lambda: self.thres_abs[1],
                        lambda val: self.setThreshold("main", [self.thres_abs[0], val]),
                        {
                            "description": (
                                "Absolute Trigger upper threshold. "
                                "A pixel value above this might trigger baseline adjustment"
                            ),
                            "min": 0, "max": self.THRES_MAX
                        }
                    )
                },
            }
        }

    def initialise(self, hexitec):
        super().initialise(hexitec)

        # initialise baseline Lookup table to all 0s
        # TODO: MAGIC NUMBERS FOR NUM_COLS/ROWS
        hexitec.setPixelLUT(self.chip_select, Region.BASELINE, 0,
                            self.num_cols, 0, self.num_rows, 0)

        baselineReg = hexitec.getChipReg(0, ChipRegisters.BASESUB)
        mask, div, dither = splitRegisterIntoValues(baselineReg,
                                                    MASK_BSUB_MODE,
                                                    MASK_BSUB_DIV,
                                                    MASK_BSUB_DITHER)

        self.divide = BaselineDivide(div)
        self.mask = BaselineMask(mask)
        self.enableDither = bool(dither)

        # thresholds
        abs_thres_read = self.hexitec.readPixelLUT(0, Region.ABS_THRES, 0, 1, 0, 1)[0]
        low_thres_read = self.hexitec.readPixelLUT(0, Region.LTHRES, 0, 1, 0, 1)[0]
        main_thres_read = self.hexitec.readPixelLUT(0, Region.MTHRES, 0, 1, 0, 1)[0]

        # must consider converting the uint16 value to a negative value for low and main
        neg_thres_offset = 0x2000  # seems to be the value to turn the unsigned value to signed

        self.thres_abs = [GET_THRES_NEG(abs_thres_read),
                          GET_THRES_POS(abs_thres_read)]
        self.thres_low = [GET_THRES_NEG(low_thres_read) - neg_thres_offset,
                          GET_THRES_POS(low_thres_read)]
        self.thres_main = [GET_THRES_NEG(main_thres_read) - neg_thres_offset,
                           GET_THRES_POS(main_thres_read)]

    def setMask(self, val: str):
        self.mask = BaselineMask[val]

        self.setBaseline()

    def setDivide(self, val: int):
        self.divide = BaselineDivide["DIVIDE{}".format(val)]
        self.setBaseline()

    def setDither(self, val: bool):
        self.enableDither = val
        self.setBaseline()

    @UsesHexitecLibrary()
    def setBaseline(self):

        # if the mask is set to fixed, we dont use the absolute trigger
        useAbsTrig = not (self.mask == BaselineMask.FIXED)

        self.hexitec.setBaselineMode(
            self.chip_select,
            self.mask, self.divide, self.enableDither,
            useAbsTrig)

    @UsesHexitecLibrary()
    def loadBaseline(self):
        """loading initial Baseline values"""
        logging.debug("Initialising Baseline")
        self.hexitec.setGlobReg(GlobalRegisters.RUN_REG, 0)
        for chip in range(self.hexitec.getNumChips()):
            # for each chip, toggle the baseline subtraction load bit

            baselineReg = self.hexitec.getChipReg(chip, ChipRegisters.BASESUB)
            # set LOAD bit in reg to 0
            self.hexitec.setChipReg(chip, ChipRegisters.BASESUB,
                                    baselineReg & ~BaselineChipVals.LOAD)
            # set LOAD bit in reg to 1
            self.hexitec.setChipReg(chip, ChipRegisters.BASESUB,
                                    baselineReg | BaselineChipVals.LOAD)

        # setup the datapath to feed into baseline
        dataPath = self.hexitec.getGlobReg(GlobalRegisters.DATA_PATH)
        dataPath = dataPath | DATA_PATH_SHORT_BURST_MODE

        self.hexitec.setGlobReg(GlobalRegisters.DATA_PATH, dataPath)
        self.hexitec.setGlobReg(GlobalRegisters.FRAME_BURST_LENGTH, 2)
        self.hexitec.setGlobReg(GlobalRegisters.RUN_REG, 1)

    @UsesHexitecLibrary()
    def isBaselineLoaded(self) -> bool:
        """Check if the baseline has finished loading"""

        mask = 0xFFFFFFFFFFFFFFF
        status = self.hexitec.getGlobReg64(GlobalRegisters.LOADING_BL)

        return (status & mask) == 0

    @UsesHexitecLibrary()
    def setThreshold(self, select: Literal["absolute", "main", "lower"],
                     val: list[int]):
        """
        Set the lower and upper values of the selected threshold variable
        """

        val.sort()
        if select == "absolute":
            self.thres_abs = val
            self.hexitec.setAbsTriggerThres(
                self.chip_select,
                0, self.num_cols,
                0, self.num_rows,
                max(val), min(val)
            )
        elif select == "lower":
            self.thres_low = val
            self.hexitec.setLowerTriggerThres(
                self.chip_select,
                0, self.num_cols,
                0, self.num_rows,
                max(val), min(val)
            )
        elif select == "main":
            logging.warning(("Setting Main Trigger Threshold. "
                             "This will overwrite any Bad Pixel Config"))
            self.thres_main = val
            self.hexitec.setMainTriggerThres(
                self.chip_select,
                0, self.num_cols,
                0, self.num_rows,
                max(val), min(val),
                True
            )
