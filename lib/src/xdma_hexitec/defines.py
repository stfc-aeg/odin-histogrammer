"""Various Enums and Flags defined by the source xdma_hexitec code"""
from enum import IntEnum, IntFlag


class HexitecGeneration(IntEnum):
    """Enum to define which Generation of the hexitec system is in use"""

    HexitecGenHexitec = 0
    """Hexitec 2x6"""

    HexitecGenMHz = 1
    """Hexitec Megahertz"""


class UdpRxConnection(IntEnum):
    """UDP Connection Type"""

    NORMAL = 0
    LOOPBACK = 1
    FROM_HOST = 2


class Region(IntEnum):
    """Defined Region Values """

    REGION_REGS           = 0
    REGION_ABS_THRES      = 1
    REGION_BASELINE       = 2
    REGION_MTHRES         = 3
    REGION_LTHRES         = 4
    REGION_LIN_A          = 5
    REGION_LIN_B          = 6
    REGION_LIN_C          = 7
    REGION_EDGE_POS_RECIP = 8
    REGION_EDGE_POS_M     = 9
    REGION_EDGE_POS_C     = 10
    REGION_NEG_NEB_RECIP  = 11
    REGION_NEG_NEB_M      = 12
    REGION_NEG_NEB_C      = 13
    REGION_ENG_MAP        = 14
    REGION_PIX_MASK       = 15
    REGION_L_POS_RECIP    = 16
    REGION_L_POS_M        = 17
    REGION_L_POS_C        = 18
    REGION_FIFO_COUNTS    = 31


class GlobalRegisters(IntEnum):
    """Defined Global Register Offsets"""

    GLB_DATA_PATH = 0
    GLB_RUN_REG = 1
    GLB_SCOPE_NUM_WORDS = 2
    GLB_SCOPE_GLOB_SRC = 3
    GLB_SCOPE_CHIP_SEL = 4
    GLB_FRAME_BURST_LENGTH = 5
    """Frame burst length register used to process a limited number of frames, generally to monitor baseline tracking."""

    GLB_ITFG_CONTROL = 8
    """Integrated time frame generator control register."""

    GLB_ITFG_INP_PER_TF = 9
    """Integrated time frame input detector frames per output time frame."""

    GLB_ITFG_NUM_TF = 10
    """Integrated time frame generator, number of output time frames"""

    GLB_ITFG_NUM_CYCLES = 12
    """Integrated time frame generator, number of time to cycle over all output time frames"""

    GLB_IRQ_ENB_RW = 16
    """Read/write access to the IRQ enable register"""

    GLB_IRQ_ENB_SET = 17
    """Write 1 to set access to the IRQ enable register"""

    GLB_IRQ_ENB_CLR = 18
    """Write 1 to clear access to the IRQ enable register"""

    GLB_FRAME_COUNT0 = 0x130
    """Hexitec MHz Frame Count (Chip 0)"""

    GLB_RAW_HIT_COUNT0 = 0x131
    """Hexitec MHz Raw Hit Count (Chip 0)"""


class BaselineMask(IntEnum):
    """Macros to describe control of how the baseline estimate is updated"""

    BSUB_MASK_NONE = 0
    """Never mask update, always apply feedback adjustment"""

    BSUB_MASK_MAIN = 1
    """Mask update when this pixel main trigger pos or neg over thres."""

    BSUB_MASK_LOW = 2
    """Mask update when this pixel Main or lower trigger pos or neg over thres."""

    BSUB_MASK_FIXED = 3
    """Never update, used Fixed baseline value with no adjustment"""

    BSUB_MASK_MAIN_OR4NEB = 5
    """Mask update when this pixel main or any of 4 neighbours  main triggers pos."""

    BSUB_MASK_MAIN_OR8NEB = 0xD
    """Mask update when this pixel main or any of 8 neighbours  main triggers pos."""

    BSUB_MASK_LOW_OR4NEB = (0x6)
    """Mask update when this pixel main or Low  or any of 4 neighbours  main triggers pos."""

    BSUB_MASK_LOW_OR8NEB = (0xE)
    """Mask update when this pixel main or Low  or any of 8 neighbours  main triggers pos."""

    BSUB_MASK_MAIN_OR_LOW_AND4NEB = (0x7)
    """Mask update when this pixel main or (low and any of 4 neighbours  main triggers pos)."""

    BSUB_MASK_MAIN_OR_LOW_AND8NEB = (0xF)
    """Mask update when this pixel main or (low and any of 8 neighbours  main triggers pos)"""


class BaselineDivide(IntEnum):
    """Macros to describe control of how the baseline error is scaled to update baseline estimate."""

    BSUB_DIVIDE256 = 0  # divide error feedback by 256, shift 8
    BSUB_DIVIDE512 = 1
    BSUB_DIVIDE1024 = 2
    BSUB_DIVIDE2048 = 3
    BSUB_DIVIDE4096 = 4
    BSUB_DIVIDE8192 = 5
    BSUB_DIVIDE16384 = 6
    BSUB_DIVIDE32768 = 7
    BSUB_DIVIDE65536 = 8


class ClusterMode(IntEnum):
    """Macros to describe how/which clusters of hist are chosen."""

    INDEPENDANT       = 0
    """Each pixel trigger independently, Charge sharing correction is bypassed"""

    LONE_POSITIVE    = 1
    """Trigger only on lone positive trigger, with no nearest neighbours"""

    LONE_POSITIVE_OR_NEGATIVE = 2
    """Trigger only on lone positive or negative trigger, with no nearest neighbours"""

    POSITIVE         = 3
    """Trigger on all recognised types of clusters with 1 to 4 positive main triggers"""

    POSITIVE_OR_NEGATIVE      = 4
    """Trigger on all recognised types of clusters with at least 1 positive trigger with positive or negative neighbours"""

    POSITIVE_OR_NEGATIVE_LOWER = 5
    """In addition to all POSNEG trigger, for single positive trigger, use lower threshold to look for one significant neighbour"""

    AUTO_OR_POSITIVE = 6
    """Pseduo randomly trigger to allow histogram of noise but also trigger using normal Positive event trigger to see events."""

    AUTO        = 7
    """Pseduo randomly trigger to allow histogram of noise"""


