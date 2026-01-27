#include <iostream>
#include <fstream>
#include <cerrno>
#include <iomanip>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h> 
#include <linux/if_packet.h>
#include <linux/ioctl.h>
#include <ifaddrs.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "errors.h"
#include "datamod.h"

#include "xdma_hbm_hist.h"
#include "xdma_hexitec.h"

#define USE_PTHREAD_CANCEL

#ifdef USE_PTHREAD_CANCEL
#include <cxxabi.h>
#endif

void XDmaHexitec::eventListOpenFiles(char * rootFileName)
{
	char fName[FILENAME_MAX+10];
	int newFd[HEXITEC_EVLIST_MAX_SOCKETS];
	
	eventListCloseFiles();
	for (int i=0; i<m_numEventWriters; i++)
	{
		if (snprintf(fName, FILENAME_MAX+1, "%s_%03d.dat", rootFileName, i) > FILENAME_MAX)
		{
			for (int j=0; i<i; j++)
				close(newFd[j]);
			fName[FILENAME_MAX] = 0;
			throw XDmaHexitecException("eventListOpenFiles: File name %s is too long so truncated", fName);
		}			
		if ((newFd[i] = open(fName, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH)) < 0)
		{
			for (int j=0; i<i; j++)
				close(newFd[j]);
			throw XDmaHexitecException("eventListOpenFiles: Cannot open file %s, errno=$%d", fName, errno);
		}
	}
	for (int i=0; i<m_numEventWriters; i++)
	{
		m_eventWriter[i].m_mutex.lock();
		m_eventWriter[i].m_fileFd = newFd[i];
		m_eventWriter[i].m_mutex.unlock();
	}
}
	
void XDmaHexitec::eventListCloseFiles()
{
	for (int i=0; i<HEXITEC_EVLIST_MAX_SOCKETS; i++)
	{	
		if (m_eventWriter[i].m_threadValid && m_eventWriter[i].m_fileFd >= 0)
		{
			m_eventWriter[i].m_mutex.lock();
			close(m_eventWriter[i].m_fileFd);
			m_eventWriter[i].m_fileFd = -1;
			m_eventWriter[i].m_mutex.unlock();
		}
	}
}	
void XDmaHexitec::eventListStop()
{
	int rc;
	eventListCloseFiles();
	for (int i=0; i<HEXITEC_EVLIST_MAX_SOCKETS; i++)
	{
		if (m_eventWriter[i].m_threadValid)
			pthread_cancel(m_eventWriter[i].m_thread.native_handle());
	}
	for (int i=0; i<HEXITEC_EVLIST_MAX_SOCKETS; i++)
	{
		if (m_eventWriter[i].m_threadValid)
			m_eventWriter[i].m_thread.join();
		m_eventWriter[i].m_threadValid = false;
	}
	for (int i=0; i<HEXITEC_EVLIST_MAX_SOCKETS; i++)
	{
		if (m_eventWriter[i].m_socketFd >= 0)
		{
			if (!m_eventWriteQdma)	
				close(m_eventWriter[i].m_socketFd);
			m_eventWriter[i].m_socketFd = -1;
		}
	}
	m_numEventWriters = 0;
}	

void XDmaHexitec::eventListCalcIpAddrs(uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, bool sameIpAddr, int i, uint32_t &accelIpAddr, uint32_t &serverIpAddr)
{
	int devNum = m_xdma->getDevNum();
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
			serverIpAddr = serverIpAddrP[0];		// serverIpAddrP is pointer to single uint32_t used for all socket, different ports on same NIC. 
		else
			serverIpAddr = serverIpAddrP[i];		// serverIpAddrP is a pointer to an array of IP addresses
	}
}

