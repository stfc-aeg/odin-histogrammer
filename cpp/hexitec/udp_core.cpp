
#include <exception>
#include <stdint.h>
#include <stdarg.h>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include "xdma.h"
#include "udp_core.h"

/*
#define  UDP_CORE_SRC_MAC_LOWER_OFFSET           0x0
#define  UDP_CORE_SRC_MAC_UPPER_OFFSET           0x4
#define  UDP_CORE_DST_MAC_LOWER_OFFSET           0xC
#define  UDP_CORE_DST_MAC_UPPER_OFFSET           0x10
#define  UDP_CORE_SRC_IPADDR_OFFSET              0x28
#define  UDP_CORE_DST_IPADDR_OFFSET              0x24
#define  UDP_CORE_UDPPORTS_OFFSET                0x2C
#define  UDP_CORE_FILTER_CFG_OFFSET              0x38
#define  UDP_CORE_UDP_COUNTS_OFFSET              0x4C
#define  UDP_CORE_ARP_COUNTS_OFFSET              0x54
#define  UDP_CORE_PING_COUNTS_OFFSET             0x50
#define  UDP_CORE_UNS_ETH_COUNTS_OFFSET          0x58
#define  UDP_CORE_UNS_IPV4_COUNTS_OFFSET         0x5C
#define  UDP_CORE_DROPPED_MAC_COUNTS_OFFSET      0x60
#define  UDP_CORE_DROPPED_IPADDR_COUNTS_OFFSET   0x64
#define  UDP_CORE_DROPPED_UDPPORT_COUNTS_OFFSET  0x68

#define UDP_CORE_UDP_MAC_LOWER_MASK             0xFFFFFFFFL
#define UDP_CORE_UDP_MAC_UPPER_MASK             0x0000FFFFL
#define UDP_CORE_UDP_SRC_PORT_MASK              0x0000FFFF
#define UDP_CORE_UDP_DST_PORT_MASK              0xFFFF0000
*/
using namespace std;

XDmaUDPCore :: XDmaUDPCore(int coreNum, XDma *xdma, int baseOffset)
{
	m_coreNum = coreNum;
	m_xdma = xdma;
	
	m_regs = (m_xdma->m_regsBAR.m_base+baseOffset/sizeof(uint32_t));
}

string  XDmaUDPCore::ipAddrToDots(uint32_t ipAddr)
{
	int i, x;
	string str;
	str = to_string(ipAddr>>24);
	for (i=1; i<4; i++)
	{
		x = (ipAddr >> (3-i)*8) & 0xFF;
		str = str + "."+ to_string(x);
	}
    return str;
}
string  XDmaUDPCore::macAddrToHex(uint64_t macAddr)
{
	stringstream sstream;	
	sstream << hex << setw(2) << setfill('0');
	int i, x;

	sstream << (macAddr>>40);
	for (i=1; i<6; i++)
	{
		x = (macAddr >> (5-i)*8) & 0xFF;
		sstream << ":" << x;
	}
	return sstream.str();
}

uint64_t XDmaUDPCore::getSrcMacAddr()
{
	uint32_t lower, upper;
	uint64_t fullMacAddr;
	lower = m_regs[UDP_CORE_SRC_MAC_LOWER_OFFSET/sizeof(uint32_t)];
	upper = m_regs[UDP_CORE_SRC_MAC_UPPER_OFFSET/sizeof(uint32_t)];
    fullMacAddr = (uint64_t)upper << 32 | lower;
	
    return fullMacAddr;
}
void XDmaUDPCore::setSrcMacAddr(uint64_t macAddr)
{
	m_regs[UDP_CORE_SRC_MAC_LOWER_OFFSET/sizeof(uint32_t)] = (uint32_t)(macAddr & UDP_CORE_UDP_MAC_LOWER_MASK);
	m_regs[UDP_CORE_SRC_MAC_UPPER_OFFSET/sizeof(uint32_t)] = (uint32_t)((macAddr >> 32) & UDP_CORE_UDP_MAC_UPPER_MASK);
}
void XDmaUDPCore::setDstMacAddr(uint64_t macAddr)
{
	m_regs[UDP_CORE_DST_MAC_LOWER_OFFSET/sizeof(uint32_t)] = (uint32_t)(macAddr & UDP_CORE_UDP_MAC_LOWER_MASK);
	m_regs[UDP_CORE_DST_MAC_UPPER_OFFSET/sizeof(uint32_t)] = (uint32_t)((macAddr >> 32) & UDP_CORE_UDP_MAC_UPPER_MASK);
}
uint64_t XDmaUDPCore::bytesToMacAddr(uint8_t bytes[6])
{
	uint64_t macAddr=0;
	int i;
	
	for (i=0; i<6; i++)
		macAddr = macAddr << 8 | (uint64_t)bytes[i];
	return macAddr;
}

