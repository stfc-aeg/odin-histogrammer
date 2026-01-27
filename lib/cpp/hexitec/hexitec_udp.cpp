#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h> 
#include <linux/if_packet.h>
#include <linux/ioctl.h>
#include <ifaddrs.h>
#include <cxxabi.h>
#include <sstream>

#include "xdma_hexitec.h"


void XDmaHexitec::udpResetCounts(bool tx)
{
	if (!tx)
	{
		if (m_generation == HexitecGenMHz)
			m_udpCore[0].resetCounts();
		else 
		{
			m_udpCore[0].resetCounts();
			m_udpCore[1].resetCounts();
		}
	}
	else
	{
	}
}

/**
	Setup UDP core(s) to receive data from the detector head (srcIpAddr) usually
	For Hexitec MHz this is a single 100 G link either from the Alpha data card (in normal use) of from the server in test mode. It can also loop back from the accelerator in a loopback test.
	For Hexitec 6x2 this  is 2 off 10 G NICS in the server to 2 off 10 G UDP cores in the accelerator
*/


void XDmaHexitec::udpRxSetup(uint32_t *srcIpAddrP, uint32_t *accelIpAddrP, int headPort, int accelPort, HexitecUdpRxConnection connType)
{
	int devNum = m_xdma->getDevNum();
	uint32_t accelIpAddr;
	uint32_t headIpAddr;
	uint64_t headMacAddr;
	uint64_t accelMacAddr;
	int i;
	
	cout <<" Setting up RX UDP Core" << endl;
	if (headPort == 0)
		headPort = UDP_RX_HEAD_PORT;
	if (accelPort == 0)
		accelPort = UDP_RX_ACCEL_PORT;
	if (connType == Loopback)
		headPort = accelPort;
// Since we are using the IP core in RX mode, the Source and Destination are swapped. 

	for (i=0; i<m_numRxUdp; i++)
	{
		if (srcIpAddrP == nullptr || srcIpAddrP[0] == 0)
			headIpAddr = 192 << 24 | 168 << 16 | 2 << 8 | (8*devNum+4*i+1);
		else
			headIpAddr = srcIpAddrP[i];
		if (accelIpAddrP == nullptr || accelIpAddrP[0] == 0)
			accelIpAddr = 192 << 24 | 168 << 16 | 2 << 8 | (8*devNum+4*i+2);
		else	
			accelIpAddr = accelIpAddrP[i];
	
		headMacAddr = 0x02L<<40  | devNum*4+2*i+0;	
		accelMacAddr = 0x02L<<40 | devNum*4+2*i+1;
		
		headMacAddr = 0x62000000010AL;		// Matches Alpha data
//		headMacAddr =  0x043f72cedd66L;		// dsgsrv1 100G card
//		headIpAddr = 10<<24 | 100 << 8 | 108;  // Hopefully now coming from hexitec_test.cpp:test_head_ip_addr[0] 
//		headPort = 61648;						// Now from hexitec_test.cpp:test_head_port
		accelMacAddr = 0xE8EBD3CCA900L;
//		accelPort = 61649;						// Now from hexitec_test.cpp:test_accel_rx_port
//		accelIpAddr = 10<<24 |100<<8 | 8;	 // Hopefully now coming from hexitec_test.cpp:test_accel_rx_ip_addr[0] 
		m_udpCore[i].setSrcIpAddr(accelIpAddr);
		m_udpCore[i].setSrcMacAddr(accelMacAddr);

		switch (connType)
		{
		case Normal:
			m_udpCore[i].setDstIpAddr(headIpAddr);
			m_udpCore[i].setDstMacAddr(headMacAddr);
			break;
		case Loopback:
			m_udpCore[i].setDstIpAddr(accelIpAddr);
			m_udpCore[i].setDstMacAddr(accelMacAddr);
			break;
		case FromHost:
			m_udpCore[i].setDstIpAddr(headIpAddr);
			m_udpCore[i].setDstMacAddr(getMacAddr(headIpAddr));
			break;
		}			
		m_udpCore[i].setSrcPort(accelPort);
		m_udpCore[i].setDstPort(headPort);

		m_udpCore[i].enableAllFiltering();
//		m_udpCore[i].disableAllFiltering();
	}
}

