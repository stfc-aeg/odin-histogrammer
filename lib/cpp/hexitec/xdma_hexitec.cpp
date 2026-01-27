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

static int regionSize[HEXITEC_NUM_REGIONS] = {HEXITEC_NUM_CHIP_REGS, 80*2, 80*2, 80*2,80*2, 80*2, 80*2, 80*2, HEXITEC_RECIP_SIZE, HEXITEC_EDGE_POS_SIZE, HEXITEC_EDGE_POS_SIZE, 
						HEXITEC_RECIP_SIZE, HEXITEC_NEG_NEB_SIZE, HEXITEC_NEG_NEB_SIZE, HEXITEC_ENG_MAP_SIZE, HEXITEC_PIX_MASK_SIZE, HEXITEC_RECIP_SIZE, HEXITEC_L_POS_SIZE, HEXITEC_L_POS_SIZE };


XDmaHexitec :: XDmaHexitec(int useQDma, int busNum, int devNum, int funcNum)
{
	volatile uint32_t *p;
	int numHBM;
	int i;
	
	memset(m_regionNames, 0, sizeof(m_regionNames));
	memset(m_regionMasks, 255, sizeof(m_regionMasks));
	m_regionNames[HEXITEC_REGION_REGS] = "Regs";
	m_regionNames[HEXITEC_REGION_ABS_THRES] = "Abs Thres";
	m_regionNames[HEXITEC_REGION_BASELINE] = "Baseline";
	m_regionNames[HEXITEC_REGION_MTHRES] = "Main Thres";
	m_regionNames[HEXITEC_REGION_LTHRES] = "Lower Thres";
	m_regionNames[HEXITEC_REGION_LIN_A] = "Linearity A";
	m_regionNames[HEXITEC_REGION_LIN_B] = "Linearity B";
	m_regionNames[HEXITEC_REGION_LIN_C] = "Linearity C";
	m_regionNames[HEXITEC_REGION_EDGE_POS_RECIP] = "EdgePos Reciprocal";
	m_regionNames[HEXITEC_REGION_EDGE_POS_M] = "EdgePos M";
	m_regionNames[HEXITEC_REGION_EDGE_POS_C] = "EdgePos C";
	m_regionNames[HEXITEC_REGION_NEG_NEB_RECIP] = "NegNeb Reciprocal";
	m_regionNames[HEXITEC_REGION_NEG_NEB_M] = "NegNeb M";
	m_regionNames[HEXITEC_REGION_NEG_NEB_C] = "NegNeb C";
	m_regionNames[HEXITEC_REGION_ENG_MAP] = "Energy Map";
	m_regionNames[HEXITEC_REGION_PIX_MASK] = "Pixel Mask";
	m_regionNames[HEXITEC_REGION_L_POS_RECIP] = "L Pos Reciprocal";
	m_regionNames[HEXITEC_REGION_L_POS_M] = "L Pos M";
	m_regionNames[HEXITEC_REGION_L_POS_C] = "L Pos C";
	
	m_regionMasks[HEXITEC_REGION_REGS] = 0xFFFFFFFF;
	m_regionMasks[HEXITEC_REGION_ABS_THRES] = 0x3FFF;
	m_regionMasks[HEXITEC_REGION_BASELINE] = 0x3FFFFFFF;
	m_regionMasks[HEXITEC_REGION_MTHRES] = 0xFFFFFFFF;
	m_regionMasks[HEXITEC_REGION_LTHRES] = 0xFFFFFFFF;
	m_regionMasks[HEXITEC_REGION_LIN_A] = 0xFFFFFFFF;
	m_regionMasks[HEXITEC_REGION_LIN_B] = 0xFFFFFFFF;
	m_regionMasks[HEXITEC_REGION_LIN_C] = 0xFFFFFFFF;
	m_regionMasks[HEXITEC_REGION_EDGE_POS_RECIP] = 0x1FFFFFF;
	m_regionMasks[HEXITEC_REGION_EDGE_POS_M] = 0x3FFFF;
	m_regionMasks[HEXITEC_REGION_EDGE_POS_C] = 0x3FFFF;
	m_regionMasks[HEXITEC_REGION_NEG_NEB_RECIP] = 0x1FFFFFF;
	m_regionMasks[HEXITEC_REGION_NEG_NEB_M] = 0x3FFFF;
	m_regionMasks[HEXITEC_REGION_NEG_NEB_C] = 0x3FFFF;
	m_regionMasks[HEXITEC_REGION_ENG_MAP] = 0xF;
	m_regionMasks[HEXITEC_REGION_PIX_MASK] = 0xFFFFFFFF;
	m_regionSize = regionSize;
	
	for (i=0; i<HEXITEC_MAX_FARM_SOCKETS; i++)
		m_udpTxTestSocket[i] = -1;

	if (useQDma)
	{
		m_xdma = new QDma(busNum, devNum, funcNum, m_useChipSel?HEXITEC_USER_SIZE_WITH_CHIP_SEL:HEXITEC_USER_SIZE);
	}
	else
		m_xdma = new XDma(devNum, m_useChipSel?HEXITEC_USER_SIZE_WITH_CHIP_SEL:HEXITEC_USER_SIZE);
	if (m_useChipSel)
	{
		m_regs = m_xdma->m_regsBAR.m_base+HEXITEC_REGS_BASE_WITH_CHIP_SEL/sizeof(uint32_t);
		m_globOffset = HEXITEC_GLB_OFFSET_WITH_CHIP_SEL;
	}
	else
	{
		m_regs = m_xdma->m_regsBAR.m_base+HEXITEC_REGS_BASE/sizeof(uint32_t);
		m_globOffset = HEXITEC_GLB_OFFSET;
	}
	p = m_regs + m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_RD_REVISION;
	m_revision = *p;
	cout << "XDmaHexitec : Revision =" << (m_revision>>16) <<"." << (m_revision & 0xFFFF) << endl;

	p = m_regs + m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_RD_FEATURES;
	for (i=0; i < HEXITEC_NUM_FEATURE_REGS; i++)
		m_features[i] = *p++;

	// ToDo : Assign feature bit to know we have data mover and data mover with UDP output.
	if (HEXITEC_FEATURE_HISTA_DATA_MOVER(m_features[HEXITEC_FEATURE_REG_HISTA]))
	{
		m_dataMoverRegs = m_xdma->m_regsBAR.m_base+HEXITEC_DATA_MOVER_BASE/sizeof(uint32_t);
		initDataMover();
		if (useQDma)
			m_xdma->createStreamQueues();
	}

	if (HEXITEC_FEATURE_PS_GENERATION(m_features[HEXITEC_FEATURE_PROC_SIZES]))
	{
		cout << "Generation = HexitecMHz" << endl;
		m_generation = HexitecGenMHz;
		m_regionMasks[HEXITEC_REGION_ABS_THRES] = 0xFFF;	// 12 bit ADC for Hexitec MHz
		m_maxAdcValue = 4095;
		m_bsubRefScale = HEXITEC_BSUB_REF_SCALE_MHZ;
		m_numRxUdp = 1;
		m_hasFIFOMon = HEXITEC_FEATURE_MONITOR_FIFOS(m_features[HEXITEC_FEATURE_MONITORS]) > 0;
		m_numProcCol = 20;
	}
	else
	{
		m_regionMasks[HEXITEC_REGION_ABS_THRES] = 0x3FFF; // 14 bit ADC for Hexitec original.
		m_generation = HexitecGenHexitec;
		m_maxAdcValue = 16383;
		m_bsubRefScale = HEXITEC_BSUB_REF_SCALE_HXT;
		m_numRxUdp = 2;
		regionSize[HEXITEC_REGION_PIX_MASK] = HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS/32;
	}
	m_numChips = HEXITEC_FEATURE_PS_NUM_CHIPS(m_features[HEXITEC_FEATURE_PROC_SIZES]);
	if (m_numChips == 12)
	{
		m_numChipCols = 6;
		m_numChipRows = 2;
	}
	if (HEXITEC_FEATURE_HISTA_UDP_OUTPUT(m_features[HEXITEC_FEATURE_REG_HISTA]))
	{
		m_numTxUdp= 1;
		m_udpTxCore[0] = move (XDmaUDPCore(i, m_xdma, HEXITEC_UDP_CORE_BASE+HEXITEC_UDP_CORE_STRIDE*2));
	}
	numHBM = HEXITEC_FEATURE_HISTB_NUM_HIST(m_features[HEXITEC_FEATURE_REG_HISTB]);

	m_nBitsAddrPWLin = HEXITEC_FEAT_LIN_NBITS_PWL(m_features[HEXITEC_FEATURE_LINEARITY]);
	int scaleB1Posn = HEXITEC_FEAT_LIN_SCALE_B1POSN(m_features[HEXITEC_FEATURE_LINEARITY]);
	int scaleC1Posn = HEXITEC_FEAT_LIN_SCALE_C1POSN(m_features[HEXITEC_FEATURE_LINEARITY]);
	if (m_nBitsAddrPWLin > 0)
		m_nBitsCoeffLin = 18;
	else 
		m_nBitsCoeffLin = 25;
	m_linABMax = (1<< (m_nBitsCoeffLin-1))-1;
	m_linABMin = -m_linABMax -1;
	if (scaleB1Posn == 0)
	{
		/* Support legacy firmware */
		m_linScaleA = 1 << (m_nBitsCoeffLin-3);
		m_linScaleB = 1 << (m_nBitsCoeffLin-3);
	}
	else
	{
		m_linScaleA = 1 << scaleB1Posn;
		m_linScaleB = 1 << scaleB1Posn;
		m_linScaleC = 1 << scaleC1Posn;
	}	
	m_maxBitsClusterGrade = HEXITEC_FEATURE_OF_CLUSTER_GRADE(m_features[HEXITEC_FEATURE_OUTPUT_FORMATS]);
	cout << "XDmaHexitec :  m_nBitsAddrPWLin = " << m_nBitsAddrPWLin << endl;
	m_hbmHist = move(XDmaHbmHist(m_xdma, numHBM));
	for (i=0; i<m_numRxUdp; i++)
		m_udpCore[i] = move (XDmaUDPCore(i, m_xdma, HEXITEC_UDP_CORE_BASE+HEXITEC_UDP_CORE_STRIDE*i));

	m_hbmHist.m_axiReorder = HEXITEC_FEATURE_HISTB_REORDER(m_features[HEXITEC_FEATURE_REG_HISTB]);

	initDMA();
	m_debug = MSG_NORMAL;

}
XDmaHexitec :: ~XDmaHexitec()
{
	eventListStop();
	cout << " ~XDmaHexitec: Deleting m_xdma" << endl;
	delete m_xdma;
}
/**
	Write to hexitec per chip control registers

@param chip			Chip number  or -1 to duplicate to all chips.
@param region		Region number, {@link HEXITEC_REGION_REGS} from registers. Others for direct access to LUTs, but see LUT functions to ease access.
@param offset		Word offset of first register to write
@param num			Number of words to write
@param data			Pointer to data buffer to write.
*/
void XDmaHexitec::writeChipRegs(int chip, int region, int offset, int num, uint32_t *data)
{
	volatile uint32_t *p, *chipSel;
	int i;
	int firstChip, lastChip;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("writeChipRegs: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;
	if (region < 0 || region >= HEXITEC_NUM_REGIONS)
		throw XDmaHexitecException("writeChipRegs: region=%d out of range 0...%d", region, 31);
		
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			p = m_regs+HEXITEC_REGION_OFFSET*region+offset;
		}
		else
		{
			p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+offset+HEXITEC_REGION_OFFSET*region;
		}
		for (i=0; i<num; i++)
			*p++ = data[i];
	}
}
	
