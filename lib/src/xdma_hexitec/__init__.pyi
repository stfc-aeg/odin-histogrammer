"""
XDMA Hexitec C++ Plugin
-----------------------

Provides a Python interface to the XDmaHexitec C++ library written by William Helsby
"""

from enum import IntEnum
from .defines import HexitecGeneration


class HexitecUdpRxConnection(IntEnum):
    """UDP Connection Type, defining where data is coming from"""

    Normal = 0
    Loopback = 1
    FromHost = 2


class XDmaHexitec:
    """
    XDmaHexitec Class, connects to and manages the hexitec histogrammer hardware, providing access
    to various registers, DMA buffers, and other such parts of the firmware.
    """

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
    def setChipReg(self) -> None:
        """

        """
    def getChipReg(self) -> None:
        """

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
        """

        """
    def writePixelLUT(self) -> None:
        """

        """
    def readPixelLUT(self) -> None:
        """

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
    def setSharedLUT(self) -> None:
        """

        """
    def writeSharedLUT(self) -> None:
        """

        """
    def readSharedLUT(self) -> None:
        """

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
    def setPixelMask(self) -> None:
        """

        """
    def loadLinearityGainAscii(self,
                               chip: int,
                               fullName: str,
                               scaleLinearity: float = 1.0,
                               offsetADUs: float = 0.0) -> None:
        """

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
    def loadBadPixelsTrigAscii(self) -> None:
        """

        """
    def loadBadPixelsOutputAscii(self) -> None:
        """

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

        :param chip         Chip number or -1 to duplicate to all chips.
        :param maskMode     The mask mode controls when the error signal from the subtracted baseline is fed back to update the baseline estimate. See HEXITEC_BSUB_MASK_DEFS
        :param divideCode   Sets the scaling (division) applied to the error from 0 applied to adjust the baseline estimate. See HEXITEC_BSUB_DIVIDE_DEFS 
        :param enbDither    Enable dither (ramping bits below binary point) used in linearity correction.

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

    def setCShareMode(self) -> None:
        """

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
        """

        """
    def iTfgTrigger(self) -> None:
        """

        """
    def iTfgSetup(self) -> None:
        """

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
                                tfExt: int, mappedView: int,
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
    def saveSpectraHdf5(self) -> None:
        """CURRENTLY UNUSED

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
    def saveSettingsHdf5(self) -> None:
        """CURRENTLY UNUSED
        """

    def loadSettingsHdf5(self) -> None:
        """CURRENTLY UNUSED

        """
    def writePixelMask(self) -> None:
        """

        """
    def readPixelMask(self) -> None:
        """

        """

