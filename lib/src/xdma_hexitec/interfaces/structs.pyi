from enum import IntEnum, IntFlag

class HexitecUdpRxConnection(IntEnum):
    """UDP Connection Type, defining where data is coming from"""

    Normal = 0
    Loopback = 1
    FromHost = 2

class HexitecITfgMode(IntEnum):
    """Enum defining the behavior of the ITFG, if used"""

    Immediate = 0
    """Run programmed burst of frames starting immediately"""

    SWFirst = 1
    """wait for rising edge of SWTrig and then run all the frames with no gaps."""

    SWCountedEach = 2
    """Wait for software trigger for each frame, accumulating programmed number of input frames and then discarding unused input frames until next SW trigger."""

    SWIncEach = 3
    """"Wait for software trigger for start of first frame, accumulating frames until next rising edge of SW Trig."""

    SWGated = 4     
    """Count While SW trigger is high, disable when increment time frame on falling edge."""

    HWFirst   = 9     
    """Wait HW Trig then run burst of nTF x nDetFrames """

    HWCountedEach = 10     
    """Wait for hardware trigger for each frame, accumulating programmed number of input frames and then discarding unused input frames until next trigger."""

    HWIncEach  = 11     
    """Wait for hardware trigger for start of first frame, accumulating frames until next rising edge of Trig."""

    HWGated   = 12      
    """Count While HW trigger is high, disable when increment time frame on falling edge."""

class HexitecSaveRestore(IntFlag):
    """Enum that defines what setting should be saved/loaded to and from a HDF5 settings file"""
    
    AbsThresPos = 0x1
    """Save/Load Absolute Threshold High value"""

    AbsThresNeg = 0x2
    """Save/Load Absolute Threshold Low Value"""

    AbsThres = 0x3
    """Save/Load both absolute threshold values"""

    MainThresPos = 0x4
    """Save/Load Main Threshold Positive value"""
    MainThresNeg = 0x8
    """Save/Load Main Threshold Negative value"""

    MainThres = 0xC
    """Save/Load both Main Threshold values"""

    TrigEnable = 0x10
    """Save/Load the Main threshold trigger enable"""

    LowThresPos = 0x20
    """Save/Load Low Threshold Positive value"""

    LowThresNeg = 0x40
    """Save/Load Low Threshold Negative value"""

    LowThres = 0x60
    """Save/Load both Low Threshold values"""

    LinCorr = 0x80
    """Save/Load Linarity Correction"""

    CShareEdgePos = 0x100
    """Save/Load Charge Sharing Edge Positive value"""

    CShareNegNeb = 0x200
    """Save/Load Charge Sharing Negative Neighbour value"""

    CShareLPos = 0x400
    """Save/Load Charge Sharing Edge Positive value"""

    CShareSpares = 0xF800
    """Reserved flag values for future Chare sharing values"""

    CShare = 0xFF00
    """Save/Load all charge sharing values"""

    OutputPixelMask = 0x10000
    """Save/Load Pixel Mask"""

    RequireAll = 0x80000000
    """Require all values to be saved/loaded"""

    All = 0x7FFFFFFF
    """Enable full flag"""

class CircWriterReadoutMode(IntEnum):
    """Readout Mode of the circular HDF Writer"""

    Unknown = 0

    PolledMemMapped = 1

    IrqMemMapped = 2

    AutoUDPThreadPerFrame = 3
    """UNSUPPORTED"""

    AutoUDPThreadPerPacket = 4
    """UNSUPPORTED"""

    AutoUDPNoTrailer = 5
    """UNSUPPORTED"""


class CircWriterUdpTxOnlyMode(IntEnum):

    TxNormal = 0
    TxOnlyLoop = 1
    TxOnly1Pass = 2

class HexitecITfgStat:
    """Struct container for reading the status of the ITFG"""

    status: int
    inpFrame: int
    timeFrame: int
    cycles: int

class DataMoverContext:
    """Struct container for Data Mover Context"""
    raw: list[int]
    """Raw context copied from BRAM"""
    readCredit: int
    """Unused readCredit issued by QDMA"""
    sixteenBitMode: int
    """16 bit mode specified by calling SW"""
    mappeView: int
    """Specify whether to read main spectra, first 8 bins of mapped spectra or all 16 bins of mapped spectra when using mapped mode."""
    sumChips: int
    """Specify whetehr to sum the EngOnly spectra over all 12 chips in Hexitec 6x2"""
    tfMode: int
    """Specify where time frame is supplied from (user SW or firmware in future)"""
    farmIndexMode: int
    """"""
    farmBase: int
    """First farm index to use (actually Ored with incrementing index)"""
    farmMask: int
    """Mask to select which (usually bottom few) bits of incrementing index are used in output farm idnex."""
    timeFrame: int
    """Timeframe specified by software or determined by firmware."""
    run: int
    """Run mode to enable activity. Set after all other bits are specified."""
    pixelColEng: int
    """Index incrementing through energy bin and pixel column"""
    pixelRow: int
    """Index of pixel row within chip. 0..79"""
    chipCol: int
    """Index of chip column 0..5"""
    chipRow: int
    """Index of chip row 0..1"""
    packetIndex: int
    """Pack index supplied to the QDMA."""