class AutoTrigMode(IntEnum):
    """Auto triggering modes for Cluster Mode"""

    AUTOTRIG_1IN16 = 0
    """Auto trigger mode triggers each pixel 1 frame in 16"""

    AUTOTRIG_1IN8 = 0
    """Auto trigger mode triggers each pixel 1 frame in 8"""

    AUTOTRIG_1IN4 = 0
    """Auto trigger mode triggers each pixel 1 frame in 4"""

    AUTOTRIG_1IN2 = 0
    """Auto trigger mode triggers each pixel 1 frame in 2,
    very fast for Hexitec MHz histogramming, probably OK for 6x2"""


class ClusterEnable(IntFlag):
    """Flags to enable various cluster patterns into the output data.
    These values can be bitwise OR'd together to enable multiple patterns"""

    LONE = (1 << 0)
    HOZ = (1 << 1)
    HOZ_NL = (1 << 2)
    HOZ_NR = (1 << 3)
    VERT = (1 << 4)
    VERT_NA = (1 << 5)
    VERT_NB = (1 << 6)
    DIAG1 = (1 << 7)
    DIAG1NL = (1 << 8)
    DIAG1NR = (1 << 9)
    DIAG2 = (1 << 10)
    DIAG2NL = (1 << 11)
    DIAG2NR = (1 << 12)
    L1 = (1 << 13)
    L2 = (1 << 14)
    L3 = (1 << 15)
    L4 = (1 << 16)
    QUAD = (1 << 17)
    ALL = (0x3FFFF)


class NumBins(IntEnum):
    """Define the number of Energy Bins"""

    ENG12    = 0
    """Make histograms with 4096 energy bins"""

    ENG11    = 1
    """Make histograms with 2048 energy bins"""

    ENG10    = 2
    """Make histograms with 1024 energy bins"""

    ENG9     = 3
    """Make histograms with 512 energy bins"""

    ENG8     = 4
    """Make histograms with 256 energy bins"""

    ENG7     = 5
    """Make histograms with 128 energy bins"""

    ENG10LSB = 6
    """Make histograms with 1024 energy bins, lowest 10 bits of energy"""


class RunMode(IntEnum):
    """Define the Run Mode"""

    NORMAL = 0
    """Normal Run Mode, includes Row and Column and energy bins"""

    ENERGY_ONLY = 1
    """0-d energy only run mode, Row and column removed"""

    ENERGY_POSITION_CLUSTER = 2
    """Debug mode with Cluster Class (4 bits), position, and energy bins"""

    ENERGY_CLUSTER = 3
    """Debug mode with Cluster Class (4 Bits) and energy bins"""

    CALIBRATION_MODE = 4
    """Calibration mode overlaying all pixels,
    4 bits of cluster class, 1024 bins of LUT addr,
    and energy bins"""

    CALIBRATION_SPECIAL = 5
    """Special Calibration Mode.
    
    Combined with NumBins.ENG10, it overlays all pixels, all 6 cluster type bits,
    1024 bins of LUT address, and 1024 Energy Bins

    Combined with NumBins.ENG9, it separates pixels, has 256 LUT address bins, and 512 Energy bins
    """

    CLUSTER_GRADE = 6
    """Run with Cluster Grade (1 bit), position, and energy bins"""

    CHARACTERISATION_PLOT = 7
    """Characterisation plot for specified number of pixel clusters.
    Not designed to be combined with NumBins"""


class MappedMode(IntEnum):
    """Define the Mapped Mode"""

    OFF = 0
    """Disable Mapped Mode"""

    ONLY = 1
    """Only Mapped Mode"""

    INTERLEAVE = 2
    """Interleaved Map Mode"""

class MappedView(IntEnum):
    """Define the spectra readout for a Data Mover"""

    SPECTRA = 0
    """Normal Spectra Readout"""

    MAPPED8 = 1
    """Read first 8 mapped spectra bins"""

    MAPPED16 = 2
    """Read all 16 mapped spectra bins"""

class AutoMode(IntEnum):
    """Define how the Data Mover might trigger output automatically"""

    OFF = 0
    """Do not automatically trigger output"""

    TRIGGER_READ = 1
    """Use TimeframeToken output from histogrammer to trigger autonomous output of data"""

    TRIGGER_READ_CLEAR = 2
    """Use TimeframeToken to trigger output of data. Also, clear the timeframe"""

class FarmIndexMode(IntEnum):
    """Define of the farm index increments for the datamover"""

    INC_EOF = 0
    """Farm Index increments at the end of every complete time frame on that queue."""

    INC_EOP = 1
    """Farm Index increments on every UDP packets, so data from a time frame is spread across ports/servers."""

    FROM_TF = 2
    """FarmIndex is equal to the LSBits of time frame, so fixed for a complete time frame."""

class TimeFrameMasks(IntFlag):
    """Mask values for extracting info from Time Frame Registers"""
    
    INPUT_COUNT   = 0x7FFFFFFFF
    """Extract Input Time Frame Count from register"""

    FLUSHED_VALID = (1 << 63)
    """Bit flag, used to verify Flushed Frame Validity"""

    FLUSHED_COUNT = 0xFFFFFFFFF
    """Extract Flushed Time Frame Count from register"""