/**
	Read from hexitec per chip control registers

@param chip			Chip number.
@param region		Region number, {@link HEXITEC_REGION_REGS} from registers. Others for direct access to LUTs, but see LUT functions to ease access.
@param offset		Word offset of first register to read
@param num			Number of words to read
@param data			Pointer to data buffer to read.
*/
void XDmaHexitec::readChipRegs(int chip, int region, int offset, int num, uint32_t *data)
{
	volatile uint32_t *p, *chipSel;
	int i;

	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("readChipRegs: chip=%d out of range 0...%d", chip, m_numChips-1);
	if (region < 0 || region >= HEXITEC_NUM_REGIONS)
		throw XDmaHexitecException("readChipRegs: region=%d out of range 0...%d", region, 31);
	if (m_useChipSel)
	{
		chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
		*chipSel = chip;
		p = m_regs+HEXITEC_REGION_OFFSET*region+offset;
	}
	else
	{
		p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region+offset;
	}
	for (i=0; i<num; i++)
		*data++ = *p++;
}

/**
	Write single value to a hexitec per chip control register

@param chip			Chip number  or -1 to duplicate to all chips.
@param offset		Word offset of register to write
@param value		Value to write
*/
void XDmaHexitec::setChipReg(int chip, int offset, uint32_t value)
{
	volatile uint32_t *p, *chipSel;
	int firstChip, lastChip;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("setChipReg: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;
	
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			p = m_regs+offset;
		}
		else
		{
			p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+offset;
		}
		*p = value;
	}
}
	
uint32_t XDmaHexitec::getChipReg(int chip, int offset)
{
	volatile uint32_t *p, *chipSel;
	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("readChipRegs: chip=%d out of range 0...%d", chip, m_numChips-1);
	if (m_useChipSel)
	{
		chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
		*chipSel = chip;
		p = m_regs+offset;
	}
	else
	{
		p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+offset;
	}
	return *p;
}

void XDmaHexitec::writeGlobRegs(int offset, int num, uint32_t *data)
{
	volatile uint32_t *p;
	int i;
	p = m_regs+m_globOffset/sizeof(uint32_t)+offset;
	for (i=0; i<num;i ++)
		*p++ = data[i];
	
}
void XDmaHexitec::readGlobRegs(int offset, int num, uint32_t *data)
{
	volatile uint32_t *p;
	int i;
	p = m_regs+m_globOffset/sizeof(uint32_t)+offset;
	for (i=0; i<num;i ++)
		data[i] = *p++ ;
}

void XDmaHexitec::setGlobReg(int offset, uint32_t value)
{
	volatile uint32_t *p;
	p = m_regs+m_globOffset/sizeof(uint32_t)+offset;
		*p = value;
	
}
uint32_t XDmaHexitec::getGlobReg(int offset)
{
	volatile uint32_t *p;
	p = m_regs+m_globOffset/sizeof(uint32_t)+offset;
	return *p;
}
/* access register as 64 bit but offset is still measured in 32 bit words */
uint64_t XDmaHexitec::getGlobReg64(int offset)
{
	volatile uint32_t *p;
	p = m_regs+m_globOffset/sizeof(uint32_t)+offset;
	return *(uint64_t *)p;
}

/**
	Write a fixed value to multiple Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block.

@param chip			Chip number  or -1 to duplicate to all chips.
@param region		Region number, {@link HEXITEC_REGION_BASELINE} to {@link HEXITEC_REGION_LIN_C}
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param value		Value to write
*/