/**
	Create sockets to allow testing of UDP transfer from host server acting as dummy head to the the accelerator card.
	For Hexitec MHz this is a single 100 G server NIC to a single UDP core in the accelerator
	For Hexitec 6x2 this  is 2 off 10 G NICS in the server to 2 off 10 G UDP cores in the accelerator
*/
void XDmaHexitec::udpRxTestCreateSockets(uint32_t *serverIpAddrP, uint32_t *accelIpAddrP, int hostPort, int accelPort)
{
	int i;
	struct hostent *hp;
	int value;
	socklen_t length;
	struct sockaddr_in hostAddress;
	struct sockaddr_in accelAddress;
	int devNum = m_xdma->getDevNum();
	uint32_t serverIpAddr;
	uint32_t accelIpAddr;
	
	if (hostPort == 0)
		hostPort = UDP_RX_HEAD_PORT;
	if (accelPort == 0)
		accelPort = UDP_RX_ACCEL_PORT;
	if (m_udpRxTestSocket[0] >= 0)
		return;
	for (i=0; i<m_numRxUdp; i++)
	{
		if (serverIpAddrP == nullptr || serverIpAddrP[0] == 0)
			serverIpAddr = 192 << 24 | 168 << 16 | 2 << 8 | (8*devNum+4*i+1);
		else
			serverIpAddr = serverIpAddrP[i];
		if (accelIpAddrP == nullptr || accelIpAddrP[0] == 0)
			accelIpAddr = 192 << 24 | 168 << 16 | 2 << 8 | (8*devNum+4*i+2);
		else	
			accelIpAddr = accelIpAddrP[i];

		m_udpRxTestSocket[i] = socket(AF_INET, SOCK_DGRAM, 0);
		if (m_udpRxTestSocket[i] < 0)
			throw XDmaException("udpRxTestCreateSockets: Cannot create socket, errno=%d", errno);

		length = sizeof(value);
		if (getsockopt(m_udpRxTestSocket[i], SOL_SOCKET, SO_RCVBUF, &value, &length) < 0)
			throw XDmaException("udpRxTestCreateSockets: Error getting UDP socket RCVBUF size");
		if (value < MAX_UDP_PACKET_BYTES)
			throw XDmaException("udpRxTestCreateSockets: Error UDP socket RCVBUF size= %d is too small. Require %d", value, MAX_UDP_PACKET_BYTES);
		printf("udpRxTestCreateSockets: Note maximum UDP packet size=%d\n", value);

	// Set IP's "QoS"
	/* Six possible values:
	 1: IPTOS_LOWDELAY (Minimize delay)
	 2: IPTOS_THROUGHPUT (Maximize throughput)
	 3: AF11 (DiffServ Class1 with low drop probabiltiy)
	 4: AF13 (DiffServ Class1 with high drop probabiltiy)
	 5: AF41 (DiffServ Class4 with low drop probabiltiy)
	 6: AF43 (DiffServ Class4 with high drop probabiltiy)
	 (EF-DiffServ with highest IP precedence needs root's privilege to set)
	 */
		value = IPTOS_THROUGHPUT;
		length = sizeof(value);
		if (setsockopt(m_udpRxTestSocket[i], IPPROTO_IP, IP_TOS, &value, length) < 0)
			throw XDmaException("udpRxTestCreateSockets :Error setting TOS bits.");

		hostAddress.sin_family = AF_INET;
		printf("serverIpAddr=0x%08X, port=%d\n", serverIpAddr, hostPort);
		hostAddress.sin_addr.s_addr = htonl(serverIpAddr);
		hostAddress.sin_port = htons(hostPort);

		length = sizeof(hostAddress);
		if (bind(m_udpRxTestSocket[i], (struct sockaddr *) &hostAddress, length)) 
			throw XDmaException("udpRxTestCreateSockets: Error binding client udp socket (errno %d), address = %s", errno, inet_ntoa(hostAddress.sin_addr));

		printf("accelIpAddr=0x%08X, port=%d\n", accelIpAddr, accelPort);
		accelAddress.sin_family = AF_INET;
		accelAddress.sin_addr.s_addr = htonl(accelIpAddr);
		accelAddress.sin_port = htons(accelPort);

		if (connect(m_udpRxTestSocket[i], (struct sockaddr *) &accelAddress, length) < 0)
			throw XDmaException("udpRxTestCreateSockets: Error connecting udp socket (errno %d)", errno);
	}
}

