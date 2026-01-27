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

#include "xdma_hexitec.h"
#include "xaxidma_hw.h"

#include <linux/ioctl.h>
#define IOCTL_XDMA_ALIGN_GET    _IOR('q', 6, int)
#define RW_MAX_SIZE	0x7ffff000


#define AXIWrite32(p, v)	*(p) = (v)
#define AXIRead32(p)  (*(p))
#define AXIWrite64(p, v)	*((uint64_t *)(p)) = (v)
#define AXIRead64(p)	(*((uint64_t *)(p)))

using namespace std;

#define DBGLEVEL(level, ...)		if (m_debug >= level) printf( __VA_ARGS__)


void XDmaHexitec::initDMA()
{
	uint64_t  pbBytes, scBytes;
	uint64_t addr;
	uint32_t pbFrameSizeBytes = HEXITEC_MHZ_PB_BYTES_HALF_FRAME;		// 80 rows * 2 chunks per row * 256 bits for each DMA
	uint32_t scFrameSizeBytes = (HEXITEC_NUM_ROWS*HEXITEC_MHZ_NUM_CHUNKS_PER_ROW+HEXITEC_MHZ_PB_HEADER_BEATS)*16;		// 80 rows * 2 chunks per row * 128 bits for each DMA
	uint32_t pbFrameSizeBytesAligned;
	uint32_t numDesc;
	int i;
	uint32_t features[HEXITEC_NUM_FEATURE_REGS];
	int numPB, numScope;
	uint64_t hbmBytesPerPort;
	
	if (m_generation == HexitecGenHexitec)
	{
		pbFrameSizeBytes = m_numChips*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*sizeof(uint16_t)+sizeof(uint64_t);
	}
	pbFrameSizeBytesAligned = (pbFrameSizeBytes+63) & 0xFFFFFFC0;
	
	hbmBytesPerPort = m_hbmHist.m_histConf.TotalMemWords*(m_hbmHist.m_histConf.NBitsDataHist/8);
	printf("initDMA: determined hbmBytesPerPort=0x%08lX\n", hbmBytesPerPort);
	for (i=0; i<HEXITEC_NUM_AXI_DMA; i++)
		m_dmaStream[i].baseAddr = 0;
	readGlobRegs(HEXITEC_GLB_RD_FEATURES, HEXITEC_NUM_FEATURE_REGS, features);
	for (i=0;i<HEXITEC_NUM_FEATURE_REGS; i++)
		printf("Feature %d = 0x%08X\n", i, features[i]);
	numPB = HEXITEC_FEATURE_PBA_NUM_PB(features[HEXITEC_FEATURE_REG_PBA]);
	numScope = HEXITEC_FEATURE_SCA_NUM_SCOPE(features[HEXITEC_FEATURE_REG_SCA]);
	m_numPbDma = numPB;
	m_numScopeDma = numScope;
	m_dmaStream[HEXITEC_DMA_PB0].baseAddr = HEXITEC_DMA_PB0_BASE;
	m_dmaStream[HEXITEC_DMA_PB0].isTx	= 1;
	m_dmaStream[HEXITEC_DMA_PB1].baseAddr = HEXITEC_DMA_PB1_BASE;
	m_dmaStream[HEXITEC_DMA_PB1].isTx	= 1;
	m_dmaStream[HEXITEC_DMA_PB0].frameRule	= DMAStream::FramePerDesc;
	m_dmaStream[HEXITEC_DMA_PB1].frameRule	= DMAStream::FramePerDesc;

	for (i=0; i<numPB; i++)
	{
		addr = HEXITEC_FEATURE_PBB_PBI_START(i,features[HEXITEC_FEATURE_REG_PBB])*hbmBytesPerPort;
		pbBytes =  HEXITEC_FEATURE_PBB_PBI_NUM(i,features[HEXITEC_FEATURE_REG_PBB])*hbmBytesPerPort;
		numDesc = pbBytes/(pbFrameSizeBytesAligned+sizeof (AXIDMADesc));
		printf("Playback %d : Maximum number of descriptors/frames=%d, Size=0x%08X, Aligned size=0x%08X\n", i, numDesc, pbFrameSizeBytes, pbFrameSizeBytesAligned);
		m_dmaStream[HEXITEC_DMA_PB0+i].numDesc	= numDesc;
		m_dmaStream[HEXITEC_DMA_PB0+i].maxBlockBytes = pbFrameSizeBytesAligned;	
		m_dmaStream[HEXITEC_DMA_PB0+i].validFrameBytes = pbFrameSizeBytes;	
		m_dmaStream[HEXITEC_DMA_PB0+i].descPhys = addr;
		m_dmaStream[HEXITEC_DMA_PB0+i].dataStart = addr + sizeof(AXIDMADesc) * numDesc;
		m_dmaStream[HEXITEC_DMA_PB0+i].dataSize = pbBytes-sizeof(AXIDMADesc) * numDesc;
		m_dmaStream[HEXITEC_DMA_PB0+i].state = HEXITEC_DMA_STATE_DESC_CONF | HEXITEC_DMA_STATE_BUFFER_CONF;
	}	
	if (numScope == 0)
	{
		/* Ok No scope mode */
	}
	else
	{		
		if (numScope == 2)
		{
			m_dmaStream[HEXITEC_DMA_SC0].baseAddr = HEXITEC_DMA_PB0_BASE;
			m_dmaStream[HEXITEC_DMA_SC0].isTx	= 0;
			m_dmaStream[HEXITEC_DMA_SC1].baseAddr = HEXITEC_DMA_PB1_BASE;
			m_dmaStream[HEXITEC_DMA_SC1].isTx	= 0;
			m_dmaStream[HEXITEC_DMA_SC0].frameRule	= DMAStream::FramePerDesc;
			m_dmaStream[HEXITEC_DMA_SC1].frameRule	= DMAStream::FramePerDesc;
		}
		else if (numScope == 4)
		{
			m_dmaStream[HEXITEC_DMA_SC0].baseAddr = HEXITEC_DMA_PB0_BASE;
			m_dmaStream[HEXITEC_DMA_SC0].isTx	= 0;
			m_dmaStream[HEXITEC_DMA_SC1].baseAddr = HEXITEC_DMA_SC1_BASE;
			m_dmaStream[HEXITEC_DMA_SC1].isTx	= 0;
			m_dmaStream[HEXITEC_DMA_SC2].baseAddr = HEXITEC_DMA_PB1_BASE;
			m_dmaStream[HEXITEC_DMA_SC2].isTx	= 0;
			m_dmaStream[HEXITEC_DMA_SC3].baseAddr = HEXITEC_DMA_SC3_BASE;
			m_dmaStream[HEXITEC_DMA_SC3].isTx	= 0;
			m_dmaStream[HEXITEC_DMA_SC0].frameRule	= DMAStream::FramePerDesc;
			m_dmaStream[HEXITEC_DMA_SC1].frameRule	= DMAStream::FramePerDesc;
			m_dmaStream[HEXITEC_DMA_SC2].frameRule	= DMAStream::FramePerDesc;
			m_dmaStream[HEXITEC_DMA_SC3].frameRule	= DMAStream::FramePerDesc;
		}
		else
			throw  XDmaHexitecException("initDMA: Unknown number of scope  DMAs =%d ",numScope);
		for(i=0; i<numScope; i++)
		{
			addr = HEXITEC_FEATURE_SCB_SCI_START(i,features[HEXITEC_FEATURE_REG_SCB])*hbmBytesPerPort;
			scBytes =  HEXITEC_FEATURE_SCB_SCI_NUM(i,features[HEXITEC_FEATURE_REG_SCB])*hbmBytesPerPort;
			numDesc = scBytes/(scFrameSizeBytes+sizeof (AXIDMADesc));
			printf("Scope Mode %d : Maximum number of descriptors/frames=%d\n", i, numDesc);
			m_dmaStream[HEXITEC_DMA_SC0+i].numDesc	= numDesc;
			m_dmaStream[HEXITEC_DMA_SC0+i].maxBlockBytes = scFrameSizeBytes;	
			m_dmaStream[HEXITEC_DMA_SC0+i].descPhys = addr;
			m_dmaStream[HEXITEC_DMA_SC0+i].dataStart = addr + sizeof(AXIDMADesc) * numDesc;
			m_dmaStream[HEXITEC_DMA_SC0+i].dataSize = scBytes-sizeof(AXIDMADesc) * numDesc;
			m_dmaStream[HEXITEC_DMA_SC0+i].state = HEXITEC_DMA_STATE_DESC_CONF | HEXITEC_DMA_STATE_BUFFER_CONF;
		}
	}
	for (i=0; i<HEXITEC_NUM_AXI_DMA; i++)
	{
		if (m_dmaStream[i].baseAddr != 0)
			m_dmaStream[i].virtBase = m_xdma->m_regsBAR.m_base+m_dmaStream[i].baseAddr/sizeof(uint32_t);
	}
}
/**
	Write to hexitec per chip control registers

@param chip			Chip number  or -1 to duplicate to all chips.
@param region		Region number, {@link HEXITEC_REGION_REGS} from registers. Others for direct access to LUTs, but see LUT functions to ease access.
@param offset		Word offset of first register to write
@param num			Number of words to write
@param data			Pointer to data buffer to write.
*/
int XDmaHexitec::getMaxPbFrames()
{
	if (m_generation == HexitecGenMHz)
		printf("getMaxPbFrames: HexitecGenMHz  =>%d\n", m_dmaStream[HEXITEC_DMA_PB0].numDesc);
	else
		printf("getMaxPbFrames: HexitecGenHexitec =>%d\n", m_dmaStream[HEXITEC_DMA_PB0].numDesc*m_numPbDma);

	if (m_generation == HexitecGenMHz)
		return m_dmaStream[HEXITEC_DMA_PB0].numDesc;
	else
		return m_dmaStream[HEXITEC_DMA_PB0].numDesc*m_numPbDma;
}
uint32_t XDmaHexitec::getPbFrameBytesAligned()
{
	return m_dmaStream[HEXITEC_DMA_PB0].maxBlockBytes;
}
int XDmaHexitec::getMaxScopeFrames()
{
	return m_dmaStream[HEXITEC_DMA_SC0].numDesc;
}

