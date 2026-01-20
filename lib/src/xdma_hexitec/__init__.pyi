"""
XDMA Hexitec C++ Plugin
-----------------------

Provides a Python interface to the XDmaHexitec C++ library written by William Helsby
"""

from enum import IntEnum, IntFlag
from .defines import HexitecGeneration


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

class HexitecITfgStat:
    """Struct container for reading the status of the ITFG"""

    status: int
    inpFrame: int
    timeFrame: int
    cycles: int


class XDmaHexitec:
    """
    XDmaHexitec Class, connects to and manages the hexitec histogrammer hardware, providing access
    to various registers, DMA buffers, and other such parts of the firmware.
    """

    class MappedView(IntEnum):
        Spectra = 0
        Mapped8 = 1
        Mapped16 = 2

    class AutonomousMode(IntEnum):
        AutoOff = 0
        AutoTriggerRead = 1
        AutoTriggerReadAndClear = 2

    class FarmIndexMode(IntEnum):
        FarmIndexIncEOF = 0
        FarmIndexIncEOP = 1
        FarmIndexFromTF = 2

    def __init__(self, useQdma: bool, busNum: int, devNum: int, funcNum: int):
        """
        Initialise the XDmaHexitec class, connected to the defined XDMA device

        :param useQdma: Enables the use of QDMA instead of XDMA for DMA access
        :param busNum: The BusID of the PCI address for the device.
        :param devNum: the DeviceID of the PCI address for the device.
        :param funcNum: the FunctionID of the PCI address for the device.
        """

    def getNumChips(self) -> int:
        """Return the total number of chips in the Hexitec device"""

    def getNumChipsCols(self) -> int:
        """Return the number of chips in a column on the sensor"""
    
    def getNumChipsRows(self) -> int:
        """Return the number of chips in a row on the sensor"""

    def getMaxAdcValue(self) -> int:
        """Return the maximum ADC value possible"""

    def getBsubRefScale(self) -> int:
        """

        """
    def getNumHBMPorts(self) -> int:
        """

        """
    def getNumProcCol(self) -> int:
        """

        """
    def getHasFIFOMon(self) -> bool:
        """

        """
    def getGeneration(self) -> HexitecGeneration:
        """

        """
    def getMaxBitsClustGrade(self) -> int:
        """

        """
    def getRegionMask(self, region: int) -> int:
        """

        """
    def getNumPbDma(self) -> int:
        """

        """
    def getNumScopeDma(self) -> int:
        """

        """
    def getNBitsAddrPWLin(self) -> int:
        """

        """
    def getNumRxUdp(self) -> int:
        """Gets the number of UDP RX Cores"""

    def getNumTxUdp(self) -> int:
        """Gets the number of UDP TX Cores"""

    def writeChipRegs(self, chip: int) -> None:
        """

        """
    def readChipRegs(self, chip: int, region: int, offset: int, num: int, data: list[int]) -> None:
        """

        """
    def setChipReg(self, chip: int, offset: int, value: int) -> None:
        """

        """
    def getChipReg(self, chip: int, offset: int) -> int:
        """Get Value of Chip Register

        """
    def writeGlobRegs(self) -> None:
        """

        """
    def readGlobRegs(self) -> None:
        """

        """
    def setGlobReg(self, offset: int, value: int) -> None:
        """Sets the value of a Global Register at the specified offset address
        
        
        :param offset: address of the register to write to
        :param value: the value to write into the register
        """

    def getGlobReg(self, offset: int) -> int:
        """Return the value of the 32 bit register at the specified offset address

        :param offset: address of the register to read

        """
    def getGlobReg64(self) -> None:
        """

        """
    def setPixelLUT(self,
                    chip: int,
                    region: int,
                    firstCol: int,
                    numCol: int,
                    firstRow: int,
                    numRow: int,
                    value: int) -> None:
        """Write a fixed value to multiple Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block.

        :param chip: Chip number  or -1 to duplicate to all chips.
        :param region: Region number, {@link HEXITEC_REGION_BASELINE} to {@link HEXITEC_REGION_LIN_C}
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols: Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows: Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param value: Value to write
        """

    def writePixelLUT(self, chip: int, region: int, firstCol: int, numCols: int, firstRow: int, numRows: int, data: list[int]) -> None:
        """Write array of value to a Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block
        The data is organised as data[numRows][numCols].

        :param chip:     Chip number  or -1 to duplicate to all chips. Note in this case the data array for 1 chip is is replicated for all chips
        :param region:   Region number, {@link HEXITEC_REGION_BASELINE} to {@link HEXITEC_REGION_LIN_C}
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols:  Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows:  Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param data:     Data to write. Note order.

        """
    def readPixelLUT(self, chip: int, region: int, firstCol: int, numCols: int, firstRow: int, numRows: int) -> list[int]:
        """Read array of values from a Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block
        The data is organised as data[numRows][numCols]

        :param chip:     Chip number.
        :param region:   Region number, {@link HEXITEC_REGION_BASELINE} to {@link HEXITEC_REGION_LIN_C}
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols:  Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows:  Number of rows of sensor 1..HEXITEC_NUM_ROWS.


        """
    def setPixelLin(self) -> None:
        """

        """
    def writePixelLin(self) -> None:
        """

        """
    def readPixelLin(self) -> None:
        """

        """
    def setSharedLUT(self, chip: int, region: int, stream: int, first: int, num: int, value: int) -> None:
        """Write fixed value (usually 0) to all locations of a Hexitec Charge Sharing correction LUTs. Currently shared across all pixels.

        :param chip: Chip number  or -1 to duplicate to all chips.
        :param region: Region number, see Region Enum for possible values
        :param stream: Processing stream (which handles 2 pairs of columns (total 4 columns). Usually -1 to replicate to all.
        :param first: First offset within LUT, usually 0,
        :param num: Number of point to write (usually full table)
        :param value: Value to write

        """
    def writeSharedLUT(self, chip: int, region: int, stream: int, first: int, num: int, data: list[int]) -> None:
        """Write array of value to a Hexitec Charge Sharing correction LUTs. Currently shred across all pixels.

        :param chip: Chip number  or -1 to duplicate to all chips. Note in this case the data array for 1 chip is is replicated for all chips
        :param region: Region number, see region Enum for possible values
        :param stream: Processing stream (which handles 2 pairs of columns (total 4 columns). Usually -1 to replicate to all.
        :param first: First offset within LUT, usually 0,
        :param num: Number of point to write (usually full table)
        :param data: Pointer to data to write.

        """
    def readSharedLUT(self, chip: int, region: int, stream: int, first: int, num: int) -> list[int]:
        """	Read array of values from a Hexitec Charge Sharing correction LUTs. Currently shared across all pixels.

        :param chip: Chip number.
        :param region: Region number, see Region Enum
        :param stream: Processing stream (which handles 2 pairs of columns (total 4 columns).
        :param first: First offset within LUT, usually 0,
        :param num: Number of point to read (usually full table)

        """
    def initRecipLUT(self,
                     chip: int,
                     region: int,
                     stream: int) -> None:
        """Initialise the Positive or Negative reciprocal Lookup Tables

        :param chip: Chip Number. -1 to apply to all chips
        :param region: Region to apply to
        :param stream: Processing Stream. -1 to apply to all


        """
    def initCShareLUTs(self, chip: int, stream: int) -> None:
        """Initialise all charge sharing correction LUTs to 0

        :param chip: Chip Number. -1 to apply to all chips
        :param stream: Processing Stream. -1 to apply to all

        """
    def initPixelMask(self, chip: int) -> None:
        """Initialise pixel mask Lookup Table so all pixels are enabled

        :param chip: Chip Number. -1 to apply to all chips
        """
    def setPixelMask(self, chip: int, col: int, row: int, disable: bool) -> None:
        """Disable/Enable the specified pixel at [col, row].

        :param chip: Chip Select. Set to -1 to apply to all chips.
        :param col: The Column coord of the pixel. -1 to apply to all colums
        :param row: The Row coord of the pixel. -1 to apply to all rows
        :param disable: True to disable the pixel in the output. False to enable it.

        """
    def loadLinearityGainAscii(self,
                               chip: int,
                               fullName: str,
                               scaleLinearity: float,
                               offsetADUs: float) -> None:
        """Load linearity correction set to be a simple gain scaling.
        The values in the file must read row 0,: col 0...79, row 1: col 0...79 etc.
        The values are ASCII doubles around 1.0, typically 1.2 for Hexitec

        :param chip: The chip number. -1 to apply to all chips
        :param fullName: the full path name to the file
        :param scaleLinearity: The scaling to apply to the gain correction
        :param offsetADUs: the linearity offset

        """
    def loadLinearityAscii(self,
                           chip: int,
                           fullName: str,
                           scaleLinearity: float = 1.0) -> None:
        """

        """
    def loadLinearityGainHDF5(self) -> None:
        """CURRENTLY UNUSED

        """
    def loadCShareAscii(self,
                        chip: int,
                        region: int,
                        fullName: str) -> None:
        """

        """
    def loadCShareAsciiMC(self,
                          chip: int,
                          region: int,
                          fullName: str) -> None:
        """

        """
    def loadEngMapAscii(self, chip: int, fileName: str) -> None:
        """Load Energy mapping LUT from ASCII file.

        :param chip: Chip Number. -1 to apply to all chips
        :param fileName: Name of the file storing the Ascii Energy Map
        """
    def initEngMapThres(self, chip: int, thres: int) -> None:
        """Initial energy map so all energies above threshold are mapped to 1, below are mapped to 0

        
        :param chip: Chip Number. -1 to apply to all chips
        :param Thres: Threshold (0...4095)
        """
    def loadBadPixelsTrigAscii(self, fullName: str) -> None:
        """Load a config file defining pixels which should not be enabled in the Main Trigger
        Each line of the provided 
        """
    def loadBadPixelsOutputAscii(self, fullName: str) -> None:
        """Load a config file defining pixels which should not be considered a part of the output
        These pixels can still use the main trigger for charge sharing events.

        """
    def dmaReset(self) -> None:
        """

        """
    def dmaBuildDesc(self) -> None:
        """

        """
    def dmaBuildPBDesc(self) -> None:
        """

        """
    def dmaStart(self) -> None:
        """

        """
    def dmaStop(self) -> None:
        """

        """
    def dmaReadStatus(self) -> None:
        """

        """
    def dmaReadCurrDesc(self) -> None:
        """

        """
    def dmaReadCurrDescNum(self) -> None:
        """

        """
    def dmaWaitIdle(self) -> None:
        """

        """
    def dmaWaitIdleNoExcept(self) -> None:
        """

        """
    def dmaPrintDesc(self) -> None:
        """

        """
    def getMaxPbFrames(self) -> None:
        """

        """
    def getMaxScopeFrames(self) -> None:
        """

        """
    def getPbFrameBytesAligned(self) -> None:
        """

        """
    def writeDmaBuff(self) -> None:
        """

        """
    def readDmaBuff(self) -> None:
        """

        """
    def setBaselineMode(self,
                        chip: int,
                        maskMode: int,
                        divideCode: int,
                        enbDither: bool,
                        useAbsTrig: bool) -> None:
        """Setup baseline subtraction feedback and tracking features

        :param chip:         Chip number or -1 to duplicate to all chips.
        :param maskMode:     The mask mode controls when the error signal from the subtracted baseline is fed back to update the baseline estimate. See HEXITEC_BSUB_MASK_DEFS
        :param divideCode:   Sets the scaling (division) applied to the error from 0 applied to adjust the baseline estimate. See HEXITEC_BSUB_DIVIDE_DEFS 
        :param enbDither:    Enable dither (ramping bits below binary point) used in linearity correction.

        """
    def loadBaseline(self) -> None:
        """

        """
    def waitLoadBaseline(self) -> None:
        """

        """
    def saveBaseline(self) -> None:
        """

        """
    def waitSaveBaseline(self) -> None:
        """

        """
    def setAbsTriggerThres(self,
                           chip: int,
                           firstCol: int,
                           numCols: int,
                           firstRow: int,
                           numRows: int,
                           highThres: int,
                           lowThres: int) -> None:
        """Write a fixed value to Hexitec Absolute trigger thresholds.
        This threshold is in ADUs and is compared against the ADC value before it has had baseline subtraction. It is used before the baseline has settled or after an errant
        event which causes the baseline to get lost such that the normal update does not occur.

        :param chip:      Chip number  or -1 to duplicate to all chips.
        :param firstCol:  First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols:   Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow:  First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows:   Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param highThres: Upper Threshold Value
        :param lowThres:  Lower Threshold Value

        """

    def setMainTriggerThres(self,
                            chip: int,
                            firstCol: int,
                            numCols: int,
                            firstRow: int,
                            numRows: int,
                            pos: int,
                            neg: int,
                            enable: bool) -> None:
        """Write a fixed value to Hexitec Main trigger thresholds

        
        :param chip:     Chip number  or -1 to duplicate to all chips.
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols:  Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows:  Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param pos:      Positive going threshold (0...HEXITEC_THRES_MAX)
        :param neg:      Negative going threshold (HEXITEC_THRES_MIN...0)
        :param enable:   Enable trigger on this pixel

        """

    def setLowerTriggerThres(self,
                             chip: int,
                             firstCol: int,
                             numCols: int,
                             firstRow: int,
                             numRows: int,
                             pos: int,
                             neg: int) -> None:
        """

        """
    def setLinearityRaw(self) -> None:
        """

        """
    def setLinearityOne(self,
                        chip: int,
                        offsetADUs: float = 0.0) -> None:
        """Write fixed values to the Linearity Correction Tables.

        :param chip: Chip number or -1 to duplicate to all chips.
        :param offsetADUs: the scale of the value offset.
        """

    def linearityAddOffset(self,
                           chip: int,
                           offsetADUs: float) -> None:
        """Add an fixed offset to the offset term of the linearity correction. The main use of this
        will be to use with auto-triggered modes to allow the baseline subtracted data noise level
        to be histogrammed

        :param chip:       Chip number or -1 to duplicate to all chips.
        :param offsetADUs: Offset to be added.
        """

    def setClusterMode(self,
                       chip: int,
                       clusterMode: int,
                       autoTrigRate: int) -> None:
        """Setup cluster recognition mode.

        :param chip: Chip number or -1 to duplicate to all chips.
        :param clusterMode: The mode which describes how and which clusters to choose
        :param autoTrigRate: Defines the frequency each pixel is triggered.
        """

    def setCShareMode(self, chip: int, 
                      enbEdgePos: bool, 
                      enbNegNeb: bool, 
                      enbLPos: bool, disSumming: bool,
                      disAdjPosn: bool) -> None:
        """  Enable or disable various charge sharing corrections.

        :param chip:       Chip number or -1 to duplicate to all chips.
        :param enbEdgePos: Enable charge summing correction where signal shares to give 2 positive signals to a neighbour on a side.
        :param enbNegNeb:  Enable charge summing correction where signal shares to give 1 positive signals  with a negative neighbour.
        :param enbLPos:    Enable charge summing correction for L positive signals
        :param disSumming: Disable charge summing, particularly for the special case of isolating the Fluorescence peaks
        :param disAdjPosn: Disable the adjustment of position again particularly for the special case of isolating the Fluorescence peaks

        """
    def setClusterTypes(self,
                        chip: int,
                        enbClusterType: int) -> None:
        """Set which cluster types are enabled into the output data set, others are discarded.

        :param chip: Chip number or -1 to duplicate to all chips.
        :param enbClusterType: Bitwise OR of flags to enable cluster modes.

        """
    def setHistFormat(self,
                      chip: int,
                      histFormat: int,
                      mappedMode: int,
                      histShift: int = 0) -> None:
        """Set the format of the Histogram

        :param chip:       Chip number or -1 to duplicate to all chips.
        :param histFormat: Histogram format
        :param mappedMode: Energy mapped to up to 16 scalar value mode enable.
        :param histShift:  Shift histogram data up 0, 1 or 2 bits to scale energy 1, 2 or 4 to show lower energies with more resolution.

        """
    def getHistFormat(self) -> None:
        """

        """
    def getnBinsEng(self) -> None:
        """

        """
    def getEngLsb10(self) -> None:
        """

        """
    def getUsePosn(self) -> None:
        """

        """
    def getnBinsClustClass(self) -> None:
        """

        """
    def getnBinsCharac(self) -> None:
        """

        """
    def getNumTF(self) -> None:
        """

        """
    def getNumTFMapped(self) -> None:
        """

        """
    def getEngOnly(self) -> None:
        """

        """
    def getUseClustGrade(self) -> None:
        """

        """
    def enableHist(self) -> None:
        """Turns off any test pattern generation, enabling proper Histogram creation

        """
    def readHistEngRowColTime(self) -> None:
        """

        """
    def readHistEngColRowTime(self) -> None:
        """

        """
    def readMappedEngRowColTime(self) -> None:
        """

        """
    def readMappedEngColRowTime(self) -> None:
        """

        """
    def readHistEngRowColCCTime(self) -> None:
        """

        """
    def readHistEngColRowCCTime(self) -> None:
        """

        """
    def readMappedEngRowColCCTime(self) -> None:
        """

        """
    def readHistEngGlobColRowTime(self) -> None:
        """

        """
    def readHistEngGlobColRowCCTime(self) -> None:
        """

        """
    def readMappedEngGlobColRowTime(self) -> None:
        """

        """
    def readHistEngTime(self) -> None:
        """

        """
    def readHistEngCCTime(self) -> None:
        """

        """
    def readHistEngCalibClass(self) -> None:
        """

        """
    def readHistCharac2d(self) -> None:
        """

        """
    def readHistCharac3d(self) -> None:
        """

        """
    def clearHistAll(self) -> None:
        """

        """
    def clearHistTimeframes(self) -> None:
        """

        """
    def setDefaultXDmaChan(self) -> None:
        """

        """
    def setDmaDescRWChan(self) -> None:
        """

        """
    def getRxEthernetReg(self) -> None:
        """

        """
    def getRxEthernetReg64(self) -> None:
        """

        """
    def setRxEthernetReg(self) -> None:
        """

        """
    def udpRxTestCreateSockets(self) -> None:
        """

        """
    def getUdpRxTestSocket(self) -> None:
        """

        """
    def setRxEthernetLoopback(self) -> None:
        """

        """
    def udpRxSetup(self,
                   srcIpAddrP: int,
                   accelIpAddrP: int,
                   headPort: int,
                   accelPort: int,
                   connType: HexitecUdpRxConnection) -> None:
        """Setup UDP core(s) to receive data from the detector head (srcIpAddr) usually
        
        For Hexitec MHz this is a single 100 G link either from the Alpha data card (in normal use) of from the server in test mode. It can also loop back from the accelerator in a loopback test.
        
        For Hexitec 6x2 this  is 2 off 10 G NICS in the server to 2 off 10 G UDP cores in the accelerator


        :param srcIpAddrP: IP address of the detector head, as a 32 bit integer.
        :param accelIpAddrP: IP address of the Histogrammer as a 32 bit integer. 
        :param headPort: port of the detector head
        :param accelPort: port of the histogrammer
        :param connType: Define the connection type as either running Normally, looping back, or getting data from the host machine 
        """
    def getMacAddr(self) -> None:
        """

        """
    def udpResetCounts(self, tx: bool) -> None:
        """Reset packet counters on the UDP cores

        :param tx: ``False`` to reset on UDP RX Cores. ``True`` currently does nothing.

        """

    def udpTxTestCreateSockets(self) -> None:
        """

        """
    def udpTxSetup(self,
                   accelIpAddrP: int, serverIpAddrP: int,
                   accelPort: int, serverPort: int,
                   farmBase: int, farmNum: int, enbFarmMode: bool,
                   interFrameGap: int, useArp: bool) -> None:
        """Setup the UDP TX cores to send histograms to the designated server address
        Farm Mode can be used to utilise multiple UDP threads to send Histograms to multiple Ports on the Server Address

        :param accelIpAddrP: IP address of the histogrammer, as a 32 bit integer.
        :param serverUpAddrP: IP address of the server to send data to (often an Odin Data instance), as a 32 bit integer
        :param accelPort: Histogrammer port number
        :param serverPort: Server Port number
        :param farmBase: the first address to use from the UDP Farm Lookup table
        :param farmNum: the number of addresses/ports to use from the UDP Farm lookup Table.
        :param enbFarmMode: Enable Farm Mode, allowing histograms to be sent to multiple Port numbers via round robin
        :param interFrameGap: set the Inter Frame Gap
        :param useArp: Enable Arp mode. If enabled, Farm Mode must also be enabled

        """

    def getUdpTxTestSocket(self) -> None:
        """

        """
    def udpTxTestReadFrame(self) -> None:
        """

        """
    def udpShowRxStatus(self) -> str:
        """

        """
    def iTfgDisable(self) -> None:
        """Disabled the Internal Time Frame Generator

        """
    def iTfgTrigger(self) -> None:
        """Manually Trigger the Internal Time Frame generator, it its awaiting a software signal

        """
    def iTfgSetup(self, mode: HexitecITfgMode, extTrigSrc: int, invertExtTrig: bool, inpFramesPerTF: int, numTF: int, numCycles: int) -> None:
        """Setup the Internal Time Frame Generator to output a specified number of time frames, based
        on the number of input frames.

        :param mode: Define how the ITFG gets triggered and how the frames are output on trigger
        :param extTrigSrc: the trigger pointer. Always set to 1
        :param invertExtTrig: Invert the trigger logic, so LOW<=>HIGH for signals
        :param inpFramesPerTF: Number of input Frames requied to output a full Time Frame
        :param numTf: The number of time frames to output
        :param numCycles: The number of cycles to run. 

        """
    def iTfgReadStatus(self) -> None:
        """

        """
    def printClockFrequencies(self) -> None:
        """

        """
    def startDataMoverStream(self) -> None:
        """

        """
    def stopDataMoverStreamUDP(self, qid: int) -> None:
        """Stop the selected datamove, stopping any UDP output

        :param qid: Queue ID of the datamover to stop.

        """

    def startDataMoverStreamUDP(self,
                                tfExt: int, mappedView: MappedView,
                                sixteenBit: bool, sumChips: bool,
                                qid: int, farmMask: int, farmBase: int,
                                autoMode: int, farmIndexMode: int) -> int:
        """Start the datamover to output frames via the 100 G Ethernet UDP interface.
        The data mover can either be triggered to output each time frame by software using this function with autoMode=AutoOff,
        or can be armed to trigger when the flushed time frame token advances in the firmware, normally with
        autoMode=AutoTriggerReadAndClear or autoMode=AutoTriggerRead for debug. If the system is configured
        with mappedMode==INTERLEAVE, then separate Queues are setup to transmit the full spectra and
        mapped spectra to separate UDP ports.

        :param tfExt:         Time frame to send when sending individual time frame. Last time frame sent (new frames are sent)  -1 to start from frame 0.
        :param mappedView:    Specify whether this queue send the full spectra, 16 bin mapped spectra or the first 8 mins of mapped spectra
        :param sixteenBit:    The firmware reads the 32 bit values but limit the range at 65535and sends as 16 bit data.
        :param sumChips:      When data for all chips when running in EngOnly Modes for e.g. Hexitec 6x2.
        :param qid:           Queue id to be started.
        :param farmMask:      Mask (typically 0, 1, 3 or 7) to be used to select the bottom 0, 1, 2 or 3 bits of the index to create the UDP core farm mode address.
        :param farmBase:      First address in the UDP core  fram LUT which is ORed with the maksed index to form the complete LUT address.
        :param autoMode:      Specifies whether the specified frame is sent now autoMode=AutoOff or whether autonomous triggering is enabled autoMode=AutoTriggerReadAndClear or autoMode=AutoTriggerRead
        :param farmIndexMode: Specifies what index is used to crease the UDP core farm LUT address.  This can be from the time frame or can increment each packet. See FarmIndexMode.

        :returns: FarmIndex
        """

    def startDataMoverEvList(self) -> None:
        """

        """
    def disableDataMoverUDPTrailer(self, disable: bool, packetShift: int) -> None:
        """

        """
    def clearDataMoverOverRun(self) -> None:
        """

        """
    def getDataMoverOverRun(self) -> None:
        """

        """
    def readDataMoverStream(self) -> None:
        """

        """
    def getDataMoverUDPIndex(self) -> None:
        """

        """
    def saveSpectraAsc(self) -> None:
        """

        """
    def saveSpectraDet(self) -> None:
        """

        """
    def saveSpectraHdf5(self, fname: str, chip: int, numEng: int, firstTF: int, numTFSpectra: int,
                        NumTFMapped: int, enbSpectra: bool, enbMapped: bool, sumChips: bool,
                        comments: list[str]) -> None:
        """
        Save the generated histograms to a HDF5 file
        
        :param fname: The name of the output file
        :type fname: str
        :param chip: Chip number. -1 to save from all chips
        :type chip: int
        :param numEng: Number of energy bins
        :type numEng: int
        :param firstTF: Number of the first time frame in the dataset
        :type firstTF: int
        :param numTFSpectra: Number of time frames in the spectra dataset
        :type numTFSpectra: int
        :param NumTFMapped: number of time frames in the mapped dataset
        :type NumTFMapped: int
        :param enbSpectra: Enable saving of spectra data
        :type enbSpectra: bool
        :param enbMapped: Enable saving of Mapped data
        :type enbMapped: bool
        :param sumChips: Sum the data from all chips together
        :type sumChips: bool
        :param comments: Notes/comments to save alongside the data
        :type comments: list[str]
        """

    def getFlushedFrame(self) -> int:
        """Return the value in the FlushedFrame register

        """
    def getBsubMaskName(self) -> None:
        """

        """
    def getDiagnosticCounters(self, frameCount: list[int], rawHitCount: list[int]) -> None:
        """Read per chips diagnostic counters for all chips.
        These counters count the number of detector frames processed and the raw number of hit (so split hits count as 2, 3 or 4) received since the start of the run.

        :param frameCount: Array of size at least m_numChips or HEXITEC_MAX_CHIPS to return the frame counters.
        :param rawHitCount: Array of size at least m_numChips or HEXITEC_MAX_CHIPS to return the raw hit counters.
        
        :returns frameCount: An array listing the count of frames per chip.
        :returns rawHitCount: An array listing the count of raw hits per chip.
        """
    # def getDiagnosticCounters(self, chip: int, frameCount: int, rawHitCount: int) -> None:
    #     """

    #     """
    def writeIrqEnable(self) -> None:
        """

        """
    def getIrqEnable(self) -> None:
        """

        """
    def setIrqEnable(self) -> None:
        """

        """
    def clearIrqEnable(self) -> None:
        """

        """
    def getEventFd(self) -> None:
        """

        """
    def supportsIrqs(self) -> None:
        """

        """
    def getOneFIFOCounts(self) -> None:
        """

        """
    def getAllFIFOCounts(self) -> None:
        """

        """
    def getInpTimeFrame(self, chip: int) -> int:
        """Read the Input Time Frame Register for the specified chip

        :param chip: which chip to read the Input Time Frame info from

        """
    def setClusterGradeReg(self) -> None:
        """

        """
    def setClusterGrade(self) -> None:
        """

        """
    def getClusterGrade(self) -> None:
        """

        """
    def saveSettingsHdf5(self, fName: str, chip: int,
                         saveFlags: HexitecSaveRestore) -> None:
        """Save various config settings to a HDF5 file

        :param fName: Name of the file to load settings from
        :param chip: Chip number. -1 to select all chips
        :param saveFlags: Flag defining which settings to save.
        """

    def loadSettingsHdf5(self, fName: str, chip: int, 
                         loadFlags: HexitecSaveRestore, scaleLinearity: float) -> None:
        """Load various config settings from a HDF5 file

        :param fName: Name of the file to load settings from
        :param chip: Chip number. -1 to select all chips
        :param loadFlags: Flag defining which settings to load.
        :param scaleLinearity: Required for linearity corrections. defaults to 1.0

        """
    def writePixelMask(self, chip: int, 
                      firstCol: int, numCols: int,
                      firstRow: int, numRows: int,
                      data: list[int]) -> None:
        """Write output pixel masks to the specified pixels
        This system independent array is packed/unpacked to suit the generation of Hexitec.

        :param chip: Chip number or -1 to duplicate to all chips.
        :param firstCol: First Column
        :param numCols: Number of columns
        :param firstRow: First row
        :param numRows: Number of rows
        :param data: The values to write to the pixels, ordered [row][col]

        """
    def readPixelMask(self, chip: int, 
                      firstCol: int, numCols: int,
                      firstRow: int, numRows: int) -> list[int]:
        """Read output pixel mask for the specified pixels
        This system independent array is packed/unpacked to suit the generation of Hexitec.

        :param chip: Chip number.
        :param firstCol: First Column
        :param numCols: Number of columns
        :param firstRow: First row
        :param numRows: Number of rows

        :return: A list ordered by [row][col] of the pixel masks requested
        :type chip: int
        :type firstCol: int
        :type numCols: int
        :type firstRow: int
        :type numRows: int
        :rtype: list[int]
        """

    def getSrcAddr(self, core: int = 0) -> int:
        """
        Get the Source IP Address
        
        :param core: Which UDP Core to read from. Defaults to 0
        :type core: int
        :return: The Source IP Addr as a 32 bit integer
        :rtype: int
        """

    def getDestAddr(self, core: int = 0) -> int:
        """
        Get the Destination IP Address
        
        :param core: Which UDP Core to read from. Defaults to 0
        :type core: int
        :return: The Desination IP Addr as a 32 bit integer
        :rtype: int
        """

    def getAccelRXAddr(self, core: int = 0) -> int:
        """
        Get the Histogrammer's recieve IP Address
        
        :param core: Which UDP Core to read from. Defaults to 0
        :type core: int
        :return: The receive IP Addr as a 32 bit integer
        :rtype: int
        """
    
    def getAccelTXAddr(self, core: int = 0) -> int:
        """
        Get the Histogrammer's Send IP Address
        
        :param core: Which UDP Core to read from. Defaults to 0
        :type core: int
        :return: The Sending IP Addr as a 32 bit integer
        :rtype: int
        """
