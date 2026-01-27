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

#include "xdma_hbm_hist.h"
#include <linux/ioctl.h>
#define IOCTL_XDMA_ALIGN_GET    _IOR('q', 6, int)
#define RW_MAX_SIZE	0x7ffff000


/** Memory ordering
The mmapped pointers are mapped 	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
From https://www.amd.com/system/files/TechDocs/24593.pdf ... It appears is sufficient to ensure strict memory ordering.
Also pointers are declared as volatile.
So hopefully don't need any more fence instructions.
*/

using namespace std;


XDmaHbmHist :: XDmaHbmHist(XDma *xdma, int numContRegs)
{
	volatile uint32_t *p;
	int i, con, physPort, logicalPort;
	const char *cacheNames[] = {"None", "Inc Cache single", "Inc Cache multiple", "Sorter", "Sorter with neighbour coalesce"};
	const char *readWriteNames[] = { "None", "Reads wait for all writes sent", "Reads wait for all writes finished", "Buffer read data until all reads sent", "Buffer read data until all reads finished"};
	
	m_xdma = xdma;
	m_numContRegs = numContRegs;
	m_histTester = (volatile struct HistTesterRegs *)(m_xdma->m_regsBAR.m_base+HBM_HIST_TESTER_BASE/sizeof(uint32_t));
	m_hasMemBAR = m_xdma->m_hasMemBAR;
	for (i=0; i<numContRegs; i++)
		m_histRegs[i] = (struct AXIHistRegs *)(m_xdma->m_regsBAR.m_base+(HBM_HIST_REGS_BASE+i*HBM_HIST_REGS_INC)/sizeof(uint32_t));
	
	p = &(m_histRegs[0]->magic);
	
	m_histConf.initFromRegs(p);
	m_portMask[0] = m_histConf.HBMPortMask;
	
	for (i=1; i<numContRegs; i++)
	{
		HBMHistConfig sibling;
		sibling.initFromRegs((volatile uint32_t *)m_histRegs[i]);
		if (!m_histConf.isSibling(sibling))
		{
			for (int j=0; j<5; j++)
				printf("Reg %d : Hist[0] = 0x%08X, Hist[%d]=%08X\n", j, ((volatile uint32_t*)m_histRegs[0])[j], i, ((volatile uint32_t*)m_histRegs[i])[j]);
			printf("NBitsDataHist: %d %d\n", m_histConf.NBitsDataHist , sibling.NBitsDataHist);
			printf("NBitsDataAXI: %d %d\n", m_histConf.NBitsDataAXI , sibling.NBitsDataAXI);
			printf("NumStreams: %d %d\n", m_histConf.NumStreams , sibling.NumStreams);
			printf("NBitsAddrTopFixed: %d %d\n", m_histConf.NBitsAddrTopFixed , sibling.NBitsAddrTopFixed);
		    printf("nf: %d %d\n", m_histConf.NBitsStream , sibling.NBitsStream);
			printf("NBitsSubStream: %d %d\n", m_histConf.NBitsSubStream , sibling.NBitsSubStream);
			printf("NBitsBankAndGroup: %d %d\n", m_histConf.NBitsBankAndGroup , sibling.NBitsBankAndGroup);
			printf("NBitsAddrMem: %d %d\n", m_histConf.NBitsAddrMem , sibling.NBitsAddrMem);
		    printf("nf: %d %d\n", m_histConf.NBitsAddrIn , sibling.NBitsAddrIn);
			printf("BankPosn: %d %d\n", m_histConf.BankPosn , sibling.BankPosn);
			printf("BankStartBit: %d %d\n", m_histConf.BankStartBit , sibling.BankStartBit);
			printf("HistWordPerAXIWord: %d %d\n", m_histConf.HistWordPerAXIWord , sibling.HistWordPerAXIWord);
		    printf("HistTPGPresent: %d %d\n", m_histConf.HistTPGPresent , sibling.HistTPGPresent);
			printf("TotalMemWords: %ld %ld\n", m_histConf.TotalMemWords , sibling.TotalMemWords);
			printf("HistWordsPerStream: %ld %ld\n", m_histConf.HistWordsPerStream , sibling.HistWordsPerStream);
			printf("AXIWordsPerStream: %ld %ld\n", m_histConf.AXIWordsPerStream , sibling.AXIWordsPerStream);
			throw XDmaHbmException("XDmaHbmHist: Hist controller %d is not a sibling of 0", i);
		}
		m_portMask[i] = sibling.HBMPortMask;
	}
	for (i=0;i <HBM_TOTAL_PORTS; i++)
		m_physicalPort[i] = -1;
	logicalPort = 0;
	m_histConf.HBMPortMask =0;
	
	for (con=0; con<m_numContRegs; con++)
	{
		m_histConf.HBMPortMask |= m_portMask[con];	// make m_histConf.HBMPortMask show all the ports used in this aggregated HMB hist
		for (physPort=0; physPort<HBM_TOTAL_PORTS; physPort++)
		{
			if (m_portMask[con] & 1 << physPort)
			{
				for (i=0;i<logicalPort;i++)
				{
					if (m_physicalPort[i] == physPort)
						throw XDmaHbmException("XDmaHbmHist: physicalPort=%d appears at logical port=%d and %d. Check coding of VHDL HBM Connected mask for controller %d=0x%08X", 
								physPort, i, logicalPort, con, m_portMask[con] );
				}
				if (logicalPort == HBM_TOTAL_PORTS)
					throw XDmaHbmException("XDmaHbmHist: logical port has reached limit=%d", HBM_TOTAL_PORTS);
						
//				printf("logicalPort=%d => physPort=%d\n", logicalPort, physPort);
				m_physicalPort[logicalPort++] = physPort;
			}
		}			
	}
	m_clearMutex = std::unique_ptr<mutex>(new mutex);
}
/*XDmaHbmHist :: ~XDmaHbmHist()
{
}
*/
int XDmaHbmHist::getPhysicalPort(int logPort)
{
	if (logPort <0 || logPort >= HBM_TOTAL_PORTS)
		throw XDmaHbmException("getPhysicalPort: logical port %d out of range 0...%d", logPort, HBM_TOTAL_PORTS-1);
	if (m_physicalPort[logPort] < 0)
		throw XDmaHbmException("getPhysicalPort: physicalPort not defined for logical port %d", logPort);
		
	return m_physicalPort[logPort];
}