void XDmaHexitec::eventListCreateSockets( uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, int accelPort, int serverPort, int numSockets, bool sameIpAddr)
{
	int i;
	struct hostent *hp;
	int value;
	socklen_t length;
	struct sockaddr_in hostAddress;
	struct sockaddr_in accelAddress;
	uint32_t accelIpAddr; 
	uint32_t serverIpAddr;

	if (numSockets < 1 || numSockets >HEXITEC_EVLIST_MAX_SOCKETS)
		throw XDmaException("eventListCreateSockets: Number of sockets=%d is out of range 1..%d", numSockets, HEXITEC_EVLIST_MAX_SOCKETS);

	if (accelPort == 0)
		accelPort = UDP_TX_ACCEL_PORT;
	if (serverPort == 0)
		serverPort = UDP_EVLIST_SERVER_PORT;
	
	eventListCloseFiles();
	if (m_eventWriteQdma)
		eventListStop();
	
	for (i=numSockets; i<HEXITEC_EVLIST_MAX_SOCKETS; i++)
	{
		// Stop any extra unused threads 
		if (m_eventWriter[i].m_threadValid)
		{
			pthread_cancel(m_eventWriter[i].m_thread.native_handle());
			m_eventWriter[i].m_thread.join();
			m_eventWriter[i].m_threadValid = false;
		}
		if (m_eventWriter[i].m_socketFd >= 0)
		{
			close(m_eventWriter[i].m_socketFd);
			m_eventWriter[i].m_socketFd = -1;
		}
	}
	m_eventListLostFrames = 0L;
	m_eventListFramingErrors = 0L;
	m_eventListFileErrors = 0L;
	for (i=0; i<numSockets; i++)
	{
		eventListCalcIpAddrs(accelIpAddrP, serverIpAddrP, sameIpAddr, i, accelIpAddr, serverIpAddr);
		if (m_eventWriter[i].m_socketFd >= 0 && (m_eventWriter[i].m_ipAddr != serverIpAddr || m_eventWriter[i].m_port != serverPort))
		{
			if (m_eventWriter[i].m_threadValid)
			{
				pthread_cancel(m_eventWriter[i].m_thread.native_handle());
				m_eventWriter[i].m_thread.join();
				m_eventWriter[i].m_threadValid = false;
			}
			close(m_eventWriter[i].m_socketFd);
			m_eventWriter[i].m_socketFd = -1;
		}
		if (m_eventWriter[i].m_socketFd < 0)
		{
			m_eventWriter[i].m_socketFd= socket(AF_INET, SOCK_DGRAM, 0);
			if (m_eventWriter[i].m_socketFd < 0)
				throw XDmaException("eventListCreateSockets: Cannot create socket, errno=%d", errno);
		
			length = sizeof(value);
			if (getsockopt(m_eventWriter[i].m_socketFd, SOL_SOCKET, SO_RCVBUF, &value, &length) < 0)
				throw XDmaException("eventListCreateSockets: Error getting UDP socket RCVBUF size");
			if (value < MAX_UDP_PACKET_BYTES)
				throw XDmaException("eventListCreateSockets: Error UDP socket RCVBUF size= %d is too small. Require %d", value, MAX_UDP_PACKET_BYTES);
			printf("eventListCreateSockets: Note SO_RCVBUF=%d\n", value);
			m_eventWriter[i].m_port = 0;

			value = IPTOS_THROUGHPUT;
			length = sizeof(value);
			if (setsockopt(m_eventWriter[i].m_socketFd, IPPROTO_IP, IP_TOS, &value, length) < 0)
				throw XDmaException("eventListCreateSockets :Error setting TOS bits.");

			hostAddress.sin_family = AF_INET;
			if (sameIpAddr)
			{
				hostAddress.sin_addr.s_addr = htonl(serverIpAddr);
				hostAddress.sin_port = htons(serverPort+i);
			}
			else
			{
				hostAddress.sin_addr.s_addr = htonl(serverIpAddr);
				hostAddress.sin_port = htons(serverPort);
			}
			length = sizeof(hostAddress);
			if (bind(m_eventWriter[i].m_socketFd, (struct sockaddr *) &hostAddress, length)) 
				throw XDmaException("eventListCreateSockets: Error binding client udp socket (errno %d), address = %s", errno, inet_ntoa(hostAddress.sin_addr));
			
			accelAddress.sin_family = AF_INET;
			accelAddress.sin_addr.s_addr = htonl(accelIpAddr);
			accelAddress.sin_port = htons(accelPort);

			if (connect(m_eventWriter[i].m_socketFd, (struct sockaddr *) &accelAddress, length) < 0)
				throw XDmaException("eventListCreateSockets: Error connecting udp socket (errno %d)", errno);
			m_eventWriter[i].m_port = serverPort;
			m_eventWriter[i].m_ipAddr = serverIpAddr;
		}
		m_eventWriter[i].m_currFrame = -1L;
		if (!m_eventWriter[i].m_threadValid)
		{
			m_eventWriter[i].m_thread = thread(&XDmaHexitec::eventListWrite, this, i);
			m_eventWriter[i].m_threadValid = true;
		}
	}
	m_numEventWriters = numSockets;
	m_eventWriteQdma = false;
}
void XDmaHexitec::eventListStartQdmaRx(int numQueues)
{
	int i;

	eventListCloseFiles();
	if (!m_eventWriteQdma)
		eventListStop();

	for (i=numQueues; i<HEXITEC_EVLIST_MAX_SOCKETS; i++)
	{
		if (m_eventWriter[i].m_threadValid)
		{
			pthread_cancel(m_eventWriter[i].m_thread.native_handle());
			m_eventWriter[i].m_thread.join();
			m_eventWriter[i].m_threadValid = false;
		}
		m_eventWriter[i].m_socketFd = -1;		// Stop Unwanted RX thread. The QDMA driver thread will still be running and we can come back to it if required
	}
	
	m_eventListFramingErrors = 0L;
	m_eventListFileErrors = 0L;
	m_eventListLostFrames = 0L;
	for (i=0; i<numQueues; i++)
	{
		m_eventWriter[i].m_socketFd = m_xdma->getEvListFd(i);
		m_eventWriter[i].m_currFrame = -1L;
		if (!m_eventWriter[i].m_threadValid)
		{
			m_eventWriter[i].m_thread = thread(&XDmaHexitec::eventListWrite, this, i);
			m_eventWriter[i].m_threadValid = true;
		}
	}
	m_numEventWriters = numQueues;
	m_eventWriteQdma = true;
}