void XDmaUDPCore::setSrcMacAddr(uint8_t bytes[6])
{
	setSrcMacAddr(bytesToMacAddr(bytes));
}
void XDmaUDPCore::setDstMacAddr(uint8_t bytes[6])
{
	setDstMacAddr(bytesToMacAddr(bytes));
}
uint64_t XDmaUDPCore::getDstMacAddr()
{
	uint32_t lower, upper;
	uint64_t fullMacAddr;
	lower = m_regs[UDP_CORE_DST_MAC_LOWER_OFFSET/sizeof(uint32_t)];
	upper = m_regs[UDP_CORE_DST_MAC_UPPER_OFFSET/sizeof(uint32_t)];
    fullMacAddr = (uint64_t)upper << 32 | lower;
	
    return fullMacAddr;
}

uint32_t XDmaUDPCore::ipAddrDots2int(const char *ipAddrDots)
{
	int i, x;
	uint32_t ipAddr=0;
	const char *start;
	char *end;
	
	for (i=0, start=ipAddrDots; i<4; i++)
	{
		x = strtol(start, &end, 10);
		if (start == end)
			throw XDmaException("ipAddrDots2int: Cannot parse IP address %s field %d", ipAddrDots, i);
		if (i < 3 && end != nullptr &&  *end != '.')
			throw XDmaException("ipAddrDots2int: Cannot parse IP address %s, unexpected separator '%c' at field %d", ipAddrDots, *end, i);
		if (x < 0 || x > 255)
			throw XDmaException("ipAddrDots2int: Cannot parse IP address %s, value %d is not in range 0...255 ", ipAddrDots, x);
		ipAddr = ipAddr << 8 | x;
		start = (const char *)end;
	}
	return ipAddr;
}