void XDmaHexitec::dmaBuildPBDesc(int numFramesTotal)
{
	int i;
	int numFrames;

	if (numFramesTotal == 0)
		numFramesTotal = getMaxPbFrames();

	dmaStop(1<<HEXITEC_DMA_PB0 | 1<<HEXITEC_DMA_PB1);

	if (m_generation == HexitecGenMHz)
	{
		// For Hexitec MHz 0.5 of a frame is supplied from each DMA, both start the full number of frames
		numFrames = numFramesTotal;
		for (i=0; i<m_numPbDma; i++)
		{
			dmaBuildDesc(HEXITEC_DMA_PB0+i, HEXITEC_DMA_PB0+i, 0, 0L, numFrames*(uint64_t)m_dmaStream[HEXITEC_DMA_PB0+i].maxBlockBytes, 0);
		}
	}
	else
	{
		// For Hexitec original, frames toggle between the 2 DMAs, each one provides a complete frame 
		// e.g 3 frames, send 2  (3+2-1)/2 = 2 frames from PB0 and then 3-2 = 1 frames from PB1.
		numFrames = (numFramesTotal+m_numPbDma-1)/m_numPbDma;
		for (i=0; i<m_numPbDma; i++)
		{
			if (i == m_numPbDma-1)
				numFrames = numFramesTotal;
			dmaBuildDesc(HEXITEC_DMA_PB0+i, HEXITEC_DMA_PB0+i, 0, 0L, numFrames*(uint64_t)m_dmaStream[HEXITEC_DMA_PB0+i].maxBlockBytes, 0);
			numFramesTotal -= numFrames; 
		}
	}
}

void XDmaHexitec::writeDmaBuff(int stream, uint64_t offset, uint64_t numBytes, char * ptr)
{
	if (stream <0 || stream >= HEXITEC_NUM_AXI_DMA)
		throw  XDmaHexitecException("writeDmaBuff: stream %d is not valid", stream);
	
	m_xdma->writeDma(ptr, m_dmaStream[stream].dataStart+offset, numBytes, m_dmaDescRWChan);
}
void XDmaHexitec::readDmaBuff(int stream, uint64_t offset, uint64_t numBytes, char * ptr)
{
	if (stream <0 || stream >= HEXITEC_NUM_AXI_DMA)
		throw  XDmaHexitecException("readDmaBuff: stream %d is not valid", stream);
	
	m_xdma->readDma(ptr, m_dmaStream[stream].dataStart+offset, numBytes, m_dmaDescRWChan);
}

void XDmaHexitec::dmaReset(uint32_t streamMask)
{
	u_int32_t stat, busy;
	int stream, i, n;
	int foundIt;
	uint32_t *baseAddr[HEXITEC_NUM_AXI_DMA];
	int resetTx[HEXITEC_NUM_AXI_DMA];

	for (i=0; i<HEXITEC_NUM_AXI_DMA; i++)
		baseAddr[i] = nullptr;

	n=0;
	for (stream=0; stream<HEXITEC_NUM_AXI_DMA; stream++)
	{
		DBGLEVEL(MSG_VERBOSE, "dmaReset reset trying stream %d, base_address=%08lX, mask=%02X\n", stream, m_dmaStream[stream].baseAddr, streamMask);
		if (m_dmaStream[stream].baseAddr != 0 && (streamMask & 1 << stream))
		{
			DBGLEVEL(MSG_VERBOSE,"dmaReset: reset for stream %d, mask=%02X\n", stream, streamMask);
			foundIt = 0;
			for (i=0; i<n; i++)
				if (m_dmaStream[stream].virtBase == baseAddr[i])
					foundIt = 1;
			if (!foundIt)
			{
				baseAddr[n] = m_dmaStream[stream].virtBase;
				resetTx[n++] = m_dmaStream[stream].isTx;
			}
		}
	}
	for (i=0; i<n && baseAddr[i] != nullptr; i++)
	{
		DBGLEVEL(MSG_VERBOSE, "dmaReset at base address %px, address=%px\n", baseAddr[i], baseAddr[i]+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4);
		if (resetTx[i])
			AXIWrite32(baseAddr[i]+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, XAXIDMA_CR_RESET_MASK);
		else		
			AXIWrite32(baseAddr[i]+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4, XAXIDMA_CR_RESET_MASK);		
	}
	do
	{
		busy = 0;
		for (i=0; i<n && baseAddr[i] != 0; i++)
		{
			DBGLEVEL(MSG_VERBOSE, "Poll reset num %d, pointer %px\n", i, baseAddr[i]);
			if (resetTx[i])
				stat = AXIRead32(baseAddr[i]+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4);
			else
				stat = AXIRead32(baseAddr[i]+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4);
			busy |= stat & XAXIDMA_CR_RESET_MASK;
		}
	} while (busy);
}
/** Build DMA descriptors to send or recieve data 
@param  stream		DMA stream number to build descriptors for
@param srcStream    DMA stream number ofuse the data buffer from, which can be different than the stream used to move the data
@param flags		Bitwise mask of control flags, for future expansion.
@param byteOffset	Byte Offset with (padded)dat buffer to start transfers, =0 to start at beginning of the buffer.
@param numBytes		Number of Bytes including any alignment padding to use from the buffer. Data transfer may be less is validFrameBytes>0. numBytes==0 uses whole buffer.
@param maxBlockBytes maxiumu number of bytes is each DMA block, which is a frame if frameRule=FramePerDesc is set.
*/
int XDmaHexitec::dmaBuildDesc(int stream, int srcStream, int flags, uint64_t byteOffset, uint64_t numBytes, uint32_t maxBlockBytes)
{
	int first=1, last=0;
	XDmaHexitec::AXIDMADesc *descBuff, *desc;
	uint64_t descPhys;
	uint64_t dmaAddr;
	uint32_t ctrl;
	int n = 0;
	uint64_t chunkNumBytes;
	int nFrames=0;
	int shortenedFrame=0;
	uint64_t numBytesReq = numBytes;

	if (stream < 0 || stream >= HEXITEC_NUM_AXI_DMA)
		throw  XDmaHexitecException("dmaBuildDesc: requires stream in range 0 to %d, not %d", HEXITEC_NUM_AXI_DMA-1, stream);

	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_CONF) == 0)
		throw  XDmaHexitecException("dmaBuildDesc: Stream %d is not Configured", stream);

	DBGLEVEL(MSG_VERBOSE, "dmaBuildDesc: srcStream=%d, byteOffset=%ld, numBytes=%ld\n", srcStream, byteOffset, numBytes);

	if (srcStream < 0 || srcStream >= HEXITEC_NUM_AXI_DMA)
		throw  XDmaHexitecException("dmaBuildDesc: srcStream %d is not valid", srcStream);
	if (!(m_dmaStream[srcStream].state & HEXITEC_DMA_STATE_BUFFER_CONF))
		throw  XDmaHexitecException("dmaBuildDesc: srcStream %d is not configured", srcStream);
	dmaAddr = m_dmaStream[srcStream].dataStart+byteOffset;
	if (numBytes == 0)
		numBytes =  m_dmaStream[srcStream].dataSize-byteOffset;
	if (maxBlockBytes == 0)
		maxBlockBytes = m_dmaStream[stream].maxBlockBytes;

	m_dmaStream[stream].transferStart = byteOffset;
	m_dmaStream[stream].transferSize  = numBytes;
	m_dmaStream[stream].state |= HEXITEC_DMA_STATE_BUFFER_CONF;

	descPhys = m_dmaStream[stream].descPhys;
	descBuff = new AXIDMADesc [m_dmaStream[stream].numDesc];

	DBGLEVEL(MSG_NORMAL, "dmaBuildDesc: Building TX Descriptors stream=%d, srcStream=%d\nDescPhys Base=%08lX\nByte offset=%08lX\nStart address=%08lX, numBytes=%08lX, max_block=%08X\n", 
							stream, srcStream, descPhys, byteOffset, dmaAddr, numBytes, maxBlockBytes);

	m_dmaStream[stream].state &= ~HEXITEC_DMA_STATE_DESC_BUILT & ~HEXITEC_DMA_STATE_DESC_DEBUG;

	desc = descBuff;
	do
	{
		memset(desc, 0, sizeof(AXIDMADesc));
		chunkNumBytes = numBytes;
		if (chunkNumBytes > HEXITEC_DMA_MAX_BYTES_DESC)
			chunkNumBytes = HEXITEC_DMA_MAX_BYTES_DESC;
		if (chunkNumBytes > maxBlockBytes)
			chunkNumBytes = maxBlockBytes;
		if (chunkNumBytes == numBytes)
			last =1;
		if (last)
			desc->physNext = m_dmaStream[stream].descPhys;	// Idea is that playback loops forever, others should not get here due to Stop on END. Also used by hist-list function.
		else
			desc->physNext = descPhys+sizeof(AXIDMADesc);
#if 0
		if (stream == HEXITEC_DMA_STREAM_BNUM_10G_TO_DRAM && numBytes != chunkNumBytes && (numBytes - chunkNumBytes <= 16 || maxNextChunk <= 16))
		{
			/* Special case is trapped at sending end so that no packet is too short (16 bytes = 2 beats (SOF/EOF)) */
			chunkNumBytes -= 16;
			shortenedFrame = 1;
		}
		else
			shortenedFrame = 0;
#endif	

		desc->physAddr = dmaAddr;
		desc->reserved1[0] = 0;
		desc->reserved1[1] = 0;
		desc->frameNum = nFrames;
		if (m_dmaStream[stream].validFrameBytes > 0 && chunkNumBytes > m_dmaStream[stream].validFrameBytes)
			ctrl = m_dmaStream[stream].validFrameBytes;
		else
			ctrl = chunkNumBytes;
		if (m_dmaStream[stream].isTx)
		{
			switch (m_dmaStream[stream].frameRule)
			{
			case XDmaHexitec::DMAStream::AllOneFrame:
				if (first)
					ctrl |= XAXIDMA_BD_CTRL_TXSOF_MASK;
				if (last)
				{
					ctrl |= XAXIDMA_BD_CTRL_TXEOF_MASK;
					nFrames ++;
				}
				break;
			case DMAStream::FramePerDesc:
				ctrl |= XAXIDMA_BD_CTRL_TXSOF_MASK | XAXIDMA_BD_CTRL_TXEOF_MASK;
				nFrames++;
				break;

			case XDmaHexitec::DMAStream::FrameByBytes:
				delete [] descBuff;
				throw XDmaHexitecException("dmaBuildDesc: Not implemented SOP/SOP by byte count yet");
			}
		}
		else
		{

		}
//		if (last && stream != HEXITEC_DMA_STREAM_BNUM_PLAYBACK)
//			ctrl |= XLLDMA_BD_STSCTRL_SOE_MASK;
		desc->control  = ctrl;
		desc->status = 0;
//		DBGLEVEL(MSG_VERBOSE, "dmaBuildDesc: desc %d, control=0x%08X\n", n, desc->control);

//		desc->app[1] = 0;

		desc++;
		descPhys += sizeof(AXIDMADesc);
		dmaAddr += chunkNumBytes;
		numBytes -= chunkNumBytes;
		n++;
		first = 0;
	} while (numBytes > 0 && n < m_dmaStream[stream].numDesc);
	if (numBytes > 0)
	{
		delete [] descBuff;
		throw XDmaHexitecException("dmaBuildDesc: Stream %d, Ran out of descriptors. Used %d of %d with %lld bytes remaining. Requested numBytes=%lld, buffser size=%lld\n", 
			stream, n, m_dmaStream[stream].numDesc, numBytes, numBytesReq, m_dmaStream[srcStream].dataSize);
	}
	m_xdma->writeDma((char *)descBuff, m_dmaStream[stream].descPhys, n*sizeof(AXIDMADesc), m_dmaDescRWChan);
	m_dmaStream[stream].state |= HEXITEC_DMA_STATE_DESC_BUILT;
	m_dmaStream[stream].definedDesc = n;
	m_dmaStream[stream].numFrames = nFrames;


	DBGLEVEL(MSG_NORMAL, "dmaBuildDesc: Built %d Descriptors for stream=%d\n", m_dmaStream[stream].definedDesc, stream); 
	delete [] descBuff;

	return m_dmaStream[stream].definedDesc;
}