uint64_t XDmaHexitec::getMacAddr(uint32_t ipAddr)
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s;
	unsigned char *mac;
	int i;
	char *ifName=nullptr;
	uint64_t macAddr=0;
	if (getifaddrs(&ifaddr) == -1)
		throw XDmaException("getMacAddr: cannot getifaddrs (errno %d)", errno);

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;

/*		printf("%s  address family: %d%s\n",
			ifa->ifa_name, family,
			(family == AF_PACKET) ? " (AF_PACKET)" :
			(family == AF_INET) ?   " (AF_INET)" :
			(family == AF_INET6) ?  " (AF_INET6)" : "");
*/
		/* For an AF_INET interface address, find the IP address */

		if (family == AF_INET)
		{
			struct sockaddr_in * sin_addr = (struct sockaddr_in*)ifa->ifa_addr;
//			printf("IP addr=0x%08X\n", ntohl(sin_addr->sin_addr.s_addr));
		
			if (ntohl(sin_addr->sin_addr.s_addr) == ipAddr)
			{
				ifName = ifa->ifa_name;
				break;
			}
		}
	}
	if (ifName == nullptr)
	{
		freeifaddrs(ifaddr);
		throw XDmaException("getMacAddr: cannot find interface with idaddr = %d.%d.%d.%d", (ipAddr>>24) & 0xFF, (ipAddr>>16) & 0xFF, (ipAddr>>8) & 0xFF, (ipAddr>>0) & 0xFF);
	}		
		
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family == AF_PACKET && strcmp(ifa->ifa_name, ifName) == 0)
		{
			struct sockaddr_ll *addr_ll = (struct sockaddr_ll *)ifa->ifa_addr;
//			printf("Matched interface %s\n", ifa->ifa_name);
			mac = addr_ll->sll_addr;
			if (mac != nullptr)
			{
/*				for (i=0; i<6; i++)
					printf(":%02X", mac[i]);
				printf("\n");
*/
				for (i=0; i<6; i++)
					macAddr = macAddr<<8 | mac[i];
				break;
			}
		}
	}
	freeifaddrs(ifaddr);
	return macAddr;
}

int XDmaHexitec::getUdpRxTestSocket(int toggle)
{
	if (toggle < 0 || toggle >= m_numPbDma)
		throw XDmaException("getUdpRxTestSocket: Invalid socket toggle %d", toggle);
	return m_udpRxTestSocket[toggle];
}
	
uint32_t XDmaHexitec::getRxEthernetReg(int ethNum, int offset)
{
	if (ethNum < 0 || ethNum >= m_numRxUdp)
		throw XDmaException("getRxEthernetReg: ethNum=%d is out of range 0...%d", ethNum, m_numRxUdp-1);
	return *(m_xdma->m_regsBAR.m_base+(HEXITEC_ETH_CORE_BASE+ethNum*HEXITEC_ETH_CORE_STRIDE+offset)/sizeof(uint32_t));
}
void XDmaHexitec::setRxEthernetReg(int ethNum, int offset, uint32_t value)
{
	if (ethNum < 0 || ethNum >= m_numRxUdp)
		throw XDmaException("getRxEthernetReg: ethNum=%d is out of range 0...%d", ethNum, m_numRxUdp-1);
	*(m_xdma->m_regsBAR.m_base+(HEXITEC_ETH_CORE_BASE+ethNum*HEXITEC_ETH_CORE_STRIDE+offset)/sizeof(uint32_t)) = value;
}
uint64_t XDmaHexitec::getRxEthernetReg64(int ethNum, int offset)
{
	
	if (ethNum < 0 || ethNum >= m_numRxUdp)
		throw XDmaException("getRxEthernetReg: ethNum=%d is out of range 0...%d", ethNum, m_numRxUdp-1);
	return *(uint64_t*)((m_xdma->m_regsBAR.m_base+(HEXITEC_ETH_CORE_BASE+ethNum*HEXITEC_ETH_CORE_STRIDE+offset)/sizeof(uint32_t)));
}

