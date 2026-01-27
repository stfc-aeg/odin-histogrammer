/* cshare_and_format.cpp
Commands to setup the charge sharing summing and corrections and the formatting of the output data.
*/


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
#define MAX_LINE 1024

/**
	Enable or disable various charge sharing corrections.

@param chip			Chip number or -1 to duplicate to all chips.
@param enbEdgePos	Enable charge summing correction where signal shares to give 2 positive signals to a neighbour on a side.
@param enbNegNeb 	Enable charge summing correction where signal shares to give 1 positive signals  with a negative neighbour.
@param disSumming 	Disable charge summing, particularly for the special case of isolating the Fluorescence peaks
@param disAdjPosn	Disable the adjustment of position again particularly for the special case of isolating the Fluorescence peaks
*/
void XDmaHexitec::setCShareMode(int chip, bool enbEdgePos, bool enbNegNeb, bool enbLPos, bool disSumming, bool disAdjPosn)
{
	uint32_t value=0;
	
	if (enbEdgePos)
		value |= HEXITEC_CSHARE_ENB_EDGE_POS_CORR;
	if (enbNegNeb)
			value |= HEXITEC_CSHARE_ENB_NEG_NEB_CORR;
	if (enbLPos)
		value |= HEXITEC_CSHARE_ENB_L_POS_CORR;
	if (disSumming)
		value |= HEXITEC_CSHARE_DIS_SUMMING;
	if (disAdjPosn)
		value |= HEXITEC_CSHARE_DIS_ADJUST_POSN;
	setChipReg(chip, HEXITEC_CHIP_CORR_A, value);
}

/**
	Enable which cluster types are enable into the output data set, others are discarded.

@param chip			Chip number or -1 to duplicate to all chips.
@param enbClusterType	Bitwise OR of flags to enable cluster modes from HEXITEC_ENB_CLUSTER_DEFS
*/
void XDmaHexitec::setClusterTypes(int chip, int enbClusterType)
{
	setChipReg(chip, HEXITEC_CHIP_ENB_CLUSTER, enbClusterType & HEXITEC_CLUSTER_ENB_ALL);
}

