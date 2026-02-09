from typing import Final

MASK_HIST_FORMAT_NUMBINS: Final[int]
MASK_HIST_FORMAT_RUNMODE: Final[int]
MASK_HIST_FORMAT_MAPPEDMODE: Final[int]

MASK_BSUB_MODE: Final[int]
MASK_BSUB_DIV: Final[int]
MASK_BSUB_DITHER: Final[int]

MASK_CSHARE_ENB_EDGE: Final[int]
MASK_CSHARE_ENB_NEG: Final[int]
MASK_CSHARE_ENB_L_POS: Final[int]
MASK_CSHARE_DIS_SUM: Final[int]
MASK_CSHARE_DIS_ADJ: Final[int]

MASK_CLUSTER_MODE: Final[int]
MASK_CLUSTER_TRIG_MODE: Final[int]

def GET_THRES_NEG(x: int) -> int:
    """Wrapper around macro: (((x)>>16)&0x7FFF)"""
def GET_THRES_POS(x: int) -> int:
    """Wrapper around macro: ((x)&0x7FFF)"""

DATA_PATH_ENB_FLUSH: Final[int]
DATA_PATH_SHORT_BURST_MODE: Final[int]
ETHERNET_PM_TICK_REG: Final[int]
DM0_AUTO_TF: Final[int]