uint32_t XDmaHexitec::dmaStop(uint32_t streamMask)
{
	int stream;
	uint32_t sr, haltedMask = 0;
	for (stream=0; stream < HEXITEC_NUM_AXI_DMA; stream++)
	{
		if (m_dmaStream[stream].baseAddr != 0 && (streamMask & 1 << stream))
		{
			if (m_dmaStream[stream].isTx)
				AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, 1 << 16); // Force Run stop to 0
			else
				AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4, 1 << 16); // Force Run stop to 0
		}
	}
	/* Now check that they have all stopped */
	for (stream=0; stream < HEXITEC_NUM_AXI_DMA; stream++)
	{
		if (m_dmaStream[stream].baseAddr != 0 && (streamMask & 1 << stream))
		{
			int timeout=1000000;
			do
			{
				if (m_dmaStream[stream].isTx)
					sr = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4);
				else
					sr = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4);
				if (sr & XAXIDMA_HALTED_MASK)
				{
					haltedMask |= 1 << stream;
					break;
				}
			} while (timeout-- > 0);
		}
	}
	return haltedMask;
}

void XDmaHexitec::dmaStart(uint32_t streamMask, int firstDescIn, int numDescIn, int options)
{
	int firstDesc, numDesc;
	uint64_t descPhys;
	AXIDMADesc *descBuff, *desc;
	uint32_t cr;
	int stream;
	int i;
	uint64_t byteOffset, remaining, chunk_bytes;


	for (stream=0; stream < HEXITEC_NUM_AXI_DMA; stream++)
	{
		if (m_dmaStream[stream].baseAddr != 0 && (streamMask & 1 << stream))
		{

			if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
					throw  XDmaHexitecException("dmaStart: No descriptors built for stream %d", stream);
#if 0
			/* Cannot directly access the descriptor so need to read into a buffer to process */
			if (options & HEXITEC_DMA_START_FRAME_NUM)
			{
				desc = m_dmaStream[stream].desc_virtBase;
				for (i=0; i<m_dmaStream[stream].definedDesc; i++)
				{
					if ((desc->control & XAXIDMA_BD_CTRL_TXSOF_MASK) && desc->frame_num == msg.firstDesc)
						break;
					desc++;
				}
				firstDesc = i;
				if (msg.numDesc > 0)
				{
					numDesc = 0;
					while (i<m_dmaStream[stream].definedDesc)
					{
						if ((desc->control & XAXIDMA_BD_CTRL_TXEOF_MASK) && desc->frame_num == msg.firstDesc+msg.numDesc-1)
						{
							numDesc = 1+i-firstDesc;
							break;
						}
						i++;
						desc++;
					}
				}
				else
				{
					numDesc = m_dmaStream[stream].definedDesc-firstDesc;
				}
				if (firstDesc >= m_dmaStream[stream].definedDesc || numDesc == 0)
				{
					printk(KERN_WARNING "[ERROR] Start: Cannot find descriptors to resend First Frame=%d and num frames=%d out of descriptorrange 0..%d, num_frames=%d\n", msg.firstDesc, msg.numDesc, m_dmaStream[stream].definedDesc-1, m_dmaStream[stream].num_frames);
					return -HEXITEC_DMA_ERROR_DESC_RANGE;
				}
			}
			else
			{
#endif
			firstDesc = firstDescIn;
			if (numDescIn == 0)
				numDesc = m_dmaStream[stream].definedDesc-firstDesc;
			else 
				numDesc = numDescIn;

			if (firstDesc >= m_dmaStream[stream].definedDesc || firstDesc+numDesc > m_dmaStream[stream].definedDesc)
				throw  XDmaHexitecException("dmaStart: First Descriptor=%d and num=%d out of range 0..%d", firstDesc, numDesc, m_dmaStream[stream].definedDesc-1);

			descPhys = m_dmaStream[stream].descPhys+firstDesc*sizeof(AXIDMADesc);
			DBGLEVEL(MSG_NORMAL, "Starting DMA Stream %d, first=%d num=%d , all=%d descriptors\n", stream, firstDesc, numDesc, m_dmaStream[stream].definedDesc);
			DBGLEVEL(MSG_NORMAL, ".... First Desc phys address=0x%010lX\n", descPhys);
			DBGLEVEL(MSG_NORMAL, ".... Transfer start=%010lX, size=%010lX, end=%010lX\n", m_dmaStream[stream].transferStart, m_dmaStream[stream].transferSize, m_dmaStream[stream].transferSize+m_dmaStream[stream].transferStart-1);


			if (m_dmaStream[stream].isTx)
			{
				AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, 1 << 16); // Force Run stop to 0, though this should be done before changing the descriptors.
				for (i=0;i<100; i++)
				{
					if (AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4) & XAXIDMA_HALTED_MASK)
						break;
				}
				AXIWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CDESC_OFFSET)/4, descPhys);
				if (options & HEXITEC_DMA_START_CIRCULAR)
					AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, XAXIDMA_CR_RUNSTOP_MASK | XAXIDMA_CR_CYCLIC_MASK | 1 << 16);
				else
					AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, XAXIDMA_CR_RUNSTOP_MASK | 1 << 16);

				if (options & HEXITEC_DMA_START_CIRCULAR)	/* Deliberately off end of desc list so should not stop */
					AXIWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_TDESC_OFFSET)/4, descPhys+(numDesc)*sizeof(AXIDMADesc));
				else
					AXIWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_TDESC_OFFSET)/4, descPhys+(numDesc-1)*sizeof(AXIDMADesc));
			}
			else
			{
				AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4, 1 << 16); // Force Run stop to 0, though this should be done before chaning the descriptors.
				for (i=0;i<100; i++)
				{
					if (AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4) & XAXIDMA_HALTED_MASK)
						break;
				}
				cr = XAXIDMA_CR_RUNSTOP_MASK | 1 << 16;