void XDmaHexitec::printClockFrequencies()
{
	int i, j;
	uint32_t regs[5];
	double freq;
	const char *name[] = {"tx_usr_clk0", "tx_usr_clk1", "rx_usr_clk0", "rx_usr_clk1", "get_ref_clk"};
	bool allValid;
	
	setGlobReg(HEXITEC_GLB_TX_USR_CLK0_FREQ, 1);	// Trigger read;
	for (i=0;i<100; i++)
	{
		readGlobRegs(HEXITEC_GLB_TX_USR_CLK0_FREQ, 5, regs);
		allValid=true;
		for (j=0;j<5; j++)
		{
			if (!(regs[j] & HEXITEC_CLOCK_MEASURE_BOTH_VALID ))
				allValid = false;
		}
		if (allValid)
			break;
		this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!allValid)
		printf("Note not all frequency measures finished\n");
	for (i=0;i<5; i++)
	{
		freq = (double)(regs[i] & 0x1FFFFFFF) /(100000*10E-3);
		printf("Freq %d = %s : Reg=0x%08X, freq=%g MHz\n", i, name[i], regs[i], freq);
	}
}

void XDmaHexitec::setRxEthernetLoopback(int flags)
{
	int i;
	uint32_t mode;
	if (m_generation == HexitecGenHexitec)
	{
		for (i=0; i< m_numRxUdp; i++)
		{
			mode = getRxEthernetReg(i, ETHERNET_MODE_REG);
			if (flags)
				mode |= ETHERNET_MODE_LOOPBACK;
			else
				mode &= ~ETHERNET_MODE_LOOPBACK;
			setRxEthernetReg(i, ETHERNET_MODE_REG, mode);
		}
	}
	else
	{
		/* For HexitecMHz this is a write to HEXITEC_GLB_DATA_PATH which is combined with other parts of setup? */
	}
}

void XDmaHexitec::udpTxSetup(uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, int accelPort, int serverPort, int farmBase, int farmNum, bool enbFarmMode, uint16_t interFrameGap, bool useArp)
{
	int devNum = m_xdma->getDevNum();

	uint64_t accelMacAddr;
	uint64_t serverMacAddr;
	uint32_t control=0;
	uint32_t accelIpAddr;
 	uint32_t serverIpAddr;
	
	cout <<" Setting up TX UDP Core with inter-frame gap=" <<interFrameGap<< endl;
	if (useArp &&!enbFarmMode)
		throw XDmaException("udpTxSetup: Requested use Arp without farm mode. Don't think this is possible???");
	
	if (accelPort == 0)
		accelPort = UDP_TX_ACCEL_PORT;
	if (serverPort == 0)
		serverPort = UDP_TX_SERVER_PORT;
	if (enbFarmMode)
		control |= UDP_CORE_CNTL_LUT_MODE;
	
	for (int i=0; i<m_numTxUdp; i++)
	{
		if (accelIpAddrP == nullptr || accelIpAddrP[0] == 0)
			accelIpAddr = 192 << 24 | 168 << 16 | 3 << 8 | (8*devNum+4*i+1); // Iterating over multiple output UDP interfaces in firmware (currently only 1)
		else
			accelIpAddr = accelIpAddrP[i];
		if (serverIpAddrP == nullptr || serverIpAddrP[0] == 0)
			serverIpAddr = 192 << 24 | 168 << 16 | 3 << 8 | (8*devNum+4*i+2);	// Iterating over multiple output UDP interfaces in firmware (currently only 1)
		else
			serverIpAddr = serverIpAddrP[i];		// serverIpAddrP is a pointer to an array of IP addresses
			
		accelMacAddr = 0x12L<<40 | devNum*4+2*i+1;	
		m_udpTxCore[i].setSrcIpAddr(accelIpAddr);
		m_udpTxCore[i].setSrcMacAddr(accelMacAddr);

		m_udpTxCore[i].setDstIpAddr(serverIpAddr);
		m_udpTxCore[i].setDstMacAddr(getMacAddr(serverIpAddr));
		if (accelPort < 0)
		{
			control |= UDP_CORE_CNTL_TUSER_SRC_PRT;
			printf("udpTxSetup: accelPort=%d, to control = 0x%08X\n", accelPort, control);
			m_udpTxCore[i].setSrcPort(1);
		}
		else
			m_udpTxCore[i].setSrcPort(accelPort);
		m_udpTxCore[i].setDstPort(serverPort);

//		m_udpTxCore[i].enableAllFiltering();
		m_udpTxCore[i].setControl(control);
		if (enbFarmMode)
		{
			if (useArp)
			{
				for (int j=0; j< farmNum; j++)
					m_udpTxCore[i].setLutEntry(farmBase+j, 0L, serverIpAddr, serverPort+j);
				m_udpTxCore[i].setArpPositions(true, farmBase, farmNum);
				m_udpTxCore[i].setArpControl(UDP_CORE_ARP_CNTL_ACTIVE);
				int timeout=0;
				do
				{
					bool allDone=true;
					for (int j=0; j< farmNum; j++)
					{
						uint32_t arpStatus= m_udpTxCore[i].getArpStatus(farmBase+j);
						printf ("Port %d : Entry %3d : status=0x%08X\n", i, farmBase+j, arpStatus);
						if (!(arpStatus & UDP_CORE_ARP_STATUS_SEEN_RESPONSE))
							allDone=false;
					}
					if (allDone)
						break;
					this_thread::sleep_for (chrono::milliseconds(100));	
				} while (++timeout < 100);
				
	
			}
			else
			{
				m_udpTxCore[i].setArpPositions(false, enbFarmMode, farmNum);
				for (int j=0; j< farmNum; j++)
					m_udpTxCore[i].setLutEntry(farmBase+j, getMacAddr(serverIpAddr), serverIpAddr, serverPort+j);
			}
		}
		m_udpTxCore[i].setInterFrameGap(interFrameGap);
		accelIpAddr += 4;
		serverIpAddr += 4;
	}
		
}