void XDmaUDPCore::setSrcIpAddr(const char *ipAddrDots)
{
	uint32_t ipAddr = ipAddrDots2int(ipAddrDots);
	m_regs[UDP_CORE_SRC_IPADDR_OFFSET/sizeof(uint32_t)] = ipAddr;
}
void XDmaUDPCore::setDstIpAddr(const char *ipAddrDots)
{
	uint32_t ipAddr = ipAddrDots2int(ipAddrDots);
	m_regs[UDP_CORE_DST_IPADDR_OFFSET/sizeof(uint32_t)] = ipAddr;
}
void XDmaUDPCore::setSrcIpAddr(uint32_t ipAddr)
{
	m_regs[UDP_CORE_SRC_IPADDR_OFFSET/sizeof(uint32_t)] = ipAddr;
}
void XDmaUDPCore::setDstIpAddr(uint32_t ipAddr)
{
	m_regs[UDP_CORE_DST_IPADDR_OFFSET/sizeof(uint32_t)] = ipAddr;
}
uint32_t XDmaUDPCore::getSrcIpAddr()
{
	return m_regs[UDP_CORE_SRC_IPADDR_OFFSET/sizeof(uint32_t)];
}
uint32_t XDmaUDPCore::getDstIpAddr()
{
	return m_regs[UDP_CORE_DST_IPADDR_OFFSET/sizeof(uint32_t)];
}
void XDmaUDPCore::setSrcPort(uint16_t srcPort)
{
	uint32_t ports = m_regs[UDP_CORE_UDPPORTS_OFFSET/sizeof(uint32_t)];
	ports = ports & UDP_CORE_UDP_DST_PORT_MASK;
	ports |= (uint32_t)srcPort;
	m_regs[UDP_CORE_UDPPORTS_OFFSET/sizeof(uint32_t)] = ports;
}
void XDmaUDPCore::setDstPort(uint16_t dstPort)
{
	uint32_t ports = m_regs[UDP_CORE_UDPPORTS_OFFSET/sizeof(uint32_t)];
	ports = ports & UDP_CORE_UDP_SRC_PORT_MASK;
	ports |= (uint32_t)dstPort<<16;
	m_regs[UDP_CORE_UDPPORTS_OFFSET/sizeof(uint32_t)] = ports;
}
uint16_t XDmaUDPCore::getSrcPort()
{
	uint32_t ports = m_regs[UDP_CORE_UDPPORTS_OFFSET/sizeof(uint32_t)];
	return (uint16_t)(ports & UDP_CORE_UDP_SRC_PORT_MASK);
}
uint16_t XDmaUDPCore::getDstPort()
{
	uint32_t ports = m_regs[UDP_CORE_UDPPORTS_OFFSET/sizeof(uint32_t)];
	return (uint16_t)((ports & UDP_CORE_UDP_DST_PORT_MASK) >> 16);
}
void XDmaUDPCore::setLutEntry(int entry, uint64_t dstMacAddr, uint32_t dstIpAddr, uint16_t dstPort)
{
	volatile uint32_t *ptr = m_regs+(UDP_CORE_BASE_FARM_MODE_LUT/sizeof(uint32_t)+entry);

	if (entry <0 || entry >= UDP_CORE_NUM_FARM_LUT)
		throw XDmaException("setLutEntry: table entry must be in range0...%d not %d", UDP_CORE_NUM_FARM_LUT-1, entry);
//	printf("setLutEntry: entry=%d : dstMacAddr=0x%012lX, dstIpAddr=%08X, dstPort=%d\n", entry, dstMacAddr, dstIpAddr, dstPort);
	ptr[UDP_CORE_LUT_LOWER_MAC_ADDR/sizeof(uint32_t)] = (uint32_t)(dstMacAddr & UDP_CORE_UDP_MAC_LOWER_MASK);
	ptr[UDP_CORE_LUT_UPPER_MAC_ADDR/sizeof(uint32_t)] = (uint32_t)((dstMacAddr >> 32)& UDP_CORE_UDP_MAC_UPPER_MASK);
	ptr[UDP_CORE_LUT_IP_ADDR/sizeof(uint32_t)] = dstIpAddr;
	ptr[UDP_CORE_LUT_DST_PORT/sizeof(uint32_t)] = (uint32_t)dstPort;
}

void XDmaUDPCore::getLutEntry(int entry, uint64_t *dstMacAddr, uint32_t *dstIpAddr, uint16_t *dstPort)
{
	volatile uint32_t *ptr = m_regs+(UDP_CORE_BASE_FARM_MODE_LUT/sizeof(uint32_t)+entry);

	if (entry <0 || entry >= UDP_CORE_NUM_FARM_LUT)
		throw XDmaException("getLutEntry: table entry must be in range0...%d not %d", UDP_CORE_NUM_FARM_LUT-1, entry);
	if (dstMacAddr != nullptr)
		*dstMacAddr = ptr[UDP_CORE_LUT_LOWER_MAC_ADDR/sizeof(uint32_t)] | ((uint64_t)(ptr[UDP_CORE_LUT_UPPER_MAC_ADDR/sizeof(uint32_t)])<<32);
	if (dstIpAddr != nullptr)
		*dstIpAddr = ptr[UDP_CORE_LUT_IP_ADDR/sizeof(uint32_t)];
	if (dstPort != nullptr)
		*dstPort = (uint16_t)ptr[UDP_CORE_LUT_DST_PORT/sizeof(uint32_t)];
}


