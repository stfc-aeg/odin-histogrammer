from enum import IntEnum
from histogrammer.lib.defines import HexitecGeneration
from histogrammer.lib.interfaces.structs import HexitecITfgMode, HexitecUdpRxConnection, DataMoverContext, HexitecSaveRestore

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

    def __init__(self, useQdma: bool, busNum: int, devNum: int, funcNum: int) -> None:
        """
        Initialise the XDmaHexitec class, connected to the defined XDMA device

        :param useQdma: Enables the use of QDMA instead of XDMA for DMA access
        :param busNum: The BusID of the PCI address for the device.
        :param devNum: the DeviceID of the PCI address for the device.
        :param funcNum: the FunctionID of the PCI address for the device.
        """

    def getNumChips(self) -> int:
        """Return the total number of chips in the Hexitec device"""

    def getGeneration(self) -> HexitecGeneration:
        """Gets the Type of Hexitec system used (Mhz or 2x6)"""

    def getNumRxUdp(self) -> int:
        """Gets the number of UDP RX Cores"""

    def getNumTxUdp(self) -> int:
        """Gets the number of UDP TX Cores"""

    def setChipReg(self, chip: int, offset: int, value: int) -> None:
        """Set register for the specified chip

        :param chip: Chip number  or -1 to duplicate to all chips.
        :type chip: int
        :param offset: Register address 
        :type offset: int
        :param value: Value to write to the register
        :type value: int
        """

    def getChipReg(self, chip: int, offset: int) -> int:
        """Read Value of Chip Register

        :param chip: Chip number.
        :type chip: int
        :param offset: Register address 
        :type offset: int
        """

    def setGlobReg(self, offset: int, value: int) -> None:
        """Sets the value of a Global Register at the specified offset address

        :param offset: address of the register to write to
        :param value: the value to write into the register
        """

    def getGlobReg(self, offset: int) -> int:
        """Read the value of the global register

        :param offset: address of the register to read
        :returns: 32 Bit value read from the register
        :rtype: int
        """

    def getGlobReg64(self, offset: int) -> int:
        """
        Read the 64 bit value of the global register
        
        :param offset: Register Address
        :type offset: int
        :return: 64 bit value, read from the register
        :rtype: int
        """

    def setPixelLUT(self, chip: int, region: int, firstCol: int, numCol: int, firstRow: int,
                    numRow: int, value: int) -> None:
        """Write a fixed value to multiple Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block.

        :param chip: Chip number  or -1 to duplicate to all chips.
        :param region: Region number, see Region Enum in defines
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols: Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows: Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param value: Value to write

        :type chip: int
        :type region: int
        :type firstCol: int
        :type numCols: int
        :type firstRow: int
        :type numRows: int
        :type value: int
        """

    def writePixelLUT(self, chip: int, region: int, firstCol: int, numCols: int, firstRow: int, numRows: int, data: list[int]) -> None:
        """Write array of value to a Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block
        The data is organised as data[numRows][numCols].

        :param chip:     Chip number  or -1 to duplicate to all chips. Note in this case the data array for 1 chip is is replicated for all chips
        :param region: Region number, see Region Enum in defines
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols:  Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows:  Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param data:     Data to write. Note order.

        :type chip: int
        :type region: int
        :type firstCol: int
        :type numCols: int
        :type firstRow: int
        :type numRows: int
        :type data: list[int]
        """

    def readPixelLUT(self, chip: int, region: int, firstCol: int, numCols: int, firstRow: int, numRows: int) -> list[int]:
        """Read array of values from a Hexitec per pixel LUTs currently in the baseline, linearity
        and trigger threshold processing block. The data is organised as data[numRows][numCols]
        
        :param chip: Chip Number
        :type chip: int
        :param region: Region number, see Region Enum in defines
        :type region: int
        :param firstCol: First column of sensor to read
        :type firstCol: int
        :param numCols: Number of columns to read
        :type numCols: int
        :param firstRow: First row of sensor to read
        :type firstRow: int
        :param numRows: number of rows to read
        :type numRows: int
        :return: List of the LUT values read out, ordered data[numRows][numCols]
        :rtype: list[int]
        """

    def writeSharedLUT(self, chip: int, region: int, stream: int, first: int, num: int, data: list[int]) -> None:
        """Write array of value to a Hexitec Charge Sharing correction LUTs. Currently shared across all pixels.

        :param chip: Chip number  or -1 to duplicate to all chips. Note in this case the data array for 1 chip is is replicated for all chips
        :param region: Region number, see region Enum for possible values
        :param stream: Processing stream (which handles 2 pairs of columns (total 4 columns). Usually -1 to replicate to all.
        :param first: First offset within LUT, usually 0,
        :param num: Number of point to write (usually full table)
        :param data: Pointer to data to write.

        :type chip: int
        :type region: int
        :type stream: int
        :type first: int
        :type num: int
        :type data: list[int]

        """
    def readSharedLUT(self, chip: int, region: int, stream: int, first: int, num: int) -> list[int]:
        """Read array of values from a Hexitec Charge Sharing correction LUTs. Currently shared across all pixels.

        :param chip: Chip number.
        :param region: Region number, see Region Enum
        :param stream: Processing stream (which handles 2 pairs of columns (total 4 columns).
        :param first: First offset within LUT, usually 0,
        :param num: Number of point to read (usually full table)
        :return: list of points read from LUT

        :type chip: int
        :type region: int
        :type stream: int
        :type first: int
        :type num: int
        :rtype: list[int]
        """

    def initRecipLUT(self,
                     chip: int,
                     region: int,
                     stream: int) -> None:
        """Initialise the Positive or Negative reciprocal Lookup Tables

        :param chip: Chip Number. -1 to apply to all chips
        :param region: Region to apply to
        :param stream: Processing Stream. -1 to apply to all

        :type chip: int
        :type region: int
        :type stream: int
        """

    def initCShareLUTs(self, chip: int, stream: int) -> None:
        """Initialise all charge sharing correction LUTs to 0

        :param chip: Chip Number. -1 to apply to all chips
        :param stream: Processing Stream. -1 to apply to all
        :type chip: int
        :type stream: int
        """

    def initPixelMask(self, chip: int) -> None:
        """Initialise pixel mask Lookup Table so all pixels are enabled

        :param chip: Chip Number. -1 to apply to all chips
        :type chip: int
        """

    def loadLinearityGainAscii(self, chip: int, fullName: str, scaleLinearity: float, offsetADUs: float) -> None:
        """Load linearity correction set to be a simple gain scaling.
        The values in the file must read row 0,: col 0...79, row 1: col 0...79 etc.
        The values are ASCII doubles around 1.0, typically 1.2 for Hexitec

        :param chip: The chip number. -1 to apply to all chips
        :param fullName: the full path name to the file
        :param scaleLinearity: The scaling to apply to the gain correction
        :param offsetADUs: the linearity offset

        :type chip: int
        :type fullName: str
        :type scaleLinearity: float
        :type offsetADUs: float

        """
    def loadLinearityAscii(self, chip: int, fullName: str, scaleLinearity: float) -> None:
        """Load full linearity correction.
        The file must contain a row per pixel, ordered row 0, col 0...79, row 1, col 0...79 etc.
        Each line must contain either 3 ASCII doubles a, b,c of ax^2+bx+c or 8 triples if using Piecewise Linearity.

        :param chip: The chip number. -1 to apply to all chips
        :type chip: int
        :param fullName: Full path to the file
        :type fullName: str
        :param scaleLinearity: The scaling to apply to the gain correction
        :type scaleLinearity: float
        """

    def loadCShareAscii(self,
                        chip: int,
                        region: int,
                        fullName: str) -> None:
        """Load CShare values.
        The file must contain a row per pixel in region, with a double on each row
        
        :param chip: The chip number. -1 to apply to all chips
        :type chip: int
        :param region: Region number, see Region Enum
        :type region: int
        :param fullName: Full path to the file
        :type fullName: str
        """

    def loadCShareAsciiMC(self,
                          chip: int,
                          region: int,
                          fullName: str) -> None:
        """
        Load CShare MC values.
        The file must contain a row per pixel in region, with 2 doubles on each row
        
        :param chip: The chip number. -1 to apply to all chips
        :type chip: int
        :param region: Region number, see Region Enum
        :type region: int
        :param fullName: Full path to the file
        :type fullName: str
        """

    def loadBadPixelsTrigAscii(self, fullName: str) -> None:
        """Load a config file defining pixels which should not be enabled in the Main Trigger
        Each line of the provided file defines a pixel to disable with 3 integers, [chip] [row] [col]
        
        :param fullName: Full path to the file
        :type fullName: str
        """

    def loadBadPixelsOutputAscii(self, fullName: str) -> None:
        """Load a config file defining pixels which should not be considered a part of the output
        These pixels can still use the main trigger for charge sharing events.
        Each line of the provided file defines a pixel to disable with 3 integers, [chip] [row] [col]

        :param fullName: Full path to the file
        :type fullName: str
        """

    def setBaselineMode(self, chip: int, maskMode: int, divideCode: int,
                        enbDither: bool, useAbsTrig: bool) -> None:
        """Setup baseline subtraction feedback and tracking features

        :param chip:         Chip number or -1 to duplicate to all chips.
        :param maskMode:     The mask mode controls when the error signal from the subtracted baseline is fed back to update the baseline estimate. See HEXITEC_BSUB_MASK_DEFS
        :param divideCode:   Sets the scaling (division) applied to the error from 0 applied to adjust the baseline estimate. See HEXITEC_BSUB_DIVIDE_DEFS 
        :param enbDither:    Enable dither (ramping bits below binary point) used in linearity correction.
        :param useAbsTrig:   Enable the use of the absolute trigger

        :type chip: int
        :type maskMode: int
        :type divideCode: int
        :type enbDither: bool
        :type useAbsTrig: bool
        """

    def setAbsTriggerThres(self, chip: int, firstCol: int, numCols: int, firstRow: int,
                           numRows: int, highThres: int, lowThres: int) -> None:
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

        :type chip: int
        :type firstCol: int
        :type numCols: int
        :type firstRow: int
        :type numRows: int
        :type highThres: int
        :type lowThres: int
        """

    def setMainTriggerThres(self, chip: int, firstCol: int, numCols: int, firstRow: int,
                            numRows: int, pos: int, neg: int, enable: bool) -> None:
        """Write a fixed value to Hexitec Main trigger thresholds

        :param chip:     Chip number  or -1 to duplicate to all chips.
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols:  Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows:  Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param pos:      Positive threshold (0...HEXITEC_THRES_MAX)
        :param neg:      Negative threshold (HEXITEC_THRES_MIN...0)
        :param enable:   Enable trigger on this pixel

        :type chip: int
        :type firstCol: int
        :type numCols: int
        :type firstRow: int
        :type numRows: int
        :type pos: int
        :type neg: int
        :type enable: bool
        """

    def setLowerTriggerThres(self, chip: int, firstCol: int, numCols: int, firstRow: int,
                             numRows: int, pos: int, neg: int) -> None:
        """Write a fixed value to Hexitec Lower trigger thresholds

        :param chip:     Chip number  or -1 to duplicate to all chips.
        :param firstCol: First column of sensor 0..HEXITEC_NUM_COLS-1
        :param numCols:  Number of columns of sensor 1..HEXITEC_NUM_COLS.
        :param firstRow: First row of sensor 0..HEXITEC_NUM_ROWS-1
        :param numRows:  Number of rows of sensor 1..HEXITEC_NUM_ROWS.
        :param pos:      Positive threshold (0...HEXITEC_THRES_MAX)
        :param neg:      Negative threshold (HEXITEC_THRES_MIN...0)

        :type chip: int
        :type firstCol: int
        :type numCols: int
        :type firstRow: int
        :type numRows: int
        :type pos: int
        :type neg: int
        """

    def setLinearityOne(self, chip: int, offsetADUs: float) -> None:
        """Write fixed values to the Linearity Correction Tables.

        :param chip: Chip number or -1 to duplicate to all chips.
        :param offsetADUs: the scale of the value offset.

        :type chip: int
        :type offsetADUs: float
        """

    def linearityAddOffset(self, chip: int, offsetADUs: float) -> None:
        """Add an fixed offset to the offset term of the linearity correction. The main use of this
        will be to use with auto-triggered modes to allow the baseline subtracted data noise level
        to be histogrammed

        :param chip:       Chip number or -1 to duplicate to all chips.
        :param offsetADUs: Offset to be added.

        :type chip: int
        :type offsetADUs: float
        """

    def setClusterMode(self, chip: int, clusterMode: int, autoTrigRate: int) -> None:
        """Setup cluster recognition mode.

        :param chip: Chip number or -1 to duplicate to all chips.
        :param clusterMode: The mode which describes how and which clusters to choose
        :param autoTrigRate: Defines the frequency each pixel is triggered.

        :type chip: int
        :type clusterMode: int
        :type autoTrigRate: int
        """

    def setCShareMode(self, chip: int, enbEdgePos: bool, enbNegNeb: bool, enbLPos: bool,
                      disSumming: bool, disAdjPosn: bool) -> None:
        """  Enable or disable various charge sharing corrections.

        :param chip:       Chip number or -1 to duplicate to all chips.
        :param enbEdgePos: Enable charge summing correction where signal shares to give 2 positive signals to a neighbour on a side.
        :param enbNegNeb:  Enable charge summing correction where signal shares to give 1 positive signals  with a negative neighbour.
        :param enbLPos:    Enable charge summing correction for L positive signals
        :param disSumming: Disable charge summing, particularly for the special case of isolating the Fluorescence peaks
        :param disAdjPosn: Disable the adjustment of position again particularly for the special case of isolating the Fluorescence peaks

        :type chip: int
        :type enbEdgePos: bool
        :type enbNegNeb: bool
        :type enbLPos: bool
        :type disSumming: bool
        :type disAdjPosn: bool
        """

    def setClusterTypes(self, chip: int, enbClusterType: int) -> None:
        """Set which cluster types are enabled into the output data set, others are discarded.

        :param chip: Chip number or -1 to duplicate to all chips.
        :param enbClusterType: Bitwise OR of flags to enable cluster modes.

        :type chip: int
        :type enbClusterType: int
        """

    def setHistFormat(self, chip: int, histFormat: int, mappedMode: int, histShift: int) -> None:
        """Set the format of the Histogram

        :param chip:       Chip number or -1 to duplicate to all chips.
        :param histFormat: Histogram format
        :param mappedMode: Energy mapped to up to 16 scalar value mode enable.
        :param histShift:  Shift histogram data up 0, 1 or 2 bits to scale energy 1, 2 or 4 to show lower energies with more resolution.

        :type chip: int
        :type histFormat: int
        :type mappedMode: int
        :type histShift: int

        """

    def enableHist(self) -> None:
        """Turns off any test pattern generation, enabling proper Histogram creation"""

    def clearHistAll(self) -> None:
        """Clear Histograms from memory"""

    def setRxEthernetReg(self, ethNum: int, offset: int, value: int) -> None:
        """
        Set the value in the specified UDP RX Ethernet Register
        
        :param ethNum: Which UDP RX Core to select
        :type ethNum: int
        :param offset: Address of the register in the UDP core
        :type offset: int
        :param value: 32 bit value to write to the register
        :type value: int
        """

    def setRxEthernetLoopback(self, flags: int) -> None:
        """
        Enable or disable the Ethernet Loopback on the RX UDP Cores

        :param flags: 0 to disable loopback, 1 to enable.
        :type flags: int
        """

    def udpRxSetup(self, srcIpAddrP: int, accelIpAddrP: int, headPort: int, accelPort: int,
                   connType: HexitecUdpRxConnection) -> None:
        """Setup UDP core(s) to receive data from the detector head (srcIpAddr) usually
        
        For Hexitec MHz this is a single 100 G link either from the Alpha data card (in normal use) of from the server in test mode. It can also loop back from the accelerator in a loopback test.
        
        For Hexitec 6x2 this  is 2 off 10 G NICS in the server to 2 off 10 G UDP cores in the accelerator


        :param srcIpAddrP: IP address of the detector head, as a 32 bit integer.
        :param accelIpAddrP: IP address of the Histogrammer as a 32 bit integer. 
        :param headPort: port of the detector head
        :param accelPort: port of the histogrammer
        :param connType: Define the connection type as either running Normally, looping back, or getting data from the host machine 

        :type srcIpAddrP: int
        :type accelIpAddrP: int
        :type headPort: int
        :type accelPort: int
        :type connType: HexitecUdpRxConnection
        """

    def udpResetCounts(self, tx: bool) -> None:
        """Reset packet counters on the UDP cores

        :param tx: ``False`` to reset on UDP RX Cores. ``True`` currently does nothing.
        :type tx: bool
        """

    def udpTxSetup(self, accelIpAddrP: int, serverIpAddrP: int,
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

        :type accelIpAddrP: int
        :type serverUpAddrP: int
        :type accelPort: int
        :type serverPort: int
        :type farmBase: int
        :type farmNum: int
        :type enbFarmMode: int
        :type interFrameGap: int
        :type useArp: bool
        """

    def iTfgDisable(self) -> None:
        """Disabled the Internal Time Frame Generator"""

    def iTfgTrigger(self) -> None:
        """Manually Trigger the Internal Time Frame generator, if it is awaiting a software signal"""

    def iTfgSetup(self, mode: HexitecITfgMode, extTrigSrc: int, invertExtTrig: bool,
                  inpFramesPerTF: int, numTF: int, numCycles: int) -> None:
        """Setup the Internal Time Frame Generator to output a specified number of time frames, based
        on the number of input frames.

        :param mode: Define how the ITFG gets triggered and how the frames are output on trigger
        :param extTrigSrc: the trigger pointer. Always set to 1
        :param invertExtTrig: Invert the trigger logic, so LOW<=>HIGH for signals
        :param inpFramesPerTF: Number of input Frames requied to output a full Time Frame
        :param numTf: The number of time frames to output
        :param numCycles: The number of cycles to run. 

        :type mode: HexitecITfgMode
        :type extTrigSrc: int
        :type invertExtTrig: bool
        :type inpFramesPerTF: int
        :type numTF: int
        :type numCycles: int
        """

    def stopDataMoverStreamUDP(self, qid: int) -> None:
        """Stop the selected datamover, stopping any UDP output

        :param qid: Queue ID of the datamover to stop.
        :type qid: int
        """

    def startDataMoverStreamUDP(self, tfExt: int, mappedView: MappedView, sixteenBit: bool,
                                sumChips: bool, qid: int, farmMask: int, farmBase: int,
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

        :type tfExt: int
        :type mappedView: MappedView
        :type sixteenBit: bool
        :type sumChips: bool
        :type qid: int
        :type farmMask: int
        :type farmBase: int
        :type autoMode: int
        :type farmIndexMode: int

        :returns: FarmIndex
        :rtype: int
        """

    def disableDataMoverUDPTrailer(self, disable: bool, packetShift: int) -> None:
        """
        Setup Data Mover Trailer values, and disable the Trailer if specified
        
        :param disable: `True` to disable the UDP trailer
        :type disable: bool
        :param packetShift: Right Shift of packet index if trailer is disabled
        :type packetShift: int
        """

    def readDataMoverStream(self, qid: int) -> DataMoverContext:
        """
        Read the context values of the selected Data Mover
        
        :param qid: The Queue ID of the Data Mover
        :type qid: int
        :return: Struct containing the data mover context
        :rtype: DataMoverContext
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
        """Return the value in the FlushedFrame register"""

    def supportsIrqs(self) -> int:
        """Read if the device is setup to support IRQs
        
        :return: 1 if not using QDMA (thus supporting IRQs), 0 otherwise
        :rtype: bool
        """

    def getInpTimeFrame(self, chip: int) -> int:
        """Read the Input Time Frame Register for the specified chip

        :param chip: which chip to read the Input Time Frame info from
        :type chip: int

        :returns: The Input Time Frame value from the register
        :rtype: int
        """

    def saveSettingsHdf5(self, fName: str, chip: int,
                         saveFlags: HexitecSaveRestore) -> None:
        """Save various config settings to a HDF5 file

        :param fName: Name of the file to load settings from
        :param chip: Chip number. -1 to select all chips
        :param saveFlags: Flag defining which settings to save.

        :type fName: str
        :type chip: int
        :type saveFlags: HexitecSaveRestore
        """

    def loadSettingsHdf5(self, fName: str, chip: int, 
                         loadFlags: HexitecSaveRestore, scaleLinearity: float) -> None:
        """Load various config settings from a HDF5 file

        :param fName: Name of the file to load settings from
        :param chip: Chip number. -1 to select all chips
        :param loadFlags: Flag defining which settings to load.
        :param scaleLinearity: Required for linearity corrections. defaults to 1.0

        :type fName: str
        :type chip: int
        :type loadFlags: HexitecSaveRestore
        :type scaleLinearity: float
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