/**
	Create sockets to allow testing of UDP TX from the accelerator card to a NIC in the host.
*/
void XDmaHexitec::udpTxTestCreateSockets(uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, int accelPort, int serverPort, int numSockets, bool sameIpAddr)
{
	int i;
	struct hostent *hp;
	int value;
	socklen_t length;
	struct sockaddr_in hostAddress;
	struct sockaddr_in accelAddress;
	int devNum = m_xdma->getDevNum();
	uint32_t accelIpAddr, serverIpAddr;
	
	if (accelPort == 0)
		accelPort = UDP_TX_ACCEL_PORT;
	if (serverPort == 0)
		serverPort = UDP_TX_SERVER_PORT;
	for (i=0; i<numSockets; i++)
	{
		if (accelIpAddrP == nullptr || accelIpAddrP[0] == 0)
			accelIpAddr = 192 << 24 | 168 << 16 | 3 << 8 | (8*devNum+1);
		else
			accelIpAddr = accelIpAddrP[0];		// Single accelerator IP output sent to multiple  NICs via switch?
		if (serverIpAddrP == nullptr || serverIpAddrP[0] == 0)
		{
			if (sameIpAddr)
				serverIpAddr = 192 << 24 | 168 << 16 | 3 << 8 | (8*devNum+2);		// Multiple port on same NIC OK
			else
				serverIpAddr = 192 << 24 | 168 << 16 | 3 << 8 | (8*devNum+2+4*i);	// Potentially multi NICs on same subnet.. need to decide on numbering
		}				
		else
		{
			if (sameIpAddr)
				serverIpAddr = serverIpAddrP[0];		// serverIpAddrP is pointer to single uint32 used for all socket, different port on same NIC. 
			else
				serverIpAddr = serverIpAddrP[i];		// serverIpAddrP is a pointer to an array of IP addresses
		}
			
		if (m_udpTxTestSocket[i] >= 0)
		{
			if (m_udpTxTestPort[i] == serverPort+i && accelIpAddr == m_txAccelIpAddr && serverIpAddr == m_txServerIpAddr[i])
				continue;
			else
				close(m_udpTxTestSocket[i]);
		}
		m_udpTxTestSocket[i] = socket(AF_INET, SOCK_DGRAM, 0);
		if (m_udpTxTestSocket[i] < 0)
			throw XDmaException("udpTxTestCreateSockets: Cannot create socket, errno=%d", errno);
		
		length = sizeof(value);
		if (getsockopt(m_udpTxTestSocket[i], SOL_SOCKET, SO_RCVBUF, &value, &length) < 0)
			throw XDmaException("udpTxTestCreateSockets: Error getting UDP socket RCVBUF size");
		if (value < MAX_UDP_PACKET_BYTES)
			throw XDmaException("udpTxTestCreateSockets: Error UDP socket RCVBUF size= %d is too small. Require %d", value, MAX_UDP_PACKET_BYTES);
		if (i ==0) printf("udpTxTestCreateSockets: Note maximum UDP packet size=%d\n", value);
		m_udpTxTestPort[i] = 0;
	// Set IP's "QoS"
	/* Six possible values:
	 1: IPTOS_LOWDELAY (Minimize delay)
	 2: IPTOS_THROUGHPUT (Maximize throughput)
	 3: AF11 (DiffServ Class1 with low drop probabiltiy)
	 4: AF13 (DiffServ Class1 with high drop probabiltiy)
	 5: AF41 (DiffServ Class4 with low drop probabiltiy)
	 6: AF43 (DiffServ Class4 with high drop probabiltiy)
	 (EF-DiffServ with highest IP precedence needs root's privilege to set)
	 */
		value = IPTOS_THROUGHPUT;
		length = sizeof(value);
		if (setsockopt(m_udpTxTestSocket[i], IPPROTO_IP, IP_TOS, &value, length) < 0)
			throw XDmaException("udpTxTestCreateSockets :Error setting TOS bits.");

		hostAddress.sin_family = AF_INET;
		if (i < 2) printf("serverIpAddr=0x%08X\n", serverIpAddr);
		if (sameIpAddr)
			hostAddress.sin_addr.s_addr = htonl(serverIpAddr);
		else
			hostAddress.sin_addr.s_addr = htonl(serverIpAddr);
		hostAddress.sin_port = htons(serverPort+i);

		length = sizeof(hostAddress);
		if (bind(m_udpTxTestSocket[i], (struct sockaddr *) &hostAddress, length)) 
			throw XDmaException("udpTxTestCreateSockets: Error binding client udp socket (errno %d), address = %s", errno, inet_ntoa(hostAddress.sin_addr));
		
		if (accelPort > 0)
		{
			accelAddress.sin_family = AF_INET;
	//		accelAddress.sin_addr.s_addr = htonl(netIpAddr+4*i+1);
			accelAddress.sin_addr.s_addr = htonl(accelIpAddr);
			accelAddress.sin_port = htons(accelPort);

			if (connect(m_udpTxTestSocket[i], (struct sockaddr *) &accelAddress, length) < 0)
				throw XDmaException("udpTxTestCreateSockets: Error connecting udp socket (errno %d)", errno);
		}
		m_udpTxTestPort[i] = serverPort+i;
		m_txServerIpAddr[i] = serverIpAddr;
	}
	m_txAccelIpAddr = accelIpAddr;
}
int XDmaHexitec::getUdpTxTestSocket(int index)
{
	if (index < 0 || index >= HEXITEC_MAX_FARM_SOCKETS)
		throw XDmaException("getUdpTxTestSocket: Invalid socket index %d", index);
	return m_udpTxTestSocket[index];
}