/**
	Clear region of histogram memory in a single stream (generally use all stream version)

@param portMask		Bitwise mask of whihc HBM ports to access 0..15 on left stack, 16..32 on right stack
@param stream		Stream Number 0.. NumStreams-1
@param startWord	First word to clear, measured within streams address space, in AXI words.
@param numWords		Number of AXI word to clear within stream.
*/
void XDmaHbmHist::startClearStream(uint32_t portMask, uint32_t stream, uint32_t startWord, uint32_t numWords, bool waitFinished)
{
	int i;
	m_clearMutex->lock();
	if (m_histConf.BankPosn == HBMHistConfig::RowColBank)
	{
		startWord *= m_histConf.NumStreams;
		startWord += stream;
	}
	else
	{
		startWord += stream * m_histConf.AXIWordsPerStream;
	}
	waitClearStart();
	for (i=0; i<m_numContRegs; i++)
	{
		if (portMask & m_portMask[i])
		{
			m_histRegs[i]->clearPortMask = portMask & m_portMask[i] ;
			m_histRegs[i]->clearCont = 0;

			m_histRegs[i]->clearStartWord = startWord;
			m_histRegs[i]->clearNumWords = numWords;
		}
	}
	if (waitFinished)
		waitClear();
	m_clearMutex->unlock();
}
/**
	Start clear region of histogram memory all streams.

@param portMask		Bitwise mask of whihc HBM ports to access 0..15 on left stack, 16..32 on right stack
@param startWord	First word to clear, measured within streams address space, in AXI words.
@param numWords		Number of AXI words to clear within stream.
*/
#if 0
// Reviewing 17/4/2024, not clear why startClearAll and startClear are separate.  Also think was clearing memory 16 times over?