/**
	Enable which cluster types are enable into the output data set, others are discarded.

@param chip			Chip number or -1 to duplicate to all chips.
@param histFormat	Histogram format see HEXITEC_FORMAT_DEFS
@param mappedMode	Energy mapped to up to 16 scalar value mode enable. See 
@param histShift	Shift histogram data up 0, 1 or 2 bits to scale energy 1, 2 or 4 to show lower energies with more resolution.
*/
void XDmaHexitec::setHistFormat(int chip, int histFormat, int mappedMode, int histShift)
{
	uint32_t value;
	int firstChip, lastChip;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("setHistFormat: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	interpretHistFormat(chip, histFormat, mappedMode, histShift);

	for (chip=firstChip; chip<=lastChip; chip++)
	{
		value = HEXITEC_HIST_FORMAT_SET(histFormat) | HEXITEC_HIST_MAPPED_MODE_SET(mappedMode) | HEXITEC_HIST_FORMAT_SHIFT_SET(histShift);
		setChipReg(chip, HEXITEC_CHIP_FORMAT, value);
	}
}

void  XDmaHexitec::interpretHistFormat(int chip, int histFormat, int mappedMode, int histShift)
{
	int firstChip, lastChip;
	int nBinsEng=4096;
	int numTF=1;
	int numTFMapped=0;
	int numTFMappedMHz=0;
	int numTFMappedHxt=0;
	int hbmPerChip=1;
	enum XDmaHexitec::MemReadAccess memReadAccess;
	int nBins[4];
	int numTFDivide;
	int nBinsClustClass;
	bool usesPosn=false;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("interpretHistFormat: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	nBins[0] = 0;
	nBins[1] = 0;
	nBins[2] = 0;
	nBins[3] = 0;
	if (m_generation == HexitecGenMHz)
	{
		numTFDivide = 32;
	}
	else
	{
		numTFDivide = 256;
		hbmPerChip = 2;
	}
	nBinsClustClass = 1;
	switch (histFormat)
	{
	case HEXITEC_HIST_FORMAT_RUN12:			//!< Normal run mode, Row, column and 4096 energy bins
		nBinsEng = 1<<12;
		memReadAccess = EngPosTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CC12:		//!< Debug mode with timeframe, 4 bits cluster class, col, row and 4096 bin of energy.
		if (m_generation == HexitecGenMHz)
			nBinsClustClass = 16;
		else 
			nBinsClustClass =  8;	// Special case for Hexitec 6x2, onlt the first 8 CC are available.
		nBinsEng = 1<<12;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CG12:		//!< Production or debug mode with timeframe, 1 bits cluster grade, col, row and 4096 bin of energy.
		if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("setHistFormat: This firmware does not support Cluster Grade");
		nBinsClustClass = 1 << m_maxBitsClusterGrade;
		nBinsEng = 1<<12;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;

	case HEXITEC_HIST_FORMAT_RUN11:			//!< Normal run mode, Row, column and 2048 energy bins		 
		nBinsEng = 1<<11;
		memReadAccess = EngPosTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CC11:		//!< Debug mode with timeframe, 4 bits cluster class, col, row and 2048 bin of energy.
		nBinsClustClass = 16;
		nBinsEng = 1<<11;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CG11:		//!< Production or debug mode with timeframe, 1 bits cluster grade, col, row and 2048 bin of energy.
		if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("setHistFormat: This firmware does not support Cluster Grade");
		nBinsClustClass = 1 << m_maxBitsClusterGrade;
		nBinsEng = 1<<11;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;

	case HEXITEC_HIST_FORMAT_RUN10:			//!< Normal run mode, Row, column and 1024 energy bins		 
	case HEXITEC_HIST_FORMAT_RUN10LSB:			
		nBinsEng = 1<<10;
		memReadAccess = EngPosTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CC10:		//!< Debug mode with timeframe, 4 bits cluster class, col, row and 1024 bins of energy.
	case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:		
		nBinsClustClass = 16;
		nBinsEng = 1<<10;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CG10:		//!< Production or debug mode with timeframe, 1 bits cluster grade, col, row and 1024 bins of energy.
	case HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB:		//!< Production or debug mode with timeframe, 1 bits cluster grade, col, row and lower 1024 bins of energy.
		if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("setHistFormat: This firmware does not support Cluster Grade");
		nBinsClustClass = 1 << m_maxBitsClusterGrade;
		nBinsEng = 1<<10;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;

	case HEXITEC_HIST_FORMAT_RUN9:			//!< Normal run mode, Row, column and 512  energy bins		 
		nBinsEng = 1<<9;
		memReadAccess = EngPosTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CC9:		//!< Debug mode with timeframe, 4 bits cluster class, col, row and 512 bin of energy.
		nBinsClustClass = 16;
		nBinsEng = 1<<9;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CG9:		//!< Production or debug mode with timeframe, 1 bits cluster grade, col, row and 512 bins of energy.
		if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("setHistFormat: This firmware does not support Cluster Grade");
		nBinsClustClass = 1 << m_maxBitsClusterGrade;
		nBinsEng = 1<<9;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;

	case HEXITEC_HIST_FORMAT_RUN8:			//!< Normal run mode, Row, column and 256  energy bins		 
		nBinsEng = 1<<8;
		memReadAccess = EngPosTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CC8:		//!< Debug mode with timeframe, 4 bits cluster class, col, row and 256 bin of energy.
		nBinsClustClass = 16;
		nBinsEng = 1<<8;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CG8:		//!< Production or debug mode with timeframe, 1 bits cluster grade, col, row and 256 bins of energy.
		if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("setHistFormat: This firmware does not support Cluster Grade");
		nBinsClustClass = 1 << m_maxBitsClusterGrade;
		nBinsEng = 1<<8;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;

	case HEXITEC_HIST_FORMAT_RUN7:			//!< Normal run mode, Row, column and 128  energy bins		 
		nBinsEng = 1<<7;
		memReadAccess = EngPosTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CC7:		//!< Debug mode with timeframe, 4 bits cluster class, col, row and 128 bin of energy.
		nBinsClustClass = 16;
		nBinsEng = 1<<7;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_ENG_POS_CG7:		//!< Production or debug mode with timeframe, 1 bits cluster grade, col, row and 128 bins of energy.
		if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("setHistFormat: This firmware does not support Cluster Grade");
		nBinsClustClass = 1 << m_maxBitsClusterGrade;
		nBinsEng = 1<<7;
		memReadAccess = EngPosCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(numTFDivide*nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(128*HEXITEC_NBINS_MAPPED);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(2048*HEXITEC_NBINS_MAPPED);
		usesPosn=true;
		break;

	case HEXITEC_HIST_FORMAT_ENG_ONLY12		:		//!< 0-d energy only run mode with row and column removed and 4096 energy bins		 
		nBinsEng = 1<<12;
		memReadAccess = EngTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12		:		//!< 0-d energy only run mode with row and column removed and 4096 energy bins		 
		nBinsClustClass = 16;
		nBinsEng = 1<<12;
		memReadAccess = EngCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;

	case HEXITEC_HIST_FORMAT_ENG_ONLY11		:		//!< 0-d energy only run mode with row and column removed and 2048 energy bins				 
		nBinsEng = 1<<11;
		memReadAccess = EngTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11		:		//!< 0-d energy only run mode with row and column removed and 2048 energy bins				 
		nBinsClustClass = 16;
		nBinsEng = 1<<11;
		memReadAccess = EngCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;

	case HEXITEC_HIST_FORMAT_ENG_ONLY10		:		//!< 0-d energy only run mode with row and column removed and 1024 energy bins				 
		nBinsEng = 1<<10;
		memReadAccess = EngTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10		:		//!< 0-d energy only run mode with row and column removed and 1024 energy bins				 
		nBinsClustClass = 16;
		nBinsEng = 1<<10;
		memReadAccess = EngCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;

	case HEXITEC_HIST_FORMAT_ENG_ONLY9		:		//!< 0-d energy only run mode with row and column removed and 512  energy bins				 
		nBinsEng = 1<<9;
		memReadAccess = EngTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9		:		//!< 0-d energy only run mode with row and column removed and 512  energy bins				 
		nBinsClustClass = 16;
		nBinsEng = 1<<9;
		memReadAccess = EngCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;

	case HEXITEC_HIST_FORMAT_ENG_ONLY8		:		//!< 0-d energy only run mode with row and column removed and 256  energy bins				 
		nBinsEng = 1<<8;
		memReadAccess = EngTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8		:		//!< 0-d energy only run mode with row and column removed and 256  energy bins				 
		nBinsClustClass = 16;
		nBinsEng = 1<<8;
		memReadAccess = EngCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;

	case HEXITEC_HIST_FORMAT_ENG_ONLY7		:		//!< 0-d energy only run mode with row and column removed and 128  energy bins				 
		nBinsEng = 1<<7;
		memReadAccess = EngTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7		:		//!< 0-d energy only run mode with row and column removed and 128  energy bins				 
		nBinsClustClass = 16;
		nBinsEng = 1<<7;
		memReadAccess = EngCCTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(nBinsEng*nBinsClustClass);
		numTFMappedMHz = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED*2);
		numTFMappedHxt = m_hbmHist.m_histConf.HistWordsPerStream/(512*HEXITEC_NBINS_MAPPED);
		break;


	case HEXITEC_HIST_FORMAT_CALIB_COMB12	:	 	//!< Calibration mode overlaying all pixels, 4 bits of Cluster class, 1024 bins of LUT address and 4096 energy bins
		nBinsEng = 1<<12;
		nBinsClustClass = 16;
		memReadAccess = EngCalibCC;
		numTF = 1;
		break;
	case HEXITEC_HIST_FORMAT_CALIB_COMB11	:	 	//!< Calibration mode overlaying all pixels, 4 bits of Cluster class, 1024 bins of LUT address and 2048 energy bins
		nBinsEng = 1<<11;
		nBinsClustClass = 16;
		memReadAccess = EngCalibCC;
		numTF = 1;
		break;
	case HEXITEC_HIST_FORMAT_CALIB_COMB10	:	 	//!< Calibration mode overlaying all pixels, 4 bits of Cluster class, 1024 bins of LUT address and 1024 energy bins
		nBinsEng = 1<<10;
		nBinsClustClass = 16;
		memReadAccess = EngCalibCC;
		numTF = 1;
		break;
	case HEXITEC_HIST_FORMAT_CALIB_COMBTYPE	:	 	//!< Calibration mode overlaying all pixels, all 6 Cluster type bits, 1024 bins of LUT address and 1024 energy bins 		 
		nBinsEng = 1<<10;
		nBinsClustClass = 64;
		memReadAccess = EngCalibCC;
		numTF = 1;
		break;
	case HEXITEC_HIST_FORMAT_CALIB_SEPARATE	:		//!< Calibration mode separating pixels, 256 bins of LUT address and 512 energy bins 		  
		nBinsEng = 1<<9;
		memReadAccess = EngPosTime;
		numTF = m_hbmHist.m_histConf.HistWordsPerStream/(nBinsEng);
		usesPosn=true;
		break;
	case HEXITEC_HIST_FORMAT_CHARAC2D12		:			//!< Characterisation plot for up to 2 pixel clusters
		memReadAccess = Charac2d;
		nBinsEng = 0;
		nBins[0] = 4096;
		nBins[1] = 8192;
		numTF = 1;
		break;

	case HEXITEC_HIST_FORMAT_CHARAC2D10		:			//!< Characterisation plot for up to 2 pixel clusters
		memReadAccess = Charac2d;
		nBinsEng = 0;
		nBins[0] = 1024;
		nBins[1] = 2048;
		numTF = 1;
		break;
		
	case HEXITEC_HIST_FORMAT_CHARAC3D		:			//!< Characterisation plot for up to 3 pixel clusters
		memReadAccess = Charac3d;
		nBinsEng = 0;
		nBins[0] = 512;
		nBins[1] = 256;
		nBins[2] = 512;
		numTF = 1;
		break;

	case HEXITEC_HIST_FORMAT_CHARAC4D		:			//!< Characterisation plot for up to 4 pixel clusters
		nBinsEng = 1;
		memReadAccess = Special;
		numTF = 1;
		break;
	default:
		throw XDmaHexitecException("setHistFormat: Unknown histFormat=%d", histFormat);
		
	}
	if (m_generation == HexitecGenMHz)
	{	
		if (mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
			numTFMapped = 0;
		else if (mappedMode == HEXITEC_HIST_MAPPED_MODE_ONLY)
		{
			numTF = 0;
			numTFMapped = m_hbmHist.m_histConf.HistWordsPerStream/(32*HEXITEC_NBINS_MAPPED);
		}
		else if (mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
		{
			numTFMapped = numTFMappedMHz;
			if ((histFormat >= HEXITEC_HIST_FORMAT_ENG_ONLY12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_ONLY7) ||
				(histFormat >= HEXITEC_HIST_FORMAT_ENG_ONLY_CC12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_ONLY_CC7))
				numTF /= 2;		// In energy only modes, use half memory for mapped and half for energy only spectra.
			else if ((histFormat >= HEXITEC_HIST_FORMAT_RUN12  && histFormat <= HEXITEC_HIST_FORMAT_RUN10LSB) ||
					 (histFormat >= HEXITEC_HIST_FORMAT_ENG_POS_CC12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB) || 
					 (histFormat >= HEXITEC_HIST_FORMAT_ENG_POS_CG12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB)	 )
			{
				/* In these modes numTf is unchanged, the mapped image is interleaved */
			}
			else
				throw XDmaHexitecException("setHistFormat: Interleaved mapped mode is incompatible with histFormat=%d", histFormat);
		}
		else
			throw XDmaHexitecException("setHistFormat: Unknown mappedMode=%d", mappedMode);
	}
	else
	{
		if (mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
			numTFMapped = 0;
		else if (mappedMode == HEXITEC_HIST_MAPPED_MODE_ONLY)
		{
			numTF = 0;
			numTFMapped = m_hbmHist.m_histConf.HistWordsPerStream*hbmPerChip/(512*HEXITEC_NBINS_MAPPED);
		}
		else if (mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
		{
			numTFMapped = numTFMappedHxt;
			if ((histFormat >= HEXITEC_HIST_FORMAT_ENG_ONLY12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_ONLY7) ||
				(histFormat >= HEXITEC_HIST_FORMAT_ENG_ONLY_CC12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_ONLY_CC7))
				numTF /= 2;		// In energy only modes, use half memory for mapped and half for energy only spectra.
			else if ((histFormat >= HEXITEC_HIST_FORMAT_RUN12  && histFormat <= HEXITEC_HIST_FORMAT_RUN10LSB) ||
					 (histFormat >= HEXITEC_HIST_FORMAT_ENG_POS_CC12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB) || 
					 (histFormat >= HEXITEC_HIST_FORMAT_ENG_POS_CG12 && histFormat <= HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB)	 )
			{
				/* In these modes numTf is unchanged, the mapped image is interleaved */
			}
			else
				throw XDmaHexitecException("setHistFormat: Interleaved mapped mode is incompatible with histFormat=%d", histFormat);
		}
		else
			throw XDmaHexitecException("setHistFormat: Unknown mappedMode=%d", mappedMode);
	}

	printf("interpretHistFormat: histFormat=0x%04X, mappedMode=%d => nBinsEng=%d, nBinsClustClass=%d, numTF=%d, numTFMapped=%d\n", histFormat, mappedMode, 
																																	nBinsEng, nBinsClustClass, numTF, numTFMapped);
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		m_dataFormat[chip].histMode = histFormat;
		m_dataFormat[chip].nBinsEng = nBinsEng;
		m_dataFormat[chip].nBinsClustClass = nBinsClustClass;
		m_dataFormat[chip].memReadAccess = memReadAccess;
		m_dataFormat[chip].numTF = numTF;
		m_dataFormat[chip].numTFMapped = numTFMapped;
		m_dataFormat[chip].mappedMode = mappedMode;
		m_dataFormat[chip].usesPosn = usesPosn;
		m_dataFormat[chip].histShift = histShift;
		for (int i=0; i<4; i++)
			m_dataFormat[chip].nBins[i] = nBins[i];
	}
}

/**
	Read histogram format from the firmware, update internal data structures and return the settings if required

@param chip			Chip number or -1 to scan all chips
@param histFormatP	Pointer to return histogram format see HEXITEC_FORMAT_DEFS
@param mappedModeP	Pointer to return energy mapped to up to 16 scalar value mode enable. See 
*/
void XDmaHexitec::getHistFormat(int chip, int *histFormatP, int *mappedModeP, int *histShiftP)
{
	int histFormat, mappedMode, histShift;
	int firstChip, lastChip;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("getHistFormat: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	for (chip=firstChip; chip<=lastChip; chip++)
	{
		uint32_t reg;
		reg = getChipReg(chip, HEXITEC_CHIP_FORMAT);
		histFormat = HEXITEC_HIST_FORMAT_GET(reg);
		mappedMode = HEXITEC_HIST_MAPPED_MODE_GET(reg);
		histShift  = HEXITEC_HIST_FORMAT_SHIFT_GET(reg);
		interpretHistFormat(chip, histFormat, mappedMode, histShift);
	}
	if (histFormatP != nullptr)
		*histFormatP = histFormat;
	if (mappedModeP != nullptr)
		*mappedModeP = mappedMode;
	if (histShiftP != nullptr)
		*histShiftP = histShift;
}

int XDmaHexitec::getnBinsEng(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getNBinsEng: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].nBinsEng;
}
bool XDmaHexitec::getEngLsb10(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getEngLsb10: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].histMode==HEXITEC_HIST_FORMAT_RUN10LSB || m_dataFormat[chip].histMode==HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB || m_dataFormat[chip].histMode==HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB;
}

bool XDmaHexitec::getUsePosn(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getUsePosn: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].usesPosn;
}

int XDmaHexitec::getHistShift(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getHistShift: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].histShift;
}

int XDmaHexitec::getnBinsClustClass(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getnBinsClustClass: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].nBinsClustClass;
}
int XDmaHexitec::getNumTF(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getNumTF: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].numTF;
}
int XDmaHexitec::getNumTFMapped(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getNumMappedTF: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].numTFMapped;
}
void XDmaHexitec::getnBinsCharac(int chip, int nBins[4])
{
	int i;
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getnBinsCharac: chip=%d out of range 0...%d", chip, m_numChips-1);
	for (i=0; i<4; i++)
		nBins[i] = m_dataFormat[chip].nBins[i];
}