#if 0
		/* By using TAILDESC, we should not get a wrap round error, instead to firmware will drop events. */
					/* Could add Start msg option to enable IRS so driver does not need to know */
						cr |= XAXIDMA_IRQ_IOC_MASK;

					if (stream == XSP3M_DMA_STREAM_BNUM_HIST_LIST)
						cr |= XAXIDMA_IRQ_ERROR_MASK;	// If wrap round, clear error and contiue.
#endif
				AXIWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CDESC_OFFSET)/4, descPhys);
				AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4, cr);
				if (options & HEXITEC_DMA_START_CIRCULAR)
					AXIWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_TDESC_OFFSET)/4, descPhys+(numDesc)*sizeof(AXIDMADesc));	// Deliberate off end of list so cycles round
				else
					AXIWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_TDESC_OFFSET)/4, descPhys+(numDesc-1)*sizeof(AXIDMADesc));
			}
			m_dmaStream[stream].readoutCurDesc = 0;
			m_dmaStream[stream].readoutErrorFlags = 0;
			m_dmaStream[stream].readoutErrorData = 0;
		}
	}
}

uint32_t XDmaHexitec::dmaReadStatus(int stream)
{
	if (stream <0 || stream >= HEXITEC_NUM_AXI_DMA)
		throw  XDmaHexitecException("dmaReadStatus: stream %d is not valid", stream);
	if (m_dmaStream[stream].isTx)
		return AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4);
	else
		return AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4);
}

void XDmaHexitec::dmaWaitIdle(int streamMask, double timeOut)
{
	int stream;
	uint32_t sr;
	const std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
	std::chrono::time_point<std::chrono::steady_clock> now;
	std::chrono::duration<double> elapsedDur;
	
	for (stream=0; stream<HEXITEC_NUM_AXI_DMA; stream++)
	{
		if (streamMask & 1 << stream)
		{
			do
			{
				if (m_dmaStream[stream].isTx)
					sr =  AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4);
				else
					sr =  AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4);
				if (sr & XAXIDMA_IDLE_MASK)
					break;
				now = std::chrono::steady_clock::now();
				elapsedDur = now-start;
				if (elapsedDur.count() > timeOut)
					throw XDmaHexitecException("dmaWaitIdle: timeout after %g seconds waiting for DMA stream %d to go idle, status=0x%08X", timeOut, stream, sr);
			} while (1);
		}
	}
}


bool XDmaHexitec::dmaWaitIdleNoExcept(int streamMask, double timeOut)
{
	int stream;
	uint32_t sr;
	const std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
	std::chrono::time_point<std::chrono::steady_clock> now;
	std::chrono::duration<double> elapsedDur;
	
	for (stream=0; stream<HEXITEC_NUM_AXI_DMA; stream++)
	{
		if (streamMask & 1 << stream)
		{
			do
			{
				if (m_dmaStream[stream].isTx)
					sr =  AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4);
				else
					sr =  AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4);
				if (sr & XAXIDMA_IDLE_MASK)
					break;
				now = std::chrono::steady_clock::now();
				elapsedDur = now-start;
				if (elapsedDur.count() > timeOut)
				{
					printf("dmaWaitIdle: timeout after %g seconds waiting for DMA stream %d to go idle, status=0x%08X\n", timeOut, stream, sr);
					return true;
				}
			} while (1);
		}
	}
	return false;
}

