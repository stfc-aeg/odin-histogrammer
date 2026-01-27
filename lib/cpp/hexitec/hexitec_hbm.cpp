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

/* Memory layout */

void XDmaHexitec::readHistEngRowColTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder)
{
	readHistEngRowColTimeInt(numEng, firstRow, numRows, firstCol, numCols, 0, 1, firstTF, numTF, chip, dmaChan, data, false, false,useReorder);
}
	
void XDmaHexitec::readMappedEngRowColTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder)
{
	readHistEngRowColTimeInt(numEng, firstRow, numRows, firstCol, numCols, 0, 1, firstTF, numTF, chip, dmaChan, data, true, false, useReorder);
}

void XDmaHexitec::readHistEngColRowTime(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder)
{
	readHistEngColRowTimeInt(numEng, firstCol, numCols, firstRow, numRows, 0, 1, firstTF, numTF, chip, dmaChan, data, false, false, useReorder);
}

void XDmaHexitec::readMappedEngColRowTime(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder)
{
	readHistEngColRowTimeInt(numEng, firstCol, numCols, firstRow, numRows, 0, 1, firstTF, numTF, chip, dmaChan, data, true, false, useReorder);
}

void XDmaHexitec::readHistEngRowColCCTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder)
{
	readHistEngRowColTimeInt(numEng, firstRow, numRows, firstCol, numCols, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, false, true, useReorder);
}
	
void XDmaHexitec::readMappedEngRowColCCTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder)
{
	readHistEngRowColTimeInt(numEng, firstRow, numRows, firstCol, numCols, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, true, true, useReorder);
}

void XDmaHexitec::readHistEngColRowCCTime(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder)
{
	readHistEngColRowTimeInt(numEng, firstCol, numCols, firstRow, numRows, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, false, true, useReorder);
}

/* the read*EngGlobColRow function present the Hexitec 6x2 (and future other shapes) as a single larger 240x160 colxrow sensor addressed by a global column and global row
*/
void XDmaHexitec::readHistEngGlobColRowTime(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numGlobRows, int firstTF, int numTF, int dmaChan, uint32_t *data)
{
	readHistEngGlobColRowTimeInt(numEng, firstGlobCol, numGlobCols, firstGlobRow, numGlobRows, 0, 1, firstTF, numTF, dmaChan, data, false, false);
}
void XDmaHexitec::readHistEngGlobColRowCCTime(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numGlobRows, int firstCC, int numCC, int firstTF, int numTF, int dmaChan, uint32_t *data)
{
	readHistEngGlobColRowTimeInt(numEng, firstGlobCol, numGlobCols, firstGlobRow, numGlobRows, firstCC, numCC, firstTF, numTF, dmaChan, data, false, true);
}

void XDmaHexitec::readMappedEngGlobColRowTime(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numGlobRows, int firstTF, int numTF, int dmaChan, uint32_t *data)
{
	readHistEngGlobColRowTimeInt(numEng, firstGlobCol, numGlobCols, firstGlobRow, numGlobRows, 0, 1, firstTF, numTF, dmaChan, data, true, false);
}