bool XDmaHexitec::getEngOnly(int chip)
{
	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("getEngOnly: chip=%d out of range 0...%d", chip, m_numChips-1);
	
	return 	(m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY7) ||
			(m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_ONLY_CC12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_ONLY_CC7);
}

bool XDmaHexitec::getUseClustGrade(int chip)
{
	if (chip <0 || chip >= m_numChips)
		throw XDmaHexitecException("getUseClustGrade: chip=%d out of range 0...%d", chip, m_numChips-1);
	return m_dataFormat[chip].histMode >= HEXITEC_HIST_FORMAT_ENG_POS_CG12 && m_dataFormat[chip].histMode <= HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB;
}

/**
	Enable or disable various charge sharing corrections.

@param chip			Chip number or -1 to duplicate to all chips.
@param enbEdgePos	Enable charge summing correction where signal shares to give 2 positive signals to a neighbour on a side.
@param enbNegNeb 	Enable charge summing correction where signal shares to give 1 positive signals  with a negative neighbour.
*/




void XDmaHexitec::loadCShareAscii(int chip, int region, char *fullName)
{
	int i;
	uint32_t m[HEXITEC_MAX_REGION_SIZE];
	FILE *inpf;
	int scale;
	
	switch (region)
	{
	case HEXITEC_REGION_EDGE_POS_M:
		scale = HEXITEC_EDGE_POS_SCALE_M;
		break;
	case HEXITEC_REGION_EDGE_POS_C:
		scale = HEXITEC_EDGE_POS_SCALE_C;
		break;
	case HEXITEC_REGION_NEG_NEB_M:
		scale = HEXITEC_NEG_NEB_SCALE_M;
		break;
	case HEXITEC_REGION_NEG_NEB_C:
		scale = HEXITEC_NEG_NEB_SCALE_C;
		break;
	case HEXITEC_REGION_L_POS_M:
		scale = HEXITEC_L_POS_SCALE_M;
		break;
	case HEXITEC_REGION_L_POS_C:
		scale = HEXITEC_L_POS_SCALE_C;
		break;
	default:
		throw XDmaHexitecException("loadCShareAscii: region=%d, is not a charge sharing correction lookup table", region);
	}
		
	
	inpf = fopen (fullName, "r");
	if (inpf == nullptr)
	{
		throw  XDmaHexitecException("loadCShareAscii: Cannot open file %s, errno=%d", fullName, errno);
	}

	for (i=0; i<m_regionSize[region]; i++)
	{
		double x;
		if (fscanf(inpf, "%lg", &x) != 1)
		{
			fclose(inpf);
			throw  XDmaHexitecException("loadCShareAscii: Cannot open read data at point=%d", i);
		}
		m[i] =  x*(double)scale;
	}
	fclose(inpf);
	writeSharedLUT(chip, region, -1, 0, m_regionSize[region], m);
}

