import logging

from ipaddress import ip_address, IPv4Address
from functools import partial
from typing import Literal

from histogrammer.lib import XDmaHexitec, HexitecUdpRxConnection
from histogrammer.lib import DATA_PATH_ENB_FLUSH, ETHERNET_PM_TICK_REG, DM0_AUTO_TF
from histogrammer.util import UsesHexitecLibrary, HexitecUnconnectedException
from histogrammer.adapter.base_handler import BaseHandler
from histogrammer.lib.defines import GlobalRegisters, HexitecGeneration, MappedMode

dataMoverStatus = Literal["stopped", "idle", "running"]


class UdpHandler(BaseHandler):
    """
    Object to handle the setup and control of the hexitec UDP Cores
    """

    def __init__(self, options: dict[str, str]):
        super().__init__(options)

        self.source_ip: IPv4Address = ip_address(options.get("source_ip", 0))
        self.dest_ip: IPv4Address = ip_address(options.get("dest_ip", 0))
        self.accel_rx_ip: IPv4Address = ip_address(options.get("accel_rx_ip", 0))
        self.accel_tx_ip: IPv4Address = ip_address(options.get("accel_tx_ip", 0))

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
            "udp_threads": (lambda: self.numUDPThreads, partial(self.setIp, "numUDPThreads"),
                            {"allowed_values": [2**x for x in range(9)]}),
            "source": {
                "ip": (lambda: self.source_ip.compressed, partial(self.setIp, "source_ip")),
                "port": (lambda: self.source_port, partial(setattr, self, "source_port"))
            },
            "accelerator": {
                "rx_ip": (lambda: self.accel_rx_ip.compressed, partial(self.setIp, "accel_rx_ip")),
                "tx_ip": (lambda: self.accel_tx_ip.compressed, partial(self.setIp, "accel_tx_ip")),
                "port": (lambda: self.accel_port, partial(setattr, self, "accel_port"))
            },
            "destination": {
                "ip": (lambda: self.dest_ip.compressed, partial(self.setIp, "dest_ip")),
                "port": (lambda: self.dest_port, partial(setattr, self, "dest_port"))
            }
        }

    def initialise(self, hexitec):
        super().initialise(hexitec)
        # read values from device
        
        self.source_ip = ip_address(hexitec.getSrcAddr())
        self.dest_ip = ip_address(hexitec.getDestAddr())
        self.accel_rx_ip = ip_address(hexitec.getAccelRXAddr())
        self.accel_tx_ip = ip_address(hexitec.getAccelTXAddr())

    def cleanup(self):
        try:
            self.stopDataMovers()
        except HexitecUnconnectedException:
            pass
        return super().cleanup()

    def setupUdp(self):
        self.setupUdpReceive()
        self.setupUdpSend()

    def setIp(self, name: str, value: int | str):
        setattr(self, name, ip_address(value))

    @UsesHexitecLibrary()
    def setupUdpReceive(self):
        """ Setup the UDP cores to receive data """
        srcIP = self.source_ip
        destIP = self.accel_rx_ip
        srcPort = self.source_port
        destPort = self.accel_port

        self.hexitec.setGlobReg(GlobalRegisters.DATA_PATH, DATA_PATH_ENB_FLUSH)
        self.hexitec.setRxEthernetLoopback(0)  # disable ethernet loopback

        self.hexitec.udpRxSetup(int(srcIP), int(destIP),
                                srcPort, destPort,
                                self.connectType)

        for i in range(self.hexitec.getNumRxUdp()):
            if self.hexitec.getGeneration() == HexitecGeneration.HexitecGenHexitec:
                self.hexitec.setRxEthernetReg(i, ETHERNET_PM_TICK_REG, 1)

        self.stopDataMovers()

    @UsesHexitecLibrary()
    def setupUdpSend(self):
        """ Setup the UDP cores to send Histograms to a server (usually an Odin Data instance)"""
        srcIP = self.accel_tx_ip
        destIP = self.dest_ip
        srcPort = self.accel_port
        destPort = self.dest_port
        mappedMode = self.mappedMode

        farmBase = 0
        numThreads = self.numUDPThreads

        if mappedMode == MappedMode.INTERLEAVE:
            # mapped interleave mode requires threads for both spectra and mapped
            numThreads = numThreads * 2
        logging.debug("Setting up UDP Tx With The Following settings:")
        logging.debug("Source IP: {}, Source Port: {}, Dest IP: {}, Dest Port: {}"
                      .format(srcIP, srcPort, destIP, destPort))

        self.hexitec.udpTxSetup(int(srcIP), int(destIP),
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

        # trailer mode is not disabled, but the default values are set by this method
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

                finished[0] = context.timeFrame == (numTF)
                if not finished[0]:
                    logging.debug("Data Mover 0 Timeframe: %d out of %d", context.timeFrame, numTF)
            else:
                finished[0] = context.run
        if mappedMode != MappedMode.OFF:
            context = self.hexitec.readDataMoverStream(1)
            if context.tfMode & DM0_AUTO_TF:
                finished[1] = context.timeFrame == (numTF)
                if not finished[1]:
                    logging.debug("Data Mover 1 Timeframe: %d out of %d", context.timeFrame, numTF)
            else:
                finished[1] = context.run
        return all(finished)