void XDmaHbmHist::startClearAll(uint32_t portMask, uint32_t startWord, uint32_t numWords)
{
	int i;
	if (m_histConf.BankPosn == HBMHistConfig::RowColBank)
	{
		startWord *= m_histConf.NumStreams;
		numWords *= m_histConf.NumStreams;
	}
	for (i=0; i<m_numContRegs; i++)
	{
		if (portMask & m_portMask[i])
		{
			m_histRegs[i]->clearPortMask = portMask & m_portMask[i];
			m_histRegs[i]->clearCont = AXI_HIST_CC_CLEAR_ALL;

			m_histRegs[i]->clearStartWord = startWord;
			m_histRegs[i]->clearNumWords = numWords;
		}
	}
}
#else
void XDmaHbmHist::startClearAll(uint32_t portMask, uint32_t startWord, uint32_t numWords, bool waitFinished)
{
	startClear(portMask, startWord, numWords, 1, waitFinished);
}
#endif 
void XDmaHbmHist::startClear(uint32_t portMask, uint32_t startWord, uint32_t numWords, int allStreams, bool waitFinished)
{
	int i;

	m_clearMutex->lock();

	waitClearStart();
	if (m_histConf.BankPosn == HBMHistConfig::RowColBank)
	{
		startWord *= m_histConf.NumStreams;
		numWords *= m_histConf.NumStreams;
	}
		
	for (i=0; i<m_numContRegs; i++)
	{
		
		if (portMask & m_portMask[i])
		{
			m_histRegs[i]->clearPortMask = portMask & m_portMask[i];
			if (allStreams)
				m_histRegs[i]->clearCont = AXI_HIST_CC_CLEAR_ALL;
			else
				m_histRegs[i]->clearCont = 0;

			m_histRegs[i]->clearStartWord = startWord;
			m_histRegs[i]->clearNumWords = numWords;
		}
	}
	if (waitFinished)
		waitClear();
	m_clearMutex->unlock();
}

void XDmaHbmHist::startClearTPG(uint32_t portMask, uint32_t startWord, uint32_t numWords, int allStreams, bool waitFinished)
{
	int i;

	m_clearMutex->lock();

	waitClearStart();

	for (i=0; i<m_numContRegs; i++)
	{
		if (portMask & m_portMask[i])
		{
			m_histRegs[i]->clearPortMask = portMask & m_portMask[i];
			if (allStreams)
				m_histRegs[i]->clearCont = AXI_HIST_CC_CLEAR_ALL | AXI_HIST_CC_USE_TPG;
			else
				m_histRegs[i]->clearCont = AXI_HIST_CC_USE_TPG;

			m_histRegs[i]->clearStartWord = startWord;
			m_histRegs[i]->clearNumWords = numWords;
		}
	}
	if (waitFinished)
		waitClear();
	m_clearMutex->unlock();
}

/**
	Wait for clear engine to go idle showing that all clears have finished
*/
void XDmaHbmHist:: waitClear()
{
	int polls=0;
	int i;
	uint32_t clearBusy;
	int numIdles=0;
	
	do
	{
		clearBusy = 0;
		for (i=0; i<m_numContRegs; i++)
			clearBusy |= m_histRegs[i]->clearBusy;
		if (clearBusy == 0)
		{
			if (numIdles++ > 2) // Clear busy could be sampled false between clear buffered in the FIFO, so check thrice on the run, without delay
				break;
		}
		else
		{
			numIdles = 0;
			
	//		printf("%08X\n", m_histRegs->clearBusy);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			polls++;
			if (polls > 2000)
				throw XDmaHbmException("waitClear: Timeout after polling for clear to finish 2000 times");
		}
	} while (1);
	m_clearPolls += polls;
}
/**
	For build with the clear FIFO, wait for Clear FIFO to not be full, so can accept another clear being requested.
	Without the clear FIFO just wait for all the clear engines to go Idle to accept another clear command.
*/
void XDmaHbmHist:: waitClearStart()
{
	int polls=0;
	int i;
	uint32_t clearBusy;

	if (!m_histConf.hasClearFIFO)
		waitClear();
	else
	{
		do
		{
			clearBusy = 0;
			for (i=0; i<m_numContRegs; i++)
				clearBusy |= m_histRegs[i]->status;
			clearBusy &=  AXI_HIST_STATUS_CLEAR_BUSY;
			if (clearBusy == 0)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			polls++;
			if (polls > 2000)
				throw XDmaHbmException("waitClearStart: Timeout after polling for clear to finish 2000 times");
		} while (1);
	}
	m_clearStartPolls += polls;
}