void XDmaHexitec::loadCShareAsciiMC(int chip, int region, char *fullName)
{
	int i;
	int regionM, regionC;
	uint32_t m[HEXITEC_MAX_REGION_SIZE];
	uint32_t c[HEXITEC_MAX_REGION_SIZE];
	FILE *inpf;
	int scaleM, scaleC;
	
	switch (region)
	{
	case HEXITEC_REGION_EDGE_POS_M:
	case HEXITEC_REGION_EDGE_POS_C:
		scaleM = HEXITEC_EDGE_POS_SCALE_M;
		scaleC = HEXITEC_EDGE_POS_SCALE_C;
		regionM = HEXITEC_REGION_EDGE_POS_M;
		regionC = HEXITEC_REGION_EDGE_POS_C;
		break;
	case HEXITEC_REGION_NEG_NEB_M:
	case HEXITEC_REGION_NEG_NEB_C:
		scaleM = HEXITEC_NEG_NEB_SCALE_M;
		scaleC = HEXITEC_NEG_NEB_SCALE_C;
		regionM = HEXITEC_REGION_NEG_NEB_M;
		regionC = HEXITEC_REGION_NEG_NEB_C;
		break;
	case HEXITEC_REGION_L_POS_M:
	case HEXITEC_REGION_L_POS_C:
		scaleM = HEXITEC_L_POS_SCALE_M;
		scaleC = HEXITEC_L_POS_SCALE_C;
		regionM = HEXITEC_REGION_L_POS_M;
		regionC = HEXITEC_REGION_L_POS_C;
		break;
	default:
		throw XDmaHexitecException("loadCShareAsciiMC: region=%d, is not a charge sharing correction lookup table", region);
	}
		
	
	inpf = fopen (fullName, "r");
	if (inpf == nullptr)
	{
		throw  XDmaHexitecException("loadCShareAsciiMC: Cannot open file %s, errno=%d", fullName, errno);
	}


	for (i=0; i<m_regionSize[regionM]; i++)
	{
		double am, ac;
		if (fscanf(inpf, "%lg %lg", &am, &ac) != 2)
		{
			fclose(inpf);
			throw  XDmaHexitecException("loadCShareAsciiMC: Cannot read data at point=%d", i);
		}
		m[i] =  am*(double)scaleM;
		c[i] =  ac*(double)scaleC;
	}
	fclose(inpf);
	writeSharedLUT(chip, regionM, -1, 0, m_regionSize[regionM], m);
	writeSharedLUT(chip, regionC, -1, 0, m_regionSize[regionC], c);
}