uint32_t XDmaHexitec::eventListCalcDataPath(int numThreads, bool useQdma, HexitecEventListSizes eventSizes)
{
	uint32_t dataPath=0;
	int i;

	dataPath = HEXITEC_DATA_PATH_EVLIST_ENB  | HEXITEC_DATA_PATH_EVLIST_FARM_BASE(HEXITEC_EVLIST_Q_BASE);
	if (useQdma)
		dataPath |= HEXITEC_DATA_PATH_EVLIST_FIXED4K;
	else
		dataPath |= HEXITEC_DATA_PATH_EVLIST_UDP;
	for (i=0;i<8; i++)
	{
		if (numThreads == 1<<i)
			break;
	}
	if (i==8)
		throw XDmaHexitecException("eventListCalcDataPath: ERROR: Unsupported number of event list threads %d, must be power of 2\n", numThreads);
	dataPath |= HEXITEC_DATA_PATH_EVLIST_FARM_MASK((1<<i)-1);
	if (eventSizes == EventSize128)
	{
		dataPath |= HEXITEC_DATA_PATH_EVLIST_DETAILED;
	}
	m_eventListEventSize = eventSizes;
	return dataPath;
}

void XDmaHexitec::eventListResetFrameCounters()
{
	m_eventListLostFrames = 0L;
	m_eventListFramingErrors = 0L;
	m_eventListFileErrors = 0L;
	for (int i=0; i<m_numEventWriters; i++)
	{
		m_eventWriter[i].m_currFrame = -1L;
	}
}