void XDmaHbmHist::readDma(char *buffer, uint64_t base, uint64_t numBytes,  int dmaChan )
{
	m_xdma->readDma(buffer, base, numBytes, dmaChan);
}

void XDmaHbmHist::writeDma(char *buffer, uint64_t base, uint64_t numBytes, int dmaChan )
{
	m_xdma->writeDma(buffer, base, numBytes, dmaChan);
}

/**
	Read region of histogram memory from a single stream.

@param port			Index of HBM ports to access 0..15 on left stack, 16..32 on right stack
@param stream		Stream Number 0.. NumStreams-1
@param offset		First word to clear, measured within streams address space, in hist words.
@param size			Number of Hist word to read.
@param data			Buffer to return the data, aligned on page boundary 4096 bytes using posix_memalign
For efficeint operation the source and destination must align and host buffer must be 4096 byte aligned.
If not another buffer is allocated, used and freed.
Note that if use_dma_reset is set, the reading memory will also reset the hist_test DMA, so stop any tets pattern being sent.
Do not use for concurrent read with test DMA 
*/

void XDmaHbmHist::readStreamDma(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data, int dmaChan, int flags)
{
	uint64_t AXIAddress;
	uint64_t numBytes;
	
	if (port<0 || port >= HBM_TOTAL_PORTS)
		throw XDmaHbmException("readStreamDma: Expected port in range %d..%d, not %d", 0, HBM_TOTAL_PORTS-1, port);
	if ( !( m_histConf.HBMPortMask & 1 << port))
		throw XDmaHbmException("readStreamDma: Port %d is not connected to a histogrammer. portMask=0x%08X", port, m_histConf.HBMPortMask);
	if (stream < 0 || stream >= m_histConf.NumStreams)
		throw XDmaHbmException("readStreamDma: Expected stream in range 0..%d, not %d", m_histConf.NumStreams, port);
		
	if (offset+size > m_histConf.HistWordsPerStream)
		throw XDmaHbmException("readStreamDma: Region %u for %u does not fit in buffer %lu words", offset, size, m_histConf.HistWordsPerStream);
//	cout << "BankPosn=" << m_histConf.BankPosn << ", redorder = " << m_axiReorder << ", flags=" << flags << endl;
	if (m_histConf.BankPosn == HBMHistConfig::BankRowCol || (!(flags&HBM_RW_NO_REORDER) && m_axiReorder > 0))
	{
		AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
		AXIAddress += (uint64_t)m_histConf.HistWordsPerStream * (m_histConf.NBitsDataHist/8)*stream;
		AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
		numBytes = (uint64_t)size * (m_histConf.NBitsDataHist/8);
		if (m_histConf.BankPosn != HBMHistConfig::BankRowCol)
			AXIAddress |= HBM_REORDER_OFFSET_STREAM;
		m_xdma->readDma((char *)data, AXIAddress, numBytes, dmaChan);
	} 
	else if (m_histConf.BankPosn == HBMHistConfig::SidRowBankColBG0)
	{
		uint64_t rowColMask = (1L << m_histConf.BankStartBit)-1;
		uint32_t numWordsCol = 1 << (m_histConf.BankStartBit-2);
		uint32_t numWords, startCol;
		uint64_t colAddr, rowAddr; 
		uint64_t streamAddr;
		streamAddr = (stream % (m_histConf.NumStreams/2)) << m_histConf.BankStartBit;
		if (stream >= m_histConf.NumStreams/2)
			streamAddr += (uint64_t)m_histConf.TotalMemWords/2* (m_histConf.NBitsDataHist/8);
		
		while (size > 0)
		{
			startCol = offset % numWordsCol;
			numWords = size;
			if (numWords > numWordsCol-startCol)
				numWords = numWordsCol-startCol;
			AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
			colAddr = AXIAddress & rowColMask;
			rowAddr = AXIAddress & ~rowColMask;
			AXIAddress = streamAddr + (rowAddr << (m_histConf.NBitsStream-1)) + colAddr;
			AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
			numBytes = (uint64_t)numWords * (m_histConf.NBitsDataHist/8);
			m_xdma->readDma((char *)data, AXIAddress, numBytes, dmaChan);
			data += numWords;
			offset += numWords;
			size -= numWords;
		}
	}
	else if (m_histConf.BankPosn == HBMHistConfig::RowBankCol)
	{
		uint64_t rowColMask = (1L << m_histConf.BankStartBit)-1;
		uint32_t numWordsCol = 1 << (m_histConf.BankStartBit-2);
		uint32_t numWords, startCol;
		uint64_t colAddr, rowAddr; 
		uint64_t streamAddr;

		streamAddr = (uint64_t)stream  << m_histConf.BankStartBit;
		
		while (size > 0)
		{
			startCol = offset % numWordsCol;
			numWords = size;
			if (numWords > numWordsCol-startCol)
				numWords = numWordsCol-startCol;
			AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
			colAddr = AXIAddress & rowColMask;
			rowAddr = AXIAddress & ~rowColMask;
			AXIAddress = streamAddr + (rowAddr << m_histConf.NBitsStream) + colAddr;
			AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
			numBytes = (uint64_t)numWords * (m_histConf.NBitsDataHist/8);
			m_xdma->readDma((char *)data, AXIAddress, numBytes, dmaChan);
			data += numWords;
			offset += numWords;
			size -= numWords;
		}
	}
	else
		throw XDmaHbmException("readStreamDma: Stream at bottom not supported yet");
		
}