void XDmaUDPCore::enableAllFiltering()
{
	uint32_t filterControl = 	UDP_CORE_BROADCAST_EN | UDP_CORE_ARP_EN | UDP_CORE_PING_EN |			// 0x077F0307
								UDP_CORE_PASS_UNS_ETHTYPE | UDP_CORE_PASS_UNS_IPV4 |
								 UDP_CORE_DST_MAC_CHK_EN | UDP_CORE_SRC_MAC_CHK_EN | UDP_CORE_DST_IP_CHK_EN | UDP_CORE_SRC_IP_CHK_EN |
								 UDP_CORE_DST_PORT_CHK_EN | UDP_CORE_SRC_PORT_CHK_EN | UDP_CORE_PACKET_COUNT_RST_N |
								 UDP_CORE_STRIP_UNS_PRO | UDP_CORE_STRIP_UNS_ETH | UDP_CORE_CHK_IP_LENGTH; 
								 
    m_regs[UDP_CORE_FILTER_CFG_OFFSET/sizeof(uint32_t)] = filterControl;
}


void XDmaUDPCore::disableAllFiltering()
{
	uint32_t filterControl = 	UDP_CORE_BROADCAST_EN | UDP_CORE_ARP_EN | UDP_CORE_PING_EN |			// 0x077F0307
								UDP_CORE_PASS_UNS_ETHTYPE | UDP_CORE_PASS_UNS_IPV4 |
								 UDP_CORE_PACKET_COUNT_RST_N |
								 UDP_CORE_STRIP_UNS_PRO | UDP_CORE_STRIP_UNS_ETH | UDP_CORE_CHK_IP_LENGTH; 
								 
    m_regs[UDP_CORE_FILTER_CFG_OFFSET/sizeof(uint32_t)] = filterControl;
}

void XDmaUDPCore:: resetCounts()
{
	uint32_t filterControl = m_regs[UDP_CORE_FILTER_CFG_OFFSET/sizeof(uint32_t)];
	filterControl &= ~UDP_CORE_PACKET_COUNT_RST_N;
    m_regs[UDP_CORE_FILTER_CFG_OFFSET/sizeof(uint32_t)] = filterControl;
	filterControl |= UDP_CORE_PACKET_COUNT_RST_N;
    m_regs[UDP_CORE_FILTER_CFG_OFFSET/sizeof(uint32_t)] = filterControl;
}


void XDmaUDPCore:: getCounts(UDPCoreCounts &counts)
{
	counts.udp 			=  m_regs[UDP_CORE_UDP_COUNTS_OFFSET/sizeof(uint32_t)];
	counts.arp 			=  m_regs[UDP_CORE_ARP_COUNTS_OFFSET/sizeof(uint32_t)];
	counts.ping			=  m_regs[UDP_CORE_PING_COUNTS_OFFSET/sizeof(uint32_t)];
	counts.unsEthernet	=  m_regs[UDP_CORE_UNS_ETH_COUNTS_OFFSET/sizeof(uint32_t)];
	counts.unsIPv4		=  m_regs[UDP_CORE_UNS_IPV4_COUNTS_OFFSET/sizeof(uint32_t)];
	counts.droppedMac	=  m_regs[UDP_CORE_DROPPED_MAC_COUNTS_OFFSET/sizeof(uint32_t)];
	counts.droppedIP	=  m_regs[UDP_CORE_DROPPED_IPADDR_COUNTS_OFFSET/sizeof(uint32_t)];
	counts.droppedUdpPort= m_regs[UDP_CORE_DROPPED_UDPPORT_COUNTS_OFFSET/sizeof(uint32_t)];
};