/**
	Load Energy mapping LUT from ASCII file.
	File format:
	
							... All others mapped to 0.
	minEng1	maxEng1			... Mapped to 1
	minEng2	maxEng2			... Mapped to 2
	....
	minEng14 maxEng14		... mapped to 14
	minEng15 maxEng15		... mapped to 15

@param chip			Chip number or -1 to duplicate to all chips.
@param fileName		File name of ASCII file containing minimum and maximum energies (0...4095) for mapped values 1..15
*/
#define MAXLINE 1024
void XDmaHexitec::loadEngMapAscii(int chip, char *fileName)
{
	uint32_t engMap[HEXITEC_NBINS_ENG_MAX];
	int eng;
	FILE *inpf;
	char lineBuf[MAXLINE+2];
	int mapped=1;
	
	if ((inpf=fopen(fileName, "r")) == nullptr)
		throw  XDmaHexitecException("loadEngMapAscii: Cannot open file %s, errno=%d", fileName, errno);
		
	for (eng=0; eng<HEXITEC_NBINS_ENG_MAX; eng++)
		engMap[eng] = 0;
	while (fgets(lineBuf, MAXLINE, inpf) != nullptr)
	{
		if (lineBuf[0]=='#')
			continue;
		int engMin, engMax;
		if (sscanf(lineBuf, "%d %d", &engMin, &engMax) != 2)
		{
			fclose(inpf);
			throw  XDmaHexitecException("loadEngMapAscii: Cannot parse engMin and engMax from line '%s'", lineBuf);
		}
		if (engMin < 0 || engMin >= HEXITEC_NBINS_ENG_MAX || engMax < 0 || engMax >= HEXITEC_NBINS_ENG_MAX || engMax < engMin)
		{
			fclose(inpf);
			throw  XDmaHexitecException("loadEngMapAscii: engMin=%d or engMax=%d out of range 0...%d or engMin > engMax", engMin, engMax);
		}
		if (mapped > 15)
		{
			fclose(inpf);
			throw  XDmaHexitecException("loadEngMapAscii: Too many mapped values, 1..15 mapped values supported");
		}
		for (eng=engMin; eng<=engMax; eng++)
			engMap[eng] = mapped;
		mapped++;
	}
	fclose(inpf);
	writeSharedLUT(chip, HEXITEC_REGION_ENG_MAP, -1, 0, HEXITEC_NBINS_ENG_MAX, engMap);	
}