void XDmaHbmHist::writeStreamDma(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data, int dmaChan, int useDmaReset)
{
	uint64_t AXIAddress;
	uint64_t numBytes;
	
	if (port<0 || port >= HBM_TOTAL_PORTS)
		throw XDmaHbmException("writeStreamDma: Expected port in range %d..%d, not %d", 0, HBM_TOTAL_PORTS-1, port);
	if ( !( m_histConf.HBMPortMask & 1 << port))
		throw XDmaHbmException("writeStreamDma: Port %d is not connected to a histogrammer. portMask=0x%08X", port, m_histConf.HBMPortMask);
	if (stream < 0 || stream >= m_histConf.NumStreams)
		throw XDmaHbmException("writeStreamDma: Expected stream in range 0..%d, not %d", m_histConf.NumStreams, port);
		
	if (offset+size > m_histConf.HistWordsPerStream)
		throw XDmaHbmException("writeStreamDma: Region %u for %u does not fit in buffer %lu words", offset, size, m_histConf.HistWordsPerStream);
		
	if (m_histConf.BankPosn == HBMHistConfig::BankRowCol)
	{
		AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
		AXIAddress += (uint64_t)m_histConf.HistWordsPerStream * (m_histConf.NBitsDataHist/8)*stream;
		AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
		numBytes = (uint64_t)size * (m_histConf.NBitsDataHist/8);

		m_xdma->writeDma((char *)data, AXIAddress, numBytes, dmaChan);
	} 
	else if (m_histConf.BankPosn == HBMHistConfig::SidRowBankColBG0)
	{
		uint64_t rowColMask = (1L << m_histConf.BankStartBit)-1;
		uint32_t numWordsCol = 1 << (m_histConf.BankStartBit-2);
		uint32_t numWords, startCol;
		uint64_t colAddr, rowAddr; 
		uint64_t streamAddr;
		streamAddr = (stream % (m_histConf.NumStreams/2)) << m_histConf.BankStartBit;
		if (stream >= m_histConf.NumStreams/2)
			streamAddr += (uint64_t)m_histConf.TotalMemWords/2* (m_histConf.NBitsDataHist/8);
		
		while (size > 0)
		{
			startCol = offset % numWordsCol;
			numWords = size;
			if (numWords > numWordsCol-startCol)
				numWords = numWordsCol-startCol;
			AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
			colAddr = AXIAddress & rowColMask;
			rowAddr = AXIAddress & ~rowColMask;
			AXIAddress = streamAddr + (rowAddr << (m_histConf.NBitsStream-1)) + colAddr;
			AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
			numBytes = (uint64_t)numWords * (m_histConf.NBitsDataHist/8);
			m_xdma->writeDma((char *)data, AXIAddress, numBytes, dmaChan);
			data += numWords;
			offset += numWords;
			size -= numWords;
		}
	}
	else if (m_histConf.BankPosn == HBMHistConfig::RowBankCol)
	{
		uint64_t rowColMask = (1L << m_histConf.BankStartBit)-1;
		uint32_t numWordsCol = 1 << (m_histConf.BankStartBit-2);
		uint32_t numWords, startCol;
		uint64_t colAddr, rowAddr; 
		uint64_t streamAddr;

		streamAddr = (uint64_t)stream  << m_histConf.BankStartBit;
		
		while (size > 0)
		{
			startCol = offset % numWordsCol;
			numWords = size;
			if (numWords > numWordsCol-startCol)
				numWords = numWordsCol-startCol;
			AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
			colAddr = AXIAddress & rowColMask;
			rowAddr = AXIAddress & ~rowColMask;
			AXIAddress = streamAddr + (rowAddr << m_histConf.NBitsStream) + colAddr;
			AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
			numBytes = (uint64_t)numWords * (m_histConf.NBitsDataHist/8);
			m_xdma->writeDma((char *)data, AXIAddress, numBytes, dmaChan);
			data += numWords;
			offset += numWords;
			size -= numWords;
		}
	}
	else
		throw XDmaHbmException("writeStreamDma: Stream at bottom not supported yet");
}