void XDmaHexitec::setPixelLUT(int chip, int region, int firstCol, int numCols, int firstRow, int numRows, uint32_t value)
{
	volatile uint32_t *selAddr, *p, *chipSel, *regionBase;
	int firstChip, lastChip;
	int rowCol, stream;
	int row, col, colTop;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("setPixelLUT: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;
	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("setPixelLUT: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("setPixelLUT: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);
	if (!(region >= HEXITEC_REGION_ABS_THRES && region <= HEXITEC_REGION_LIN_C))
		throw XDmaHexitecException("setPixelLUT: region=%d, is not a per pixel lookup table", region);
		
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			regionBase = m_regs+HEXITEC_REGION_OFFSET*region;
			p = regionBase;
		}
		else
		{
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
			regionBase = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region;
			p = regionBase;
		}
		if (m_generation == HexitecGenMHz)
		{
			if (firstCol==0 && numCols==HEXITEC_NUM_COLS)
			{
				*selAddr = HEXITEC_SEL_ADDR_BROADCAST;
				p += firstRow*2;
				for (rowCol=0; rowCol< numRows*2; rowCol++)
					*p++ = value;
			}
			else
			{
				for (col=firstCol; col<firstCol+numCols; col++)
				{
					stream = col % HEXITEC_NUM_STREAMS_BLTR;
					colTop = col / HEXITEC_NUM_STREAMS_BLTR;
					*selAddr = stream;
					for (row=firstRow; row<firstRow+numRows; row++)
					{
						rowCol = row*2+colTop;
						p[rowCol] = value;
					}
				}
			}
		}
		else
		{
			*selAddr = 0;		// All chips use SelAddr 0. VHDL was gong to use SelAddr but does not.
			for (row=0; row<numRows; row++)
			{
				p =regionBase+(row+firstRow)*HEXITEC_NUM_COLS+firstCol;
				for (col=0; col<numCols; col++)
					*p++ = value;
			}
		}
	}
}

/**
	Write array of value to a Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block
	The data is organised as data[numRows][numCols].

@param chip			Chip number  or -1 to duplicate to all chips. Note in this case the data array for 1 chip is is replicated for all chips
@param region		Region number, {@link HEXITEC_REGION_BASELINE} to {@link HEXITEC_REGION_LIN_C}
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param data			Pointer to data to write. Note order.
*/

void XDmaHexitec::writePixelLUT(int chip, int region, int firstCol, int numCols, int firstRow, int numRows, uint32_t *data)
{
	volatile uint32_t *selAddr, *p, *chipSel, *regionBase;
	uint32_t *src;
	int firstChip, lastChip;
	int rowCol, stream;
	int row, col, colTop;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("writePixelLUT: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;
	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("writePixelLUT: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("writePixelLUT: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);
	if (!(region >= HEXITEC_REGION_ABS_THRES && region <= HEXITEC_REGION_LIN_C))
		throw XDmaHexitecException("writePixelLUT: region=%d, is not a per pixel lookup table", region);
		
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			regionBase = m_regs+HEXITEC_REGION_OFFSET*region;
		}
		else
		{
			regionBase = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region;
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
		}
		if (m_generation == HexitecGenMHz)
		{
			p = regionBase;
			for (col=firstCol; col<firstCol+numCols; col++)
			{
				src = data+(col-firstCol);
				stream = col % HEXITEC_NUM_STREAMS_BLTR;
				colTop = col / HEXITEC_NUM_STREAMS_BLTR;
				*selAddr = stream;
				for (row=firstRow; row<firstRow+numRows; row++)
				{
					rowCol = row*2+colTop;
					p[rowCol] = *src;
					src += numCols;
				}
			}
		}
		else
		{
			*selAddr = 0;
			src = data;
			for (row=0; row<numRows; row++)
			{
				p = regionBase+(row+firstRow)*HEXITEC_NUM_COLS+firstCol;				
				for (col=0; col<numCols; col++)
				{
					*p++ = *src++;
				}
			}
		}
	}
}
/**
	Read array of values from a Hexitec per pixel LUTs currently in the baseline, linearity and trigger threshold processing block
	The data is organised as data[numRows][numCols]

@param chip			Chip number.
@param region		Region number, {@link HEXITEC_REGION_BASELINE} to {@link HEXITEC_REGION_LIN_C}
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param data			Pointer to receive data. Note order.
*/

void XDmaHexitec::readPixelLUT(int chip, int region, int firstCol, int numCols, int firstRow, int numRows, uint32_t *data)
{
	volatile uint32_t *selAddr, *p, *chipSel, *regionBase;
	uint32_t *dst;
	int rowCol, stream;
	int row, col, colTop;

	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("readPixelLUT: chip=%d out of range 0...%d", chip, m_numChips-1);
	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("readPixelLUT: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("readPixelLUT: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);
	if (!(region >= HEXITEC_REGION_ABS_THRES && region <= HEXITEC_REGION_LIN_C))
		throw XDmaHexitecException("readPixelLUT: region=%d, is not a per pixel lookup table", region);
		
	if (m_useChipSel)
	{
		chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
		*chipSel = chip;
		selAddr = m_regs;
		regionBase = m_regs+HEXITEC_REGION_OFFSET*region;
//		printf("readPixelLUT: chip=%d, chipSel=%p, selAddr=%p, regionBase=%p\n", chip, chipSel, selAddr, regionBase); 
	}
	else
	{
		regionBase = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region;
		selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
	}
	if (m_generation == HexitecGenMHz)
	{
		p = regionBase;
		for (col=firstCol; col<firstCol+numCols; col++)
		{
			dst = data+(col-firstCol);
			stream = col % HEXITEC_NUM_STREAMS_BLTR;
			colTop = col / HEXITEC_NUM_STREAMS_BLTR;
			*selAddr = stream;
			for (row=firstRow; row<firstRow+numRows; row++)
			{
				rowCol = row*2+colTop;
//				cout << "col="<<col<<", stream="<<stream<<", rolCol="<<rowCol<<", data="<<p[rowCol]<<endl;
				*dst = p[rowCol];
				dst += numCols;
			}
		}
	}
	else
	{
		*selAddr = 0;
		dst = data;
		for (row=0; row<numRows; row++)
		{
			p = regionBase+(row+firstRow)*HEXITEC_NUM_COLS+firstCol;				
			for (col=0; col<numCols; col++)
			{
				*dst = *p++ ;
				dst ++;
			}
		}
	}
}

/**
	Write a fixed value to multiple Hexitec per pixel Linearity LUTs .
	Note that the Linearity LUTS can be built so that the top few (currently 3) ADC bits select different tables to allow a piecewise quadratic linearity correction.
	For this command, the value can be written to a single part of the table or  replicated to all. (addrTop == -1).

@param chip			Chip number  or -1 to duplicate to all chips.
@param region		Region number, {@link HEXITEC_REGION_LIN_A} to {@link HEXITEC_REGION_LIN_C}
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param adcTop		adcTop value 0..2^NBitsAddrPWLin-1, or -1 for all
@param value		Value to write
*/

void XDmaHexitec::setPixelLin(int chip, int region, int firstCol, int numCols, int firstRow, int numRows, int adcTop, uint32_t value)
{
	volatile uint32_t *selAddr, *regionBase, *p, *chipSel;
	int firstChip, lastChip;
	int rowCol, stream;
	int row, col, colTop;
	int firstAdcTop, lastAdcTop;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("setPixelLin: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;
	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("setPixelLin: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("setPixelLin: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);
	if (!(region >= HEXITEC_REGION_LIN_A && region <= HEXITEC_REGION_LIN_C))
		throw XDmaHexitecException("setPixelLin: region=%d, is not a per pixel linearity lookup table", region);
	if (adcTop >= (1<<m_nBitsAddrPWLin))
		throw XDmaHexitecException("setPixelLin: adcTop=%d, is not in range 0...%d", adcTop, (1<<m_nBitsAddrPWLin)-1);
	if (adcTop < 0)
	{
		firstAdcTop = 0;
		lastAdcTop = (1<<m_nBitsAddrPWLin)-1;
	}
	else
	{
		firstAdcTop = adcTop;
		lastAdcTop = adcTop;
	}
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			regionBase = m_regs+HEXITEC_REGION_OFFSET*region;
		}
		else
		{
			regionBase = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region;
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
		}
		if (m_generation == HexitecGenMHz)
		{
			if (firstCol==0 && numCols==HEXITEC_NUM_COLS)
			{
				*selAddr = HEXITEC_SEL_ADDR_BROADCAST;
				for (row=firstRow; row<firstRow+numRows; row++)
				{
					for (colTop=0; colTop<2; colTop++)
					{
						p = regionBase + ((row*2+colTop)<< m_nBitsAddrPWLin);
						for (adcTop=firstAdcTop; adcTop <= lastAdcTop; adcTop++)
							p[adcTop] = value;
					}
				}
			}
			else
			{
				for (col=firstCol; col<firstCol+numCols; col++)
				{
					stream = col % HEXITEC_NUM_STREAMS_BLTR;
					colTop = col / HEXITEC_NUM_STREAMS_BLTR;
					*selAddr = stream;
					p = regionBase;
					for (row=firstRow; row<firstRow+numRows; row++)
					{
						rowCol = row*2+colTop;
						p = regionBase + (rowCol << m_nBitsAddrPWLin);
						for (adcTop=firstAdcTop; adcTop <= lastAdcTop; adcTop++)
							p[adcTop] = value;
					}
				}
			}
		}
		else
		{
			int stride = 1 << m_nBitsAddrPWLin;
			*selAddr = 0;		// All chips use SelAddr 0. VHDL was going to use SelAddr but does not.
			for (row=0; row<numRows; row++)
			{
				p = regionBase+(((row+firstRow)*HEXITEC_NUM_COLS+firstCol)<<m_nBitsAddrPWLin) ;
				for (col=0; col<numCols; col++)
				{
					for (adcTop=firstAdcTop; adcTop <= lastAdcTop; adcTop++)
						p[adcTop] = value;
					p += stride;
				}
			}
		}
	}
}