bool XDmaHexitec::eventListAnyBusy()
{
	for (int i=0; i<m_numEventWriters; i++)
	{
		if (m_eventWriter[i].m_busy)
			return true;
	}
	return false;
}

void XDmaHexitec::eventListWaitIdle()
{
	int poll=100;
	
	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		if (eventListAnyBusy())
			poll = 100;
		else
			poll--;
	} while (poll > 0);
}


void XDmaHexitec::eventListWrite(int tNum)
{
	ssize_t bytes;
	uint32_t txBuff[MAX_UDP_PACKET_BYTES/sizeof(uint32_t)+1];
	uint32_t *rxBuff=&txBuff[1];
	int chip=-1;
	int rc;
	bool expectHeader;
	size_t maxReadSize=MAX_UDP_PACKET_BYTES;
	int packet = 0;
	
	printf("eventListWrite:  Thread number %d, started\n", tNum);

#ifndef USE_PTHREAD_CANCEL
	int epollFd;
	struct epoll_event event;
	if (!m_eventWriteQdma)
	{
		if ((epollFd = epoll_create1(0)) < 0)
			throw XDmaException("evListHist: Cannot create epoll fd, errno=%d", errno);
		memset(&event, 0 , sizeof(event));
		event.events = EPOLLIN;
		event.data.fd = m_evListSocket[tNum];
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, m_evListSocket[tNum], &event) < 0)
			throw XDmaException("evListHist: Cannot add socket fd=%d to epoll_ctl, errno=%d", m_evListSocket[tNum], errno);
		event.data.fd = m_eventFd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, m_eventFd, &event) < 0)
			throw XDmaException("evListHist: Cannot add shared eventFd=%d to epoll_ctl, errno=%d", m_eventFd, errno);
	}