uint64_t XDmaHexitec::dmaReadCurrDesc(int stream)
{
	if (stream <0 || stream >= HEXITEC_NUM_AXI_DMA)
		throw  XDmaHexitecException("dmaReadCurrDesc: stream %d is not valid", stream);
	if (m_dmaStream[stream].isTx)
		return AXIRead64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
	else
		return AXIRead64(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
}

uint32_t XDmaHexitec::dmaReadCurrDescNum(int stream)
{
	uint64_t currDesc;
	if (stream <0 || stream >= HEXITEC_NUM_AXI_DMA)
		throw  XDmaHexitecException("dmaReadCurrDesc: stream %d is not valid", stream);
	if (m_dmaStream[stream].isTx)
		currDesc =  AXIRead64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
	else
		currDesc = AXIRead64(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
	return (uint32_t)(((currDesc-m_dmaStream[stream].descPhys)/sizeof(AXIDMADesc))&0xFFFFFFFF);
}

int XDmaHexitec::dmaPrintDesc(int stream, int firstDesc, int numDesc)
{
	int i;
	AXIDMADesc *desc;
	uint64_t descPhys, curDesc;

	if (stream < 0 || stream >= HEXITEC_NUM_AXI_DMA )
		throw  XDmaHexitecException("dmaPrintDesc: stream %d is not valid", stream);

	printf("Print Desc Stream=%d, first=%d, num=%d\n", stream, firstDesc, numDesc);
	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
		throw  XDmaHexitecException("dmaPrintDesc: stream %d has no DMA descriptors", stream);

	if (firstDesc < 0 || numDesc < 1 ||firstDesc >= m_dmaStream[stream].definedDesc || firstDesc+numDesc > m_dmaStream[stream].definedDesc)
		throw  XDmaHexitecException("dmaPrintDesc: First Descriptor=%d and num=%d out of range 0..%d\n", firstDesc, numDesc, m_dmaStream[stream].definedDesc-1);

	desc = new AXIDMADesc [numDesc];
	descPhys = m_dmaStream[stream].descPhys+firstDesc*sizeof(AXIDMADesc);
	m_hbmHist.readDma((char *)desc, descPhys, numDesc*sizeof(AXIDMADesc));

	for (i=0; i<numDesc; i++)
	{
		printf("Desc=%d, Addr=%010lX, Nxt=%010lX, DMA addr=%010lX, numBytes=0x%08X=%d, ctrl=%08X, status=%08X\n",
				i+firstDesc, descPhys, desc->physNext, desc->physAddr, desc->control & 0x7FFFFF, desc->control & 0x7FFFFF, desc->control, desc->status );
		desc++;
		descPhys += sizeof(AXIDMADesc);
	}
	if (m_dmaStream[stream].isTx)
		curDesc = AXIRead64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
	else
		curDesc = AXIRead64(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
	printf("Current Desc phys addr=%010lX\n", curDesc);

	return 0;
}


#if 0
int zynqmp_dma_resend(DevStatics *dev_stat, ZynqMPPb *param)
{
	int i;
	AXIDMADesc *desc, *firstDesc;
	uint64_t first_phys, last_phys;
	m_dmaStream *dma = m_dmaStream;
	int stream = param->num;
	HEXITEC_DMA_MsgResend msg;
	int numDesc=0;

	if (copy_from_user((void *) &msg, (void __user *)param->ptr, sizeof(HEXITEC_DMA_MsgResend)))
		return -EFAULT;
	if (stream < 0)
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA resend  command requires 1 and only 1 DMA stream at a time, not 0x%X\n", param->num);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	DBGLEVEL(MSG_NORMAL, "Re-send Stream=%d, first=%d, num=%d\n", stream, msg.firstDesc, msg.numDesc);
	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
	{
		printk(KERN_WARNING "[ERROR] Stream %d has no DMA descriptors\n", stream);
		return -HEXITEC_DMA_ERROR_UNCONF_DESC;
	}
	if (msg.firstDesc >= m_dmaStream[stream].definedDesc || msg.firstDesc+msg.numDesc > m_dmaStream[stream].definedDesc)
	{
		printk(KERN_WARNING "[ERROR] Resend: First Descriptor=%d and num=%d out of range 0..%d\n", msg.firstDesc, msg.numDesc, m_dmaStream[stream].definedDesc-1);
		return -HEXITEC_DMA_ERROR_DESC_RANGE;
	}
	firstDesc = NULL;
	first_phys = 0;
	if (msg.options & HEXITEC_DMA_RESEND_FRAME_NUM)
	{
		desc=m_dmaStream[stream].desc_virtBase;
		for (i=0; i<m_dmaStream[stream].definedDesc; i++)
		{
			if ((desc->control & XAXIDMA_BD_CTRL_TXSOF_MASK) && desc->frame_num == msg.firstDesc)
			{
				firstDesc = desc;
				first_phys = m_dmaStream[stream].desc_dma+i*sizeof(AXIDMADesc);
			}
			if ((desc->control & XAXIDMA_BD_CTRL_TXEOF_MASK) && desc->frame_num == msg.firstDesc+msg.numDesc-1)
			{
				numDesc = desc-firstDesc;
				numDesc++;
				last_phys = m_dmaStream[stream].desc_dma+i*sizeof(AXIDMADesc);
				break;
			}
			desc++;
		}
		if (firstDesc == NULL || numDesc == 0)
		{
			printk(KERN_WARNING "[ERROR] Resend: Cannot find descriptors to resend First Frame=%d and num frames=%d out of descriptorrange 0..%d, num_frames=%d\n", msg.firstDesc, msg.numDesc, m_dmaStream[stream].definedDesc-1, m_dmaStream[stream].num_frames);
			return -HEXITEC_DMA_ERROR_DESC_RANGE;

		}
	}
	else
	{
		firstDesc = m_dmaStream[stream].desc_virtBase+msg.firstDesc;
		if (!(firstDesc->control & XAXIDMA_BD_CTRL_TXSOF_MASK) || !(firstDesc[msg.numDesc-1].control & XAXIDMA_BD_CTRL_TXEOF_MASK))
		{
			printk(KERN_WARNING "[ERROR] Resend: First Descriptor=%d and num=%d Descriptor should start and end Frame\n", msg.firstDesc, msg.numDesc);
			return -HEXITEC_DMA_ERROR_DESC_RANGE;
		}
		numDesc = msg.numDesc;
		first_phys = m_dmaStream[stream].desc_dma+msg.firstDesc*sizeof(AXIDMADesc);
		last_phys = first_phys+(msg.numDesc-1)*sizeof(AXIDMADesc);
	}

//	Xil_DCacheInvalidateRange((uint32_t)m_dmaStream[stream].desc, sizeof(AXIDMADesc)*m_dmaStream[stream].definedDesc);

	desc = firstDesc; 
	for (i=0; i<numDesc; i++)
	{
		desc->status = 0;
		desc->app[3] = 0;
		desc++;
	}
	dma_wmb();
	if (dev_stat->access_flags[m_dmaStream[stream].desc_addr_space] & ACCESS_CACHEFLUSH)
		__flush_dcache_area(firstDesc, numDesc*sizeof(AXIDMADesc));
	if (debug >= MSG_VERBOSE)
	{
		uint64_t desc_dma;
		desc = firstDesc;
		desc_dma = first_phys;
		for (i=0; i<numDesc && i < 10; i++)
		{
			printk(KERN_WARNING "Resend desc/frame. %ld, PAddr=%010llX, Next=%010llX, DMA addr=%010llX, numBytes=0x%08X=%d, ctrl=%08X, status=%08X\n", i+(firstDesc-desc), desc_dma, desc->phys_next, desc->phys_addr, desc->control & 0x7fffff, desc->control & 0x7fffff, desc->control, desc->status);
			desc++;
			desc_dma+=sizeof(AXIDMADesc);
		}
	}
	desc = firstDesc;

	dev_stat->last_checked[stream] = -1;
	dev_stat->num_good[stream] = 0;
	dev_stat->num_completed[stream] = 0;
	dev_stat->next_time_frame[stream] = 0;
	if (m_dmaStream[stream].isTx)
	{
//		Xil_DCacheFlushRange((uint32_t) (m_dmaStream[stream].transfer_start),  m_dmaStream[stream].transfer_size); // Assume if wanted this we have flushed it.
				// If it is from the other CPU we want to invalidate this. Hope the other has flushed it

		AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, 0 | 1 << 16); // Stop DMA
		while (!(AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4) & XAXIDMA_HALTED_MASK ));
		DBGLEVEL(MSG_VERBOSE, "Finished halting DMA, status=%08X\n", AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4) );
		HEXITEC_IOWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CDESC_OFFSET)/4, first_phys );
		if (msg.options & HEXITEC_DMA_START_CIRCULAR) // Not clear how/whether resend would be used with circular TX as it should just keep on running.
			AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, XAXIDMA_CR_RUNSTOP_MASK | XAXIDMA_CR_CYCLIC_MASK | 1 << 16);
		else
			AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CR_OFFSET)/4, XAXIDMA_CR_RUNSTOP_MASK | 1 << 16);

		HEXITEC_IOWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_TDESC_OFFSET)/4, last_phys);
	}
	else
	{
//		Xil_DCacheInvalidateRange((uint32_t) (m_dmaStream[stream].data_start),  m_dmaStream[stream].data_size);
		AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4, 0 | 1 << 16); // Stop DMA
		while (!(AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4) & XAXIDMA_HALTED_MASK ));
		HEXITEC_IOWrite64(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CDESC_OFFSET)/4, first_phys);
		AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CR_OFFSET)/4, XAXIDMA_CR_RUNSTOP_MASK | 1 << 16);
		AXIWrite32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_TDESC_OFFSET)/4, last_phys);
	}
	return -HEXITEC_DMA_ERROR_OK;
}

int zynqmp_dma_read_status(DevStatics *dev_stat, ZynqMPPb *param, uint32_t *statusP)
{
	uint32_t stat=0xFFFFFFFF;
	int stream;
	m_dmaStream *dma = m_dmaStream;
	int streamMask = param->num;

	for (stream=1; stream < HEXITEC_NUM_AXI_DMA; stream++)
	{
		if (m_dmaStream[stream].base_addr != 0 && (streamMask & 1 << stream))
		{
			if (stream == 0)
				return -HEXITEC_DMA_ERROR_BAD_STREAM;
			if (m_dmaStream[stream].isTx)
			{
				stat = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4);
			}
			else
			{
				stat = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4);
			}
			dev_stat->status_block.status[stream] = stat;
			DBGLEVEL(MSG_NORMAL,"Stream %d, Status=0x%08X\n", stream, stat);
		}
	}
	if (statusP != NULL)
		*statusP=stat;
	return 0;
}