string XDmaUDPCore::getCountsString()
{
	stringstream sstr;
	UDPCoreCounts counts;
	getCounts(counts);
	
	sstr << "UDP Counts:                       " << counts.udp  << endl;
	sstr << "ARP Counts:                       " << counts.arp  << endl;
	sstr << "Ping Counts:                      " << counts.ping  << endl;
	sstr << "Unsupported Ethernet Type Counts: " << counts.unsEthernet	  << endl;
	sstr << "Unsupported IPv4 Protocol Counts: " << counts.unsIPv4		  << endl;
	sstr << "Dropped MAC Counts:               " << counts.droppedMac	  << endl;
	sstr << "Dropped IP Address Counts:        " << counts.droppedIP	  << endl;
	sstr << "Dropped UDP Port Counts:          " << counts.droppedUdpPort  << endl;
	return sstr.str();
}

string XDmaUDPCore::getConfig()
{
	stringstream sstr;
	sstr << "Source MAC Addr:      " << macAddrToHex(getSrcMacAddr()) << endl;
	sstr << "Source IP Addr:       " << ipAddrToDots(getSrcIpAddr()) << endl;
	sstr << "Source UDP Port:      " << getSrcPort() << endl;
	sstr << endl;
	sstr << "Destination MAC Addr: " << macAddrToHex(getDstMacAddr()) << endl;
	sstr << "Destination IP Addr:  " << ipAddrToDots(getDstIpAddr()) << endl;
	sstr << "Destination UDP Port: " << getDstPort() << endl;

	return sstr.str();
}

void XDmaUDPCore::setControl(uint32_t control)
{
	m_regs[UDP_CORE_CONTROL_OFFSET/sizeof(uint32_t)] = control;
}

uint32_t  XDmaUDPCore::getControl()
{
	return m_regs[UDP_CORE_CONTROL_OFFSET/sizeof(uint32_t)];
}

void XDmaUDPCore::setInterFrameGap(uint16_t gap)
{
	m_regs[UDP_CORE_INTER_FRAME_GAP_OFFSET/sizeof(uint32_t)] = (uint32_t)gap;
}
uint16_t XDmaUDPCore::getInterFrameGap()
{
	return (uint16_t)(m_regs[UDP_CORE_INTER_FRAME_GAP_OFFSET/sizeof(uint32_t)] & 0xFFFF);
}

void XDmaUDPCore::setArpControl(uint32_t control)
{
	m_regs[UDP_CORE_ARP_CNTL_OFFSET/sizeof(uint32_t)] = control;
}
uint32_t XDmaUDPCore::getArpControl()
{
	return m_regs[UDP_CORE_ARP_CNTL_OFFSET/sizeof(uint32_t)];
}
void XDmaUDPCore::setArpPositions(bool enable, int first, int num)
{
	if (first ==0 && num < 0)
		num = UDP_CORE_ARP_POSITIONS;

	if (first < 0 | num < 1 || first+num > UDP_CORE_ARP_POSITIONS)
		throw XDmaException("enbArpPositions:first=%d and num=%d not in range 0 for %d", first, num, UDP_CORE_ARP_POSITIONS);
	for (int regNum=0; regNum<8; regNum++)
	{
		uint32_t reg = m_regs[UDP_CORE_ARP_ACTIVE_OFFSET/sizeof(uint32_t)+regNum];
		for (int bit=0; bit<32; bit++)
		{
			int posn = 32*regNum+bit;
			if (posn >= first && posn < first+num)
			{
				if (enable)
					reg |= 1 << bit;
				else
					reg &= ~(1<<bit);
			}
		}
		m_regs[UDP_CORE_ARP_ACTIVE_OFFSET/sizeof(uint32_t)+regNum] = reg;
	}
}
uint32_t XDmaUDPCore::getArpStatus(int posn)
{
	if (posn < 0 || posn >= UDP_CORE_ARP_POSITIONS)
		throw XDmaException("getArpStatus:Position=%d not in range 0..%d", posn, UDP_CORE_ARP_POSITIONS-1);
		
	return m_regs[UDP_CORE_ARP_STATUS_OFFSET+posn];
}