/**
	Write array of value to a Hexitec per pixel Linearity LUTs.
	The data is organised as data[numRows][numCols][1<<nBitsAddrPWLin] 
	Note that the Linearity LUTS can be built so that the top few (currently 3) ADC bits select different tables to allow a piecewise quadratic linearity correction.
	For this command, all 2^nBitsAddrPWLin values can be written to the LUT or a single value can be replicated to all 2^nBitsAddrPWLin locations, using replicateAdcTop.

@param chip			Chip number  or -1 to duplicate to all chips. Note in this case the data array for 1 chip is is replicated for all chips
@param region		Region number, {@link HEXITEC_REGION_LIN_A} to {@link HEXITEC_REGION_LIN_C}
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param replicateAdcTop If false, data must contain 2**nBitsAddrPWLin value pe point. If false data must contain 1 value/point which is replicated to all 2**nBitsAddrPWLin locations.
@param data			Pointer to data to write. Note order.
*/

void XDmaHexitec::writePixelLin(int chip, int region, int firstCol, int numCols, int firstRow, int numRows,  bool replicateAdcTop, uint32_t *data)
{
	volatile uint32_t *selAddr, *regionBase, *p, *chipSel;
	uint32_t *src;
	int firstChip, lastChip;
	int rowCol, stream;
	int row, col, colTop;
	int adcTop;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("writePixelLin: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("writePixelLin: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);
	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("writePixelLin: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (!(region >= HEXITEC_REGION_LIN_A && region <= HEXITEC_REGION_LIN_C))
		throw XDmaHexitecException("setPixelLin: region=%d, is not a per pixel linearity lookup table", region);
		
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			regionBase = m_regs+HEXITEC_REGION_OFFSET*region;
		}
		else
		{
			regionBase = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region;
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
		}
		if (m_generation == HexitecGenMHz)
		{
			for (col=firstCol; col<firstCol+numCols; col++)
			{
				if (replicateAdcTop)
					src = data+(col-firstCol);
				else
					src = data +((col-firstCol)<<m_nBitsAddrPWLin);
				stream = col % HEXITEC_NUM_STREAMS_BLTR;
				colTop = col / HEXITEC_NUM_STREAMS_BLTR;
				*selAddr = stream;
				for (row=firstRow; row<firstRow+numRows; row++)
				{
					rowCol = row*2+colTop;
					p = regionBase+(rowCol << m_nBitsAddrPWLin);
					if (replicateAdcTop)
					{
						for (adcTop=0; adcTop < 1 << m_nBitsAddrPWLin; adcTop++)
							*p++ = *src;
						src += numCols;
					}
					else
					{
						uint32_t *pixSrc = src;
						for (adcTop=0; adcTop < 1 << m_nBitsAddrPWLin; adcTop++)
							*p++ = *pixSrc++;
						src += numCols << m_nBitsAddrPWLin;
					}
				}
			}
		}
		else
		{
			*selAddr = 0;
			src = data;
			for (row=0; row<numRows; row++)
			{
				p = regionBase+(((row+firstRow)*HEXITEC_NUM_COLS+firstCol)<<m_nBitsAddrPWLin) ;
				for (col=0; col<numCols; col++)
				{
					if (replicateAdcTop)
					{
						for (adcTop=0; adcTop < 1 << m_nBitsAddrPWLin; adcTop++)
							*p++ = *src;
						src++;
					}
					else
					{
						for (adcTop=0; adcTop < 1 << m_nBitsAddrPWLin; adcTop++)
							*p++ = *src++;
					}
				}
			}
		}
	}
}
/**
	Read array of values from a Hexitec per pixel Lineaerity LUTs.
	The data is organised as data[numRows][numCols][1<<nBitsAddrPWLin]
	Size depends on m_nBitsAddrPWLin

@param chip			Chip number.
@param region		Region number, {@link HEXITEC_REGION_LIN_A} to {@link HEXITEC_REGION_LIN_C}
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param data			Pointer to receive data.
*/

void XDmaHexitec::readPixelLin(int chip, int region, int firstCol, int numCols, int firstRow, int numRows, uint32_t *data)
{
	volatile uint32_t *selAddr, *regionBase, *p, *chipSel;
	uint32_t *dst;
	int rowCol, stream;
	int row, col, colTop, adcTop;

	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("readPixelLin: chip=%d out of range 0...%d", chip, m_numChips-1);
	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("readPixelLin: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("readPixelLin: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);
	if (!(region >= HEXITEC_REGION_LIN_A && region <= HEXITEC_REGION_LIN_C))
		throw XDmaHexitecException("readPixelLin: region=%d, is not a per pixel linearity lookup table", region);
		
	if (m_useChipSel)
	{
		chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
		*chipSel = chip;
		selAddr = m_regs;
		regionBase = m_regs+HEXITEC_REGION_OFFSET*region;
	}
	else
	{
		regionBase = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region;
		selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
	}
	if (m_generation == HexitecGenMHz)
	{
		for (col=firstCol; col<firstCol+numCols; col++)
		{
			stream = col % HEXITEC_NUM_STREAMS_BLTR;
			colTop = col / HEXITEC_NUM_STREAMS_BLTR;
			*selAddr = stream;
			dst = data+((col-firstCol) << m_nBitsAddrPWLin);
			for (row=firstRow; row<firstRow+numRows; row++)
			{
				uint32_t *pixDst=dst;
				rowCol = row*2+colTop;
				p = regionBase + (rowCol << m_nBitsAddrPWLin);
				for (adcTop=0; adcTop < 1 << m_nBitsAddrPWLin; adcTop++)
					*pixDst++ = *p++;
				dst += numCols << m_nBitsAddrPWLin;
			}
		}
	}
	else
	{
		dst = data;
		*selAddr = 0;
		for (row=0; row<numRows; row++)
		{
			p = regionBase+(((row+firstRow)*HEXITEC_NUM_COLS+firstCol)<<m_nBitsAddrPWLin) ;
			for (col=0; col<numCols; col++)
			{
				for (adcTop=0; adcTop < 1 << m_nBitsAddrPWLin; adcTop++)
					*dst++ = *p++;
			}
		}
		
	}
}


/**
	Write fixed value (usually 0) to all locations of a Hexitec Charge Sharing correction LUTs. Currently shared across all pixels.

@param chip			Chip number  or -1 to duplicate to all chips.
@param region		Region number, {@link HEXITEC_REGION_EDGE_POS_RECIP} to {@link HEXITEC_REGION_ENG_MAP}
@param stream		Processing stream (which handles 2 pairs of columns (total 4 columns). Usually -1 to replicate to all.
@param first		First offset within LUT, usually 0,
@param num			Number of point to write (usually full table)
@param value			Value to write
*/