void XDmaHbmHist::readStreamCpu(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data)
{
	uint64_t AXIAddress;
	uint64_t numBytes;
	
	if (!m_hasMemBAR)
		throw XDmaHbmException("readStreamCpu:Does not have a MemBAR, use DMA read/write instead");
	if (m_histConf.BankPosn == HBMHistConfig::RowColBank)
		throw XDmaHbmException("readStreamCpu: Stream at bottom not supported yet");
	if (port<0 || port >= HBM_TOTAL_PORTS)
		throw XDmaHbmException("readStreamDma: Expected port in range %d..%d, not %d", 0, HBM_TOTAL_PORTS-1, port);
	if ( !( m_histConf.HBMPortMask & 1 << port))
		throw XDmaHbmException("readStreamDma: Port %d is not connected to a histogrammer. portMask=0x%08X", port, m_histConf.HBMPortMask);
	if (stream < 0 || stream >= m_histConf.NumStreams)
		throw XDmaHbmException("readStreamCpu: Expected stream in range 0..%d, not %d", m_histConf.NumStreams, port);
		
	if (offset+size > m_histConf.HistWordsPerStream)
		throw XDmaHbmException("readStreamCpu: Region %u for %u does not fit in buffer %lu words", offset, size, m_histConf.HistWordsPerStream);
		
	AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
	AXIAddress += (uint64_t)m_histConf.HistWordsPerStream * (m_histConf.NBitsDataHist/8)*stream;
	AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
	numBytes = (uint64_t)size * (m_histConf.NBitsDataHist/8);

	memcpy(data, ((char *)m_histMemBase)+AXIAddress, numBytes);
}