int zynqmp_dma_check_rx_desc(DevStatics *dev_stat, ZynqMPPb *param, uint32_t *num_complete)
{
	int i;
	AXIDMADesc *desc;
	int numDesc;
	int checks=0;
	m_dmaStream *dma = m_dmaStream;
	int stream = param->num;
	HEXITEC_DMA_MsgCheckDesc msg;
	uint32_t status;
	uint64_t descPhys;
	int error_desc=-1;
	uint64_t total_bytes=0;

	if (copy_from_user((void *) &msg, (void __user *)param->ptr, sizeof(HEXITEC_DMA_MsgCheckDesc)))
		return -EFAULT;

	if (stream < 0 || stream >= HEXITEC_NUM_AXI_DMA )
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA check desc command requires stream in range 0 to %d, not %dX\n", HEXITEC_NUM_AXI_DMA-1, param->num);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	DBGLEVEL(MSG_NORMAL, "Check Desc Stream=%d, first=%d, num=%d, options=%04X\n", stream, msg.firstDesc, msg.numDesc, msg.options);
	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
	{
		printk(KERN_WARNING "[ERROR] Check Desc: Stream %d has no DMA descriptors\n", stream);
		return -HEXITEC_DMA_ERROR_UNCONF_DESC;
	}
	if (msg.firstDesc >= m_dmaStream[stream].definedDesc || msg.firstDesc+msg.numDesc > m_dmaStream[stream].definedDesc)
	{
		printk(KERN_WARNING "[ERROR] Check Desc: First Descriptor=%d and num=%d out of range 0..%d\n", msg.firstDesc, msg.numDesc, m_dmaStream[stream].definedDesc-1);
		return -HEXITEC_DMA_ERROR_DESC_RANGE;
	}
	if (msg.options == 0)
	{
		switch (stream)
		{
		case HEXITEC_DMA_STREAM_BNUM_10G_TO_DRAM:
			checks = HEXITEC_DMA_MSG_CHECK_10GRX | HEXITEC_DMA_MSG_CHECK_LENGTH | HEXITEC_DMA_MSG_CHECK_FRAME_PER_DESC;
			break;


		case NGZMP_DMA_STREAM_BNUM_SCOPE0:
		case NGZMP_DMA_STREAM_BNUM_SCOPE1:
		case NGZMP_DMA_STREAM_BNUM_SCOPE2:
			checks = HEXITEC_DMA_MSG_CHECK_1_FRAME;
			break;

		default:
			checks = 0;
			break;
		}
	}
	else
		checks = msg.options;

//	Xil_DCacheInvalidateRange((uint32_t)m_dmaStream[stream].desc, sizeof(AXIDMADesc)*m_dmaStream[stream].definedDesc);
	desc = m_dmaStream[stream].desc_virtBase+msg.firstDesc;
	numDesc = msg.numDesc;
	if (msg.numDesc == 0)
		numDesc = m_dmaStream[stream].definedDesc-msg.firstDesc;

	if (dev_stat->access_flags[m_dmaStream[stream].desc_addr_space] & ACCESS_CACHEFLUSH)
		__inval_dcache_area(desc, numDesc*sizeof(AXIDMADesc));

	for (i=0; i<numDesc; i++)
	{
		status = desc->status;
		if (!(status & XAXIDMA_BD_STS_COMPLETE_MASK))
			break;
		total_bytes += desc->status & 0x7FFFFF;
		if (error_desc == -1)
		{
			if (status & XAXIDMA_BD_STS_ALL_ERR_MASK)
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Error detected by AXI DMA, Status=%08X\n", i+msg.firstDesc, status);
				error_desc = i;
			}

			if ((checks & HEXITEC_DMA_MSG_CHECK_FRAME_PER_DESC) && (status & (XAXIDMA_BD_STS_RXSOF_MASK |XAXIDMA_BD_STS_RXEOF_MASK)) != (XAXIDMA_BD_STS_RXSOF_MASK |XAXIDMA_BD_STS_RXEOF_MASK))
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Missing SOF or EOF Status=%08X\n", i+msg.firstDesc, status);
				error_desc = i;
			}
			if (checks & HEXITEC_DMA_MSG_CHECK_1_FRAME)
			{
				if (m_dmaStream[stream].definedDesc == 1)
				{
					// Special case of single desc
					if ((status & (XAXIDMA_BD_STS_RXSOF_MASK | XAXIDMA_BD_STS_RXEOF_MASK)) != (XAXIDMA_BD_STS_RXSOF_MASK | XAXIDMA_BD_STS_RXEOF_MASK))
					{
						printk(KERN_WARNING "Check_DESC: Desc %d: Missing SOF or EOF Status=%08X\n", i+msg.firstDesc, status);
						error_desc = i;
					}
				}
				else
				{
					if (i+msg.firstDesc == 0)
					{
						if ( (status & (XAXIDMA_BD_STS_RXSOF_MASK | XAXIDMA_BD_STS_RXEOF_MASK)) != (XAXIDMA_BD_STS_RXSOF_MASK ))
						{
							printk(KERN_WARNING "Check_DESC: Desc %d: Missing SOF or extra EOF on first descriptor Status=%08X\n", i+msg.firstDesc, status);
							error_desc = i;
						}
					}
					else if (i+msg.firstDesc == m_dmaStream[stream].definedDesc-1)
					{
						if ( (status & (XAXIDMA_BD_STS_RXSOF_MASK | XAXIDMA_BD_STS_RXEOF_MASK)) != (XAXIDMA_BD_STS_RXEOF_MASK ))
						{
							printk(KERN_WARNING "Check_DESC: Desc %d: Extra SOF or missing EOF on last descriptor Status=%08X\n", i+msg.firstDesc, status);
							error_desc = i;
						}
					}
					else
					{
						if ( (status & (XAXIDMA_BD_STS_RXSOF_MASK | XAXIDMA_BD_STS_RXEOF_MASK)) != 0)
						{
							printk(KERN_WARNING "Check_DESC: Desc %d: Extra SOF or EOF on intermediate descriptor Status=%08X\n", i+msg.firstDesc, status);
							error_desc = i;
						}
					}
				}
			}
			if ((checks & HEXITEC_DMA_MSG_CHECK_LENGTH) && (desc->control & 0x7FFFFF) != (status & 0x7FFFFF))
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Length mismatch. Requested=%08X, received=%08X\n", i+msg.firstDesc, desc->control, status);
				error_desc = i;
			}
			if ((checks & HEXITEC_DMA_MSG_CHECK_TIMEFRAME) && desc->app[0] != i)
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Time Frame mismatch, found time frame %d\n", i+msg.firstDesc, desc->app[0]);
				error_desc = i;
			}
		}
		desc++;
	}
	desc--;
	if (param->ptr3 != NULL)
	{
		if (put_user(i, (int32_t*)(param->ptr3))) // Return the total number of frames with the completed bit set 
		{
			printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3));
			return -EFAULT;
		}
		if (stream == HEXITEC_DMA_STREAM_BNUM_10G_TO_DRAM)
		{
			if (put_user(total_bytes, (int32_t*)&(param->ptr3[1]))) // Return total bytes for 10G to DRAM stream
			{
				printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3+1));
				return -EFAULT;
			}
		}
		else
		{
			if (i > 0)
			{
				if (put_user(desc->app[0], (int32_t*)&(param->ptr3[1]))) // Return app[0], which is often the frame number
				{
					printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3+1));
					return -EFAULT;
				}
			}
			else
			{
				// No Valid descriptors to read frames from
				int32_t dummy_fnum=0;
				if (put_user(dummy_fnum, (int32_t*)&(param->ptr3[1]))) // Return app[0], which is often the frame number
				{
					printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3+1));
					return -EFAULT;
				}
			}
		}
	}

	if (error_desc != -1)
	{
		desc = m_dmaStream[stream].desc_virtBase+msg.firstDesc+error_desc;
		descPhys = m_dmaStream[stream].desc_dma + sizeof(AXIDMADesc)*(error_desc+msg.firstDesc);
		printk(KERN_WARNING "Desc=%d, Addr=%010llX, Nxt=%010llX, DMA addr=%010llX, numBytes=0x%08X=%d, ctrl=%08X, status=%08X\n",
				error_desc+msg.firstDesc, descPhys, desc->phys_next, desc->phys_addr, desc->control & 0x7FFFFF, desc->control & 0x7FFFFF, desc->control, desc->status );
		DBGLEVEL(MSG_NORMAL, ".... Check desc found %d good descriptors out of %d\n", error_desc, i);
		if (param->ptr3 != NULL)
		{
			if (put_user(desc->status, (param->ptr3+2)))
			{
				printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3+2));
				return -EFAULT;
			}
		}
		if (num_complete != NULL)
			*num_complete = error_desc;
		return 0;
	}
	else
	{
		if (param->ptr3 != NULL)
		{
			if (i > 0)
			{
				if (put_user(desc->status, (int32_t*)(param->ptr3+2)))
				{
					printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3+2));
					return -EFAULT;
				}
			}
			else
			{
				// No Valid descriptors to read status from
				int32_t dummy_status=0;
				if (put_user(dummy_status, (int32_t*)(param->ptr3+2)))
				{
					printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3+2));
					return -EFAULT;
				}
			}
		}
	}
	DBGLEVEL(MSG_NORMAL, ".... Check desc found %d all good descriptors\n", i);
	if (num_complete != NULL)
		*num_complete = i;
	return 0;
}


int zynqmp_dma_check_stream(DevStatics *dev_stat, int stream)
{
	if (stream < NGZMP_DMA_STREAM_BNUM_PLAYBACK0 || stream >= HEXITEC_NUM_AXI_DMA)
		return -1;
	if (m_dmaStream[stream].base_addr == 0)
		return -1;	

	return 0;
} 

int zynqmp_dma_get_desc_status(DevStatics *dev_stat, ZynqMPPb *param)
{
	AXIDMADesc *desc;
	m_dmaStream *dma = m_dmaStream;
	int stream = param->num;
	HEXITEC_DMA_MsgGetDescStatus msg;

	if (copy_from_user((void *) &msg, (void __user *)param->ptr, sizeof(HEXITEC_DMA_MsgGetDescStatus)))
		return -EFAULT;

	if (stream < 0 || stream >= HEXITEC_NUM_AXI_DMA )
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA get desc status requires stream in range 0 to %d, not %dX\n", HEXITEC_NUM_AXI_DMA-1, param->num);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	DBGLEVEL(MSG_NORMAL, "Get Desc Status Stream=%d, first=%d\n", stream, msg.firstDesc);
	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
	{
		printk(KERN_WARNING "[ERROR] Get Desc Status: Stream %d has no DMA descriptors\n", stream);
		return -HEXITEC_DMA_ERROR_UNCONF_DESC;
	}
	if (msg.firstDesc >= m_dmaStream[stream].definedDesc)
	{
		printk(KERN_WARNING "[ERROR] Get Desc Status: First Descriptor=%d out of range 0..%d\n", msg.firstDesc, m_dmaStream[stream].definedDesc-1);
		return -HEXITEC_DMA_ERROR_DESC_RANGE;
	}

//	Xil_DCacheInvalidateRange((uint32_t)m_dmaStream[stream].desc, sizeof(AXIDMADesc)*m_dmaStream[stream].definedDesc);
	desc = m_dmaStream[stream].desc_virtBase+msg.firstDesc;

	if (dev_stat->access_flags[m_dmaStream[stream].desc_addr_space] & ACCESS_CACHEFLUSH)
		__inval_dcache_area(desc, sizeof(AXIDMADesc));

	if (param->ptr3 != NULL)
	{
		if (put_user(desc->status, (int32_t*)(param->ptr3))) // Return the status word from teh specified descriptor. 
		{
			printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3));
			return -EFAULT;
		}
	}
	return 0;
}
#if 0