void XDmaHexitec::setSharedLUT(int chip, int region, int stream, int first, int num, uint32_t value)
{
	volatile uint32_t *selAddr, *p, *chipSel;
	int firstChip, lastChip;
	int i;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("setSharedLUT: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	if (!((region >= HEXITEC_REGION_EDGE_POS_RECIP && region <= HEXITEC_REGION_ENG_MAP) || (region >= HEXITEC_REGION_L_POS_RECIP && region <= HEXITEC_REGION_L_POS_C)))
		throw XDmaHexitecException("setSharedLUT: region=%d, is not a shared lookup table", region);

	if (first < 0 || first >= regionSize[region] || num < 1 || first+num > regionSize[region])
		throw XDmaHexitecException("setSharedLUT: first=%d, num=%d out of range 0...%d", first, num, regionSize[region]);
		
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			p = m_regs+HEXITEC_REGION_OFFSET*region+first;
		}
		else
		{
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
			p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region+first;
		}
		if (stream < 0)
			*selAddr = HEXITEC_SEL_ADDR_BROADCAST;
		else
			*selAddr = stream;
			
		for (i=0; i<num; i++)
			*p++ = value;
	}
}


/**
	Write array of value to a Hexitec Charge Sharing correction LUTs. Currently shred across all pixels.

@param chip			Chip number  or -1 to duplicate to all chips. Note in this case the data array for 1 chip is is replicated for all chips
@param region		Region number, {@link HEXITEC_REGION_EDGE_POS_RECIP} to {@link HEXITEC_REGION_ENG_MAP}
@param stream		Processing stream (which handles 2 pairs of columns (total 4 columns). Usually -1 to replicate to all.
@param first		First offset within LUT, usually 0,
@param num			Number of point to write (usually full table)
@param data			Pointer to data to write. 
*/

void XDmaHexitec::writeSharedLUT(int chip, int region, int stream, int first, int num, uint32_t *data)
{
	volatile uint32_t *selAddr, *p, *chipSel;
	uint32_t *dptr;
	int firstChip, lastChip;
	int i;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("writeSharedLUT: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	if (!((region >= HEXITEC_REGION_EDGE_POS_RECIP && region <= HEXITEC_REGION_ENG_MAP) || (region >= HEXITEC_REGION_L_POS_RECIP && region <= HEXITEC_REGION_L_POS_C)))
		throw XDmaHexitecException("writeSharedLUT: region=%d, is not a shared lookup table", region);

	if (first < 0 || first >= regionSize[region] || num < 1 || first+num > regionSize[region])
		throw XDmaHexitecException("writeSharedLUT: first=%d, num=%d out of range 0...%d", first, num, regionSize[region]);
		
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			p = m_regs+HEXITEC_REGION_OFFSET*region+first;
		}
		else
		{
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
			p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region+first;
		}
		if (stream < 0)
			*selAddr = HEXITEC_SEL_ADDR_BROADCAST;
		else
			*selAddr = stream;
		dptr = data;
		for (i=0; i<num; i++)
			*p++ = *dptr++;
	}
}

/**
	Read array of values from a Hexitec Charge Sharing correction LUTs. Currently shared across all pixels.

@param chip			Chip number.
@param region		Region number, {@link HEXITEC_REGION_EDGE_POS_RECIP} to {@link HEXITEC_REGION_ENG_MAP}
@param stream		Processing stream (which handles 2 pairs of columns (total 4 columns).
@param first		First offset within LUT, usually 0,
@param num			Number of point to write (usually full table)
@param data			Pointer to receive data. 
*/

void XDmaHexitec::readSharedLUT(int chip, int region, int stream, int first, int num, uint32_t *data)
{
	volatile uint32_t *selAddr, *p, *chipSel;
	int i;

	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("readSharedLUT: chip=%d out of range 0...%d", chip, m_numChips-1);

	if (!((region >= HEXITEC_REGION_EDGE_POS_RECIP && region <= HEXITEC_REGION_ENG_MAP) || (region >= HEXITEC_REGION_L_POS_RECIP && region <= HEXITEC_REGION_L_POS_C)))
		throw XDmaHexitecException("readSharedLUT: region=%d, is not a shared lookup table", region);

	if (first < 0 || first >= regionSize[region] || num < 1 || first+num > regionSize[region])
		throw XDmaHexitecException("readSharedLUT: first=%d, num=%d out of range 0...%d", first, num, regionSize[region]);
		
	if (m_useChipSel)
	{
		chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
		*chipSel = chip;
		selAddr = m_regs;
		p = m_regs+HEXITEC_REGION_OFFSET*region+first;
	}
	else
	{
		selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
		p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*region+first;
	}

	*selAddr = stream;
		
	for (i=0; i<num; i++)
		*data++ = *p++;
}
/**
	Initialise  either of the 2 positive or the positive plus negative neighbour reciprocal LUTs
@param chip			Chip number  or -1 to duplicate to all chips.
@param region		Region number, {@link HEXITEC_REGION_EDGE_POS_RECIP} or {@link HEXITEC_REGION_NEG_NEB_RECIP}
@param stream		Processing stream (which handles 2 pairs of columns (total 4 columns). Usually -1 to replicate to all.
*/

void XDmaHexitec::initRecipLUT(int chip, int region, int stream)
{
	uint32_t data[HEXITEC_RECIP_SIZE];
	int i, recip;
	if (region != HEXITEC_REGION_EDGE_POS_RECIP && region != HEXITEC_REGION_NEG_NEB_RECIP && region != HEXITEC_REGION_L_POS_RECIP)
		throw XDmaHexitecException("initRecipLUT: region=%d, is not a reciprocal lookup table", region);
		
	for (i=0; i<HEXITEC_RECIP_SIZE; i++)
	{
		if (i <= 1)
			recip = 0x1FFFFFF;
		else
			recip = (0x2000000+i/2)/i;
		data[i] = recip;
	}
	writeSharedLUT(chip, region, stream, 0, HEXITEC_RECIP_SIZE, data);
}

/**
	Initialise all charge sharing correction LUTs to 0

@param chip			Chip number  or -1 to duplicate to all chips. 
@param stream		Processing stream (which handles 2 pairs of columns (total 4 columns). Usually -1 to replicate to all.
@param first		First offset within LUT, usually 0,
@param num			Number of point to write (usually full table)
@param value		Value to write
*/

void XDmaHexitec::initCShareLUTs(int chip, int stream)
{
	setSharedLUT(chip, HEXITEC_REGION_EDGE_POS_M, stream, 0, HEXITEC_EDGE_POS_SIZE, 0);
	setSharedLUT(chip, HEXITEC_REGION_EDGE_POS_C, stream, 0, HEXITEC_EDGE_POS_SIZE, 0);
	setSharedLUT(chip, HEXITEC_REGION_NEG_NEB_M, stream, 0, HEXITEC_NEG_NEB_SIZE, 0);
	setSharedLUT(chip, HEXITEC_REGION_NEG_NEB_C, stream, 0, HEXITEC_NEG_NEB_SIZE, 0);
	setSharedLUT(chip, HEXITEC_REGION_L_POS_M, stream, 0, HEXITEC_L_POS_SIZE, 0);
	setSharedLUT(chip, HEXITEC_REGION_L_POS_C, stream, 0, HEXITEC_L_POS_SIZE, 0);
}




	
/**
	Set the default XDma channel number to used to access the HBM histogram when the value -1 is passed to the function.

@param dmaChan		XDma channnel number
**/

void XDmaHexitec::setDefaultXDmaChan(int dmaChan)
{
	m_defaultXDmaChan = dmaChan;
}
	
void XDmaHexitec::setDmaDescRWChan(int dmaChan)
{
	m_dmaDescRWChan = dmaChan;
}

void XDmaHexitec::setPbNumFrames(int chip, uint32_t pbNumFrames)
{
	setChipReg(chip, HEXITEC_CHIP_NUM_PB, pbNumFrames);
}

uint32_t XDmaHexitec::getPbNumFrames(int chip)
{
	return getChipReg(chip, HEXITEC_CHIP_NUM_PB);
}