/**
	Initial energy map so all energies above threshold are mapped to 1, below are mapped to 0

@param chip			Chip number or -1 to duplicate to all chips.
@param thres		Threshold (0...4095)
*/
void  XDmaHexitec::initEngMapThres(int chip, int thres)
{
	uint32_t engMap[HEXITEC_NBINS_ENG_MAX];
	int eng;
	
	if (thres <0 || thres >= HEXITEC_NBINS_ENG_MAX)
		throw  XDmaHexitecException("initEngMapThres: Threshold %d is out of range 0...%d", thres, HEXITEC_NBINS_ENG_MAX-1);
		
	for (eng=0; eng<thres; eng++)
		engMap[eng] = 0;
	for (eng=thres; eng<HEXITEC_NBINS_ENG_MAX; eng++)
		engMap[eng] = 1;

	writeSharedLUT(chip, HEXITEC_REGION_ENG_MAP, -1, 0, HEXITEC_NBINS_ENG_MAX, engMap);	
}

/**
	Initial Pixel mask LUT so that all pixels are enabled

@param chip			Chip number or -1 to duplicate to all chips.
*/
void  XDmaHexitec::initPixelMask(int chip)
{
	int firstChip, lastChip;
	int i;
	volatile uint32_t *selAddr, *p, *chipSel;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("initPixelMask: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			p = m_regs+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
		}
		else
		{
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
			p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
		}
		if (m_generation == HexitecGenMHz)
			*selAddr = HEXITEC_SEL_ADDR_BROADCAST;
		else
			*selAddr = 0;
		for (i=0; i<m_regionSize[HEXITEC_REGION_PIX_MASK]; i++)
			*p++ = 0;
	}
}