int zynqmp_dma_reuse(DevStatics *dev_stat, ZynqMPPb *param)
{
	int i, last, desc_num;
	AXIDMADesc *desc;
	m_dmaStream *dma = m_dmaStream;
	int stream = param->num;
	HEXITEC_DMA_MsgReuse msg;

	if (copy_from_user((void *) &msg, (void __user *)param->ptr, sizeof(HEXITEC_DMA_MsgReuse)))
		return -EFAULT;

	if (stream < 0 || stream >= HEXITEC_NUM_AXI_DMA )
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA reuse command requires stream in range 0 to %d, not %dX\n", HEXITEC_NUM_AXI_DMA-1, param->num);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	if (stream != HEXITEC_DMA_STREAM_BNUM_SCALERS && stream != XSP3M_DMA_STREAM_BNUM_HIST_FRAMES)
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA reuse command should be used only on Scalars or Hist Frames streams  not %d\n", stream);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	DBGLEVEL(MSG_VERBOSE, "Re-send Stream=%d, first=%d, num=%d\n", stream, msg.firstDesc, msg.numDesc);
	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
	{
		printk(KERN_WARNING "[ERROR] Stream %d has no DMA descriptors\n", stream);
		return -HEXITEC_DMA_ERROR_UNCONF_DESC;
	}
	if (msg.firstDesc >= m_dmaStream[stream].definedDesc)	// Allow to wrap round.
	{
		printk(KERN_WARNING "[ERROR] Resend: First Descriptor=%d and num=%d out of range 0..%d\n", msg.firstDesc, msg.numDesc, m_dmaStream[stream].definedDesc-1);
		return -HEXITEC_DMA_ERROR_DESC_RANGE;
	}


	last = (msg.firstDesc+msg.numDesc-1) % m_dmaStream[stream].definedDesc;
	desc = m_dmaStream[stream].desc_virtBase+last;

	if ((desc->status & (XAXIDMA_BD_STS_COMPLETE_MASK | XAXIDMA_BD_STS_RXSOF_MASK |XAXIDMA_BD_STS_RXEOF_MASK)) != (XAXIDMA_BD_STS_COMPLETE_MASK | XAXIDMA_BD_STS_RXSOF_MASK | XAXIDMA_BD_STS_RXEOF_MASK))
	{
		printk(KERN_WARNING "[ERROR] Resend: Last descriptor=%d does not have correct completed status=0x%08X\n", last, desc->status);
		return -HEXITEC_DMA_ERROR_DESC_RANGE;
	}
	
	desc = m_dmaStream[stream].desc_virtBase+msg.firstDesc;
	desc_num = msg.firstDesc;
	for (i=0; i<msg.numDesc; i++)
	{
		desc->status = 0;
		desc->app[3] = 0;
		desc++;
		desc_num++;
		if (desc_num == m_dmaStream[stream].definedDesc)
		{
			desc = m_dmaStream[stream].desc_virtBase;
			desc_num = 0;
		}
	}
	if (dev_stat->access_flags[m_dmaStream[stream].desc_addr_space] & ACCESS_CACHEFLUSH)
		__flush_dcache_area(m_dmaStream[stream].desc_virtBase, m_dmaStream[stream].numDesc*sizeof(AXIDMADesc));
	return -HEXITEC_DMA_ERROR_OK;
}


int zynqmp_dma_check_rx_desc_circular(DevStatics *dev_stat, ZynqMPPb *param)
{
	int i;
	AXIDMADesc *desc;
	int numDesc;
	m_dmaStream *dma = m_dmaStream;
	int stream = param->num;
	uint32_t dma_status;
	uint32_t status;
	int error_desc=-1;
	uint64_t cur_descPhys;
	int desc_num, cur_desc;
	uint64_t num_good, num_completed, next_time_frame, cur_time_frame=0;
	uint32_t l, h;
	int error_code=0;

	if (stream < 0)
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA check desc command requires 1 and only 1 DMA stream at a time, not 0x%X\n", param->num);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	DBGLEVEL(MSG_NORMAL, "Check Desc Circular Stream=%d\n", stream);
	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
	{
		printk(KERN_WARNING "[ERROR] Check Desc: Stream %d has no DMA descriptors\n", stream);
		return -HEXITEC_DMA_ERROR_UNCONF_DESC;
	}
	 
	if (m_dmaStream[stream].isTx)
	{
		dma_status = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_SR_OFFSET)/4);
		cur_descPhys = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_TX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
	}
	else
	{
		dma_status = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET)/4);
		cur_descPhys = AXIRead32(m_dmaStream[stream].virtBase+(XAXIDMA_RX_OFFSET+XAXIDMA_CDESC_OFFSET)/4);
	}
	dev_stat->status_block.status[stream] = dma_status;

	cur_desc = cur_descPhys - m_dmaStream[stream].desc_dma;

/*	Possible cases:
	For easy backwards compatible there is the return code which gets back to libxspress3 via the num_ops field and 3 off 32 bits words which get back via the extra1,2,3 fields
	But loses info so now looking to return 3 off 64 bit numbers plus various 32 bit status words.	

	Not done any

	All OK
		Number of correct frames
		Number of compete frames (same)
		Time fame of last frame (num_done-1)
	
	Stopped with over run -- quiet possible

	Some other error.

*/

	desc_num = dev_stat->last_checked[stream]+1;
	numDesc = m_dmaStream[stream].definedDesc;
	num_good = dev_stat->num_good[stream];
	num_completed = dev_stat->num_completed[stream];
	next_time_frame = dev_stat->next_time_frame[stream];

	if (dev_stat->access_flags[m_dmaStream[stream].desc_addr_space] & ACCESS_CACHEFLUSH)
		__inval_dcache_area(m_dmaStream[stream].desc_virtBase, m_dmaStream[stream].numDesc*sizeof(AXIDMADesc));
	for (i=0; i<numDesc; i++)
	{
		desc_num %= numDesc;
		if (desc_num == cur_desc)
			break;

		desc = m_dmaStream[stream].desc_virtBase+desc_num;
		status = desc->status;
		if (!(status & XAXIDMA_BD_STS_COMPLETE_MASK))
			break;

		cur_time_frame = *(uint64_t *)(desc->app);
		if (error_desc == -1)
		{
			if (status & XAXIDMA_BD_STS_ALL_ERR_MASK)
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Error detected by AXI DMA, Status=%08X\n", desc_num, status);
				error_desc = desc_num;
				error_code = -HEXITEC_DMA_ERROR_FIRMWARE_DETECTED;
			}

			if ((status & (XAXIDMA_BD_STS_RXSOF_MASK |XAXIDMA_BD_STS_RXEOF_MASK)) != (XAXIDMA_BD_STS_RXSOF_MASK |XAXIDMA_BD_STS_RXEOF_MASK))
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Missing SOF or EOF Status=%08X\n", desc_num, status);
				error_desc = desc_num;
				error_code = -HEXITEC_DMA_ERROR_DESC_SOF_EOF;
			}

			if ((desc->control & 0x7FFFFF) != (status & 0x7FFFFF))
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Length mismatch. Requested=%08X, received=%08X\n", desc_num, desc->control, status);
				error_desc = desc_num;
				error_code = -HEXITEC_DMA_ERROR_DESC_LENGTH;
			}
			if (cur_time_frame != next_time_frame)
			{
				printk(KERN_WARNING "Check_DESC: Desc %d: Time Frame mismatch, found time frame %ju\n", desc_num, cur_time_frame);
				error_desc = desc_num;
				error_code = HEXITEC_DMA_ERROR_DESC_TIME_FRAME;
			}
		}
		if (error_desc == -1 && num_good == num_completed)
		num_good++;
		num_completed++;
		next_time_frame++;
		desc_num++;
	}
	desc_num--;
	if (desc_num == -1)
		desc_num = numDesc-1;
	dev_stat->last_checked[stream] = desc_num;
	dev_stat->num_good[stream] = num_good;
	dev_stat->num_completed[stream] = num_completed;
	dev_stat->next_time_frame[stream] = next_time_frame;
	desc = m_dmaStream[stream].desc_virtBase+desc_num;

	if (param->ptr3 != NULL)
	{
		DBGLEVEL(MSG_NORMAL, "zynqmp_dma_check_rx_desc_circular: returning num_good=%d, num_completed=%d, cur_time_frame=%d\n", (int)num_good, (int)num_completed, (int)desc->app[0]);
		l = num_good & 0xFFFFFFFF;
		h = num_good >> 32;
		if (put_user(l, (param->ptr3)) || put_user(h, (param->ptr3+1))) // Return the Number of good frames with the completed bit set and time frame matching from the start
		{
			printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (param->ptr3));
			return -EFAULT;
		}
		l = num_completed & 0xFFFFFFFF;
		h = num_completed >> 32;
		if (put_user(l, (param->ptr3+2)) || put_user(h, (param->ptr3+3))) // Return the total number of frames with the completed bit set 
		{
			printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (param->ptr3+2));
			return -EFAULT;
		}
		l = desc->app[0];
		h = desc->app[1];
		if (put_user(l, (param->ptr3+4)) || put_user(h, (param->ptr3+5))) // Return the Time frame from the last descriptor processed
		{
			printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (param->ptr3+4));
			return -EFAULT;
		}
		if (dma_status & XAXIDMA_ERR_ALL_MASK)
		{
			/* Cur desc is now the descriptor causing the error, so look it up to get more info */
			error_code = -HEXITEC_DMA_ERROR_FIRMWARE_DETECTED;
			l = 0;
			h = 0;
			i = cur_desc;
			desc = m_dmaStream[stream].desc_virtBase+i;
		}
		else if (error_desc != -1)
		{
			/* Error detector in descriptors */
			desc = m_dmaStream[stream].desc_virtBase+error_desc;
			l = desc->app[0];
			h = desc->app[1];
			i = error_desc;
		}
		else if (num_completed == 0)
		{
			i = 0;
			desc = m_dmaStream[stream].desc_virtBase+i;
			l = 0;
			h = 0;
		}
		else
		{
			i = desc_num;
			desc = m_dmaStream[stream].desc_virtBase+i;
			l = desc->app[0];
			h = desc->app[1];
		}			
		if (put_user(error_code, (int32_t*)(param->ptr3+6)) || put_user(i, (int32_t*)(param->ptr3+7)) || put_user(dma_status, (param->ptr3+8)) ||	 // Return Error Code and Last or Failing Descriptor, Status register from Core, status from descriptor
			put_user(desc->status, (param->ptr3+9)) || put_user(l, (param->ptr3+10)) || put_user(h, (param->ptr3+11)) )								// Return the Descriptor status, and time frame when defined
		{
			printk(KERN_WARNING "%s: Cannot return data to Address %px\n", (int32_t *) (param->ptr3+6));
			return -EFAULT;
		}
	}
	return 12;	// The number of 32 bit words to return as this is a read type command
}