void XDmaHbmHist::writeStreamCpu(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data)
{
	uint64_t AXIAddress;
	uint64_t numBytes;
	
	if (!m_hasMemBAR)
		throw XDmaHbmException("writeStreamCpu:Does not have a MemBAR, use DMA read/write instead");
	if (m_histConf.BankPosn == HBMHistConfig::RowColBank)
		throw XDmaHbmException("writeStreamCpu: Stream at bottom not supported yet");
	if (port<0 || port >= HBM_TOTAL_PORTS)
		throw XDmaHbmException("readStreamDma: Expected port in range %d..%d, not %d", 0, HBM_TOTAL_PORTS-1, port);
	if ( !( m_histConf.HBMPortMask & 1 << port))
		throw XDmaHbmException("readStreamDma: Port %d is not connected to a histogrammer. portMask=0x%08X", port, m_histConf.HBMPortMask);
	if (stream < 0 || stream >= m_histConf.NumStreams)
		throw XDmaHbmException("writeStreamCpu: Expected stream in range 0..%d, not %d", m_histConf.NumStreams, port);
		
	if (offset+size > m_histConf.HistWordsPerStream)
		throw XDmaHbmException("writeStreamCpu: Region %u for %u does not fit in buffer %lu words", offset, size, m_histConf.HistWordsPerStream);
		
	AXIAddress = (uint64_t)offset * (m_histConf.NBitsDataHist/8);
	AXIAddress += (uint64_t)m_histConf.HistWordsPerStream * (m_histConf.NBitsDataHist/8)*stream;
	AXIAddress += (uint64_t)m_histConf.TotalMemWords* (m_histConf.NBitsDataHist/8) * port;
	numBytes = (uint64_t)size * (m_histConf.NBitsDataHist/8);

	memcpy(((char *)m_histMemBase)+AXIAddress, data, numBytes);
}

/**
	Start read of data via AXIS interface reading from a single stream.

@param portMask		Bitwise mask of which HBM ports to access 0..15 on left stack, 16..32 on right stack
@param stream		Stream Number 0.. NumStreams-1
@param startWord	First word to read, measured within streams address space, in AXI words.
@param numWords		Number of AXI words to read within stream.
*/
void XDmaHbmHist::startAXISReadStream(uint32_t portMask, uint32_t stream, uint32_t startWord, uint32_t numWords)
{
	int i;
	if (m_histConf.BankPosn == HBMHistConfig::RowColBank)
	{
		startWord *= m_histConf.NumStreams;
		startWord += stream;
	}
	else
	{
		startWord += stream * m_histConf.AXIWordsPerStream;
	}
	for (i=0; i<m_numContRegs; i++)
	{
		if (portMask & m_portMask[i])
		{
			m_histRegs[i]->readPortMask = portMask & m_portMask[i];
			m_histRegs[i]->readCont = 0;

			m_histRegs[i]->readStartWord = startWord;
			m_histRegs[i]->readNumWords = numWords;
		}
	}
}

/**
	Start read of data via AXIS interface reading sequentially, across stream in BankPosn == RowColBank configuration. Use for test patter data

@param portMask		Bitwise mask of which HBM ports to access 0..15 on left stack, 16..32 on right stack
@param startWord	First word to clear, measured within streams address space, in AXI words.
@param numWords		Number of AXI word to clerar within stream.
*/
void XDmaHbmHist::startAXISReadSeq(uint32_t portMask, uint32_t startWord, uint32_t numWords)
{
	int i;
	for (i=0; i<m_numContRegs; i++)
	{
		m_histRegs[i]->readPortMask = portMask & m_portMask[i];
		if (portMask & m_portMask[i])
		{
			m_histRegs[i]->readCont = AXI_HIST_RC_ACROSS_STREAMS;

			m_histRegs[i]->readStartWord = startWord;
			m_histRegs[i]->readNumWords = numWords;
		}
	}
}

void XDmaHbmHist::startTPG(uint32_t portMask, int tpgCont, uint32_t numEvents, uint32_t tpgSize)
{
	int i;
	fflush(stdout);
	std::this_thread::sleep_for(std::chrono::seconds(1));

	for (i=0; i<m_numContRegs; i++)
	{
		m_histRegs[i]->TPGPortMask = portMask  & m_portMask[i]; // Enable test pattern generator on required ports. Even if 0, write this to stop TPG
		if (portMask & m_portMask[i])
		{
			m_histRegs[i]->TPGCont = tpgCont;
			m_histRegs[i]->TPGNumEvents = numEvents;
			m_histRegs[i]->TPGSize = tpgSize;
		}
	}
}
void XDmaHbmHist::setTPGPortMask(uint32_t value)
{
	int i;

	for (i=0; i<m_numContRegs; i++)
		m_histRegs[i]->TPGPortMask = value;
}
void XDmaHbmHist::setTPGCont(uint32_t value)
{
	int i;
	for (i=0; i<m_numContRegs; i++)
		m_histRegs[i]->TPGCont = value;
}