#endif
	if (m_eventWriteQdma)
		maxReadSize = 4096;	// Fixed 4 k buffer size for QDMA Linux driver version.
	while (1)
	{
		chip = -1;
#ifndef USE_PTHREAD_CANCEL
		if (!m_eventWriteQdma)
		{
			memset(&event, 0 , sizeof(event));
			rc = epoll_wait(epollFd, &event, 1, -1);
//			printf("tNum=%d: epoll_wait return %d, event.data.fd=%d\n", tNum, rc, event.data.fd);
			if (rc < 0)
				throw XDmaException("evListHist: Error on epoll_wait, errno=%d", errno);
			if (event.data.fd == m_eventFd)
				break;
			if (rc == 0)
				continue;
		}
			bytes = read(m_evListSocket[tNum], reinterpret_cast<char *>(rxBuff), maxReadSize);
#else
		m_eventWriter[tNum].m_busy = true;
		try 
		{
			bytes = recv(m_eventWriter[tNum].m_socketFd, reinterpret_cast<char *>(rxBuff), maxReadSize,  MSG_DONTWAIT); // Get data immediately if available
		} 
		catch (abi::__forced_unwind&)
		{  // handle pthread_cancel stack unwinding exception
			throw;
		}
		if (bytes <0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			m_eventWriter[tNum].m_busy = false;
			try 
			{
				bytes = recv(m_eventWriter[tNum].m_socketFd, reinterpret_cast<char *>(rxBuff), maxReadSize,  0); // Wait for data marking threads as not busy
			} 
			catch (abi::__forced_unwind&)
			{  // handle pthread_cancel stack unwinding exception
				throw;
			}
			m_eventWriter[tNum].m_busy = true;		// Now mark thread as busy while it processes this data
		}			
		
#endif
//		printf("tnum%d, packet=%d: received %d bytes, errno=%d\n", tNum, packet, bytes, errno);
		if (bytes < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				continue;
			if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
				printf("eventListWrite:: tNum=%d, read(%d) returns %zd, errno=%d\n", tNum, m_eventWriter[tNum].m_socketFd, bytes, errno);
		}
		else if (bytes < 12)
		{
			if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
				printf("eventListWrite::tNum=%d, packet=%d, short packet length=%zd\n", tNum, packet, bytes);
			continue;
		}
		else
		{
			int dataInc=(m_eventListEventSize==EventSize128)?4:1;
			expectHeader = true;
			for (int i=0; i<bytes/sizeof(uint32_t); i+=dataInc)
			{
				if (expectHeader)
				{
					if (HEXITEC_EVLIST_GET_TAGS(rxBuff[i]) != HEXITEC_EVLIST_TAG_HEADER)
					{
						if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
							printf("eventListWrite: tNum=%d, packet=%d: Length=%zd, Missing header at offset=%d, data = 0x%08X, %08X, %08X, %08X\n", tNum, packet, bytes, i, rxBuff[i+0], rxBuff[i+1], rxBuff[i+2], rxBuff[i+3]);
					}
				}
				else
				{
					if (HEXITEC_EVLIST_GET_TAGS(rxBuff[i]) == HEXITEC_EVLIST_TAG_HEADER)
					{
						if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
						{
							if (i > 0)
								printf("eventListWrite: tNum=%d, packet=%d: Length=%zd,  Unexpected header at offset=%d, data[%d] = 0x%08X, data[%d..] = 0x%08X, %08X, %08X, %08X\n", 
									tNum, packet, bytes, i, i-1, rxBuff[i-1], i, rxBuff[i+0], rxBuff[i+1], rxBuff[i+2], rxBuff[i+3]);
							else
								printf("eventListWrite: tNum=%d, packet=%d: Length=%zd,  Unexpected header at offset=%d, data = 0x%08X, %08X, %08X, %08X\n", tNum, packet, bytes, i, rxBuff[i+0], rxBuff[i+1], rxBuff[i+2], rxBuff[i+3]);
						}
					}
				}
				if (expectHeader || HEXITEC_EVLIST_GET_TAGS(rxBuff[i]) == HEXITEC_EVLIST_TAG_HEADER)
				{
					if (HEXITEC_EVLIST_GET_TAGS(rxBuff[i+0]) != HEXITEC_EVLIST_TAG_HEADER || HEXITEC_EVLIST_GET_TAGS(rxBuff[i+1]) != HEXITEC_EVLIST_TAG_HEADER_CONT ||
						HEXITEC_EVLIST_GET_TAGS(rxBuff[i+2]) != HEXITEC_EVLIST_TAG_HEADER_CONT || HEXITEC_EVLIST_GET_TAGS(rxBuff[i+3]) != HEXITEC_EVLIST_TAG_HEADER_CONT)
					{
						if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
							printf("eventListWrite: tNum=%d, packet=%d: Length=%zd, Bad header tags bits at %d in words 0x%08X, %08X, %08X, %08X\n", tNum, packet, bytes, i, rxBuff[i+0], rxBuff[i+1], rxBuff[i+2], rxBuff[i+3]);
						break;
					}

					int64_t frameNum = HEXITEC_EVLIST_H0_GET_FNUM(rxBuff[i+0]) | (HEXITEC_EVLIST_H1_GET_FNUM(rxBuff[i+1]) << 30);

					int newChip = HEXITEC_EVLIST_H1_GET_CHIP(rxBuff[i+1]);
/*
					timeFrame = HEXITEC_EVLIST_H2_GET_TF(rxBuff[i+2]);
					extTrig = HEXITEC_EVLIST_H3_GET_EXTTRIG(rxBuff[i+3]);
*/
					if (newChip >= m_numChips)
					{
						if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
							printf("eventListWrite: tNum=%d: Bad header chip number %d at %d from words 0x%08X, %08X, %08X, %08X\n", tNum, newChip, i, rxBuff[i+0], rxBuff[i+1], rxBuff[i+2], rxBuff[i+3]);
						break;
					}

					if (m_eventWriter[tNum].m_currFrame < 0)
						m_eventWriter[tNum].m_currFrame = frameNum;
					else if (frameNum != m_eventWriter[tNum].m_currFrame)
					{
						if (frameNum == tNum)
							m_eventWriter[tNum].m_currFrame = tNum;		// Wrap round, particularly using playback testinbg
						else if (frameNum == m_eventWriter[tNum].m_currFrame+m_numEventWriters)
							m_eventWriter[tNum].m_currFrame = frameNum;
						else if (frameNum % m_numEventWriters != tNum)
						{
							if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
								printf("eventListWrite: tNum=%d: Unexpect frameNumber=%d, modulus=%ld, expecting \n", tNum, frameNum, frameNum % m_numEventWriters, m_eventWriter[tNum].m_currFrame.load());
						}
						else
						{
							int64_t skip = frameNum - m_eventWriter[tNum].m_currFrame+m_numEventWriters;
							if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
								printf("eventListWrite: tNum=%d: frame jump from %ld to %ld, skip=%ld\n", tNum, m_eventWriter[tNum].m_currFrame.load(), frameNum, skip);
							skip /= m_numEventWriters;
							if (skip > 1)
								skip--;
							if (skip > 0)
								m_eventListLostFrames += skip;
							m_eventWriter[tNum].m_currFrame = frameNum;
						}
					}


					if (m_eventListEventSize==EventSize32)
						i += 3;
					expectHeader = false;
				}
				else if (HEXITEC_EVLIST_GET_TAGS(rxBuff[i]) == HEXITEC_EVLIST_TAG_HEADER_CONT)
				{
					if (m_eventListFramingErrors++ < m_eventListMaxPrintErrors);
						printf("evListHist: tNum=%d, packet=%d: Unexpected header continue tag bits at %d from words 0x%08X, %08X, %08X, %08X\n", tNum, packet, i, rxBuff[i+0], rxBuff[i+1], rxBuff[i+2], rxBuff[i+3]);
					break;
				}
				else if (HEXITEC_EVLIST_GET_TAGS(rxBuff[i]) == HEXITEC_EVLIST_TAG_TRAILER || (m_eventListEventSize==EventSize128 && HEXITEC_EVLIST_GET_TAGS(rxBuff[i+3]) == HEXITEC_EVLIST_TAG_TRAILER ))
				{
					expectHeader = true;	// Expect header for next chip.
					if (m_eventWriteQdma && i < bytes/sizeof(uint32_t)-1 && HEXITEC_EVLIST_GET_TAGS(rxBuff[i+1]) == HEXITEC_EVLIST_TAG_DATA && HEXITEC_EVLIST_D_GET_CC(rxBuff[i+1]) == HEXITEC_CLUSTER_CLASS_PADDING)
						break;		// With QDMA, expect data to be padded to 4k.
				}
			}
			m_eventWriter[tNum].m_mutex.lock();
			if (m_eventWriter[tNum].m_fileFd >= 0)
			{
				txBuff[0] = bytes;
				char *cp = reinterpret_cast<char *>(txBuff);
				ssize_t remaining = bytes;
				if (m_eventWriteIncludeLength)
				{
					remaining += sizeof(uint32_t);
				}
				else
					cp += sizeof(uint32_t);

				while (remaining > 0)
				{
					ssize_t b = write(m_eventWriter[tNum].m_fileFd, cp, remaining);
					if (b < 0)
					{
						if (errno == EINTR)
							continue;
						else 
						{
							if (m_eventListFileErrors++ < m_eventListMaxPrintErrors)
								printf("eventListWrite: tNum=%d: write returned errno %d\n", tNum, errno);
							break;
						}
					}
					remaining -= b;
					cp += b;
				}
			}		
			m_eventWriter[tNum].m_mutex.unlock();
			packet++;
		}
	}
	printf("Exiting thread %d\n", tNum);
#ifndef USE_PTHREAD_CANCEL
	close(epollFd);
#endif
}