int zynqmp_dma_read_tf_status(DevStatics *dev_stat, ZynqMPPb *param)
{
	int i, desc_num;
	AXIDMADesc *desc;
	m_dmaStream *dma = m_dmaStream;
	int stream = zynqmp_get_stream_num(param->num);
	uint32_t first = param->addr_space;
	uint32_t num = param->and_mask;
	Xsp3TFStatus tf_status, *ptr;

	if (stream < 0)
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA read TF Status command requires 1 and only 1 DMA stream at a time, not 0x%X\n", param->num);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	if (stream != HEXITEC_DMA_STREAM_BNUM_SCALERS && stream != XSP3M_DMA_STREAM_BNUM_HIST_FRAMES)
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA read TF Status command should be used only on Scalars or Hist Frames streams  not %d\n", stream);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
	{
		printk(KERN_WARNING "[ERROR] Stream %d has no DMA descriptors\n", stream);
		return -HEXITEC_DMA_ERROR_UNCONF_DESC;
	}

	if (first >= m_dmaStream[stream].definedDesc)	// Allow to wrap round.
	{
		printk(KERN_WARNING "[ERROR] read TF Status: First Descriptor=%u out of range 0..%d\n", first, m_dmaStream[stream].definedDesc-1);
		return -HEXITEC_DMA_ERROR_DESC_RANGE;
	}
	desc = m_dmaStream[stream].desc_virtBase+first;

	ptr = (Xsp3TFStatus *)param->ptr3;
	desc_num= first;
	if (dev_stat->access_flags[m_dmaStream[stream].desc_addr_space] & ACCESS_CACHEFLUSH)
		__inval_dcache_area(desc, num*sizeof(AXIDMADesc));
	for (i=0; i<num; i++)
	{
		if (desc->status & XAXIDMA_BD_STS_COMPLETE_MASK)
			tf_status.state = 1;
		else
			tf_status.state = 0;
	
		tf_status.time_frame = (int64_t)(desc->app[0]) | (int64_t)(desc->app[1]) << 32;	//!< Extended time frame in circular buffer mode
		tf_status.markers   = desc->app[2];

		if (copy_to_user((void __user *)ptr, (void *) &tf_status, sizeof(Xsp3TFStatus)))
		{
			return -EFAULT;
		}
		ptr++;
		desc++;
		desc_num++;
		if (desc_num == m_dmaStream[stream].definedDesc)
		{
			desc = m_dmaStream[stream].desc_virtBase;
			desc_num = 0;
		}
	}
	return num;
}


int zynqmp_dma_read_markers(DevStatics *dev_stat, ZynqMPPb *param)
{
	int i, desc_num;
	AXIDMADesc *desc;
	m_dmaStream *dma = m_dmaStream;
	int stream = zynqmp_get_stream_num(param->num);
	uint32_t first = param->addr_space;
	uint32_t num = param->and_mask;
	uint32_t *ptr;

	if (stream < 0)
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA read TF Markers command requires 1 and only 1 DMA stream at a time, not 0x%X\n", param->num);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	if (stream != HEXITEC_DMA_STREAM_BNUM_SCALERS && stream != XSP3M_DMA_STREAM_BNUM_HIST_FRAMES)
	{
		DBGLEVEL(MSG_ERROR, "[ERROR] DMA read TF Markers command should be used only on Scalars or Hist Frames streams  not %d\n", stream);
		return -HEXITEC_DMA_ERROR_BAD_STREAM;
	}

	if ((m_dmaStream[stream].state & HEXITEC_DMA_STATE_DESC_BUILT) == 0)
	{
		printk(KERN_WARNING "[ERROR] Stream %d has no DMA descriptors\n", stream);
		return -HEXITEC_DMA_ERROR_UNCONF_DESC;
	}

	if (first >= m_dmaStream[stream].definedDesc)	// Allow to wrap round.
	{
		printk(KERN_WARNING "[ERROR] read TF Markers: First Descriptor=%u out of range 0..%d\n", first, m_dmaStream[stream].definedDesc-1);
		return -HEXITEC_DMA_ERROR_DESC_RANGE;
	}
	desc = m_dmaStream[stream].desc_virtBase+first;

	ptr = (uint32_t *)param->ptr3;
	desc_num= first;
	if (!access_ok( ptr, sizeof(int32_t)*num))
		return -EFAULT;

	if (dev_stat->access_flags[m_dmaStream[stream].desc_addr_space] & ACCESS_CACHEFLUSH)
		__inval_dcache_area(desc, num*sizeof(AXIDMADesc));
	for (i=0; i<num; i++)
	{
		__put_user(desc->app[2], ptr);

		ptr++;
		desc++;
		desc_num++;
		if (desc_num == m_dmaStream[stream].definedDesc)
		{
			desc = m_dmaStream[stream].desc_virtBase;
			desc_num = 0;
		}
	}
	return num;
}
#endif

uint32_t * zynqmp_dma_buffer_ptr(DevStatics *dev_stat, int stream, uint64_t byteOffset, uint64_t *remaining_bytes, uint64_t *chunk_bytes, int *addr_space, uint64_t *dma_base, int *seg_num)
{
	uint64_t max_bytes;
	int i;

	if (stream <0 || stream >= HEXITEC_NUM_AXI_DMA)
		return NULL;
	
	for (i=0; i<HEXITEC_NUM_DMA_SEGMENTS; i++)
	{
		if (byteOffset < m_dmaStream[stream].buff_seg[i].size)
		{
			max_bytes = m_dmaStream[stream].buff_seg[i].size-byteOffset;
			if (max_bytes > *remaining_bytes)
			{
				*chunk_bytes = *remaining_bytes;
				*remaining_bytes = 0;
			}
			else
			{
				*chunk_bytes = max_bytes;
				*remaining_bytes -= max_bytes;
			}
			*addr_space = m_dmaStream[stream].buff_seg[i].addr_space;
			if (dma_base != NULL)
				*dma_base= m_dmaStream[stream].buff_seg[i].dma_base+byteOffset;
			if (seg_num != NULL)
				*seg_num = i;
			return (uint32_t *)((u_int8_t *)(m_dmaStream[stream].buff_virtBase[i])+byteOffset);
		}
		else
		{
			byteOffset -= m_dmaStream[stream].buff_seg[i].size;
		}
	}
	return NULL;
}
#endif