void XDmaHexitec::startDataMoverStream(int timeFrame, enum MappedView mappedView, bool sixteenBit, bool sumChips, enum AutonomousMode autoMode)
{
	volatile uint32_t *ptr;
	volatile uint8_t * p8;
	uint32_t readoutMode=0;
	ptr = m_dataMoverRegs + HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+m_xdma->getStQid()*8;

	switch (mappedView)
	{
	case MappedViewSpectra:
		readoutMode = HEXITEC_DM0_MV_FULL;
		break;

	case MappedViewMapped8:
		readoutMode = HEXITEC_DM0_MV_MAPPED8;
		break;
		
	case MappedViewMapped16:
		readoutMode = HEXITEC_DM0_MV_MAPPED16;
		break;
	}

	switch (autoMode)
	{
	case AutoTriggerRead: readoutMode |= HEXITEC_DM0_AUTO_TF; break;
	case AutoTriggerReadAndClear:readoutMode |= HEXITEC_DM0_AUTO_TF | HEXITEC_DM0_AUTO_TF_CLEAR; break;
	}

	if (sixteenBit)
		readoutMode |= HEXITEC_DM0_16BIT;
	if (sumChips)
		readoutMode |= HEXITEC_DM0_SUM_CHIPS;
	ptr[0] = readoutMode;
	// ptr[1] is read credit. It is cleared by SW at start and then only updated by FW from commands from QDMA.
	p8 = (uint8_t *)(ptr+2);   // LSByte of ptr[2] is alos readcredit, so do not touch.
	p8[1] = HEXITEC_DM2BYTE1_TF0(timeFrame);
	p8[2] = HEXITEC_DM2BYTE2_TF1(timeFrame);
	p8[3] = HEXITEC_DM2BYTE3_TF2(timeFrame);
	ptr[3] = HEXITEC_DM3_TF3(timeFrame) | HEXITEC_DM3_RUN;
}
/**
	Wait for the data mover to have finished output the specified number of frames. 
	The function determines whether the queue is working in autonomous mode. If so it wait for the time frame to reach numTF-1. 
	If it is in non-autonomous mode, software triggers, it waits for the Run bit to drop.
@param numTF-1		Number of time frames to have finished. 	
@param qid			Queue id to be started.
*/
void XDmaHexitec::waitDataMoverFinished(int64_t numTF, int qid)
{
	volatile uint32_t *ptr;
	int64_t tf;
	int numMatch;
	int timeout=0;
	ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+qid*8;
	if (*ptr & HEXITEC_DM0_AUTO_TF)
	{
		numMatch = 0;
		do 
		{
			tf = ptr[2] >> 8;
			tf |= static_cast<int64_t>((ptr[3] & 0xfff) << 24);
			printf("waitDataMoverFinished: quid=%d, current TF=%ld\n", qid, tf);
			if (tf == numTF-1)
				numMatch++;
			else
				numMatch=0;
			if (numMatch >= 2)
				break;
			this_thread::sleep_for(chrono::milliseconds(10));
		} while (timeout++ < 1000);
		if (timeout == 1000)
			throw XDmaHexitecException("waitDataMoverFinished: Timeout waiting DataMover queue %d to finish %ld frames, Last polled=%ld", qid, numTF, tf);
	}
	else
	{
		do 
		{
			if (ptr[3] & HEXITEC_DM3_RUN)
				break;
			this_thread::sleep_for(chrono::milliseconds(10));
		} while (timeout++ < 1000);
		if (timeout == 1000)
			throw XDmaHexitecException("waitDataMoverFinished: Timeout waiting DataMover queue %d to finish %ld frames", qid, numTF);
	}
}


/**
	Start the datamover to output frames via the 100 G Ethernet UDP interface.
	The data mover can either be triggerted to output each time frame by software using this function with autoMode=AutoOff, 
	or can be armed to trigger when the flushed time frame token advances in the firmware, normaly with autoMode=AutoTriggerReadAndClear or autoMode=AutoTriggerRead for debug.
	If the system is configured with mappeMode==HEXITEC_HIST_MAPPED_MODE_INTL, then separate Queues are setup to transmit the full spectra and mapped spectra to separate UDP ports.

@param tfExt		Time frame to send when sending individual time frame. Last time frame sent (new frames are sent)  -1L to start from frame 0.
@param mappedView	Specifiy whether this queue send the full specrate, 16 bin mapped spectra or the first 8 mins of mapped spectra
@param sixteenBit	The firmware reads the 32 bit values but limit the range at 65535and sends as 16 bit data.
@param sumChips		When data for all chips when running in EngOnly Modes for e.g. Hexitec 6x2.
@param qid			Queue id to be started.
@param farmMask		Mask (typically 0, 1, 3 or 7) to be used to select the bottom 0, 1, 2 or 3 bits of the index to create the UDP core farm mode address.
@param farmBase		First address in the UDP core  fram LUT which is ORed with the maksed index to form the complete LUT address.
@param autoMode		Specifies whether the specified frame is sent now autoMode=AutoOff or whether autonomous triggering is enabled autoMode=AutoTriggerReadAndClear or autoMode=AutoTriggerRead
@param farmIndexMode Specifies what index is used to crease the UDP core farm LUT address.  This can be from the time frame or can incremetn each packet. See FarmIndexMode.
*/

int XDmaHexitec::startDataMoverStreamUDP(int64_t tfExt, enum MappedView mappedView, bool sixteenBit, bool sumChips, int qid, int farmMask, int farmBase, enum AutonomousMode autoMode, enum FarmIndexMode farmIndexMode)
{
	volatile uint32_t *ptr;
	volatile uint8_t * p8;
	uint32_t readoutMode=HEXITEC_DM0_UDP_MODE;
	int farmIndex;
	if (qid == m_xdma->getStQid())
		throw XDmaHexitecException("startDataMoverStreamUDP: Do not use QDMA stream QueueId=%d for UDP output at qid=%d", m_xdma->getStQid(), qid);

	readoutMode |= HEXITEC_DM0_FARM_MASK(farmMask) | HEXITEC_DM0_FARM_BASE(farmBase);

	ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+qid*8;
	switch (mappedView)
	{
	case MappedViewSpectra:
		readoutMode |= HEXITEC_DM0_MV_FULL;
		break;

	case MappedViewMapped8:
		readoutMode |= HEXITEC_DM0_MV_MAPPED8;
		break;
		
	case MappedViewMapped16:
		readoutMode |= HEXITEC_DM0_MV_MAPPED16;
		break;
	}

	switch (farmIndexMode)
	{
	case FarmIndexIncEOF:
		readoutMode |= HEXITEC_DM0_FARM_INC_EOF;
		break;
	case FarmIndexIncEOP: 
		readoutMode |= HEXITEC_DM0_FARM_INC_EOP;
		break;
	case FarmIndexFromTF:
		readoutMode |= HEXITEC_DM0_FARM_FROM_TF;
		break;
	default:
		throw XDmaHexitecException("startDataMoverStreamUDP: Unknown farmIndexMode=%d", farmIndexMode);
	}
	switch (autoMode)
	{
	case AutoOff:
		break;
	case AutoTriggerRead: 
		readoutMode |= HEXITEC_DM0_AUTO_TF; 
		break;
	case AutoTriggerReadAndClear:
		readoutMode |= HEXITEC_DM0_AUTO_TF | HEXITEC_DM0_AUTO_TF_CLEAR; 
		break;
	}
	if (sixteenBit)
		readoutMode |= HEXITEC_DM0_16BIT;
	if (sumChips)
		readoutMode |= HEXITEC_DM0_SUM_CHIPS;

//	printf("startDataMoverStreamUDP: setting readoutMode=%08X\n", readoutMode);

	ptr[0] = readoutMode;
	// ptr[1] is read credit. It is cleared by SW at start and then only updated by FW from commands from QDMA.
	p8 = (uint8_t *)(ptr+2);   // LSByte of ptr[2] is also readcredit, so do not touch.
	p8[1] = HEXITEC_DM2BYTE1_TF0(tfExt);
	p8[2] = HEXITEC_DM2BYTE2_TF1(tfExt);
	p8[3] = HEXITEC_DM2BYTE3_TF2(tfExt);
	
	ptr[4] = 0;
	ptr[5] = 0;
	ptr[6] = 0x100;
	ptr[7] = 0;
#if 0
	p8 = (uint8_t *)(ptr+7);
	p8[0] = 0; // Force Farm index counter back to 0 each time we restart UDP output
#endif
	farmIndex = ptr[7] & 0xFF;

	ptr[3] = HEXITEC_DM3_TF3(tfExt) | HEXITEC_DM3_RUN;
	printf("startDataMoverStreamUDP: quid=%d, Wrote ptr[3]=0x%08X, read 0x%08X\n", qid, HEXITEC_DM3_TF3(tfExt) | HEXITEC_DM3_RUN, ptr[3]);
	return farmIndex;
}