int XDmaHexitec::udpTxTestReadFrame(int index, int64_t & timeFrame, char *buf, size_t payloadBytes, int debugTag, bool discardStale, int64_t *dataMoverOverRunPtr )
{
	char sideBuffer[HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES];
	char *p;
	size_t bytesRead=0, bytesRemaining=payloadBytes;
	int packetNum=-1;
	int packetCount=1;
	bool useSideBuffer=false;
	ssize_t rc;
	uint64_t *tptr;
	int recvPacket;
	int64_t recvFrame;
	int pNumErrors=0;
	int fNumErrors=0;
	int64_t dataMoverOverRunFrame=(dataMoverOverRunPtr!=nullptr)?*dataMoverOverRunPtr:-1L;
	
	
	if (index < 0 || index >= HEXITEC_MAX_FARM_SOCKETS || m_udpTxTestSocket[index] < 0)
		throw XDmaException("udpTxTestReadFrame: Invalid socket index %d", index);
	
	do
	{
		if (payloadBytes-bytesRead >= HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES)
		{
			useSideBuffer = false;
			p = buf + bytesRead;
		}
		else
		{
			useSideBuffer = true;
			p = sideBuffer;
		}
		try 
		{
			rc = recv(m_udpTxTestSocket[index], p, HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES, 0);
		} 
		catch (abi::__forced_unwind&)
		{  // handle pthread_cancel stack unwinding exception
			throw;
		}

		if (rc < 0)
		{
			if (errno == EINTR)
				continue;
			else
				throw XDmaException("udpTxTestReadFrame: recv returnd error, errno=%d", errno);
		}
		if (rc < HEXITEC_UDP_TRAILER_BYTES)
		{
			printf("Received only %zu bytes, which is less than the trailer ... WHY?\n", rc);
			continue;
		}
		tptr = (uint64_t *)(p+rc-HEXITEC_UDP_TRAILER_BYTES);
		recvPacket = HEXITEC_UDP_TRAILER7_PACKET(tptr[7]);
		recvFrame  = HEXITEC_UDP_TRAILER7_FRAME(tptr[7]);
		if (timeFrame < 0)
			timeFrame = recvFrame;
		else if (timeFrame != recvFrame)
		{
			if (++fNumErrors < 4);
				printf("Time frame mismatch tag=%d: expecting %ld, received %ld at packet=%d\n", debugTag, timeFrame, recvFrame, recvPacket);
		}
		int dataMoverOverrun = (int) HEXITEC_UDP_TRAILER5_OVERRUN(tptr[5]);
		if (dataMoverOverrun != 0)
		{
			if (dataMoverOverRunFrame < 0)
			{
				int64_t processedFrame=HEXITEC_UDP_TRAILER5_PROCSSED_FRAME(tptr[5]);
				printf("\nudpTxTestReadFrame: Data mover detected overrun at processed frame %ld, readout frame=%ld\n", processedFrame, recvFrame);
			}
			if (dataMoverOverRunFrame < 0 ||  recvFrame < dataMoverOverRunFrame)
				dataMoverOverRunFrame = recvFrame;
		}
		
//		printf("\nPacketCount=%d, Received %zu bytes, frame=%d, packetIndex=%d at bytesRead=%zu of %zu\r", packetCount, rc, recvFrame, recvPacket, bytesRead, payloadBytes);
		if (packetNum == -1)
			packetNum = recvPacket;
		else if (packetNum != recvPacket)
		{
			if (++pNumErrors < 4)
				printf("\nPacket jump at tag=%d, recvFrame=%ld, Expected packetNum=%d, jump=%d\n", debugTag, recvFrame, packetNum, recvPacket-packetNum);
			if (recvPacket > packetNum)
			{
				size_t oldBytesRead = bytesRead;
				bytesRead = (recvPacket-1)*(rc-HEXITEC_UDP_TRAILER_BYTES); // Adjust 
				if (bytesRead+rc-HEXITEC_UDP_TRAILER_BYTES > payloadBytes)
					break;
				if (!useSideBuffer)
				{
					memcpy(buf+bytesRead, p, rc-HEXITEC_UDP_TRAILER_BYTES);
					memset(p, 0, bytesRead-oldBytesRead);
				}
			}
			packetNum = recvPacket;
		}
		fflush(stdout);
		if (useSideBuffer)
		{
			size_t toCopy = rc-HEXITEC_UDP_TRAILER_BYTES;
			if (toCopy > payloadBytes-bytesRead)
			{
				printf("Warning at Frame %ld, packet %d received bytes seem to over run the buffer. Last packet=%zu, payloadBytes=%zu, bytesRead=%zu\n", timeFrame, packetNum, rc, payloadBytes, bytesRead);  
				toCopy = payloadBytes-bytesRead;
			}
			memcpy(buf+bytesRead, sideBuffer, toCopy);
		}
		bytesRead += rc-HEXITEC_UDP_TRAILER_BYTES;
		packetNum++; // packetNum is updated with dropped packets to follow the incoming packet number
		packetCount++; //packetCount just counts the received packets
	} while (bytesRead < payloadBytes);
	if (dataMoverOverRunPtr !=nullptr)
		*dataMoverOverRunPtr = dataMoverOverRunFrame;
	return fNumErrors+pNumErrors;
//	printf("\n");
}

string XDmaHexitec::udpShowRxStatus()
{
	int i;
	uint32_t status = getGlobReg(HEXITEC_GLB_UDP_STATUS);
	uint64_t frame, packet;
	stringstream sstream; 
	for (i=0;i<m_numRxUdp; i++)
	{
		frame = getGlobReg64(HEXITEC_GLB_UDP_ERROR_FRAME0+2*i);
		packet = getGlobReg64(HEXITEC_GLB_UDP_ERROR_PACKET0+2*i);
		sstream << "UDP RX" << i << "UnexpectedSOF=" << HEXITEC_RX_STAT_UNEXPECTED_SOF_MHZ(status,i);
		sstream << ", Bad packet Num=" << HEXITEC_RX_STAT_UNEXPECTED_SOF_MHZ(status, i) << ", Missing EOF=" << HEXITEC_RX_STAT_MISSING_EOF_MHZ(status, i) << endl;
		sstream << "Flags=" << HEXITEC_RX_STAT_FLAGS_MHZ(status, i) << ", Packet=" << packet <<	", Frame=" << frame << endl;
	}
	return sstream.str();
}