void XDmaHexitec::readHistEngRowColTimeInt(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass, bool useReorder)
{
	uint32_t *buff=NULL, *sp, *dp, *colPtr; 
	int firstRowBot, firstRowTop;
	int lastRowBot, lastRowTop;
	int bufNumRows;
	int t, row, col, rowBot, rowTop;
	int it, ic, ir;
	uint64_t axiAddress, numBytes;
	int logicalPort, hbmPort;
	int stream;
	int numStreams = m_hbmHist.m_histConf.NumStreams;
	int srcnBinsEng;
	int maxNumTF;
	int maxNumCC;
	
	const char *funcName = mapped?"readMappedEngRowColTime":"readHistEngRowColTime";

	if (useClustClass)
		funcName = mapped?"readMappedEngRowColCCTime":"readHistEngRowColCCTime";

	if (chip < 0 || chip >= m_numChips)
		throw  XDmaHexitecException("%s: chip %d is not in range 0...%d", funcName, chip, m_numChips-1);
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;
	if (mapped)
	{
		srcnBinsEng = HEXITEC_NBINS_MAPPED;
		maxNumTF = m_dataFormat[chip].numTFMapped;
		maxNumCC = 1;
	}
	else
	{		
		srcnBinsEng = m_dataFormat[chip].nBinsEng;
		maxNumTF = m_dataFormat[chip].numTF;
		maxNumCC = m_dataFormat[chip].nBinsClustClass;
	}
	
	if (mapped && m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
		throw  XDmaHexitecException("%s: Attempt to read mapped energy, but mappedMode=%d ==OFF", funcName, m_dataFormat[chip].mappedMode);
		
	if (numEng < 1 || numEng > srcnBinsEng)
		throw  XDmaHexitecException("%s: numEng %d is not in range 1... %d", funcName, numEng, srcnBinsEng);
	if (firstRow <0 || firstRow >= HEXITEC_NUM_ROWS || numRows <1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw  XDmaHexitecException("%s: firstRow=%d, numRows=%d is not in range 0... %d", funcName, firstRow, numRows, HEXITEC_NUM_ROWS);
	if (firstCol <0 || firstCol >= HEXITEC_NUM_COLS || numCols <1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw  XDmaHexitecException("%s: firstCol=%d, numCols=%d is not in range 0... %d", funcName, firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstCC < 0 || firstCC > maxNumCC || numCC < 1 || firstCC+numCC > maxNumCC)
		throw  XDmaHexitecException("%s: firstCC=%d, numCC=%d is not in range 0... %d", funcName, firstCC, numCC, maxNumCC);
	if (firstTF < 0 || firstTF > maxNumTF || numTF < 1 || firstTF+numTF > maxNumTF)
		throw  XDmaHexitecException("%s: firstTF=%d, numTF=%d is not in range 0... %d", funcName, firstTF, numTF, maxNumTF);
	if (mapped && m_generation == HexitecGenHexitec && m_dataFormat[chip].mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF && 
		(m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY7)) 
	{
		// OK even though main read mode is energyOnly
	}
	else
	{
		if ((useClustClass && m_dataFormat[chip].memReadAccess != EngPosCCTime) || (!useClustClass && m_dataFormat[chip].memReadAccess != EngPosTime))
			throw  XDmaHexitecException("%s: Current memory layout %d, histMode=0x%04X, mapped=%d, useClusterClass=%d is not supported by this read function", funcName, 
					m_dataFormat[chip].memReadAccess,  m_dataFormat[chip].histMode, mapped, useClustClass);
	}
	if ((m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGROWCOL)&&  useReorder)
	{
		readHistEngRowColTimeReordered(numEng, firstRow, numRows, firstCol, numCols, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, mapped, useClustClass);
	}	
	else
	{
		if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::RowBankCol)
		{
			if (m_dataFormat[chip].histMode==HEXITEC_HIST_FORMAT_RUN12)
			{
				/* Slow software reorder hard coded for Nbits eng=12 for testing only (slow far?) */
				int  eh, ne;
				dp = data;
				for (it=0;it<numTF; it++)
				{
					for (int icc=0; icc<numCC; icc++)
					{
						t = (it+firstTF)*maxNumCC+icc+firstCC;
						for (ic=0; ic<numCols; ic++)
						{
							col = ic+firstCol;
							if (col >= 40)
								col+=24;
							hbmPort = m_hbmHist.getPhysicalPort((col>>1)&0x1f);
							for (ir=0; ir<numRows; ir++)
							{
								row = ir+firstRow;
								for (eh=0; eh <=(numEng-1)/256; eh++)
								{
									if (eh == (numEng-1)/256)
										ne = numEng-256*eh;
									else
										ne = 256;
									axiAddress = (col &1) << 10 | (col&64) << (11-6) | (row&3) << 12 | eh <<14;
									axiAddress |= (row & 0x7C) << (18-2);
									axiAddress |= t << 23;
									axiAddress |= ((uint64_t)hbmPort)<<m_hbmHist.m_histConf.NBitsAddrMem;
	//								printf ("eng=%d, row=%d, col=%d, t=%d => axiAddress=0x%010lX\n", eh*256, row, col, t, axiAddress);
									numBytes = ne << 2;
									m_hbmHist.readDma((char *)dp, axiAddress, numBytes, dmaChan);
									dp += ne;
								}
							}
						}
					}
				}
			}
			else
				throw XDmaHexitecException("%s: Cannot read RowBankCol without reorder block", funcName);
		}
		else if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::BankRowColBG0)
		{
			/* Separate the parts of the row used to in the address main hist address field and the bit (Row[1..0] used to select the stream */
			/* With LSB of strawm=Row0 at the bottom of the AXI word address, we read multiples of 2 rows at a time. */ 
			firstRowBot = firstRow % 4;
			firstRowTop = firstRow / 4;
			lastRowBot = (firstRow+numRows-1)%4;
			lastRowTop = (firstRow+numRows-1)/4;

			bufNumRows = 1+lastRowTop-firstRowTop;
			bufNumRows *= 2;
			posix_memalign((void **)&buff, 4096 /*alignment */ , bufNumRows*srcnBinsEng*sizeof(uint32_t));
			if (buff == nullptr)
				throw runtime_error("readHistEngRowColTime: Out of memory");
#if 0
			for (it=0; it<numTF; it++)
			{
				for (logicalPort=0; logicalPort<m_numHBMPorts; logicalPort++)
				{
					hbmPort = m_hbmHist.getPhysicalPort(logicalPort);
					for (stream=0; stream<numStreams/2; stream++)
					{
						col = logicalPort << 1;
						if (stream & 4)  col ++;
						if (stream & 2) col += 40;
						if (col < firstCol || col >=firstCol+numCols)
							continue;
						colPtr = data+it*numEng*numRows*numCols+(colfirstCol)*numEng*numRows;
											
						for (rowMid=0; rowMid<4; rowMid+=2)
						{
							axiAddress = (uint64_t)hbmPort<<m_hbmHist.m_histConf.NBitsAddrMem | ((uint64_t)stream<<(m_hbmHist.m_histConf.NBitsAddrIn+2));
							
							axiAddress += sizeof(uint32_t)*firstRowTop * srcnBinsEng;
							numBytes = sizeof(uint32_t)*(1+stopRow-startRow)*srcnBinsEng;
	//						printf("it=%d, hbmPort=%d, stream=%d, rowBot=%d => read from %010llX, numBytes=0x%010llX\n", it, hbmPort, stream, rowBot, axiAddress, numBytes);
	//						printf(".... startRow=%d, stopRow=%d, nBinsEng=%d => numBytes=0x%010llX\n", startRow, stopRow, srcnBinsEng, numBytes);
							m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
							row = rowBot+startRow*4-firstRow;
							sp = buff;
							dp = colPtr+row*numEng;
							for (rowTop=startRow; rowTop<=stopRow; rowTop++)
							{
								
								memcpy(dp, sp, sizeof(uint32_t)*numEng);
								sp += srcnBinsEng;
								dp += 4*numEng;
							}
							for (rowBot=0; rowBot<2; rowBot++)
							{
								int startRow, stopRow;
								if (rowBot < firstRowBot)
									startRow = firstRowTop+1;
								else
									startRow = firstRowTop;
								if (rowBot > lastRowBot)
									stopRow = lastRowTop-1;
								else
									stopRow = lastRowTop;
								if (stopRow < startRow)
									continue;
								
								axiAddress = (uint64_t)hbmPort<<m_hbmHist.m_histConf.NBitsAddrMem | ((uint64_t)stream<<(m_hbmHist.m_histConf.NBitsAddrIn+2));
						
								for (ir=0; ir<numRows; ir++) 
#endif	
		}
		else
		{
			firstRowBot = firstRow % 4;
			firstRowTop = firstRow / 4;
			lastRowBot = (firstRow+numRows-1)%4;
			lastRowTop = (firstRow+numRows-1)/4;

			bufNumRows = 1+lastRowTop-firstRowTop;
			posix_memalign((void **)&buff, 4096 /*alignment */ , bufNumRows*srcnBinsEng*sizeof(uint32_t));
			if (buff == nullptr)
				throw runtime_error("readHistEngRowColTime: Out of memory");
			for (it=0; it<numTF; it++)
			{
				for (int icc=0; icc<numCC; icc++)
				{
					t = (it+firstTF)*maxNumCC+icc+firstCC;
					for (logicalPort=0; logicalPort<m_numHBMPorts; logicalPort++)
					{
						hbmPort = m_hbmHist.getPhysicalPort(logicalPort);
						for (stream=0; stream<numStreams; stream++)
						{
							int startRow, stopRow;
							col = logicalPort << 1;
							if (stream & 1)  col ++;
							if (stream & 2) col += 40;
							if (col < firstCol || col >=firstCol+numCols)
								continue;
							colPtr = data+(it*numCC+icc)*numEng*numRows*numCols+(col-firstCol)*numEng*numRows;
							rowBot= stream>>2;
							if (rowBot < firstRowBot)
								startRow = firstRowTop+1;
							else
								startRow = firstRowTop;
							if (rowBot > lastRowBot)
								stopRow = lastRowTop-1;
							else
								stopRow = lastRowTop;
							if (stopRow < startRow)
								continue;
								
							axiAddress = (uint64_t)hbmPort<<m_hbmHist.m_histConf.NBitsAddrMem | ((uint64_t)stream<<(m_hbmHist.m_histConf.NBitsAddrIn+2));
							if (mapped && m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
							{
								int intlTF;
								intlTF = t;
								switch (m_dataFormat[chip].histMode)
								{
								case HEXITEC_HIST_FORMAT_RUN7:	intlTF = ((intlTF  &  1)  | ((intlTF &0x0FFFFFFE) << 2))+5; break;
								case HEXITEC_HIST_FORMAT_RUN8:	intlTF = ((intlTF  &  3)  | ((intlTF & 0x0FFFFFC) <<2))+(5<<1); break;
								case HEXITEC_HIST_FORMAT_RUN9:	intlTF = ((intlTF &   7)  | ((intlTF & 0x0FFFFF8) <<2))+(5<<2); break;
								case HEXITEC_HIST_FORMAT_RUN10:
								case HEXITEC_HIST_FORMAT_RUN10LSB:			
									intlTF = ((intlTF & 0xF)  | ((intlTF & 0x0FFFFF0) <<2))+(5<<3); break;
								case HEXITEC_HIST_FORMAT_RUN11:	intlTF = ((intlTF & 0x1F) | ((intlTF & 0x0FFFFE0) <<2))+(5<<4); break;
								case HEXITEC_HIST_FORMAT_RUN12:	intlTF = ((intlTF & 0x3F) | ((intlTF & 0x0FFFFC0) <<2))+(5<<5); break;
								}
								axiAddress += sizeof(uint32_t)*srcnBinsEng*32*intlTF;
		//						printf("firstRow=%d, numRows=%d, rowBot=%d => startRow=%d, stopRow=%d, dest row=%d\n", firstRow, numRows, rowBot, startRow, stopRow, row);
		//						printf("it=%d intlTF=0x%02X, hbmPort=%d, stream=%d, rowBot=%d => read from %010llX, numBytes=0x%010llX\n", it, intlTF, hbmPort, stream, rowBot, axiAddress, numBytes);
		//						printf(".... startRow=%d, stopRow=%d, nBinsEng=%d => numBytes=0x%010llX\n", startRow, stopRow, srcnBinsEng, numBytes);
							}
							else
								axiAddress += sizeof(uint32_t)*srcnBinsEng*32*(t);
							axiAddress += sizeof(uint32_t)*startRow * srcnBinsEng;
							numBytes = sizeof(uint32_t)*(1+stopRow-startRow)*srcnBinsEng;
		//					printf("tf=%d, readDma(0x%010lX,  0x%010lX)\n", t, axiAddress, numBytes); 
							m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
							row = rowBot+startRow*4-firstRow;
		//					if (firstRow==2 && numRows ==3)
		//					{
		//						printf("firstRow=%d, numRows=%d, rowBot=%d => startRow=%d, stopRow=%d, dest row=%d\n", firstRow, numRows, rowBot, startRow, stopRow, row);
		//						printf("it=%d, hbmPort=%d, stream=%d, rowBot=%d => read from %010llX, numBytes=0x%010llX\n", it, hbmPort, stream, rowBot, axiAddress, numBytes);
		//						printf(".... startRow=%d, stopRow=%d, nBinsEng=%d => numBytes=0x%010llX\n", startRow, stopRow, srcnBinsEng, numBytes);
		//					}
							sp = buff;
							dp = colPtr+row*numEng;
							for (rowTop=startRow; rowTop<=stopRow; rowTop++)
							{
								
								memcpy(dp, sp, sizeof(uint32_t)*numEng);
								sp += srcnBinsEng;
								dp += 4*numEng;
							}
						}
					}
				}
			}
			free (buff);
		}
	}
}
void XDmaHexitec::readHistEngTime(int numEng, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorderSumming)
{
	readHistEngTimeInt(numEng, 0, 1, firstTF, numTF, chip, dmaChan, data, false, useReorderSumming);
}
void XDmaHexitec::readHistEngCCTime(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorderSumming)
{
	readHistEngTimeInt(numEng, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, true, useReorderSumming);
}

void XDmaHexitec::readHistEngTimeInt(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass, bool useReorderSumming)
{
	uint32_t *buff=NULL, *sp, *dp;
	int it, eng;
	uint64_t axiAddress, numBytes;
	int logicalPort, hbmPort;
	int stream;
	int numStreams = m_hbmHist.m_histConf.NumStreams;
	int srcnBinsEng;
	int maxNumTF;
	const char *funcName = useClustClass?"readHistEngCCTime":"readHistEngTime";
	int formatChip=chip;
	int maxNumCC;
	
	if (chip < 0)
	{
		checkFormatMatch(funcName);
		formatChip = 0;
	}
	if (chip >= m_numChips)
		throw  XDmaHexitecException("%s: chip %d is not in range 0... %d", funcName, chip, m_numChips-1);
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;

	srcnBinsEng = m_dataFormat[formatChip].nBinsEng;
	maxNumTF = m_dataFormat[formatChip].numTF;
	maxNumCC = m_dataFormat[formatChip].nBinsClustClass;
	
	if (numEng < 1 || numEng > srcnBinsEng)
		throw  XDmaHexitecException("%s: numEng %d is not in range 1...%d", funcName, numEng, srcnBinsEng);
	if (firstCC < 0 || firstCC > maxNumCC || numCC < 1 || firstCC+numCC > maxNumCC)
		throw  XDmaHexitecException("%s: firstCC=%d, numCC=%d is not in range 0... %d", funcName, firstCC, numCC, maxNumCC);
	if (firstTF < 0 || firstTF > maxNumTF || numTF < 1 || firstTF+numTF > maxNumTF)
		throw  XDmaHexitecException("%s: firstTF=%d, numTF=%d is not in range 0... %d", funcName, firstTF, numTF, maxNumTF);
	if ((useClustClass && m_dataFormat[formatChip].memReadAccess != EngCCTime) || (!useClustClass && m_dataFormat[formatChip].memReadAccess != EngTime))
		throw  XDmaHexitecException("%s: Current memory layout %d, histFormat=0x%04X, useClusterClas=%d is not supported by this read function", funcName, 
			m_dataFormat[formatChip].memReadAccess, m_dataFormat[formatChip].histMode, useClustClass );
		
	if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::RowBankCol)
	{
		if ((m_hbmHist.m_axiReorder & HEXITEC_REORDER_SUMMED) && useReorderSumming)
//		if (m_generation == HexitecGenHexitec && (m_hbmHist.m_axiReorder & HEXITEC_REORDER_SUMMED) && useReorderSumming)
		{
//			if (m_generation == HexitecGenMHz)
//				readHistEngTimeSummedMHz(numEng, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, useClustClass);
//			else
			readHistEngTimeSummedHxt(numEng, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, useClustClass);
		}
		else if (m_hbmHist.m_axiReorder)
		{
			if (m_generation == HexitecGenMHz)
				readHistEngTimeReorderedMHz(numEng, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, useClustClass);
			else
				readHistEngTimeReorderedHxt(numEng, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, useClustClass);
		}
		else
			throw XDmaHexitecException("%s: Cannot read RowBankCol without reorder block", funcName);
	}
	else if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::BankRowColBG0)
	{
	}
	else
	{
		posix_memalign((void **)&buff, 4096 /*alignment */ , numTF*maxNumCC*srcnBinsEng*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngTime: Out of memory");
		memset(data, 0, sizeof(uint32_t)*numEng*numTF);
		for (logicalPort=0; logicalPort<m_numHBMPorts; logicalPort++)
		{
			hbmPort = m_hbmHist.getPhysicalPort(logicalPort);
			for (stream=0; stream<numStreams; stream++)
			{
				axiAddress = (uint64_t)hbmPort<<m_hbmHist.m_histConf.NBitsAddrMem | ((uint64_t)stream<<(m_hbmHist.m_histConf.NBitsAddrIn+2));
				axiAddress += sizeof(uint32_t)*srcnBinsEng*(firstTF*maxNumCC); // Read jump to start of Time frame (clusterClass=0)
				numBytes = sizeof(uint32_t)*numTF*maxNumCC*srcnBinsEng;  // Read all cluster class for specified number of time frames
				m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
				for (it=0; it<numTF; it++)
				{
					for (int icc=0; icc<numCC; icc++)
					{
						sp = buff+(it*maxNumCC+icc+firstCC)*srcnBinsEng; // jump to require cluster class (firstCC+icc) within time frame of all CC.
						dp = data+(it*numCC+icc)*numEng;
						for (eng=0; eng<numEng; eng++)
							*dp++ += *sp++;
					}
				}
			}
		}
		free (buff);
	}
}

void XDmaHexitec::readHistEngCalibClass(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int icc, eng;
	int startLutAddr, stopLutAddr;
	int enb, lutAddrBot, lutAddr;
	uint64_t axiAddress, numBytes;
	int logicalPort, hbmPort;
	int stream, firstStream, lastStream;
	int numStreams = m_hbmHist.m_histConf.NumStreams;
	int srcnBinsEng;
	int maxNumTF;
	const char *funcName = "readHistEngCalibClass";
	int formatChip=chip;
	
	if (chip < 0)
	{
		checkFormatMatch(funcName);
		formatChip = 0;
	}

	if (chip >= m_numChips)
		throw  XDmaHexitecException("%s: chip %d is not in range 0...%d", funcName, chip, m_numChips-1);
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;

	srcnBinsEng = m_dataFormat[formatChip].nBinsEng;
	maxNumTF = m_dataFormat[formatChip].numTF;

		
	if (firstEng<0 || firstEng >=srcnBinsEng || numEng < 1 || firstEng+numEng > srcnBinsEng)
		throw  XDmaHexitecException("%s: firstEng=%d, numEng=%d is not in range 1... %d", funcName, firstEng, numEng, srcnBinsEng);
	if (firstLutAddr <0 || firstLutAddr >= HEXITEC_RECIP_SIZE || numLutAddr <1 || firstLutAddr+numLutAddr > HEXITEC_RECIP_SIZE)
		throw  XDmaHexitecException("%s: firstLutAddr=%d, numLutAddr=%d is not in range 0... %d", funcName, firstLutAddr, numLutAddr, HEXITEC_RECIP_SIZE);
	if (firstClusterClass <0 || firstClusterClass >= HEXITEC_NUM_CLUSTER_CLASS || numClusterClass <1 || firstClusterClass+numClusterClass > HEXITEC_NUM_CLUSTER_CLASS)
		throw  XDmaHexitecException("%s: firstClusterClass=%d, numClusterClass=%d is not in range 0... %d", funcName, firstClusterClass, numClusterClass, HEXITEC_NUM_CLUSTER_CLASS);

	if (m_dataFormat[formatChip].memReadAccess != EngCalibCC)
		throw  XDmaHexitecException("%s: Current memory layout %d is not supported by this read function", funcName, m_dataFormat[formatChip].memReadAccess);
	if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::RowBankCol)
	{
		if (m_hbmHist.m_axiReorder> 0)
		{
			if (m_generation == HexitecGenMHz)
				readHistEngCalibClassSummedMHz(firstEng, numEng, firstLutAddr, numLutAddr, firstClusterClass, numClusterClass, chip, dmaChan, data);
			else
				readHistEngCalibClassReorderedHxt(firstEng, numEng, firstLutAddr, numLutAddr, firstClusterClass, numClusterClass, chip, dmaChan, data);
		}
		else
			throw XDmaHexitecException("%s: Cannot read RowBankCol without reorder block", funcName);
	}
	else if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::BankRowColBG0)
	{
		memset(data, 0, sizeof(uint32_t)*numEng*numLutAddr*numClusterClass);
	}
	else
	{
		memset(data, 0, sizeof(uint32_t)*numEng*numLutAddr*numClusterClass);
		posix_memalign((void **)&buff, 4096 /*alignment */ , 64*srcnBinsEng*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngRowColTime: Out of memory");
		for (icc=0; icc<numClusterClass; icc++)
		{
			for (logicalPort=0; logicalPort<m_numHBMPorts; logicalPort++)
			{
				hbmPort = m_hbmHist.getPhysicalPort(logicalPort);
				firstStream = firstLutAddr/64;
				lastStream = (firstLutAddr+numLutAddr-1)/64;
				
				for (stream=firstStream; stream<=lastStream; stream++)
				{
					axiAddress = (uint64_t)hbmPort<<m_hbmHist.m_histConf.NBitsAddrMem | ((uint64_t)stream<<(m_hbmHist.m_histConf.NBitsAddrIn+2));
					axiAddress += sizeof(uint32_t)*((icc+firstClusterClass)<<18);

					numBytes = sizeof(uint32_t)*64*srcnBinsEng;
					m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);

					if (stream==firstStream)
						startLutAddr = firstLutAddr % 64;
					else
						startLutAddr = 0;
					if (stream==lastStream)
						stopLutAddr = (firstLutAddr+numLutAddr-1) % 64;
					else
						stopLutAddr = 63;
					
					for (lutAddrBot=startLutAddr; lutAddrBot<=stopLutAddr; lutAddrBot++)
					{
						lutAddr = lutAddrBot+stream*64;
						dp = data+numLutAddr*numEng*icc;
						dp += (lutAddr-firstLutAddr)*numEng;
						sp = buff + srcnBinsEng*lutAddrBot+firstEng;
						for (eng=0; eng<numEng; eng++)
							*dp++ += *sp++;
					}
				}
			}
		}
		free (buff);
	}
}


void XDmaHexitec::readHistCharac2d(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int m, n, neb, nebAddr;
	int firstNebTop, lastNebTop;
	uint64_t axiAddress, numBytes;
	int logicalPort, hbmPort;
	int stream, firstStream, lastStream;
	int numStreams = m_hbmHist.m_histConf.NumStreams;
	int srcnBinsMain, srcnBinsNeb;
	int maxNumTF;
	int diagShift=21;
	const char *funcName = "readHistCharac2d";
	int formatChip=chip;
	
	if (chip < 0)
	{
		checkFormatMatch(funcName);
		formatChip = 0;
	}

	if ( chip >= m_numChips)
		throw  XDmaHexitecException("%s: chip %d is not in range 0...%d", funcName, chip, m_numChips-1);
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;

	srcnBinsMain = m_dataFormat[formatChip].nBins[0];
	srcnBinsNeb  = m_dataFormat[formatChip].nBins[1];
	maxNumTF = m_dataFormat[formatChip].numTF;
	if (m_dataFormat[formatChip].histMode == HEXITEC_HIST_FORMAT_CHARAC2D10)
		diagShift = 17;
		
	if (firstMain<0 || firstMain >=srcnBinsMain || numMain < 1 || firstMain+numMain > srcnBinsMain)
		throw  XDmaHexitecException("%s: firstMain=%d, numMain=%d is not in range 1... %d", funcName, firstMain, numMain, srcnBinsMain);
	if (firstNeb <0 || firstNeb >= srcnBinsNeb || numNeb <1 || firstNeb+numNeb > srcnBinsNeb)
		throw  XDmaHexitecException("%s: firstNeb=%d, numNeb=%d is not in range 0... %d", funcName, firstNeb, numNeb, srcnBinsNeb);
	if (diag <0 || diag >= 2 )
		throw  XDmaHexitecException("%s: diag=%d, is not in range 0..1", funcName, diag);

	if (m_dataFormat[formatChip].memReadAccess != Charac2d)
		throw  XDmaHexitecException("%s: Current memory layout %d is not supported by this read function", funcName, m_dataFormat[formatChip].memReadAccess);
	if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::RowBankCol)
	{
		if (m_hbmHist.m_axiReorder> 0)
		{
			if (m_generation == HexitecGenMHz)
				readHistCharac2dSummedMHz(firstMain, numMain, firstNeb, numNeb, diag, chip, dmaChan, data);
			else
				readHistCharac2dReorderedHxt(firstMain, numMain, firstNeb, numNeb, diag, chip, dmaChan, data);
		}
		else
			throw XDmaHexitecException("%s: Cannot read RowBankCol without reorder block", funcName);
	}
	else if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::BankRowColBG0)
	{
		memset(data, 0, sizeof(uint32_t)*numMain*numNeb);
	}
	else
	{
		memset(data, 0, sizeof(uint32_t)*numMain*numNeb);
		posix_memalign((void **)&buff, 4096 /*alignment */ , (srcnBinsNeb/16)*srcnBinsMain*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistCharac2d: Out of memory");
		for (logicalPort=0; logicalPort<m_numHBMPorts; logicalPort++)
		{
			hbmPort = m_hbmHist.getPhysicalPort(logicalPort);
			for (stream=0; stream<16; stream++)
			{
				axiAddress = (uint64_t)hbmPort<<m_hbmHist.m_histConf.NBitsAddrMem | ((uint64_t)stream<<(m_hbmHist.m_histConf.NBitsAddrIn+2));
				axiAddress += sizeof(uint32_t)*(diag<<diagShift);

				numBytes = sizeof(uint32_t)*(srcnBinsNeb/16)*srcnBinsMain;
				m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);

				firstNebTop = firstNeb/16;
				lastNebTop = (firstNeb+numNeb-1)/16;
				for (n=firstNebTop;n<=lastNebTop; n++)
				{
					neb = n*16+stream;
					if (neb < firstNeb || neb >= firstNeb+numNeb)
						continue;
					// Neighbour is signed, but for scatter plots want the negative in 0..4095 and positive in 4096...8191
					if (neb >= srcnBinsNeb/2)
						nebAddr = n-srcnBinsNeb/32;
					else
						nebAddr = n+srcnBinsNeb/32;

					dp = data+numMain*(neb-firstNeb);
					sp = buff + srcnBinsMain*nebAddr+firstMain;
					for (m=0; m<numMain; m++)
						*dp++ += *sp++;
				}
			}
		}
		free (buff);
	}
}

void XDmaHexitec::readHistCharac3d(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int cornAddr,  v, vertAddr;
	int firstCornTop, lastCornTop, cornTop, corner;
	int h, hozAddr;
	uint64_t axiAddress, numBytes;
	int logicalPort, hbmPort;
	int stream, firstStream, lastStream;
	int numStreams = m_hbmHist.m_histConf.NumStreams;
	int srcnBinsHoz, srcnBinsCorn, srcnBinsVert;
	int maxNumTF;
	const char *funcName = "readHistCharac3d";
	int formatChip=chip;
	
	if (chip < 0)
	{
		checkFormatMatch(funcName);
		formatChip = 0;
	}

	if (chip >= m_numChips)
		throw  XDmaHexitecException("%s: chip %d is not in range 0...%d", funcName, chip, m_numChips-1);
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;

	srcnBinsHoz = m_dataFormat[formatChip].nBins[0];
	srcnBinsVert= m_dataFormat[formatChip].nBins[1];
	srcnBinsCorn = m_dataFormat[formatChip].nBins[2];
	maxNumTF = m_dataFormat[formatChip].numTF;
	
		
	if (firstHoz<0 || firstHoz >=srcnBinsHoz || numHoz < 1 || firstHoz+numHoz > srcnBinsHoz)
		throw  XDmaHexitecException("%s: firstHoz=%d, numHoz=%d is not in range 1... %d", funcName, firstHoz, numHoz, srcnBinsHoz);
	if (firstCorn <0 || firstCorn >= srcnBinsCorn || numCorn <1 || firstCorn+numCorn > srcnBinsCorn)
		throw  XDmaHexitecException("%s: firstCorn=%d, numCorn=%d is not in range 0... %d", funcName, firstCorn, numCorn, srcnBinsCorn);
	if (firstVert <0 || firstVert >= srcnBinsVert || numVert <1 || firstVert+numVert > srcnBinsVert)
		throw  XDmaHexitecException("%s: firstVert=%d, numVert=%d is not in range 0... %d", funcName, firstVert, numVert, srcnBinsVert);

	if (m_dataFormat[formatChip].memReadAccess != Charac3d)
		throw  XDmaHexitecException("%s: Current memory layout %d is not supported by this read function", funcName, m_dataFormat[formatChip].memReadAccess);
	if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::RowBankCol)
	{
		if (m_hbmHist.m_axiReorder> 0)
		{
			if (m_generation == HexitecGenMHz)
				readHistCharac3dSummedMHz(firstHoz, numHoz, firstVert, numVert, firstCorn, numCorn, chip, dmaChan, data);
			else
				readHistCharac3dReorderedHxt(firstHoz, numHoz, firstVert, numVert, firstCorn, numCorn, chip, dmaChan, data);
		}	
		else
			throw XDmaHexitecException("%s: Cannot read RowBankCol without reorder block", funcName);
	}
	else if (m_hbmHist.m_histConf.BankPosn==HBMHistConfig::BankRowColBG0)
	{
		memset(data, 0, sizeof(uint32_t)*numHoz*numCorn*numVert);
	}
	else
	{
		memset(data, 0, sizeof(uint32_t)*numHoz*numCorn*numVert);
		posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsHoz*srcnBinsVert*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistCharac2d: Out of memory");
		for (logicalPort=0; logicalPort<m_numHBMPorts; logicalPort++)
		{
			hbmPort = m_hbmHist.getPhysicalPort(logicalPort);
			for (stream=0; stream<16; stream++)
			{
				firstCornTop = firstCorn/16;
				lastCornTop = (firstCorn+numCorn-1)/16;
				for (cornTop=firstCornTop;cornTop<=lastCornTop; cornTop++)
				{
					corner = cornTop*16+stream;
					if (corner < firstCorn || corner >= firstCorn+numCorn)
						continue;
					// All are signed, Neighbour is signed, but for scatter plots want the negative in 0..255 and positive in 256...511
					cornAddr = cornTop ^ (1 << 4);
					axiAddress = (uint64_t)hbmPort<<m_hbmHist.m_histConf.NBitsAddrMem | ((uint64_t)stream<<(m_hbmHist.m_histConf.NBitsAddrIn+2));
					axiAddress += cornAddr << (17+2);
					numBytes = sizeof(uint32_t)*srcnBinsHoz*srcnBinsVert;
					m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
					for (v=0; v<numVert; v++)
					{
						vertAddr = (v+firstVert) ^ (1 << 7);
						sp = buff +(vertAddr<<9);
						dp = data+v*numHoz+(corner-firstCorn)*numHoz*numVert;
						for (h=0;h<numHoz; h++)
						{
							hozAddr = (h+firstHoz) ^ (1<<8);
							*dp++ += sp[hozAddr];
						}
					}
				}
			}
		}
		free (buff);
	}
}


void XDmaHexitec::readHistEngRowColTimeReordered(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int it, ic, ir;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;
	int maxNumCC;
	
	const char *funcName = mapped?"readMappedEngRowColTime":"readHistEngRowColTime";
	if (useClustClass)
		funcName = mapped?"readMappedEngRowColCCTime":"readHistEngRowColCCTime";

	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;
	if (mapped)
	{
		srcnBinsEng = HEXITEC_NBINS_MAPPED;
		maxNumTF = m_dataFormat[chip].numTFMapped;
		maxNumCC = 1;
	}
	else
	{		
		srcnBinsEng = m_dataFormat[chip].nBinsEng;
		maxNumTF = m_dataFormat[chip].numTF;
		maxNumCC = m_dataFormat[chip].nBinsClustClass;
	}
	
	if (numEng == srcnBinsEng)
	{
		/* At least can read Complete spectra */
		for (it=0; it< numTF; it++)
		{
			for (int icc=0; icc<numCC; icc++)
			{
				int t = (firstTF+it)*maxNumCC+icc+firstCC;
				for(ic=0; ic<numCols; ic++)
				{
					dp = data + numEng*numRows*(ic+numCols*(it*numCC+icc));
					col = ic+firstCol;
					if (col >= 40)
						col += 24;	// Adjust for 40 columns processed in parallel, then Col[6]==1 implies Right hand half.
					axiAddress = firstRow*srcnBinsEng*sizeof(uint32_t) | HEXITEC_REORDER_OFFSET_ROWCOL;
					axiAddress += col*HEXITEC_MHZ_ROW_STRIDE*srcnBinsEng*sizeof(uint32_t);
					axiAddress += (uint64_t)(t)*HEXITEC_MHZ_ROW_STRIDE*HEXITEC_MHZ_COL_STRIDE*srcnBinsEng*sizeof(uint32_t);
					numBytes = sizeof(uint32_t)*numEng*numRows;
					m_hbmHist.readDma((char *)dp, axiAddress, numBytes, dmaChan);
				}
			}
		}
	}
	else
	{
		posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numRows*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngRowColTimeReordered: Out of memory");
		for (it=0; it< numTF; it++)
		{
			for (int icc=0; icc<numCC; icc++)
			{
				int t = (firstTF+it)*maxNumCC+icc+firstCC;
				for(ic=0; ic<numCols; ic++)
				{
					col = ic+firstCol;
					if (col >= 40)
						col += 24;	// Adjust for 40 columns processed in parallel, then Col[6]==1 implies Right hand half.
					axiAddress = firstRow*srcnBinsEng*sizeof(uint32_t) | HEXITEC_REORDER_OFFSET_ROWCOL;
					axiAddress += col*HEXITEC_MHZ_ROW_STRIDE*srcnBinsEng*sizeof(uint32_t);
					axiAddress += (uint64_t)(t)*HEXITEC_MHZ_ROW_STRIDE*HEXITEC_MHZ_COL_STRIDE*srcnBinsEng*sizeof(uint32_t);
					numBytes = sizeof(uint32_t)*srcnBinsEng*numRows;
					m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
					dp = data + numEng*numRows*(ic+numCols*(it*numCC+icc));
					for (ir=0; ir<numRows; ir++)
					{
						sp = buff + srcnBinsEng*ir;
						memcpy(dp, sp, numEng*sizeof(uint32_t));
						dp += numEng;
					}
				}
			}
		}
		free(buff);
	}
}

#if 0
void XDmaHexitec::readHistEngCalibClassReorderedMHz(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int ic, icc, l;
	int eng;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;

	srcnBinsEng = m_dataFormat[chip].nBinsEng;

	memset(data, 0, sizeof(uint32_t)*numEng*numLutAddr*numClusterClass);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numLutAddr*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistEngTimeReordered: Out of memory");
	for (ic=0;ic<HEXITEC_NUM_COLS/2; ic++) /* The data is distributed by row[1:0] and col to achieve histogram performance, so iterate over these and sum */
	{
		col = ic;
		if (col >= 20)
			col += 12;
		for (icc=0; icc<numClusterClass; icc++)
		{
			axiAddress = (uint64_t)col << HEXITEC_MHZ_PORT_SHIFT;
			axiAddress  |= HEXITEC_REORDER_OFFSET_COLROW_GAPS;
			axiAddress |= (icc+firstClusterClass)*srcnBinsEng*m_dataFormat[chip].nBinsLutAddr*sizeof(uint32_t);
			numBytes = srcnBinsEng*numLutAddr*sizeof(uint32_t); 
			m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
			dp = data+icc*numEng*numLutAddr;
			for (l=0; l<numLutAddr; l++)
			{
				sp = buff+firstEng+l*srcnBinsEng;
				for (eng=0; eng<numEng; eng++)
					*dp++ += *sp++;
			}
		}
	}
	free (buff);
}
#endif
void XDmaHexitec::readHistEngCalibClassSummedMHz(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int icc, l;
	int eng;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;
	int formatChip = 0;

	if (chip < 0)
		formatChip = 0;
	srcnBinsEng = m_dataFormat[formatChip].nBinsEng;

	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numLutAddr*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistEngTimeReordered: Out of memory");
	for (icc=0; icc<numClusterClass; icc++)
	{
		axiAddress  = HEXITEC_REORDER_OFFSET_SUMMED;
		axiAddress |= (icc+firstClusterClass)*srcnBinsEng*m_dataFormat[formatChip].nBinsLutAddr*sizeof(uint32_t);
		numBytes = srcnBinsEng*numLutAddr*sizeof(uint32_t); 
		m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
		dp = data+icc*numEng*numLutAddr;
		for (l=0; l<numLutAddr; l++)
		{
			sp = buff+firstEng+l*srcnBinsEng;
			for (eng=0; eng<numEng; eng++)
				*dp++ = *sp++;
		}
	}
	free (buff);
}
#if 0
void XDmaHexitec::readHistCharac2dReorderedMHz(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int ic, ineb, im;
	uint64_t axiAddress, numBytes;
	int srcnBinsMain, srcnBinsNeb;
	int maxNumTF;

	srcnBinsMain = m_dataFormat[chip].nBins[0];
	srcnBinsNeb  = m_dataFormat[chip].nBins[1];
	maxNumTF = m_dataFormat[chip].numTF;

	memset(data, 0, sizeof(uint32_t)*numMain*numNeb);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsMain*numNeb*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistCharac2dReorderedMHz: Out of memory");
	for (ic=0;ic<HEXITEC_NUM_COLS/2; ic++) /* The data is distributed by col[5:1] to achieve histogram performance, so iterate over these and sum */
	{
		col = ic;
		if (col >= 20)
			col += 12;
		axiAddress = (uint64_t)col << HEXITEC_MHZ_PORT_SHIFT;
		axiAddress  |= HEXITEC_REORDER_OFFSET_COLROW_GAPS;
		axiAddress |= firstNeb*srcnBinsMain*sizeof(uint32_t);
		numBytes = srcnBinsMain*numNeb*sizeof(uint32_t); 
		m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
		dp = data;
		for (ineb=0; ineb<numNeb; ineb++)
		{
			sp = buff+firstMain+ineb*srcnBinsMain;
			for (im=0; im<numMain; im++)
				*dp++ += *sp++;
		}
	}
	free (buff);
}

void XDmaHexitec::readHistCharac3dReorderedMHz(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int ic, icorn, h, v;
	uint64_t axiAddress, numBytes;
	int srcnBinsHoz, srcnBinsCorn, srcnBinsVert;
	int maxNumTF;

	srcnBinsHoz = m_dataFormat[chip].nBins[0];
	srcnBinsVert= m_dataFormat[chip].nBins[1];
	srcnBinsCorn = m_dataFormat[chip].nBins[2];
	maxNumTF = m_dataFormat[chip].numTF;

	memset(data, 0, sizeof(uint32_t)*numHoz*numVert*numCorn);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsHoz*numVert*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistCharac3dReorderedMHz: Out of memory");
	for (ic=0;ic<HEXITEC_NUM_COLS/2; ic++) /* The data is distributed by col[5:1] to achieve histogram performance, so iterate over these and sum */
	{
		col = ic;
		if (col >= 20)
			col += 12;
		for (icorn=0; icorn<numCorn; icorn++)
		{
			axiAddress = (uint64_t)col << HEXITEC_MHZ_PORT_SHIFT;
			axiAddress  |= HEXITEC_REORDER_OFFSET_COLROW_GAPS;
			axiAddress += firstVert*srcnBinsHoz*sizeof(uint32_t);
			axiAddress += (icorn+firstCorn)*srcnBinsHoz*srcnBinsVert*sizeof(uint32_t);
			numBytes = srcnBinsHoz*numVert*sizeof(uint32_t); 
			m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
			dp = data+icorn*numHoz*numVert;
			for (v=0; v<numVert; v++)
			{
				sp = buff+firstHoz+v*srcnBinsHoz;
				for (h=0; h<numHoz; h++)
					*dp++ += *sp++;
			}
		}
	}
	free (buff);
}
#endif
void XDmaHexitec::readHistCharac2dSummedMHz(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int ineb, im;
	uint64_t axiAddress, numBytes;
	int srcnBinsMain, srcnBinsNeb;
	int maxNumTF;

	srcnBinsMain = m_dataFormat[chip].nBins[0];
	srcnBinsNeb  = m_dataFormat[chip].nBins[1];
	maxNumTF = m_dataFormat[chip].numTF;

	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsMain*numNeb*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistCharac2dReorderedMHz: Out of memory");
	axiAddress  = HEXITEC_REORDER_OFFSET_SUMMED;
	axiAddress |= diag * srcnBinsMain * srcnBinsNeb*sizeof(uint32_t);
	axiAddress |= firstNeb*srcnBinsMain*sizeof(uint32_t);
	numBytes = srcnBinsMain*numNeb*sizeof(uint32_t); 
	m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
	dp = data;
	for (ineb=0; ineb<numNeb; ineb++)
	{
		sp = buff+firstMain+ineb*srcnBinsMain;
		for (im=0; im<numMain; im++)
			*dp++ = *sp++;
	}
	free (buff);
}

void XDmaHexitec::readHistCharac3dSummedMHz(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int icorn, h, v;
	uint64_t axiAddress, numBytes;
	int srcnBinsHoz, srcnBinsCorn, srcnBinsVert;
	int maxNumTF;

	srcnBinsHoz = m_dataFormat[chip].nBins[0];
	srcnBinsVert= m_dataFormat[chip].nBins[1];
	srcnBinsCorn = m_dataFormat[chip].nBins[2];
	maxNumTF = m_dataFormat[chip].numTF;

	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsHoz*numVert*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistCharac3dReorderedMHz: Out of memory");
	for (icorn=0; icorn<numCorn; icorn++)
	{
		axiAddress  = HEXITEC_REORDER_OFFSET_SUMMED;
		axiAddress += firstVert*srcnBinsHoz*sizeof(uint32_t);
		axiAddress += (icorn+firstCorn)*srcnBinsHoz*srcnBinsVert*sizeof(uint32_t);
		numBytes = srcnBinsHoz*numVert*sizeof(uint32_t); 
		m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
		dp = data+icorn*numHoz*numVert;
		for (v=0; v<numVert; v++)
		{
			sp = buff+firstHoz+v*srcnBinsHoz;
			for (h=0; h<numHoz; h++)
				*dp++ = *sp++;
		}
	}
	free (buff);
}



void XDmaHexitec::readHistEngColRowTimeInt(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass, bool useReorder)
{
	uint32_t *buff=NULL, *sp, *dp, *colPtr; 
	int firstRowBot, firstRowTop;
	int lastRowBot, lastRowTop;
	int bufNumRows;
	int t, row, col, rowBot, rowTop;
	int it, ic, ir;
	uint64_t axiAddress, numBytes;
	int logicalPort, hbmPort;
	int stream;
	int numStreams = m_hbmHist.m_histConf.NumStreams;
	int srcnBinsEng;
	int maxNumTF;
	int maxNumCC;
	const char *funcName = mapped?"readMappedEngColRowTime":"readHistEngColRowTime";

	if (useClustClass)
		funcName = mapped?"readMappedEngColRowCCTime":"readHistEngColRowCCTime";

	if (chip < 0 || chip >= m_numChips)
		throw  XDmaHexitecException("%s: chip %d is not in range 0...%d", funcName, chip, m_numChips-1);
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;
	if (mapped)
	{
		srcnBinsEng = HEXITEC_NBINS_MAPPED;
		maxNumTF = m_dataFormat[chip].numTFMapped;
		maxNumCC = 1;
	}
	else
	{		
		srcnBinsEng = m_dataFormat[chip].nBinsEng;
		maxNumTF = m_dataFormat[chip].numTF;
		maxNumCC = m_dataFormat[chip].nBinsClustClass;
	}
	
	if (mapped && m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
		throw  XDmaHexitecException("%s: Attempt to read mapped energy, but mappedMode=%d ==OFF", funcName, m_dataFormat[chip].mappedMode);
		
	if (numEng < 1 || numEng > srcnBinsEng)
		throw  XDmaHexitecException("%s: numEng %d is not in range 1... %d", funcName, numEng, srcnBinsEng);
	if (firstRow <0 || firstRow >= HEXITEC_NUM_ROWS || numRows <1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw  XDmaHexitecException("%s: firstRow=%d, numRows=%d is not in range 0... %d", funcName, firstRow, numRows, HEXITEC_NUM_ROWS);
	if (firstCol <0 || firstCol >= HEXITEC_NUM_COLS || numCols <1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw  XDmaHexitecException("%s: firstCol=%d, numCols=%d is not in range 0... %d", funcName, firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstCC < 0 || firstCC > maxNumCC || numCC < 1 || firstCC+numCC > maxNumCC)
		throw  XDmaHexitecException("%s: firstCC=%d, numCC=%d is not in range 0... %d", funcName, firstCC, numCC, maxNumCC);
	if (firstTF < 0 || firstTF > maxNumTF || numTF < 1 || firstTF+numTF > maxNumTF)
		throw  XDmaHexitecException("%s: firstTF=%d, numTF=%d is not in range 0... %d", funcName, firstTF, numTF, maxNumTF);
	if (mapped && m_dataFormat[chip].mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF && 
		((m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY7) || 
		 (m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY_CC12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY_CC7) ||
		 (m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_RUN12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_RUN10LSB) ||
		 (m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_POS_CC12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB) 

		 ) ) 
	{
		// Mapped readout is now OK with Energy Posn, Energy Posn CC, energy only and energy CC mode
	}
	else
	{
		if ((useClustClass && m_dataFormat[chip].memReadAccess != EngPosCCTime) || (!useClustClass && m_dataFormat[chip].memReadAccess != EngPosTime))
			throw  XDmaHexitecException("%s: Current memory layout %d, histMode=0x%04X, mapped=%d,  is not supported by this read function", funcName, 
					m_dataFormat[chip].memReadAccess,  m_dataFormat[chip].histMode, mapped);
	}
	if ((m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGCOLROW_GAPS)&&  useReorder)
	{
		readHistEngColRowTimeReorderedMHz(numEng, firstCol, numCols, firstRow, numRows, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, mapped, useClustClass);
	}	
	else if ((m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGCOLROW_CONT)&&  useReorder)
	{
		readHistEngColRowTimeReorderedHxt(numEng, firstCol, numCols, firstRow, numRows, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, mapped, useClustClass);
	}	
	else
	{
		throw XDmaHexitecException("%s: UNSUPORTED without VHDL reorder, m_axiReorder=0x%08X, useReorder=%d", funcName, m_hbmHist.m_axiReorder, useReorder);
	}
}

void XDmaHexitec::readHistEngColRowTimeReorderedMHz(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int it, ic, ir;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;
	int maxNumCC;
	const char *funcName = mapped?"readMappedEngColRowTime":"readHistEngColRowTime";
	uint64_t accessPage = HEXITEC_REORDER_OFFSET_COLROW_GAPS;
	if (useClustClass && !mapped)
		funcName = "readHistEngColRowCCTime";
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;
	if (mapped)
	{
		if (numEng <= HEXITEC_NBINS_MAPPED_HALF)
		{
			srcnBinsEng = HEXITEC_NBINS_MAPPED_HALF;
			accessPage = HEXITEC_REORDER_OFFSET_MAPPED_HALF;
		}
		else
		{
			srcnBinsEng = HEXITEC_NBINS_MAPPED;
			accessPage = HEXITEC_REORDER_OFFSET_MAPPED_ALL;
		}
		maxNumTF = m_dataFormat[chip].numTFMapped;
		if (m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
		{
			if ((m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY7) ||
				(m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY_CC12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY_CC7))
			firstTF += maxNumTF;
		}
		maxNumCC = 1;
	}
	else
	{		
		srcnBinsEng = m_dataFormat[chip].nBinsEng;
		maxNumTF = m_dataFormat[chip].numTF;
		maxNumCC = m_dataFormat[chip].nBinsClustClass;
	}
	//printf("readHistEngColRowTimeReorderedMHz: srcnBinsEng=%d, maxNumTF=%d, maxNumCC=%d\n", srcnBinsEng, maxNumTF, maxNumCC);
	if (numEng == srcnBinsEng)
	{
		/* At least can read Complete spectra */
		for (it=0; it< numTF; it++)
		{
			for (int icc=0; icc<numCC; icc++)
			{
				int st = (firstTF+it)*maxNumCC+icc+firstCC;
				int dt = it*numCC+icc;
				for(ir=0; ir<numRows; ir++)
				{
					dp = data + numEng*numCols*(ir+numRows*dt);
					axiAddress = firstCol*srcnBinsEng*sizeof(uint32_t) | accessPage;
					axiAddress += (ir+firstRow)*HEXITEC_MHZ_ROW_STRIDE*srcnBinsEng*sizeof(uint32_t);
					axiAddress += (uint64_t)(st)*HEXITEC_MHZ_ROW_STRIDE*HEXITEC_MHZ_COL_STRIDE*srcnBinsEng*sizeof(uint32_t);
					numBytes = sizeof(uint32_t)*numEng*numCols;
					m_hbmHist.readDma((char *)dp, axiAddress, numBytes, dmaChan);
				}
			}
		}
	}
	else
	{
		posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numCols*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngColRowTimeReorderedMHz: Out of memory");
		for (it=0; it< numTF; it++)
		{
			for (int icc=0; icc<numCC; icc++)
			{
				int st = (firstTF+it)*maxNumCC+icc+firstCC;
				int dt = it*numCC+icc;
				for(ir=0; ir<numRows; ir++)
				{
					axiAddress = firstCol*srcnBinsEng*sizeof(uint32_t) | accessPage;
					axiAddress += (ir+firstRow)*HEXITEC_MHZ_ROW_STRIDE*srcnBinsEng*sizeof(uint32_t);
					axiAddress += (uint64_t)(st)*HEXITEC_MHZ_ROW_STRIDE*HEXITEC_MHZ_COL_STRIDE*srcnBinsEng*sizeof(uint32_t);
					numBytes = sizeof(uint32_t)*srcnBinsEng*numCols;
					m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
					dp = data + numEng*numCols*(ir+numRows*dt);
					for (ic=0; ic<numCols; ic++)
					{
						sp = buff + srcnBinsEng*ic;
						memcpy(dp, sp, numEng*sizeof(uint32_t));
						dp += numEng;
					}
				}
			}
		}
		free(buff);
	}
}

void XDmaHexitec::readHistEngColRowTimeReorderedHxt(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int it, ic, ir;
	uint64_t axiAddress, numBytes;
	uint64_t reorderPage = HEXITEC_REORDER_OFFSET_COLROW_CONT;
	int srcnBinsEng;
	int maxNumTF;
	int maxNumCC;
	
	const char *funcName = mapped?"readMappedEngColRowTime":"readHistEngColRowTime";
	if (useClustClass)
		funcName = mapped?"readMappedEngColRowCCTime":"readHistEngColRowCCTime";

	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;
	if (mapped)
	{
		if (numEng > HEXITEC_NBINS_MAPPED/2)
		{
			srcnBinsEng = HEXITEC_NBINS_MAPPED;
			reorderPage = HEXITEC_REORDER_OFFSET_MAPPED_ALL;
		}
		else
		{
			srcnBinsEng = HEXITEC_NBINS_MAPPED/2;
			reorderPage = HEXITEC_REORDER_OFFSET_MAPPED_HALF;
		}
		maxNumTF = m_dataFormat[chip].numTFMapped;
/*		This comes from combining mapped mode with EngOnlyModes, but the reorder_addr VHDL does this for us when required

		if (m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
			firstTF += maxNumTF;
*/
		maxNumCC = 1;
	}
	else
	{		
		srcnBinsEng = m_dataFormat[chip].nBinsEng;
		maxNumTF = m_dataFormat[chip].numTF;
		reorderPage = HEXITEC_REORDER_OFFSET_COLROW_CONT;
		maxNumCC = m_dataFormat[chip].nBinsClustClass;
	}
/*	printf("using : readHistEngColRowTimeReorderedHxt: mapped=%d, useClustClass=%d, firstCC=%d, numCC=%d, firstTF=%d, numTF=%d\n", 
		mapped, useClustClass, firstCC, numCC, firstTF, numTF);
*/
	if (numEng == srcnBinsEng && numCols == HEXITEC_NUM_COLS)
	{
		/* Can read complete time frames */
		for (it=0; it< numTF; it++)
		{
			for (int icc=0; icc<numCC; icc++)
			{
				int t = (firstTF+it)*maxNumCC+icc+firstCC;
				dp = data + (ptrdiff_t)numEng*numCols*(ptrdiff_t)(numRows*(it*numCC+icc));
				axiAddress = reorderPage;
				axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
				axiAddress += firstRow*HEXITEC_NUM_COLS*srcnBinsEng*sizeof(uint32_t);
				axiAddress += (uint64_t)(t)*HEXITEC_T_STRIDE*srcnBinsEng*sizeof(uint32_t);
				numBytes = sizeof(uint32_t)*numEng*numCols*numRows;
//				printf("Reading from 0x%010lX for 0x%010lX\n", axiAddress, numBytes);
				m_hbmHist.readDma((char *)dp, axiAddress, numBytes, dmaChan);
			}
		}
	}
	else if (numEng == srcnBinsEng)
	{
		/* At least can read Complete rows */
		for (it=0; it< numTF; it++)
		{
			for (int icc=0; icc<numCC; icc++)
			{
				int t = (firstTF+it)*maxNumCC+icc+firstCC;
				for(ir=0; ir<numRows; ir++)
				{
					dp = data + (ptrdiff_t)numEng*numCols*(ptrdiff_t)(ir+numRows*(it*numCC+icc));
					axiAddress = firstCol*srcnBinsEng*sizeof(uint32_t) | reorderPage;
					axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
					axiAddress += (ir+firstRow)*HEXITEC_NUM_COLS*srcnBinsEng*sizeof(uint32_t);
					axiAddress += (uint64_t)(t)*HEXITEC_T_STRIDE*srcnBinsEng*sizeof(uint32_t);
					numBytes = sizeof(uint32_t)*numEng*numCols;
					m_hbmHist.readDma((char *)dp, axiAddress, numBytes, dmaChan);
				}
			}
		}
	}
	else
	{
		posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numCols*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngColRowTimeReorderedMHz: Out of memory");
		for (it=0; it< numTF; it++)
		{
			for (int icc=0; icc<numCC; icc++)
			{
				int t = (firstTF+it)*maxNumCC+icc+firstCC;
				for(ir=0; ir<numRows; ir++)
				{
					axiAddress = firstCol*srcnBinsEng*sizeof(uint32_t) | reorderPage;
					axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
					axiAddress += (ir+firstRow)*HEXITEC_NUM_COLS*srcnBinsEng*sizeof(uint32_t);
					axiAddress += (uint64_t)(t)*HEXITEC_T_STRIDE*srcnBinsEng*sizeof(uint32_t);
					numBytes = sizeof(uint32_t)*srcnBinsEng*numCols;
					m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
					dp = data + (ptrdiff_t)numEng*numCols*(ptrdiff_t)(ir+numRows*(it*numCC+icc));
					for (ic=0; ic<numCols; ic++)
					{
						sp = buff + srcnBinsEng*ic;
						memcpy(dp, sp, numEng*sizeof(uint32_t));
						dp += numEng;
					}
				}
			}
		}
		free(buff);
	}
}

void XDmaHexitec::readHistEngTimeReorderedMHz(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int it, ic, ir, rowCol;
	int eng;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;
	const char * funcName = useClustClass?"readHistEngCCTime":"readHistEngTime";
	int maxNumCC;
	
	srcnBinsEng = m_dataFormat[chip].nBinsEng;
	maxNumTF = m_dataFormat[chip].numTF;
	maxNumCC = m_dataFormat[chip].nBinsClustClass;
	
	memset(data, 0, sizeof(uint32_t)*numEng*numTF*numCC);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numTF*maxNumCC*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistEngTimeReordered: Out of memory");
	for (ic=0;ic<HEXITEC_NUM_COLS; ic++) /* The data is distributed by row[1:0] and col to achieve histogram performance, so iterate over these and sum */
	{
		col = ic;
		for (ir=0; ir<4; ir++)
		{
			rowCol = ir+4*col;
			axiAddress = (uint64_t)rowCol << HEXITEC_MHZ_ROWCOL_SHIFT_EO;
			axiAddress += firstTF*maxNumCC*srcnBinsEng*sizeof(uint32_t);
			axiAddress  |= HEXITEC_REORDER_OFFSET_COLROW_GAPS;
			numBytes = srcnBinsEng*numTF*maxNumCC*sizeof(uint32_t); 
			m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
			dp = data;
			for (it=0; it<numTF; it++)
			{
				for (int icc=0; icc<numCC; icc++)
				{
					sp = buff+(it*maxNumCC+icc+firstCC)*srcnBinsEng;
					for (eng=0; eng<numEng; eng++)
						*dp++ += *sp++;
				}
			}
		}
	}
	free (buff);
}

void XDmaHexitec::readHistEngTimeReorderedHxt(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int it, rowCol;
	int eng;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;
	const char * funcName = useClustClass?"readHistEngCCTime":"readHistEngTime";
	int firstChip, lastChip;
	int maxNumCC;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		chip = 0;
	}
	else
	{
		firstChip = lastChip = chip;
	}
	
	srcnBinsEng = m_dataFormat[chip].nBinsEng;
	maxNumTF = m_dataFormat[chip].numTF;
	maxNumCC = m_dataFormat[chip].nBinsClustClass;
	
	memset(data, 0, sizeof(uint32_t)*numEng*numTF*numCC);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numTF*maxNumCC*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistEngTimeReordered: Out of memory");
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (firstChip != lastChip)
			printf("readHistEngTimeReorderedHxt: Summing chip %d\n", chip);
		for (rowCol=0;rowCol<HEXITEC_NUM_ROWCOL_SUM; rowCol++) /* The data is distributed by rowCOl[3:0] to achieve histogram performance, so iterate over these and sum */
		{
			axiAddress  = HEXITEC_REORDER_OFFSET_COLROW_CONT;
			axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
			axiAddress |= (uint64_t)rowCol << HEXITEC_ROWCOL_SHIFT_EO  ;
			axiAddress += (uint64_t)firstTF*maxNumCC*srcnBinsEng*sizeof(uint32_t);
			numBytes = (uint64_t)srcnBinsEng*numTF*maxNumCC*sizeof(uint32_t); 
			m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
			dp = data;
			for (it=0; it<numTF; it++)
			{
				for (int icc=0; icc<numCC; icc++)
				{
					sp = buff+(it*maxNumCC+icc+firstCC)*srcnBinsEng;
					for (eng=0; eng<numEng; eng++)
						*dp++ += *sp++;
				}
			}
		}
	}
	free (buff);
}

void XDmaHexitec::readHistEngTimeSummedMHz(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass)
{
	throw runtime_error("readHistEngTimeSummedMHz: Not implmented yet");
}

void XDmaHexitec::readHistEngTimeSummedHxt(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int it, rowCol;
	int eng;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;
	const char * funcName = useClustClass?"readHistEngCCTime":"readHistEngTime";
	int maxNumCC;
	
	if (chip < 0)
	{
		srcnBinsEng = m_dataFormat[0].nBinsEng;
		maxNumTF = m_dataFormat[0].numTF;
		maxNumCC = m_dataFormat[0].nBinsClustClass;
		chip = HEXITEC_REORDER_CHIP_SUM_ALL_HXT;
	}
	else
	{
		srcnBinsEng = m_dataFormat[chip].nBinsEng;
		maxNumTF = m_dataFormat[chip].numTF;
		maxNumCC = m_dataFormat[chip].nBinsClustClass;
	}

	if (numEng == srcnBinsEng && (numCC == maxNumCC || numTF == 1))
	{
		axiAddress  = HEXITEC_REORDER_OFFSET_SUMMED;
		axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;			// bits 33:30 are used in Hexitec 6x2. They are set in HexitecMHz, but ignored in reorder_addr_mhz16g.vhd
		axiAddress += firstTF*maxNumCC*srcnBinsEng*sizeof(uint32_t);
		numBytes = srcnBinsEng*numTF*numCC*sizeof(uint32_t); 
		m_hbmHist.readDma((char *)data, axiAddress, numBytes, dmaChan);
	}
	else
	{
		posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numTF*maxNumCC*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngTimeReordered: Out of memory");
		axiAddress  = HEXITEC_REORDER_OFFSET_SUMMED;
		axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
		axiAddress += firstTF*maxNumCC*srcnBinsEng*sizeof(uint32_t);
		numBytes = srcnBinsEng*numTF*maxNumCC*sizeof(uint32_t); 
		m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
		dp = data;
		for (it=0; it<numTF; it++)
		{
			for (int icc=0; icc <numCC; icc++)
			{
				sp = buff+(it*maxNumCC+icc+firstCC)*srcnBinsEng;
				memcpy(dp, sp, sizeof(uint32_t)*numEng);
				dp+= numEng;
			}
		}
		free (buff);
	}
}

void XDmaHexitec::readHistEngCalibClassReorderedHxt(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int eng, l, icc;
	uint64_t axiAddress, numBytes;
	int srcnBinsEng;
	int maxNumTF;
	int firstChip, lastChip;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		chip = 0;
	}
	else
	{
		firstChip = lastChip = chip;
	}
	srcnBinsEng = m_dataFormat[chip].nBinsEng;

	memset(data, 0, sizeof(uint32_t)*numEng*numLutAddr*numClusterClass);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*numLutAddr*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistEngCalibClassReorderedHxt: Out of memory");
	for(chip=firstChip; chip<=lastChip; chip++)
	{
		for (icc=0; icc<numClusterClass; icc++)
		{
			axiAddress  = HEXITEC_REORDER_OFFSET_COLROW_CONT;
			axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
			axiAddress |= (icc+firstClusterClass)*srcnBinsEng*m_dataFormat[chip].nBinsLutAddr*sizeof(uint32_t);
			numBytes = srcnBinsEng*numLutAddr*sizeof(uint32_t); 
			m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
			dp = data+icc*numEng*numLutAddr;
			for (l=0; l<numLutAddr; l++)
			{
				sp = buff+firstEng+l*srcnBinsEng;
				for (eng=0; eng<numEng; eng++)
					*dp++ += *sp++;
			}
		}
	}
	free (buff);
}

void XDmaHexitec::readHistCharac2dReorderedHxt(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int ineb, im;
	uint64_t axiAddress, numBytes;
	int srcnBinsMain, srcnBinsNeb;
	int maxNumTF;
	int firstChip, lastChip;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		chip = 0;
	}
	else
	{
		firstChip = lastChip = chip;
	}

	srcnBinsMain = m_dataFormat[chip].nBins[0];
	srcnBinsNeb  = m_dataFormat[chip].nBins[1];
	maxNumTF = m_dataFormat[chip].numTF;

	memset(data, 0, sizeof(uint32_t)*numMain*numNeb);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsMain*numNeb*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistCharac2dReorderedHxt: Out of memory");
	for(chip=firstChip; chip<=lastChip; chip++)
	{
		axiAddress  = HEXITEC_REORDER_OFFSET_COLROW_CONT;
		axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
		axiAddress |= firstNeb*srcnBinsMain*sizeof(uint32_t);
		numBytes = srcnBinsMain*numNeb*sizeof(uint32_t); 
		m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
		dp = data;
		for (ineb=0; ineb<numNeb; ineb++)
		{
			sp = buff+firstMain+ineb*srcnBinsMain;
			for (im=0; im<numMain; im++)
				*dp++ += *sp++;
		}
	}
	free (buff);
}

void XDmaHexitec::readHistCharac3dReorderedHxt(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data)
{
	uint32_t *buff=NULL, *sp, *dp;
	int col;
	int ic, icorn, h, v;
	uint64_t axiAddress, numBytes;
	int srcnBinsHoz, srcnBinsCorn, srcnBinsVert;
	int maxNumTF;
	int firstChip, lastChip;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		chip = 0;
	}
	else
	{
		firstChip = lastChip = chip;
	}
	srcnBinsHoz = m_dataFormat[chip].nBins[0];
	srcnBinsVert= m_dataFormat[chip].nBins[1];
	srcnBinsCorn = m_dataFormat[chip].nBins[2];
	maxNumTF = m_dataFormat[chip].numTF;

	memset(data, 0, sizeof(uint32_t)*numHoz*numVert*numCorn);
	posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsHoz*numVert*sizeof(uint32_t));
	if (buff == nullptr)
		throw runtime_error("readHistCharac3dReorderedMHz: Out of memory");
	for(chip=firstChip; chip<=lastChip; chip++)
	{
		for (icorn=0; icorn<numCorn; icorn++)
		{
			axiAddress  = HEXITEC_REORDER_OFFSET_COLROW_CONT;
			axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
			axiAddress += firstVert*srcnBinsHoz*sizeof(uint32_t);
			axiAddress += (icorn+firstCorn)*srcnBinsHoz*srcnBinsVert*sizeof(uint32_t);
			numBytes = srcnBinsHoz*numVert*sizeof(uint32_t); 
			m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
			dp = data+icorn*numHoz*numVert;
			for (v=0; v<numVert; v++)
			{
				sp = buff+firstHoz+v*srcnBinsHoz;
				for (h=0; h<numHoz; h++)
					*dp++ += *sp++;
			}
		}
	}
	free (buff);
}

