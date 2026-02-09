import logging

from functools import partial
from typing import Literal

from xdma_hexitec import XDmaHexitec, HexitecUdpRxConnection
from xdma_hexitec import DATA_PATH_ENB_FLUSH, ETHERNET_PM_TICK_REG, DM0_AUTO_TF
from histogrammer.util import UsesHexitecLibrary, HexitecUnconnectedException
from histogrammer.base_handler import BaseHandler
from xdma_hexitec.defines import GlobalRegisters, HexitecGeneration, MappedMode

dataMoverStatus = Literal["stopped", "idle", "running"]

def getIntFromIP(ip: str) -> int:
    """
    Turn an IP address string (eg 192.168.0.0) into the required 32 Bit Integer value
    
    :param ip: the IP address to convert, in the standard dotted decimal notation
    :type ip: str
    :return: The IP address provided as a 32 bit number. Returns 0 if the addr is invalid
    :rtype: int
    """
    parts = [int(x) for x in ip.split(".")]
    if not len(parts) == 4:
        return 0
    return parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3]

class UdpHandler(BaseHandler):
    """
    Object to handle the setup and control of the hexitec UDP Cores
    """

    def __init__(self, options: dict[str, str]):
        super().__init__(options)

        self.source_ip = options.get("source_ip", "default")
        self.dest_ip = options.get("dest_ip", "default")
        self.accel_rx_ip = options.get("accel_rx_ip", "default")
        self.accel_tx_ip = options.get("accel_tx_ip", "default")

        self.source_port = int(options.get("source_port", 0))
        self.accel_port = int(options.get("accel_port", 0))
        self.dest_port = int(options.get("dest_port", 0))

        self.connectType = HexitecUdpRxConnection.Normal
        self.mappedMode = MappedMode.OFF
        self.numUDPThreads = 8
        self.inter_frame_gap = 4095

        self.dmStatus: list[dataMoverStatus] = ["stopped", "stopped"]

        self.param_tree = {
            "setup": (None, lambda _: self.setupUdp()),
            "udp_threads": (lambda: self.numUDPThreads, partial(setattr, self, "numUDPThreads"),
                            {"allowed_values": [2**x for x in range(9)]}),
            "source": {
                "ip": (lambda: self.source_ip, partial(setattr, self, "source_ip")),
                "port": (lambda: self.source_port, partial(setattr, self, "source_port"))
            },
            "accelerator": {
                "rx_ip": (lambda: self.accel_rx_ip, partial(setattr, self, "accel_rx_ip")),
                "tx_ip": (lambda: self.accel_tx_ip, partial(setattr, self, "accel_tx_ip")),
                "port": (lambda: self.accel_port, partial(setattr, self, "accel_port"))
            },
            "destination": {
                "ip": (lambda: self.dest_ip, partial(setattr, self, "dest_ip")),
                "port": (lambda: self.dest_port, partial(setattr, self, "dest_port"))
            }
        }

    def initialise(self, hexitec: XDmaHexitec):
        super().initialise(hexitec)

    def cleanup(self):
        try:
            self.stopDataMovers()
        except HexitecUnconnectedException:
            pass
        return super().cleanup()

    def setupUdp(self):
        self.setupUdpReceive()
        self.setupUdpSend()

    @UsesHexitecLibrary()
    def setupUdpReceive(self, srcIP: str = None, destIP: str = None,
                        srcPort: int = None, destPort: int = None):
        """
        Setup the UDP cores to receive data
        
        :param srcIP: IP address of the data source (Likely the Alpha Data card)
        :type srcIP: str
        :param destIP: IP address the data is sent to (the address of the Histogrammer module)
        :type destIP: str
        :param srcPort: Port number of the data source
        :type srcPort: int
        :param destPort: Port number of the histogrammer
        :type destPort: int
        """
        srcIP = self.source_ip if srcIP is None else srcIP
        destIP = self.accel_rx_ip if destIP is None else destIP
        srcPort = self.source_port if srcPort is None else srcPort
        destPort = self.accel_port if destPort is None else destPort

        srcIP_int = getIntFromIP(srcIP)
        destIP_int = getIntFromIP(destIP)

        self.hexitec.setGlobReg(GlobalRegisters.GLB_DATA_PATH, DATA_PATH_ENB_FLUSH)
        self.hexitec.setRxEthernetLoopback(0) # disable ethernet loopback

        self.hexitec.udpRxSetup(srcIP_int, destIP_int,
                         srcPort, destPort,
                         self.connectType)
        
        for i in range(self.hexitec.getNumRxUdp()):
            if self.hexitec.getGeneration() == HexitecGeneration.HexitecGenHexitec:
                self.hexitec.setRxEthernetReg(i, ETHERNET_PM_TICK_REG, 1)

        self.stopDataMovers()

    @UsesHexitecLibrary(logging.DEBUG)
    def setupUdpSend(self, srcIP: str = None, destIP: str = None,
                     srcPort: int = None, destPort: int = None,
                     mappedMode: MappedMode = None):
        """
        Setup the UDP cores to send Histograms to a server (usually an Odin Data instance)
        
        :param srcIP: THe IP address of the Histogrammer
        :type srcIP: str
        :param destIP: The IP address of the destination server, to send histograms to
        :type destIP: str
        :param srcPort: Port number of the Histogrammer
        :type srcPort: int
        :param destPort: Port number of the server. This will be the first port number if multiple threads are used
        :type destPort: int
        :param mappedMode: Enum value that defines the Mapped Mode of the histogram.
        :type mappedMode: MappedMode
        """
        srcIP = self.accel_tx_ip if srcIP is None else srcIP
        destIP = self.dest_ip if destIP is None else destIP
        srcPort = self.accel_port if srcPort is None else srcPort
        destPort = self.dest_port if destPort is None else destPort
        mappedMode = self.mappedMode if mappedMode is None else mappedMode

        srcIP_int = getIntFromIP(srcIP)
        destIP_int = getIntFromIP(destIP)

        farmBase = 0
        numThreads = self.numUDPThreads

        if mappedMode == MappedMode.INTERLEAVE:
            numThreads = numThreads * 2  # mapped interleave mode requires threads for spectra and mapped
        logging.debug("Setting up UDP Tx With The Following settings:")
        logging.debug("Source IP: {}, Source Port: {}, Dest IP: {}, Dest Port: {}".format(srcIP, srcPort, destIP, destPort))

        self.hexitec.udpTxSetup(srcIP_int, destIP_int,
                                srcPort, destPort,
                                farmBase, numThreads,
                                True, self.inter_frame_gap, False)
        

    @UsesHexitecLibrary()
    def stopDataMovers(self):
        """Disable any datamovers that might be running"""

        self.hexitec.stopDataMoverStreamUDP(0)
        self.hexitec.stopDataMoverStreamUDP(1)

        self.dmStatus = ["stopped", "stopped"]

    @UsesHexitecLibrary()
    def startDataMovers(self, mappedMode: MappedMode):

        farmBase = 0
        timeframe_start = -1
        farmMask = self.numUDPThreads - 1

        autoMode = XDmaHexitec.AutonomousMode.AutoTriggerReadAndClear
        farmIndex = XDmaHexitec.FarmIndexMode.FarmIndexFromTF

        # trailer mode is not disabled (becasue False), but the default values are set by this method
        self.hexitec.disableDataMoverUDPTrailer(False, 0)

        # if mapped mode is set to allow spectra (either mappedMode OFF or mappedMode INTERLEAVE)
        if mappedMode != MappedMode.ONLY:
            # setup data mover farm mode for Spectra
            logging.debug("Setting up UDP DataMover for Spectra Output")
            self.hexitec.startDataMoverStreamUDP(timeframe_start,
                                                 XDmaHexitec.MappedView.Spectra, False,
                                                 True, 0, farmMask, farmBase, autoMode, farmIndex)
            self.dmStatus[0] = "idle"
            farmBase += farmMask + 1
        
        # if mapped mode is set to allow Mapped output (ONLY or INTERLEAVE)
        if mappedMode != MappedMode.OFF:
            logging.debug("Setting up UDP Datamover for Mapped Output")
            self.hexitec.startDataMoverStreamUDP(timeframe_start,
                                                 XDmaHexitec.MappedView.Mapped16, False,
                                                 True, 1, farmMask, farmBase, autoMode, farmIndex)
            self.dmStatus[1] = "idle"

    @UsesHexitecLibrary()
    def resetCounters(self):
        self.hexitec.udpResetCounts(False)

    @UsesHexitecLibrary()
    def areDataMoversFinished(self, numTF: int, mappedMode: MappedMode) -> bool:
        finished: list[bool] = [True, True]
        if mappedMode != MappedMode.ONLY:
            context = self.hexitec.readDataMoverStream(0)
            if context.tfMode & DM0_AUTO_TF:

                
                finished[0] = context.timeFrame == (numTF - 1)
                if not finished[0]:
                    logging.debug("Data Mover 0 Timeframe: %d out of %d", context.timeFrame, numTF)
            else:
                finished[0] = context.run
        if mappedMode != MappedMode.OFF:
            
            context = self.hexitec.readDataMoverStream(1)
            if context.tfMode & DM0_AUTO_TF:
                
                finished[1] = context.timeFrame == (numTF - 1)
                if not finished[1]:
                    logging.debug("Data Mover 1 Timeframe: %d out of %d", context.timeFrame, numTF)
            else:
                finished[1] = context.run
        return all(finished)