/**
	Set or clear individual pixel LUT entry.

@param chip			Chip number or -1 to duplicate to all chips.
*/
void  XDmaHexitec::setPixelMask(int chip, int col, int row, bool disable)
{
	volatile uint32_t *selAddr, *p, *chipSel;
	int firstChip, lastChip;
	int firstRow, lastRow, firstCol, lastCol;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("setPixelMask: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	if (col < 0)
	{
		firstCol = 0;
		lastCol  = HEXITEC_NUM_COLS-1;
	}
	else if (col >= HEXITEC_NUM_COLS)
		throw XDmaHexitecException("setPixelMask: col=%d is out of range 0...%d", col, HEXITEC_NUM_COLS-1);
	else
		firstCol = lastCol = col;
	if (row < 0)
	{
		firstRow=0;
		lastRow = HEXITEC_NUM_ROWS-1;
	}
	else if (row >= HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("setPixelMask: row=%d is out of range 0...%d", row, HEXITEC_NUM_ROWS-1);
	else
		firstRow = lastRow = row;
		
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			p = m_regs+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
		}
		else
		{
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
			p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
		}
		if (m_generation == HexitecGenMHz)
		{
			for (col=firstCol; col<=lastCol; col++)
			{
				int c = (col>=40)?col+24:col;
				int colOffset= (c & 1) << 7 ;
				colOffset |= (c & 0x40) << 2;
				*selAddr = (c & 0x3E) >> 1;
				for (row=firstRow; row<=lastRow; row++)
				{
					int bitOffset = colOffset | row;
					int wordOffset = bitOffset/32;
					bitOffset %= 32;
					printf("setPixelMask: row=%d, col=%d, wordOffset=%d, bitOffset=%d\n", row, col, wordOffset, bitOffset);
					uint32_t pixMask = p[wordOffset];
					if (disable)
						pixMask |= 1<<bitOffset;
					else
						pixMask &= ~ (1<<bitOffset);
					p[wordOffset] = pixMask;
				}
			}
		}
		else
		{
			*selAddr = 0;
			for (row=firstRow; row<=lastRow; row++)
			{
				for (col=firstCol; col<=lastCol; col++)
				{
					int bitOffset = row*HEXITEC_NUM_COLS+col;
					int wordOffset = bitOffset/32;
					bitOffset %= 32;
					uint32_t pixMask = p[wordOffset];
					if (disable)
						pixMask |= 1<<bitOffset;
					else
						pixMask &= ~ (1<<bitOffset);
					p[wordOffset] = pixMask;
				}
			}
		}
	}
}


/**
	Write output pixel mask from a mask arranged as uint8_t disableMask[row][col].
	This system independent array is packed/unpacked to suit the generation of Hexitec.

@param chip			Chip number or -1 to duplicate to all chips.
@param firstCol		First Column
@param numCols		Number of columns
@param firstRow		First row
@param numRows		Number of rows
@param disableMask	Arrays of format uint8_t disableMask[row][col] where nonzero pixels are disabled in data output
*/
void  XDmaHexitec::writePixelMask(int chip, int firstCol, int numCols, int firstRow, int numRows, uint8_t *disableMask)
{
	volatile uint32_t *selAddr, *p, *chipSel;
	int firstChip, lastChip;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("writePixelMask: chip=%d out of range 0...%d", chip, m_numChips-1);
	else
		firstChip=lastChip=chip;

	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("writePixelMask: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("writePixelMask: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);

	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (m_useChipSel)
		{
			chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
			*chipSel = chip;
			selAddr = m_regs;
			p = m_regs+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
		}
		else
		{
			selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
			p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
		}
		if (m_generation == HexitecGenMHz)
		{
			for (int col=firstCol; col<firstCol+numCols; col++)
			{
				int c = (col>=40)?col+24:col;
				int colOffset= (c & 1) << 7 ;
				colOffset |= (c & 0x40) << 2;
				*selAddr = (c & 0x3E) >> 1;
				uint32_t rowBuf[3];
				if (!(firstRow == 0 && numRows == HEXITEC_NUM_ROWS))
				{
					for (int i=0; i<3; i++)
						rowBuf[i] = p[colOffset/32+i];
				}
				else
				{
					for (int i=0; i<3; i++)
						rowBuf[i] = 0;
				}
				for (int row=firstRow; row<firstRow+numRows; row++)
				{
					int bitOffset =  row % 32;
					int wordOffset = row/32;

					int disableMask_addr = ((numCols-firstCol)*(row-firstRow)) + (col-firstCol);
					if (disableMask[disableMask_addr])
						rowBuf[wordOffset] |= 1<<bitOffset;
					else
						rowBuf[wordOffset] &= ~ (1<<bitOffset);
				}
				for (int i=0; i<3; i++)
					p[colOffset/32+i] = rowBuf[i];
			}
		}
		else
		{
			*selAddr = 0;
			for (int row=firstRow; row<firstRow+numRows; row++)
			{
				uint32_t pixMask = 0;
				for (int col=firstCol; col<firstCol+numCols; col++)
				{
					int bitOffset = row*HEXITEC_NUM_COLS+col;
					int wordOffset = bitOffset/32;
					bitOffset %= 32;
					if (col==firstCol || bitOffset == 0)	// Each new word read from HW
						pixMask = p[wordOffset];
					if (disableMask[row*HEXITEC_NUM_COLS+col])
						pixMask |= 1<<bitOffset;
					else
						pixMask &= ~ (1<<bitOffset);
					if (col==firstCol+numCols-1 || bitOffset==31)
						p[wordOffset] = pixMask;
				}
			}
		}
	}
}


