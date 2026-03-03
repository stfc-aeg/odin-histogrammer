
#ifndef _XDMA_UDP_CORE_H
#define _XDMA_UDP_CORE_H 1
#include "xdma.h"
#include <exception>
#include <stdint.h>
#include <stdarg.h>

#define UDP_CORE_LOOPBACK_NONE		0		//!< Normal operation, no loopback
#define UDP_CORE_LOOPBACK_NEAR_PCS	1		//!< Near end (sending end) loopback after 8B/10B etc but before PISO)
#define UDP_CORE_LOOPBACK_NEAR_PMA	2		//!< Near end (sending end) loopback just before output driver
#define UDP_CORE_LOOPBACK_FAR_PMA	4		//!< Far end (receiving end) loopback after SIPO.
#define UDP_CORE_LOOPBACK_FAR_PCS	6		//!< Far end (receiving end) loopback after all processing, just before RX interface

#define UDP_CORE_BASE_UDP_CORE_CONTROL     0x000    	//!< Base address offset for UDP core control registers
#define UDP_CORE_BASE_ARP_MODE_CONTROL     0x800      	//!< Base address offset for ARP mode control registers
#define UDP_CORE_BASE_FARM_MODE_LUT        0x1000      	//!< Base address offset for UDP core Farm Mode Lut

#define  UDP_CORE_SRC_MAC_LOWER_OFFSET           0x0
#define  UDP_CORE_SRC_MAC_UPPER_OFFSET           0x4
#define  UDP_CORE_DST_MAC_LOWER_OFFSET           0xC
#define  UDP_CORE_DST_MAC_UPPER_OFFSET           0x10
#define  UDP_CORE_SRC_IPADDR_OFFSET              0x28
#define  UDP_CORE_DST_IPADDR_OFFSET              0x24
#define  UDP_CORE_UDPPORTS_OFFSET                0x2C
#define  UDP_CORE_FILTER_CFG_OFFSET              0x38
#define  UDP_CORE_INTER_FRAME_GAP_OFFSET         0x40
#define  UDP_CORE_CONTROL_OFFSET 				 0x48
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



#define UDP_CORE_BROADCAST_EN         0x00000001 		//!< Enables Broadcast Recieving
#define UDP_CORE_ARP_EN               0x00000002 		//!< Enables ARPs
#define UDP_CORE_PING_EN              0x00000004 		//!< Enables Pings
#define UDP_CORE_PASS_UNS_ETHTYPE     0x00000100 		//!< Enable Passing of Unsupported Ethernet Packets
#define UDP_CORE_PASS_UNS_IPV4        0x00000200 		//!< Enable Passing of Unsupported IPv4 Packets
#define UDP_CORE_DST_MAC_CHK_EN       0x00010000 		//!< Check Dest MAC address Field Of Incoming Packet
#define UDP_CORE_SRC_MAC_CHK_EN       0x00020000 		//!< Check Source MAC address Field Of Incoming Packet
#define UDP_CORE_DST_IP_CHK_EN        0x00040000 		//!< Check Dest IP address Field Of Incoming Packet
#define UDP_CORE_SRC_IP_CHK_EN        0x00080000 		//!< Check Source IP address Field Of Incoming Packet
#define UDP_CORE_DST_PORT_CHK_EN      0x00100000 		//!< Check Dest UDP Port Field Of Incoming Packet
#define UDP_CORE_SRC_PORT_CHK_EN      0x00200000 		//!< Check Source UDP Port Field Of Incoming Packet
#define UDP_CORE_PACKET_COUNT_RST_N   0x00400000 		//!< Reset Packet Counts
#define UDP_CORE_STRIP_UNS_PRO        0x01000000 		//!< Remove The Header From Incoming Unsupported IPv4 Data
#define UDP_CORE_STRIP_UNS_ETH        0x02000000 		//!< Remove The Header From Incoming Unsupported Ethernet Data
#define UDP_CORE_CHK_IP_LENGTH        0x04000000 		//!< Check Received Packets Total IP Length and Cut off Extra Padding



#define UDP_CORE_CNTL_FIXED_PKT_SIZE    0x00000008		//!< Use Fixed Packet Size In Tx For Outgoing Packets - Currently Unused"/>
#define UDP_CORE_CNTL_UDP_CHECKSUM_ZERO 0x00000010		//!< Set UDP Checksum to Zero - Currently Unused"/>
#define UDP_CORE_CNTL_LUT_MODE          0x00000020		//!< LUT Mode Enable - Use To Set Dst Addresses Of Outgoing UDP Packets From LUT"/>
#define UDP_CORE_CNTL_TUSER_DST_PRT     0x00000040		//!< Use Bits 15:0 of tuser for destination UDP port of outgoing packets"/>
#define UDP_CORE_CNTL_TUSER_SRC_PRT     0x00000080		//!< Use Bits 31:16 of tuser for source UDP port of outgoing packets"/>
#define UDP_CORE_CNTL_RESET_N           0x00008000		//!< Soft Reset - Currently Unused"/>
#define UDP_CORE_CNTL_UDP_LENGTH(x)     (((x),,16)&0xFFFF0000)		//!< UDP Length For Fixed Packet Length - Currently Unused"/>

