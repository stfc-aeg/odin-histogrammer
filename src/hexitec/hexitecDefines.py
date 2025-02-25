class HexitecDefines():

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

    CLUSTER_MODE_INDEP       = 0  # Each pixel trigger independently, Charge sharing correction is bypassed
    CLUSTER_MODE_LONE_POS    = 1  # Trigger only on lone positive trigger, with no nearest neighbours
    CLUSTER_MODE_LONE_POSNEG = 2  # Trigger only on lone positive or negative trigger, with no nearest neighbours
    CLUSTER_MODE_POS         = 3  # Trigger on all recognised types of clusters with 1 to 4 positive main triggers
    CLUSTER_MODE_POSNEG      = 4  # Trigger on all recognised types of clusters with at least 1 positive trigger with positive or negative neighbours
    CLUSTER_MODE_POSNEG_LOW  = 5  # In addition to all POSNEG trigger, for single positive trigger, use lower threshold to look for one significant neighbour
    CLUSTER_MODE_AUTO_OR_POS = 6  # Pseduo randomly trigger to allow histogram of noise but also trigger using normal Positive event trigger to see events.
    CLUSTER_MODE_AUTO        = 7  # Pseduo randomly trigger to allow histogram of noise

    HIST_SHIFT_ENG12    = 0  # Make histograms with 4096 energy bins 
    HIST_SHIFT_ENG11    = 1  # Make histograms with 2048 energy bins 
    HIST_SHIFT_ENG10    = 2  # Make histograms with 1024 energy bins 
    HIST_SHIFT_ENG9     = 3  # Make histograms with 512  energy bins 
    HIST_SHIFT_ENG8     = 4  # Make histograms with 256  energy bins 
    HIST_SHIFT_ENG7     = 5  # Make histograms with 128  energy bins
    HIST_SHIFT_ENG10LSB = 6  # Make histograms with 128  energy bins

    HIST_FORMAT_RUN12 = ((0 << 3) | HIST_SHIFT_ENG12)  # Normal run mode, Row, column and 4096 energy bins
    HIST_FORMAT_RUN11 = ((0 << 3) | HIST_SHIFT_ENG11)  # Normal run mode, Row, column and 2048 energy bins   
    HIST_FORMAT_RUN10 = ((0 << 3) | HIST_SHIFT_ENG10)  # Normal run mode, Row, column and 1024 energy bins   
    HIST_FORMAT_RUN9 = ((0 << 3) | HIST_SHIFT_ENG9)  # Normal run mode, Row, column and 512  energy bins   
    HIST_FORMAT_RUN8 = ((0 << 3) | HIST_SHIFT_ENG8)  # Normal run mode, Row, column and 256  energy bins   
    HIST_FORMAT_RUN7 = ((0 << 3) | HIST_SHIFT_ENG7)  # Normal run mode, Row, column and 128  energy bins   
    HIST_FORMAT_RUN10LSB = ((0 << 3) | HIST_SHIFT_ENG10LSB)  # Normal run mode, Row, column and bottom 10 bits of Energy (for lower energy experiments)

    HIST_MAPPED_MODE_OFF = 0
    HIST_MAPPED_MODE_ONLY = 1
    HIST_MAPPED_MODE_INTL = 2

    CLUSTER_ENB_LONE = (1 << 0)
    CLUSTER_ENB_HOZ = (1 << 1)
    CLUSTER_ENB_HOZ_NL = (1 << 2)
    CLUSTER_ENB_HOZ_NR = (1 << 3)
    CLUSTER_ENB_VERT = (1 << 4)
    CLUSTER_ENB_VERT_NA = (1 << 5)
    CLUSTER_ENB_VERT_NB = (1 << 6)
    CLUSTER_ENB_DIAG1 = (1 << 7)
    CLUSTER_ENB_DIAG1NL = (1 << 8)
    CLUSTER_ENB_DIAG1NR = (1 << 9)
    CLUSTER_ENB_DIAG2 = (1 << 10)
    CLUSTER_ENB_DIAG2NL = (1 << 11)
    CLUSTER_ENB_DIAG2NR = (1 << 12)
    CLUSTER_ENB_L1 = (1 << 13)
    CLUSTER_ENB_L2 = (1 << 14)
    CLUSTER_ENB_L3 = (1 << 15)
    CLUSTER_ENB_L4 = (1 << 16)
    CLUSTER_ENB_QUAD = (1 << 17)
    CLUSTER_ENB_ALL = (0x3FFFF)

    CLUSTER_ENB_EDGE_POS = CLUSTER_ENB_HOZ | CLUSTER_ENB_VERT
    CLUSTER_ENB_DIAG_POS = CLUSTER_ENB_DIAG1 | CLUSTER_ENB_DIAG2
    CLUSTER_ENB_EDGE_NEG = CLUSTER_ENB_HOZ_NL | CLUSTER_ENB_HOZ_NR | CLUSTER_ENB_VERT_NA | CLUSTER_ENB_VERT_NB
    CLUSTER_ENB_DIAG_NEG = CLUSTER_ENB_DIAG1NL | CLUSTER_ENB_DIAG1NR | CLUSTER_ENB_DIAG2NL | CLUSTER_ENB_DIAG2NR

    CLUSTER_ENB_CLUST2 = CLUSTER_ENB_EDGE_POS | CLUSTER_ENB_DIAG_POS | CLUSTER_ENB_EDGE_NEG | CLUSTER_ENB_DIAG_NEG
    CLUSTER_ENB_CLUST3 = CLUSTER_ENB_L1 | CLUSTER_ENB_L2 | CLUSTER_ENB_L3 | CLUSTER_ENB_L4

    GLB_DATA_PATH = 0
    GLB_RUN_REG = 1
    GLB_SCOPE_NUM_WORDS = 2
    GLB_SCOPE_GLOB_SRC = 3
    GLB_SCOPE_CHIP_SEL = 4
    GLB_FRAME_BURST_LENGTH = 5  # Frame busrt length register used to process a limited number of frames, generally to monitor baseline tracking.
    GLB_ITFG_CONTROL = 8  # Integrated time frame generator control register.
    GLB_ITFG_INP_PER_TF = 9  # Integrated time frame input detector frames per output time frame.
    GLB_ITFG_NUM_TF = 10  # Integrated time frame generator, number of output time frames
    GLB_ITFG_NUM_CYCLES = 12  # Integrated time frame generator, number of time to cycle over all output time frames
    GLB_IRQ_ENB_RW = 16  # Read/write access to the IRQ enable register
    GLB_IRQ_ENB_SET = 17  # Write 1 to set access to the IRQ enable register
    GLB_IRQ_ENB_CLR = 18  # Write 1 to clear access to the IRQ enable register

    GLB_SCOPE_STATUS = 0x114  # Status (over run) of scope mode FIFOs/DMAs and Burst running 
    GLB_RD_ITFG_STATUS = 0x116  # Word offset of Integrated time frame generator status register
    GLB_RD_ITFG_INP_FRAME = 0x117  # Monitor Count DOWN of specified number of detector frames to accumulate into current output frame
    GLB_TD_ITFG_TIME_FRAME = 0x118  # Monitor Count UP of current output time frame.
    GLB_TD_ITFG_CYCLES = 0x119  # Monitor Count UP of current output time frame.

    GLB_FRAME_COUNT = 0x130  # Hexitec MHz Frame Count (Chip 0)
    GLB_RAW_HIT_COUNT = 0x131  # Hexitec MHz Raw Hit Count  (Chip 0)
    GLB_REORDER_STATUS = 0x11A  # Hexitec 6x2: Reorder block status

    DATA_PATH_ENB_FLUSH = (1 << 12)  # Enable firmware initiate histogram cache flush at end of frame. Use for circular buffer mode. Do not use if cycling over frames multiple times.
    DATA_PATH_SHORT_BURST_MODE = (1 << 14)  # Enable (short) burst mode where only the number of frames specified by HEXITEC_GLB_FRAME_BURST_LENGTH are processed

    ETHERNET_GT_RESET_REG = 0x0000
    ETHERNET_RESET_REG    = 0x0004
    ETHERNET_MODE_REG     = 0x0008
    ETHERNET_RX_CONF_REG  = 0x0014
    ETHERNET_PM_TICK_REG  = 0x0020

    BASELINE_ENB = 1
    BASELINE_USE_REF = 2
    BASELINE_USE_ABS = 4

    BSUB_MASK_NONE = 0
    BSUB_MASK_MAIN = 1
    BSUB_MASK_LOW = 2
    BSUB_MASK_FIXED = 3
    BSUB_MASK_MAIN_OR4NEB = 5  # Mask update when this pixel main or any of 4 neighbours  main triggers pos.
    BSUB_MASK_MAIN_OR8NEB = 0xD  # Mask update when this pixel main or any of 8 neighbours  main triggers pos.
    BSUB_MASK_LOW_OR4NEB = (0x6)  # Mask update when this pixel main or Low  or any of 4 neighbours  main triggers pos.
    BSUB_MASK_LOW_OR8NEB = (0xE)  # Mask update when this pixel main or Low  or any of 8 neighbours  main triggers pos.
    BSUB_MASK_MAIN_OR_LOW_AND4NEB = (0x7)  # Mask update when this pixel main or (low and any of 4 neighbours  main triggers pos).
    BSUB_MASK_MAIN_OR_LOW_AND8NEB = (0xF)  # Mask update when this pixel main or (low and any of 8 neighbours  main triggers pos)

    BSUB_DIVIDE256 = 0  # divide error feedback by 256, shift 8
    BSUB_DIVIDE512 = 1
    BSUB_DIVIDE1024 = 2
    BSUB_DIVIDE2048 = 3
    BSUB_DIVIDE4096 = 4
    BSUB_DIVIDE8192 = 5
    BSUB_DIVIDE16384 = 6
    BSUB_DIVIDE32768 = 7
    BSUB_DIVIDE65536 = 8

    RUN_RUN = (1 << 0)                   # System Run bit, asserted to start system with normal (UDP) or placback data
    RUN_DIS_RESET_FRAME_COUNT = (1 << 1) # Disabel reset of fraem count at start of run, particularly for use with short bursts when debugging baseline settling

    FLUSHED_FRAME_VALID = (1 << 63)  # mask to determine if any time frame token has reached the histogram output this run
    FLUSHED_FRAME_GET = 0xFFFFFFFFF  # Extract flushed frame token from the output word

    SCOPE_STAT_RUNNING = (1<<31)  # Sysytem is running and has not reached the end of a short busrt.