void XDmaHexitec::stopDataMoverStreamUDP(int qid)
{
	volatile uint32_t *ptr;
	volatile uint8_t * p8;
	int timeout=0;
	if (qid == m_xdma->getStQid())
		throw XDmaHexitecException("stopDataMoverStreamUDP: Do not use QDMA stream QueueId=%d for UDP output at qid=%d", m_xdma->getStQid(), qid);

	ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+qid*8;
	if (m_debug >= 2)
		printf ("stopDataMoverStreamUDP: qid=%d, Start read ptr[3]=0x%08X, run=%d\n", qid, ptr[3], !!(ptr[3] & HEXITEC_DM3_RUN));
	ptr[0] |= HEXITEC_DM0_AUTO_STOP;
	if (m_debug >= 2)
		printf ("stopDataMoverStreamUDP: qid=%d, Set ptr[0] = 0x%08X\n", qid, ptr[0]);
	do
	{
		if (m_debug >= 2)
			printf ("stopDataMoverStreamUDP: qid=%d, loop=%d, read ptr[3]=0x%08X, run=%d\n", qid, timeout, ptr[3], !!(ptr[3] & HEXITEC_DM3_RUN));
		if (!(ptr[3] & HEXITEC_DM3_RUN))
			break;
		this_thread::sleep_for (chrono::milliseconds(1));
	} while (++timeout < 100);
	if (timeout >= 100)
		printf("stopDataMoverStreamUDP: timeout waiting for data mover to stop, forcing run to 0\n");
	ptr[3] = 0;
	ptr[0] &= ~HEXITEC_DM0_AUTO_STOP;
}
void XDmaHexitec::disableDataMoverUDPTrailer(bool disable, int packetShift)
{
	uint32_t x;
	x = m_dataMoverRegs[HEXITEC_DATA_MOVER_CONTROL];
	x &= ~HEXITEC_DMC_DISABLE_TRAILER & ~HEXITEC_DMC_SET_PACKET_SHIFT(0xFFFF);
	if (disable)
	{
		x |= HEXITEC_DMC_DISABLE_TRAILER;
		x |= HEXITEC_DMC_SET_PACKET_SHIFT(packetShift);
	}
	m_dataMoverRegs[HEXITEC_DATA_MOVER_CONTROL] = x;
}		

void XDmaHexitec::startDataMoverEvList(int numQueues)
{
	int i, qid;
	volatile uint32_t *ptr;
	uint32_t readoutMode=HEXITEC_DM0_EVLIST;

	for (i=0; i<numQueues; i++)
	{
		qid = HEXITEC_EVLIST_Q_BASE+i;
		ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+qid*8;
		
		ptr[0] = readoutMode;
		ptr[3] = qid | HEXITEC_DM3_RUN; // Current plan is to use Queues 128 onwards (<255) for event list to QDMA and also Farm Mode LUT 128....25 for Event list UDP.
										// The Qid and farm lut destination number can be different as the TimeFrame field is reused and checked for a match.
	}
}

int XDmaHexitec::getDataMoverUDPIndex(int qid)
{
	volatile uint32_t *ptr;
	
	if (qid == m_xdma->getStQid())
		throw XDmaHexitecException("getDataMoverUDPIndex: Do not use QDMA stream QueueId=%d for UDP output at qid=%d", m_xdma->getStQid(), qid);

	ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+qid*8;
	return ptr[7] & 0xFF;
}

void XDmaHexitec::readDataMoverStream(DataMoverContext *dm, int qid)
{
	volatile uint32_t *ptr;
	uint32_t x;
	
	if (qid <0)
		qid = m_xdma->getStQid();
	
	ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+qid*8;
	for (int i=0; i<8; i++)
		dm->raw[i] = ptr[i];
		
	dm->readCredit = ptr[1] | (uint64_t)(ptr[2] & 0xFF)<<32;
	x= ptr[0];
	dm->sixteenBitMode = x & HEXITEC_DM0_16BIT;
	dm->mappeView = HEXITEC_DM0_GET_MV(x);
	dm->sumChips  = !!(x & HEXITEC_DM0_SUM_CHIPS);
	dm->tfMode = HEXITEC_DM0_GET_TF_MODE(x);
	dm->farmIndexMode = HEXITEC_DM0_GET_FARM_INDEX(x);
	dm->farmMask = HEXITEC_DM0_GET_FARM_MASK(x);
	dm->farmBase = HEXITEC_DM0_GET_FARM_BASE(x);
	x = ptr[3];
	dm->timeFrame = HEXITEC_DM2_3_GET_TF(ptr[2], x);
	dm->run =  !!(x & HEXITEC_DM3_RUN);
	x = ptr[4];
	dm->pixelColEng = x & 0xFFFF;
	x = ptr[5];
	dm->pixelRow = x & 0x7F;
	dm->chipCol = (x>>8) & 0xFF; // Select all 8 bits to allow for future expansion?
	dm->chipRow = (x>>16) & 0xFF; // Select all 8 bits to allow for future expansion?
	x = ptr[6];
	dm->packetIndex = (x >> 8) & 0xFFFF;
	
}

void XDmaHexitec::initDataMover()
{
	volatile uint32_t *base = m_dataMoverRegs, *ptr;
	int i;

	if (base[HEXITEC_DATA_MOVER_CONTROL] & HEXITEC_DMC_ENABLE)
	{
		// Data mover is enabled, overwriting the tables may crash the data_mover_cmds FSM.
#if 1
		for (i=0; i<HEXITEC_DM_NUM_QUEUES; i++)
		{
			ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+i*8;
			ptr[0] |= HEXITEC_DM0_AUTO_STOP;
		}
#else
		base[HEXITEC_DATA_MOVER_CONTROL] |= HEXITEC_DMC_STOP_REQUEST;
#endif
		int running = 0;
		for( int timeOut=0; timeOut<200; timeOut++)
		{
			running = 0;
			for (i=0; i<HEXITEC_DM_NUM_QUEUES; i++)
			{
				ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+i*8;
				running |= ptr[3] & HEXITEC_DM3_RUN;
			}
			if (!running)
				break;
			this_thread::sleep_for (chrono::milliseconds(1));
		}
		if (running)
		{
			for (i=0; i<HEXITEC_DM_NUM_QUEUES; i++)
			{
				ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+i*8;
				if (ptr[3] & HEXITEC_DM3_RUN)
					printf("WARNING: Could not politely stop data mover queue %d\n", i);
			}
		}
		base[HEXITEC_DATA_MOVER_CONTROL] = 0;

	}
	ptr = base + HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t);
	for (i=0; i<HEXITEC_DM_NUM_QUEUES; i++)
	{
		*ptr++ = 0;		// bit 0 read credit
		*ptr++ = 0;		// bit 32 read credit
		*ptr++ = 0;		// bit 64 16 bit mode etc
		*ptr++ = 0;		// bit 96 Time frame
		*ptr++ = 0;		// bit 128 Data mover indexes, pixelColumn and energy
		*ptr++ = 0;		// bit 160 PixelRow
		*ptr++ = 0x100;	// Bit 192 Packet index at bit 200 .. Start at packet index 1
		*ptr++ = 0;		// Bit 224 Spare.
	}
	*base = HEXITEC_DMC_ENABLE;	// Start data mover so that any credit issued is accumulated.
	base[HEXITEC_DATA_MOVER_OVERRUN_CONT] = HEXITEC_DM_ORC_CLEAR;
	base[HEXITEC_DATA_MOVER_OVERRUN_CONT] = 0;
}