void XDmaHexitec::checkFormatMatch(const char * funcName)
{
	int chip;
	
	for (chip=1; chip<m_numChips; chip++)
	{
		if (!(m_dataFormat[0] == m_dataFormat[chip]))
			throw  XDmaHexitecException("%s: memory format does not match between chip 0 and  %d, so cannot sum data", funcName, chip);
	}
}

void XDmaHexitec::clearHistTimeframes(int firstTF, int numTF, int chip, bool mapped, bool wait)
{
	int formatChip = chip;
	int srcnBinsEng;
	int numCC, maxNumTF;
	int startWord, numWords;
	int cc, tf;
	bool engOnly;
	if (chip < 0)
	{	
		checkFormatMatch("clearHistTimeframes");
		formatChip = 0;  // chip == -1 => clear all chip, format must match
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("clearHistTimeframes: chip %d is not in range 0...%d", chip, m_numChips-1);

	if (mapped)
	{
		srcnBinsEng = HEXITEC_NBINS_MAPPED;
		maxNumTF = m_dataFormat[formatChip].numTFMapped;
		numCC = 1;
	}
	else
	{		
		srcnBinsEng = m_dataFormat[formatChip].nBinsEng;
		maxNumTF = m_dataFormat[formatChip].numTF;
		numCC = m_dataFormat[formatChip].nBinsClustClass;
	}
	engOnly = getEngOnly(formatChip);
	
	if (firstTF < 0 || firstTF > maxNumTF || numTF < 1 || firstTF+numTF > maxNumTF)
		throw  XDmaHexitecException("clearHistTimeframes: mapped=%d, firstTF=%d, numTF=%d is not in range 0...%d", mapped, firstTF, numTF, maxNumTF);
	if (mapped && m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF) 
		throw  XDmaHexitecException("clearHistTimeframes: With mapped==true, but mappedMode==HEXITEC_HIST_MAPPED_MODE_OFF");
	
	if (!mapped && m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_ONLY) 
		throw  XDmaHexitecException("clearHistTimeframes: With mapped==false, but mappedMode==HEXITEC_HIST_MAPPED_MODE_ONLY");
	
	for (tf=firstTF; tf<firstTF+numTF; tf++)
	{
		for (cc=0; cc<numCC; cc++)
		{
			int tfCC = tf*numCC+cc;
			int tfCCmangled; 
			if (m_generation == HexitecGeneration::HexitecGenMHz)
			{
				if (!mapped)
				{
					if (engOnly)
					{
						switch (m_dataFormat[formatChip].histMode)
						{
						case HEXITEC_HIST_FORMAT_ENG_ONLY12:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:
							tfCCmangled = tfCC & 0x7F | (tfCC & 0x380) << 1;
							if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
								tfCCmangled |= (tfCC & 0x400) >> 3;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY11:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:
							tfCCmangled = tfCC & 0xFF | (tfCC & 0x700) << 1;
							if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
								tfCCmangled |= (tfCC & 0x800) >> 3;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY10:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:
							tfCCmangled = tfCC & 0x1FF | (tfCC & 0xE00) << 1;
							if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
								tfCCmangled |= (tfCC & 0x1000) >> 3;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY9:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:
							tfCCmangled = tfCC & 0x3FF | (tfCC & 0x1C00) << 1;
							if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
								tfCCmangled |= (tfCC & 0x2000) >> 3;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY8:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:
							tfCCmangled = tfCC & 0x7FF | (tfCC & 0x3800) << 1;
							if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
								tfCCmangled |= (tfCC & 0x4000) >> 3;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY7:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:
							tfCCmangled = tfCC & 0xFFF | (tfCC & 0x7000) << 1;
							if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
								tfCCmangled |= (tfCC & 0x8000) >> 3;
							break;
						default:
							throw  XDmaHexitecException("clearHistTimeframes: With mapped==false, engOnly=True, unexpected histFormat=%X", m_dataFormat[formatChip].histMode);
						}
						startWord = srcnBinsEng*(tfCCmangled);
						numWords = srcnBinsEng;
					}
					else
					{
						startWord = srcnBinsEng*HEXITEC_MHZ_ROW_STRIDE*(tfCC)/4; // Note 2 bits of row are included in stream. All of col are in stream and port
						numWords = srcnBinsEng* HEXITEC_NUM_ROWS/4;		
					}
				}
				else if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
				{
					switch (m_dataFormat[formatChip].histMode)
					{
					case HEXITEC_HIST_FORMAT_RUN12:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC12:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG12:
						startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*((tfCC&0x3F)+(5<<5)+((tfCC&0xFC0)<<2));
						break;
					case HEXITEC_HIST_FORMAT_RUN11: 
					case HEXITEC_HIST_FORMAT_ENG_POS_CC11:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG11:
						startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*((tfCC&0x1F)+(5<<4)+((tfCC&0xFE0)<<2));
						break;	
					case HEXITEC_HIST_FORMAT_RUN10:	
					case HEXITEC_HIST_FORMAT_ENG_POS_CC10:
					case HEXITEC_HIST_FORMAT_RUN10LSB:			
					case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG10:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB:
						startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*((tfCC&0x0F)+(5<<3)+((tfCC&0xFF0)<<2));
						break;	
					case HEXITEC_HIST_FORMAT_RUN9:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC9:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG9:
						startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*((tfCC&0x07)+(5<<2)+((tfCC&0xFF8)<<2));
						break;	
					case HEXITEC_HIST_FORMAT_RUN8:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC8:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG8:
						startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*((tfCC&0x03)+(5<<1)+((tfCC&0xFFC)<<2));
						break;	
					case HEXITEC_HIST_FORMAT_RUN7:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC7:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG7:
						startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*((tfCC&0x01)+(5<<0)+((tfCC&0xFFE)<<2));
						break;	

					case HEXITEC_HIST_FORMAT_ENG_ONLY12:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:
					case HEXITEC_HIST_FORMAT_ENG_ONLY11:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:
					case HEXITEC_HIST_FORMAT_ENG_ONLY10:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:
					case HEXITEC_HIST_FORMAT_ENG_ONLY9:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:
					case HEXITEC_HIST_FORMAT_ENG_ONLY8:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:
					case HEXITEC_HIST_FORMAT_ENG_ONLY7:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:
						tfCCmangled = tfCC & 0x3FF | (tfCC & 0x1C00) << 1 | 1 << 10;
						startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*(tfCCmangled);
						break;
					default:
						throw  XDmaHexitecException("clearHistTimeframes: With mapped==false, engOnly=False, unexpected histFormat=%X", m_dataFormat[formatChip].histMode);
					}
					numWords = srcnBinsEng* HEXITEC_NUM_ROWS/4;
				}
				else
				{
					tfCCmangled = tfCC & 0x3FF | (tfCC & 0x1C00) << 1 | (tfCC & 0x2000) >> 3;
					startWord = HEXITEC_NBINS_MAPPED*(HEXITEC_MHZ_ROW_STRIDE/4)*tfCCmangled;
					numWords = srcnBinsEng* HEXITEC_NUM_ROWS/4;
				}
				startWord /= m_hbmHist.m_histConf.HistWordPerAXIWord;
				numWords /= m_hbmHist.m_histConf.HistWordPerAXIWord;
				m_hbmHist.startClear(m_hbmHist.m_histConf.HBMPortMask, startWord, numWords, 1);
			}
			else
			{
				uint32_t portMask = 0;
				int firstChip, lastChip;
				if (chip < 0)
				{
					firstChip=0;
					lastChip = m_numChips-1;
				}
				else
					firstChip = lastChip = chip;
				
				if (!mapped)
				{
					if (engOnly)
					{
						int tfTop=0;
						startWord = (tfCC*srcnBinsEng) & 0x7FFFFF;
						numWords = srcnBinsEng;
						switch (m_dataFormat[formatChip].histMode)
						{
						case HEXITEC_HIST_FORMAT_ENG_ONLY12:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:
							tfTop = (tfCC >> 11) & 1;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY11:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:
							tfTop = (tfCC >> 12) & 1;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY10:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:
							tfTop = (tfCC >> 13) & 1;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY9:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:
							tfTop = (tfCC >> 14) & 1;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY8:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:
							tfTop = (tfCC >> 15) & 1;
							break;
						case HEXITEC_HIST_FORMAT_ENG_ONLY7:
						case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:
							tfTop = (tfCC >> 16) & 1;
							break;
						default:
							throw  XDmaHexitecException("clearHistTimeframes: With mapped==false, engOnly=True, unexpected histFormat=%X", m_dataFormat[formatChip].histMode);
						}

						for (chip=firstChip; chip<=lastChip; chip++)
						{
							int port = m_hbmHist.getPhysicalPort(chip);
							if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF && tfTop)
								port += 16;
							portMask |= 1 << port;
						}
					}
					else
					{
						startWord = (tfCC*srcnBinsEng<<8) & 0x7FFFFF;
						numWords = srcnBinsEng*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS/(16*2);	// words spread over 16 streams an 2 ports
						for (chip=firstChip; chip<=lastChip; chip++)
						{
							int port = m_hbmHist.getPhysicalPort(chip);
							portMask |= 0x10001 << port;
						}
					}
				}
				else if (m_dataFormat[formatChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
				{
					//* ToDo: Write Hexitec 6x2 versions
					int portPosn=0x10001; // Mapped with Posn is split between LHS an RHS stack
					numWords = HEXITEC_NBINS_MAPPED*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS/(16*2);	// Spread over 16 streams and LHS and RHS
					numWords += 0x80;		// Because Row col toggle between LHS/RHS on RC*, 25*256. Need to round up from 12.5*2*256 to 12*2*256
					switch (m_dataFormat[formatChip].histMode)
					{
					case HEXITEC_HIST_FORMAT_RUN12:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC12:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG12:
						startWord = (((tfCC&0x1F)+0xC8)<<12) | ((tfCC&0xE0)<<15);
						break;
					case HEXITEC_HIST_FORMAT_RUN11: 
					case HEXITEC_HIST_FORMAT_ENG_POS_CC11:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG11:
						startWord = (((tfCC&0xF)+0x64)<<12) | ((tfCC&0xF0)<<15);
						break;	
					case HEXITEC_HIST_FORMAT_RUN10:	
					case HEXITEC_HIST_FORMAT_ENG_POS_CC10:
					case HEXITEC_HIST_FORMAT_RUN10LSB:			
					case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG10:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB:
						startWord = (((tfCC&0x7)+0x32)<<12) | ((tfCC&0xF8)<<15);
						break;	
					case HEXITEC_HIST_FORMAT_RUN9:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC9:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG9:
						startWord = (((tfCC&0x3)+0x19)<<12) | ((tfCC&0xFC)<<15);
						break;	
					case HEXITEC_HIST_FORMAT_RUN8:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC8:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG8:
						startWord = (((tfCC&0x1)+0xC)<<12) | ((tfCC&0xFE)<<15) | 1 << 11;
						break;	
					case HEXITEC_HIST_FORMAT_RUN7:
					case HEXITEC_HIST_FORMAT_ENG_POS_CC7:
					case HEXITEC_HIST_FORMAT_ENG_POS_CG7:
						startWord = (tfCC<<15) | 0x19 << 10;
						break;	

					case HEXITEC_HIST_FORMAT_ENG_ONLY12:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:
					case HEXITEC_HIST_FORMAT_ENG_ONLY11:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:
					case HEXITEC_HIST_FORMAT_ENG_ONLY10:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:
					case HEXITEC_HIST_FORMAT_ENG_ONLY9:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:
					case HEXITEC_HIST_FORMAT_ENG_ONLY8:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:
					case HEXITEC_HIST_FORMAT_ENG_ONLY7:
					case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:
						numWords = HEXITEC_NBINS_MAPPED*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS/16;	// Spread over 16 streams all in RHS
						startWord = (tfCC << 13 ) & 0x7FFFFF;
						portPosn=0x1000;	// Mapped with EngOnly are in the RHS stack
						break;
					default:
						throw  XDmaHexitecException("clearHistTimeframes: With mapped==false, engOnly=False, unexpected histFormat=%X", m_dataFormat[formatChip].histMode);
					}
					for (chip=firstChip; chip<=lastChip; chip++)
					{
						int port = m_hbmHist.getPhysicalPort(chip);
						portMask |= portPosn << port;
					}
//					printf("Intl Mapped portMask=0x%08X, startWord=%08X, numWords=%08X\n", portMask, startWord, numWords);
				}
				else /* Must be mapped only */
				{
					startWord = (tfCC << 13 ) & 0x7FFFFF;
					numWords = HEXITEC_NBINS_MAPPED*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS/16;	// Spread over 16 streams
					for (chip=firstChip; chip<=lastChip; chip++)
					{
						int port = m_hbmHist.getPhysicalPort(chip);
						if (tfCC & 0x400)
							port += 16;
						portMask |= 1 << port;
					}
				}
				startWord /= m_hbmHist.m_histConf.HistWordPerAXIWord;
				numWords /= m_hbmHist.m_histConf.HistWordPerAXIWord;
				m_hbmHist.startClear(portMask, startWord, numWords, 1);
			}
		}
	}
	if (wait)
		m_hbmHist.waitClear();
}
		
void XDmaHexitec::clearHistAll()
{
	uint32_t numWords = m_hbmHist.m_histConf.AXIWordsPerStream ; // * m_hbmHist.m_histConf.NumStreams;
	m_hbmHist.startClearAll(m_hbmHist.m_histConf.HBMPortMask, 0, numWords);
	m_hbmHist.waitClear();
}
void XDmaHexitec::enableHist()
{
	int i;
	m_hbmHist.m_histTester->histEnable = 0; // Turn off enables for hist test patterns from list mode
	m_hbmHist.m_histTester->control = 0; 		// Turn off UseListTestPattern

	m_hbmHist.setTPGPortMask(0);	// Turn off any running internal test patterns
	m_hbmHist.setTPGCont(AXI_HIST_TC_HIST_TPG(AXI_HIST_TPG_NONE));

}

/**
	Read histogram data interleaving chips in 6x2 (or other) multi-chip versions;
*/
void XDmaHexitec::readHistEngGlobColRowTimeInt(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numGlobRows, int firstCC, int numCC, int firstTF, int numTF, int dmaChan, uint32_t *data, bool mapped, bool useClustClass)
{
	uint32_t *buff=NULL;
	int lastGlobRow;
	int firstChipRow, lastChipRow;
	int lastGlobCol;
	int firstChipCol, lastChipCol;
	uint64_t reorderPage = HEXITEC_REORDER_OFFSET_COLROW_CONT;
	int srcnBinsEng;
	int maxNumTF;
	int maxNumCC;
	const char *funcName = mapped?"readMappedEngGlobColRowTime":"readHistEngGlobColRowTime";
	int chip=0;
	
	if (useClustClass)
		funcName = mapped?"readMappedEngGlobColRowCCTime":"readHistEngGlobColRowCCTime";

	if ((m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGCOLROW_GAPS))
		reorderPage = HEXITEC_REORDER_OFFSET_COLROW_GAPS;		// HexitecMHz has gaps at the end of every Column
	else
		reorderPage = HEXITEC_REORDER_OFFSET_COLROW_CONT;		// hexitec would allow contigous seng/col/row
	if (m_numChips == 1)
	{
		readHistEngColRowTimeInt(numEng, firstGlobCol, numGlobCols, firstGlobRow, numGlobRows, firstCC, numCC, firstTF, numTF, 0, dmaChan, data, mapped, useClustClass, true);
		return;
	}
	if (dmaChan < 0)
		dmaChan = m_defaultXDmaChan;
	if (mapped)
	{
		if (numEng > HEXITEC_NBINS_MAPPED/2)
		{
			srcnBinsEng = HEXITEC_NBINS_MAPPED;
			reorderPage = HEXITEC_REORDER_OFFSET_MAPPED_ALL;
		}
		else
		{
			srcnBinsEng = HEXITEC_NBINS_MAPPED/2;
			reorderPage = HEXITEC_REORDER_OFFSET_MAPPED_HALF;
		}
		maxNumTF = m_dataFormat[chip].numTFMapped;
		maxNumCC = 1;
		
	}
	else
	{		
		srcnBinsEng = m_dataFormat[chip].nBinsEng;
		maxNumTF = m_dataFormat[chip].numTF;
		maxNumCC = m_dataFormat[chip].nBinsClustClass;
	}
	
	if (mapped && m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
		throw  XDmaHexitecException("%s: Attempt to read mapped energy, but mappedMode=%d ==OFF", funcName, m_dataFormat[chip].mappedMode);
		
	if (numEng < 1 || numEng > srcnBinsEng)
		throw  XDmaHexitecException("%s: numEng %d is not in range 1... %d", funcName, numEng, srcnBinsEng);
	if (firstGlobRow <0 || firstGlobRow >= HEXITEC_NUM_ROWS*m_numChipRows || numGlobRows <1 || firstGlobRow+numGlobRows > HEXITEC_NUM_ROWS*m_numChipRows)
		throw  XDmaHexitecException("%s: firstRow=%d, numRows=%d is not in range 0... %d", funcName, firstGlobRow, numGlobRows, HEXITEC_NUM_ROWS*m_numChipRows);
	if (firstGlobCol <0 || firstGlobCol >= HEXITEC_NUM_COLS*m_numChipCols || numGlobCols <1 || firstGlobCol+numGlobCols > HEXITEC_NUM_COLS*m_numChipCols)
		throw  XDmaHexitecException("%s: firstCol=%d, numCols=%d is not in range 0... %d", funcName, firstGlobCol, numGlobCols, HEXITEC_NUM_COLS*m_numChipCols);
	if (firstCC < 0 || firstCC > maxNumCC || numCC < 1 || firstCC+numCC > maxNumCC)
		throw  XDmaHexitecException("%s: firstCC=%d, numCC=%d is not in range 0... %d", funcName, firstCC, numCC, maxNumCC);
	if (firstTF < 0 || firstTF > maxNumTF || numTF < 1 || firstTF+numTF > maxNumTF)
		throw  XDmaHexitecException("%s: firstTF=%d, numTF=%d is not in range 0... %d", funcName, firstTF, numTF, maxNumTF);
	if (mapped && m_dataFormat[chip].mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF && 
		((m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY7) || 
		 (m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY_CC12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY_CC7) ||
		 (m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_RUN12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_RUN10LSB) ||
		 (m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_POS_CC12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB) 

		 ) ) 
	{
		// Mapped readout is now OK with Energy Posn, Energy Posn CC, energy only and energy CC mode
	}
	else
	{
		if ((useClustClass && m_dataFormat[chip].memReadAccess != EngPosCCTime) || (!useClustClass && m_dataFormat[chip].memReadAccess != EngPosTime))
			throw  XDmaHexitecException("%s: Current memory layout %d, histMode=0x%04X, mapped=%d,  is not supported by this read function", funcName, 
					m_dataFormat[chip].memReadAccess,  m_dataFormat[chip].histMode, mapped);
	}
	if ( !(m_hbmHist.m_axiReorder & (HEXITEC_REORDER_ENGCOLROW_GAPS | HEXITEC_REORDER_ENGCOLROW_CONT))) 
		throw XDmaHexitecException("%s: UNSUPORTED without VHDL reorder, m_axiReorder=0x%08X", funcName, m_hbmHist.m_axiReorder);

	lastGlobRow = firstGlobRow+numGlobRows-1;
	if (m_numChipRows == 1)
		firstChipRow = lastChipRow=0;
	else
	{
		firstChipRow = firstGlobRow/HEXITEC_NUM_ROWS;
		lastChipRow = lastGlobRow/HEXITEC_NUM_ROWS;
	}
	lastGlobCol = firstGlobCol+numGlobCols-1;
	if (m_numChipCols == 1)
		firstChipCol = lastChipCol = 0;
	else
	{
		firstChipCol = firstGlobCol/HEXITEC_NUM_COLS;
		lastChipCol = lastGlobCol/HEXITEC_NUM_COLS;
	}
	if (firstChipCol==lastChipCol && firstChipRow==lastChipRow)
	{
		readHistEngColRowTimeReorderedHxt(numEng, firstGlobCol-HEXITEC_NUM_COLS*firstChipCol, numGlobCols, firstGlobRow-firstChipRow*HEXITEC_NUM_ROWS, numGlobRows, 
			firstCC, numCC, firstTF, numTF, firstChipRow*m_numChipCols+firstChipCol, dmaChan, data, mapped, useClustClass);
		return;
	}

	if (mapped && (m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGCOLROW_CONT))
	{
		int bufNumRows = numGlobRows;
		if (bufNumRows > HEXITEC_NUM_ROWS)
			bufNumRows = HEXITEC_NUM_ROWS;
		posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*bufNumRows*HEXITEC_NUM_COLS*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngColRowTimeReorderedMHz: Out of memory");
	}
	else if (numEng != srcnBinsEng)
	{
		int bufNumCols = numGlobCols;
		if (bufNumCols > HEXITEC_NUM_COLS)
			bufNumCols = HEXITEC_NUM_COLS;
		posix_memalign((void **)&buff, 4096 /*alignment */ , srcnBinsEng*bufNumCols*sizeof(uint32_t));
		if (buff == nullptr)
			throw runtime_error("readHistEngColRowTimeReorderedMHz: Out of memory");
	}
	
	int bufRow = 0;
	for (int chipRow=firstChipRow; chipRow<=lastChipRow; chipRow++)
	{
		int firstRow, lastRow, numRows;
		if (chipRow==firstChipRow)
			firstRow = firstGlobRow-firstChipRow*HEXITEC_NUM_ROWS;
		else
			firstChipRow = 0;
		if (chipRow == lastChipRow)
			lastRow = lastGlobRow-lastChipRow*HEXITEC_NUM_ROWS;
		else
			lastRow = HEXITEC_NUM_ROWS-1;
		numRows = lastRow+1-firstRow;
		int bufCol=0;
		for(int chipCol=firstChipCol; chipCol <= lastChipCol; chipCol++)
		{
			int firstCol, lastCol, numCols;
			if (chipCol==firstChipCol)
				firstCol = firstGlobCol-firstChipCol*HEXITEC_NUM_COLS;
			else
				firstCol = 0;
			if (chipCol == lastChipCol)
				lastCol = lastGlobCol-lastChipCol*HEXITEC_NUM_COLS;
			else
				lastCol = HEXITEC_NUM_COLS-1;
			numCols = 1+lastCol-firstCol;
			chip = chipRow*m_numChipCols + chipCol;

			uint32_t *chipBase = data+(bufCol+(ptrdiff_t)bufRow*numGlobCols)*numEng;

			for (int it=0; it< numTF; it++)
			{
				for (int icc=0; icc<numCC; icc++)
				{
					int st = (firstTF+it)*maxNumCC+icc+firstCC;
					int dt = it*numCC+icc;
					if (mapped && (m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGCOLROW_CONT))
					{
						// For mapped mode the overhead of going in/out of read suggest that it is better to read all into a buffer and then memcpy out into the correct place
						uint64_t axiAddress;
						if ((m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGCOLROW_GAPS))  // FixMe: This code is not acccessible yet (or needed?) Would be multi chip HextecMHz?
						{
							axiAddress = reorderPage;		// Don't start at specified col, read all cols
							axiAddress += (firstRow)*HEXITEC_MHZ_ROW_STRIDE*srcnBinsEng*sizeof(uint32_t);
							axiAddress += (uint64_t)(st)*HEXITEC_MHZ_ROW_STRIDE*HEXITEC_MHZ_COL_STRIDE*srcnBinsEng*sizeof(uint32_t);
						}
						else
						{
							axiAddress = reorderPage;	// Don't start at specified col, read all cols
							axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
							axiAddress += (firstRow)*HEXITEC_NUM_COLS*srcnBinsEng*sizeof(uint32_t);
							axiAddress += (uint64_t)(st)*HEXITEC_T_STRIDE*srcnBinsEng*sizeof(uint32_t);
						}
						size_t numBytes = sizeof(uint32_t)*srcnBinsEng*HEXITEC_NUM_COLS*numRows;
						m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);

						for(int ir=0; ir<numRows; ir++)
						{
							uint32_t *dp = chipBase + ((ptrdiff_t)numEng)*numGlobCols*(ir+numGlobRows*dt);
							for (int ic=0; ic<numCols; ic++)
							{
								uint32_t *sp = buff + srcnBinsEng*(firstCol+ic+ir*HEXITEC_NUM_COLS);
								memcpy(dp, sp, numEng*sizeof(uint32_t));
								dp += numEng;
							}
						}
					}
					else					
					{
						for(int ir=0; ir<numRows; ir++)
						{
							uint64_t axiAddress;
							uint32_t *dp = chipBase + ((ptrdiff_t)numEng)*numGlobCols*(ir+numGlobRows*dt);
							if ((m_hbmHist.m_axiReorder & HEXITEC_REORDER_ENGCOLROW_GAPS))
							{
								axiAddress = firstCol*srcnBinsEng*sizeof(uint32_t) | reorderPage;
								axiAddress += (ir+firstRow)*HEXITEC_MHZ_ROW_STRIDE*srcnBinsEng*sizeof(uint32_t);
								axiAddress += (uint64_t)(st)*HEXITEC_MHZ_ROW_STRIDE*HEXITEC_MHZ_COL_STRIDE*srcnBinsEng*sizeof(uint32_t);
							}
							else
							{
								axiAddress = firstCol*srcnBinsEng*sizeof(uint32_t) | reorderPage;
								axiAddress |= (uint64_t)chip << HEXITEC_CHIP_SHIFT;
								axiAddress += (ir+firstRow)*HEXITEC_NUM_COLS*srcnBinsEng*sizeof(uint32_t);
								axiAddress += (uint64_t)(st)*HEXITEC_T_STRIDE*srcnBinsEng*sizeof(uint32_t);
							}
							size_t numBytes = sizeof(uint32_t)*srcnBinsEng*numCols;
							if (numEng == srcnBinsEng)
							{
								m_hbmHist.readDma((char *)dp, axiAddress, numBytes, dmaChan);
							}
							else
							{
								m_hbmHist.readDma((char *)buff, axiAddress, numBytes, dmaChan);
								for (int ic=0; ic<numCols; ic++)
								{
									uint32_t *sp = buff + srcnBinsEng*ic;
									memcpy(dp, sp, numEng*sizeof(uint32_t));
									dp += numEng;
								}
							}
						}
					}
				}
			}
			bufCol += numCols;
		}
		bufRow += numRows;
	}
	free (buff);
}