/**
	Read output pixel mask from a mask arranged as uint8_t disableMask[row][col].
	This system independent array is packed/unpacked to suit the generation of Hexitec.

@param chip			Chip number or -1 to duplicate to all chips.
@param firstCol		First Column
@param numCols		Number of columns
@param firstRow		First row
@param numRows		Number of rows
@param disableMask	Arrays of format uint8_t disableMask[row][col] where nonzero pixels are disabled in data output
*/
void  XDmaHexitec::readPixelMask(int chip, int firstCol, int numCols, int firstRow, int numRows, uint8_t *disableMask)
{
	volatile uint32_t *selAddr, *p, *chipSel;

	if (chip < 0 || chip >= m_numChips)
		throw XDmaHexitecException("readPixelMask: chip=%d out of range 0...%d", chip, m_numChips-1);

	if (firstCol < 0 || firstCol >= HEXITEC_NUM_COLS || numCols < 1 || firstCol+numCols > HEXITEC_NUM_COLS)
		throw XDmaHexitecException("readPixelMask: firstCol=%d, numCols=%s out of range 0...%d", firstCol, numCols, HEXITEC_NUM_COLS);
	if (firstRow < 0 || firstRow >= HEXITEC_NUM_ROWS || numRows < 1 || firstRow+numRows > HEXITEC_NUM_ROWS)
		throw XDmaHexitecException("readPixelMask: firstRow=%d, numRows=%s out of range 0...%d", firstRow, numRows, HEXITEC_NUM_ROWS);

	if (m_useChipSel)
	{
		chipSel = m_regs+m_globOffset/sizeof(uint32_t)+HEXITEC_GLB_SCOPE_CHIP_SEL;
		*chipSel = chip;
		selAddr = m_regs;
		p = m_regs+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
	}
	else
	{
		selAddr = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip);
		p = m_regs+(HEXITEC_NUM_REGIONS*HEXITEC_REGION_OFFSET*chip)+HEXITEC_REGION_OFFSET*HEXITEC_REGION_PIX_MASK;
	}
	if (m_generation == HexitecGenMHz)
	{
		for (int col=firstCol; col<firstCol+numCols; col++)
		{
			int c = (col>=40)?col+24:col;
			int colOffset= (c & 1) << 7 ;
			colOffset |= (c & 0x40) << 2;
			*selAddr = (c & 0x3E) >> 1;
			uint32_t rowBuf[3];
			for (int i=0; i<3; i++)
				rowBuf[i] = p[colOffset/32+i];
			for (int row=firstRow; row<firstRow+numRows; row++)
			{
				int bitOffset =  row % 32;
				int wordOffset = row/32;
				int disableMask_addr = ((numCols-firstCol)*(row-firstRow)) + (col-firstCol);
				disableMask[disableMask_addr] = !!(rowBuf[wordOffset] & (1<<bitOffset));
			}
		}
	}
	else
	{
		*selAddr = 0;
		for (int row=firstRow; row<firstRow+numRows; row++)
		{
			uint32_t pixMask = 0;
			for (int col=firstCol; col<firstCol+numCols; col++)
			{
				int bitOffset = row*HEXITEC_NUM_COLS+col;
				int wordOffset = bitOffset/32;
				bitOffset %= 32;
				if (col==firstCol || bitOffset == 0)	// Each new word read from HW
					pixMask = p[wordOffset];
				disableMask[row*HEXITEC_NUM_COLS+col] = !!(	pixMask & 1<<bitOffset);
			}
		}
	}
}



void XDmaHexitec::loadBadPixelsOutputAscii(char *fullName)
{
	int chip;
	int row, col;
	int i;
	char lbuf[MAX_LINE+2];
	FILE *inpf= nullptr;
	
	inpf = fopen (fullName, "r");
	if (inpf == nullptr)
	{
		throw  XDmaHexitecException("loadBadPixelsOutputAscii: Cannot open file %s, errno=%d", fullName, errno);
	}

	i = 1;
	while (fgets(lbuf, MAX_LINE, inpf) != nullptr)
	{
		if (sscanf(lbuf, "%d %d %d", &chip, &row, &col) != 3)
		{
			fclose(inpf);
			throw  XDmaHexitecException("loadBadPixelsOutputAscii: Cannot bad pixel chip, row, col at line %d : '%s'", i, lbuf);
		}
		printf("Disabling output at chip=%d, row=%d, col=%d\n", chip, row, col);
		setPixelMask(chip, col, row, true);
		i++;
	}
	fclose(inpf);
}

void XDmaHexitec::setClusterGradeReg(int chip, uint32_t value)
{
	setChipReg(chip, HEXITEC_CHIP_CLUSTER_GRADE, value);
}
		

void XDmaHexitec::setClusterGrade(int chip, int gradeMap[HEXITEC_CLUSTER_CLASS_NUMBER])
{
	if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("setClusterGrade: This firmware does not support Cluster Grade");

	uint32_t value;
	uint32_t mask = (1 << m_maxBitsClusterGrade)-1;
	for (int i=0; i<HEXITEC_CLUSTER_CLASS_NUMBER; i++)
		value |= (gradeMap[i] & mask) << (i*m_maxBitsClusterGrade);

	setChipReg(chip, HEXITEC_CHIP_CLUSTER_GRADE, value);
}
void XDmaHexitec::getClusterGrade(int chip, int gradeMap[HEXITEC_CLUSTER_CLASS_NUMBER])
{
	if (m_maxBitsClusterGrade == 0)
			throw XDmaHexitecException("getClusterGrade: This firmware does not support Cluster Grade");

	uint32_t value;
	uint32_t mask = (1 << m_maxBitsClusterGrade)-1;

	value = getChipReg(chip, HEXITEC_CHIP_CLUSTER_GRADE);

	for (int i=0; i<HEXITEC_CLUSTER_CLASS_NUMBER; i++)
		gradeMap[i] = (value >> (i*m_maxBitsClusterGrade)) &  mask;

}