void XDmaHexitec::clearDataMoverOverRun()
{
	m_dataMoverRegs[HEXITEC_DATA_MOVER_OVERRUN_CONT] = HEXITEC_DM_ORC_CLEAR;
	m_dataMoverRegs[HEXITEC_DATA_MOVER_OVERRUN_CONT] = 0;
}
uint32_t XDmaHexitec::getDataMoverStatus()
{
	return m_dataMoverRegs[HEXITEC_DATA_MOVER_OVERRUN_STAT] ;
}

uint32_t XDmaHexitec::getDataMoverOverRun()
{
	return m_dataMoverRegs[HEXITEC_DATA_MOVER_OVERRUN_STAT] & ( HEXITEC_DM_STAT_OVERRUN_SPECTRA | HEXITEC_DM_STAT_OVERRUN_MAPPED) ;
}

uint64_t XDmaHexitec::getFlushedFrame()
{
	return getGlobReg64(HEXITEC_GLB_FLUSHED_FRAME);
}

/**
	Read 1 pair of per chips diagnostic counters.
	These counter count the number of detector frames processed and the raw number of hit (so split hits count as 2, 3 or 4) received since the start of the run.

@param chip			Chip number.
@param frameCount	Pointer to return the frame counter.
@param rawHitCount	Pointer to return the raw hit counter.
*/
void XDmaHexitec::getDiagnosticCounters(int chip, uint32_t *frameCount, uint32_t *rawHitCount)
{
	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("getDiagnosticCounters: chip=%d out of range 0...%d", chip, m_numChips-1);
	*frameCount = getGlobReg(HEXITEC_GLB_FRAME_COUNT0+2*chip);
	*rawHitCount = getGlobReg(HEXITEC_GLB_RAW_HIT_COUNT0+2*chip);
}

/**
	Read per chips diagnostic counters for all chips.
	These counter count the number of detector frames processed and the raw number of hit (so split hits count as 2, 3 or 4) received since the start of the run.

@param frameCount	Pointer array of size at least m_numChips or HEXITEC_MAX_CHIPS to return the frame counters.
@param rawHitCount	Pointer array of size at least m_numChips or HEXITEC_MAX_CHIPS to return the raw hit counters.
*/
void XDmaHexitec::getDiagnosticCounters(uint32_t *frameCount, uint32_t *rawHitCount)
{
	int chip;
	for (chip=0; chip< m_numChips; chip++)
	{
		*frameCount++ = getGlobReg(HEXITEC_GLB_FRAME_COUNT0+2*chip);
		*rawHitCount++ = getGlobReg(HEXITEC_GLB_RAW_HIT_COUNT0+2*chip);
	}
}

/**
	Write to the IRQ enable register.

@param irqEnb		New value to be written into the IRQ enable register.
*/
void XDmaHexitec::writeIrqEnable(uint32_t irqEnb)
{
	setGlobReg(HEXITEC_GLB_IRQ_ENB_RW, irqEnb);
}
/**
	Read the IRQ enable register.
@return Returns the IRQ Enable register
*/
uint32_t XDmaHexitec::getIrqEnable()
{
	return getGlobReg(HEXITEC_GLB_IRQ_ENB_RW);
}

/**
	Enable (set) one or more IRQ enables within the IRQ enable register.
	Performs irqEnb = irqEnb | irqSet;
	The firmware provides the read OR write action to make this operation atomic, thread and interrupt safe.

@param irqSet		Additional irq bits to be ORed into the IRQ enable register.
*/
void XDmaHexitec::setIrqEnable(uint32_t irqSet)
{
	setGlobReg(HEXITEC_GLB_IRQ_ENB_SET, irqSet);
}
/**
	Disable (clear) one or more IRQ enables within the IRQ enable register.
	Any bit set to 1 in the irqClr mask are cleared by the firmware in the IRW enable register.
	Performs irqEnb = irqEnb & ~irqClr
	The firmware provides the read AND write action to make this operation atomic, thread and interrupt safe.

@param irqClr		Bit mask of irq bits to be disabled by ANDing with the not of this.
*/
void XDmaHexitec::clearIrqEnable(uint32_t irqClr)
{
	setGlobReg(HEXITEC_GLB_IRQ_ENB_CLR, irqClr);
}

int XDmaHexitec::getEventFd(int irqNum)
{
	if (irqNum < 0 || irqNum >= HEXITEC_NUM_IRQS)
		throw XDmaException("XDmaHexitec::getEventFd: irqNum=%d is out of range 0...%d", irqNum, HEXITEC_NUM_IRQS-1);
	return m_xdma->getEventFd(irqNum);
}

int64_t XDmaHexitec::getOneFIFOCounts(int chip, int procCol, int offset)
{
	volatile uint32_t *p, *chipSel;
	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("readChipRegs: chip=%d out of range 0...%d", chip, m_numChips-1);
	if (m_useChipSel)
	{
		chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
		*chipSel = chip;
		p = m_regs+(offset+procCol*32)*2+HEXITEC_REGION_OFFSET*HEXITEC_REGION_FIFO_COUNTS;
	}
	else
	{
		p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+(offset+procCol*32)*2+HEXITEC_REGION_OFFSET*HEXITEC_REGION_FIFO_COUNTS;
	}
	return *reinterpret_cast<volatile int64_t *>(p);
}
		
HexitecFIFOCounts XDmaHexitec::getAllFIFOCounts(int chip)
{
	HexitecFIFOCounts counts;
	chip=0;
	int procCol;
	for (procCol=0; procCol<m_numProcCol; procCol++)
	{
		counts.totalDirectDroppedWrite += counts.directDroppedWrite[procCol] = getOneFIFOCounts(chip, procCol, 0);
		counts.totalNebDroppedWrite += counts.nebDroppedWrite[procCol] = getOneFIFOCounts(chip, procCol, 1);
		counts.totalMappedDroppedWrite += counts.mappedDroppedWrite[procCol] = getOneFIFOCounts(chip, procCol, 2);
		counts.totalNebMappedDroppedWrite += counts.nebMappedDroppedWrite[procCol] = getOneFIFOCounts(chip, procCol, 3);
		counts.totalDirectStalledRead += counts.directStalledRead[procCol] = getOneFIFOCounts(chip, procCol, 4);
		counts.totalNebStalledRead += counts.nebStalledRead[procCol] = getOneFIFOCounts(chip, procCol, 5);
		counts.totalMappedStalledRead += counts.mappedStalledRead[procCol] = getOneFIFOCounts(chip, procCol, 6);
		counts.totalNebMappedStalledRead += counts.nebMappedStalledRead[procCol] = getOneFIFOCounts(chip, procCol, 7);
		counts.totalHistFIFOProgFull += counts.histFIFOProgFull[procCol] = getOneFIFOCounts(chip, procCol, 8);
		counts.totalHistAlmostFullA += counts.histAlmostFullA[procCol] = getOneFIFOCounts(chip, procCol, 9);
		counts.totalForceFlushBC += counts.forceFlushBC[procCol] = getOneFIFOCounts(chip, procCol, 10);
		counts.totalHistFIFOWrite += counts.histFIFOWrite[procCol] = getOneFIFOCounts(chip, procCol, 11);
		counts.totalHistFIFORead += counts.histFIFORead[procCol] = getOneFIFOCounts(chip, procCol, 12);

		counts.totalDirectWrite += counts.directWrite[procCol] = getOneFIFOCounts(chip, procCol, 13);
		counts.totalNebWrite += counts.nebWrite[procCol] = getOneFIFOCounts(chip, procCol, 14);
		counts.totalMappedWrite += counts.mappedWrite[procCol] = getOneFIFOCounts(chip, procCol, 15);
		counts.totalNebMappedWrite += counts.nebMappedWrite[procCol] = getOneFIFOCounts(chip, procCol, 16);
	}
	return counts;
};

uint64_t XDmaHexitec::getInpTimeFrame(int chip)
{
	volatile uint32_t *p, *chipSel;
	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("getInpTimeFrame: chip=%d out of range 0...%d", chip, m_numChips-1);
	return getGlobReg64(HEXITEC_GLB_INP_TIME_FRAME0+2*chip);
}