#define UDP_CORE_LUT_LOWER_MAC_ADDR   0x0          		//!< 256 4 byte entries of destination MAC address lower 32 bits
#define UDP_CORE_LUT_UPPER_MAC_ADDR   0x400         	//!< 256 4 byte entries of Destination MAC Addr[47..32]
#define UDP_CORE_LUT_IP_ADDR          0x800				//!< 256 4 byte entries LUT Destination IP Addr[31..0]
#define UDP_CORE_LUT_DST_PORT         0xC00         	//!< 256 4 byte entries LUT Destination Port[15..0]

#define UDP_CORE_NUM_FARM_LUT			256			//!< Maximum number of entries in Farm mode socket LUT

#define UDP_CORE_ARP_CNTL_OFFSET	(UDP_CORE_BASE_ARP_MODE_CONTROL+0)	//!< Control register for ARP block.
#define UDP_CORE_ARP_CNTL_ACTIVE	(1<<0)								//!< Activate ARP mode (overall)
#define UDP_CORE_ARP_CNTL_RESET		(1<<1)								//!< Reset ARP Entry Timers TODO

#define UDP_CORE_ARP_ACTIVE_OFFSET	(UDP_CORE_BASE_ARP_MODE_CONTROL+4)	//!< Array of 8 off 32 bit register to make ARP position active.
#define UDP_CORE_ARP_TIMEOUT_OFFSET	(UDP_CORE_BASE_ARP_MODE_CONTROL+0x24)	//!< Sets Timeout Lengths for ARP Entries
#define UDP_CORE_ARP_TIMEOUT_REQUEST(x) 	(((x)&0xFFF)<<4)			//!< Time To Wait For After Sending Request
#define UDP_CORE_ARP_TIMEOUT_REFRESH(x) 	(((x)&0xFFFF)<<16)			//!< Time To Wait Before Refresh Needed

#define UDP_CORE_ARP_STATUS_OFFSET	(UDP_CORE_BASE_ARP_MODE_CONTROL+0x28)	//!< Array of 256 off 32 bit LUT entries
#define UDP_CORE_ARP_STATUS_ACTIVE				(1<<0)                  //!< Asserted When Position Is Active
#define UDP_CORE_ARP_STATUS_TIMED_OUT			(1<<1)                  //!< Asserted If ARP Refresh Timer Has Timed Out
#define UDP_CORE_ARP_STATUS_SEEN_RESPONSE		(1<<2)                  //!< Asserted If Response Has Been Seen From This Position
#define UDP_CORE_ARP_STATUS_REQUEST_SENT		(1<<3)                  //!< Asserted If A Request Has Been Sent and Awaiting Response
#define UDP_CORE_ARP_STATUS_REQUEST_TIMEOUT		0x0000FFF0              //!< How Long Before Request Times Out
#define UDP_CORE_ARP_STATUS_REFRESH_TIMEOUT		0xFFFF0000              //!< How Long Before Position Needs Refresh

#define UDP_CORE_ARP_POSITIONS	256

struct UDPCoreCounts 
{
	uint32_t udp, arp, ping, unsEthernet, unsIPv4, droppedMac, droppedIP, droppedUdpPort;
};

class XDmaUDPCore {
	XDma *m_xdma=nullptr;
	volatile uint32_t *m_regs=nullptr;
	int m_coreNum=-1;
public :
	XDmaUDPCore () { m_xdma= nullptr;}
	XDmaUDPCore (int coreNum, XDma *xdma, int baseOffset);
	~XDmaUDPCore() {}
	void setControl(uint32_t control);
	uint32_t getControl();
	string  ipAddrToDots(uint32_t ipAddr);
	string  macAddrToHex(uint64_t macAddr);
	uint64_t getSrcMacAddr();
	void setSrcMacAddr(uint64_t macAddr);
	void setDstMacAddr(uint64_t macAddr);
	uint64_t bytesToMacAddr(uint8_t bytes[6]);
	void setSrcMacAddr(uint8_t bytes[6]);
	void setDstMacAddr(uint8_t bytes[6]);
	uint64_t getDstMacAddr();
	uint32_t ipAddrDots2int(const char *ipAddrDots);
	void setSrcIpAddr(const char *ipAddrDots);
	void setDstIpAddr(const char *ipAddrDots);
	void setSrcIpAddr(uint32_t ipAddr);
	void setDstIpAddr(uint32_t ipAddr);
	uint32_t getSrcIpAddr();
	uint32_t getDstIpAddr();
	void setSrcPort(uint16_t srcPort);
	void setDstPort(uint16_t dstPort);
	uint16_t getSrcPort();
	uint16_t getDstPort();
	void enableAllFiltering();
	void disableAllFiltering();
	void resetCounts();
	void getCounts(UDPCoreCounts &counts);
	string getCountsString();
	string getConfig();
	void setLutEntry(int entry, uint64_t dstMacAddr, uint32_t dstIpAddr, uint16_t dstPort);
	void getLutEntry(int entry, uint64_t *dstMacAddr, uint32_t *dstIpAddr, uint16_t *dstPort);
	void setInterFrameGap(uint16_t gap);
	uint16_t getInterFrameGap();

	void setArpControl(uint32_t control);
	uint32_t getArpControl();
	void setArpPositions(bool enable, int first, int num);
	uint32_t getArpStatus(int posn);

	};

#endif
