#include <iostream>
#include <sstream>
#include "xdma_hbm_hist.h"
#include "xdma_hexitec.h"


string XDmaHexitec::decodeDataPath()
{
	return decodeDataPath(getGlobReg(HEXITEC_GLB_DATA_PATH));
}

string XDmaHexitec::decodeDataPath(uint32_t dataPath)
{
	stringstream sstr;
	sstr << "dataPath: reg=0x"<<hex<<dataPath<<dec <<" Playback="<< !!(dataPath & HEXITEC_DATA_PATH_PLAYBACK);
	sstr << ", DisableHist="<< !!(dataPath & HEXITEC_DATA_PATH_DISABLE_HIST);
	sstr << ", Transmit raw data to UDP Out="<< !!(dataPath & HEXITEC_DATA_PATH_TRANSMIT_UDP);
	sstr << ", Transmit Playback="<< !!(dataPath & HEXITEC_DATA_PATH_TRANSMIT_PB);

	sstr << ", Scope Mode="<< !!(dataPath & HEXITEC_DATA_PATH_SCOPEMODE);
	if (dataPath & HEXITEC_DATA_PATH_EVLIST_ENB)
	{
		sstr << ", EventList enabled: ";
		if ((dataPath & HEXITEC_DATA_PATH_EVLIST_UDP)) 
			sstr <<" TO UDP";
		else
			sstr <<" To QDMA";
		sstr << ", Fixed4k="<< !!(dataPath & HEXITEC_DATA_PATH_EVLIST_FIXED4K);
		sstr << ", Detailed="<< !!(dataPath & HEXITEC_DATA_PATH_EVLIST_DETAILED);
	}
	else
	{
		sstr << ", EventList disabled";
	}
	sstr <<", Enb Timeframe Flush=" <<!!(dataPath & HEXITEC_DATA_PATH_ENB_FLUSH);
	sstr <<", Enb Timeframe FlushAll=" <<!!(dataPath & HEXITEC_DATA_PATH_ENB_FLUSH_ALL);
	sstr <<", ShortBurst=" <<!!(dataPath & HEXITEC_DATA_PATH_SHORT_BURST_MODE);
	sstr <<", ReplicateUDP=" <<!!(dataPath & HEXITEC_DATA_PATH_REPLICATE_UDP);
	
	return sstr.str();	
}

string XDmaHexitec::decodeBaseline(int chip)
{
	return decodeBaseline(getChipReg(chip, HEXITEC_CHIP_BASESUB));
}
string XDmaHexitec::decodeBaseline(uint32_t baselineReg)
{
	stringstream sstr;
//	sstr << "Baseline: reg=0x"<<hex<<baselineReg<<dec <<" mode="<< HEXITEC_BSUB_GET_MODE(baselineReg) << " = " << m_bsubMaskNames[HEXITEC_BSUB_GET_MODE(baselineReg)]<< ", divideCode="<<HEXITEC_BSUB_GET_DIVIDE(baselineReg);
	sstr << "Baseline: mode="<< HEXITEC_BSUB_GET_MODE(baselineReg) << " = " << m_bsubMaskNames[HEXITEC_BSUB_GET_MODE(baselineReg)]<< ", divideCode="<<HEXITEC_BSUB_GET_DIVIDE(baselineReg);
	sstr << " => " << (256<< HEXITEC_BSUB_GET_DIVIDE(baselineReg)) <<", dither=" << HEXITEC_BSUB_GET_DITHER(baselineReg) <<", useAbsTrig=" <<  HEXITEC_BSUB_GET_ABS_TRIG(baselineReg);
	if (baselineReg & HEXITEC_BSUB_LOAD)
		sstr <<", Loading";
	if (baselineReg & HEXITEC_BSUB_SAVE)
		sstr << ", Saving";
	return sstr.str();
}

string XDmaHexitec::decodeClusterMode(int chip)
{
	return decodeClusterMode(getChipReg(chip, HEXITEC_CHIP_CLUSTER));
}
string XDmaHexitec::decodeClusterMode(uint32_t clusterMode)
{
	stringstream sstr;
	sstr << "Cluster: mode="<<HEXITEC_CLUSTER_MODE_GET(clusterMode) <<" = " << m_clusterModeNames[HEXITEC_CLUSTER_MODE_GET(clusterMode)] <<", autoTrigRate=" << HEXITEC_CLUSTER_AUTO_RATE_GET(clusterMode);
	sstr << " = " << m_autoTrigNames[HEXITEC_CLUSTER_AUTO_RATE_GET(clusterMode)];
	return sstr.str();
}

string XDmaHexitec::decodeClusterEnable(int chip)
{
	return decodeClusterEnable(getChipReg(chip, HEXITEC_CHIP_ENB_CLUSTER));
}
string XDmaHexitec::decodeClusterEnable(uint32_t clusterEnb)
{
	stringstream sstr;
	sstr << "ClusterEnable:";
	if ((clusterEnb & HEXITEC_CLUSTER_ENB_ALL) == HEXITEC_CLUSTER_ENB_ALL)
		sstr << " All";
	else if ((clusterEnb & HEXITEC_CLUSTER_ENB_ALL) == 0)
		sstr << " Nothing enabled (Problem?)";
	else
	{
		if (clusterEnb & HEXITEC_CLUSTER_ENB_LONE) 		sstr << " Lone";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_HOZ)		sstr << " Hoz(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_HOZ_NL)	sstr << " Hoz(negLft)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_HOZ_NR)	sstr << " Hoz(negRgh)"; 
		if (clusterEnb & HEXITEC_CLUSTER_ENB_VERT)		sstr << " Vert(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_VERT_NA)	sstr << " Vert(negAbv)"; 
		if (clusterEnb & HEXITEC_CLUSTER_ENB_VERT_NB)	sstr << " Vert(negBlw)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_DIAG1)		sstr << " Diag1(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_DIAG1NL)	sstr << " Diag1(negL)"; 
		if (clusterEnb & HEXITEC_CLUSTER_ENB_DIAG1NR)	sstr << " Diag1(negR)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_DIAG2)		sstr << " Diag2(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_DIAG2NL)	sstr << " Diag2(negL)"; 
		if (clusterEnb & HEXITEC_CLUSTER_ENB_DIAG2NR)	sstr << " Diag2(negR)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_L1)		sstr << " L1(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_L2)		sstr << " L2(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_L3)		sstr << " L3(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_L4)		sstr << " L4(pos)";
		if (clusterEnb & HEXITEC_CLUSTER_ENB_QUAD)		sstr << " Quad(pos)";

	}
	return sstr.str();
}

string XDmaHexitec::decodeClusterGrade(int chip)
{
	return	decodeClusterGrade(getChipReg(chip, HEXITEC_CHIP_CLUSTER_GRADE));
}
string XDmaHexitec::decodeClusterGrade(uint32_t clusterGrade)
{
	stringstream sstr;
	sstr << "ClusterGrade = 0x" << std::hex << clusterGrade << std::dec;
	return sstr.str();
}
	

string XDmaHexitec::decodeHistFormat(int chip)
{
	int histFormat, mappedMode, engShift;
	stringstream sstr;
	if (chip << 0 || chip >= m_numChips)
		XDmaHexitecException("decodeHistFormat: chip=%d out of range 0...%d", chip, m_numChips-1); 
	getHistFormat(chip, &histFormat, &mappedMode, &engShift);
	sstr << "HistFormat: mode " <<  m_dataFormat[chip].histMode << " : " ;
	if (mappedMode == HEXITEC_HIST_MAPPED_MODE_ONLY)
		sstr << " Spectra Disabled, mapped only. numTFMapped = " << m_dataFormat[chip].numTFMapped;
	else
	{
		switch (m_dataFormat[chip].histMode)
		{
		case HEXITEC_HIST_FORMAT_RUN10LSB:
			engShift = 2;	// Override engShift
		case HEXITEC_HIST_FORMAT_RUN12:
		case HEXITEC_HIST_FORMAT_RUN11:
		case HEXITEC_HIST_FORMAT_RUN10:
		case HEXITEC_HIST_FORMAT_RUN9:
		case HEXITEC_HIST_FORMAT_RUN8:
		case HEXITEC_HIST_FORMAT_RUN7:
			sstr << "Eng x posn (Time, row , col,  Energy) = ("<< m_dataFormat[chip].numTF <<", 80, 80, " << m_dataFormat[chip].nBinsEng << "), engShift=" << engShift;
			break;
			
		case HEXITEC_HIST_FORMAT_ENG_ONLY12:
		case HEXITEC_HIST_FORMAT_ENG_ONLY11:
		case HEXITEC_HIST_FORMAT_ENG_ONLY10:
		case HEXITEC_HIST_FORMAT_ENG_ONLY9:
		case HEXITEC_HIST_FORMAT_ENG_ONLY8:
		case HEXITEC_HIST_FORMAT_ENG_ONLY7:
			sstr << "Eng only (Time, Energy) = ("<< m_dataFormat[chip].numTF <<", " << m_dataFormat[chip].nBinsEng << "), engShift=" << engShift;
			break;
		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:
			engShift = 2;	// Override engShift
		case HEXITEC_HIST_FORMAT_ENG_POS_CC12:
		case HEXITEC_HIST_FORMAT_ENG_POS_CC11:
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10:
		case HEXITEC_HIST_FORMAT_ENG_POS_CC9:
		case HEXITEC_HIST_FORMAT_ENG_POS_CC8:
		case HEXITEC_HIST_FORMAT_ENG_POS_CC7:
			sstr << "Eng x posn CusterClass (Time, ClustClass, row , col,  Energy) = ("<< m_dataFormat[chip].numTF << ", " << m_dataFormat[chip].nBinsClustClass << ", 80, 80, " << m_dataFormat[chip].nBinsEng << "), engShift=" << engShift;
			break;

		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:
			sstr << "Eng x CusterClass (Time, ClustClass, Energy) = ("<< m_dataFormat[chip].numTF << ", " << m_dataFormat[chip].nBinsClustClass << ", " << m_dataFormat[chip].nBinsEng << "), engShift=" << engShift;
			break;

		case HEXITEC_HIST_FORMAT_CALIB_COMB12:
		case HEXITEC_HIST_FORMAT_CALIB_COMB11:
		case HEXITEC_HIST_FORMAT_CALIB_COMB10:
			sstr << "Calibration all pixels (ClustClass, LUTAddr, Energy) = ("  << m_dataFormat[chip].nBinsClustClass << ", " << m_dataFormat[chip].nBinsLutAddr << ", " << m_dataFormat[chip].nBinsEng << ")";
			break;

		case HEXITEC_HIST_FORMAT_CALIB_COMBTYPE:
			sstr << "Calibration all pixels (ClustType, LUTAddr, Energy) = ("  << m_dataFormat[chip].nBinsClustClass << ", " << m_dataFormat[chip].nBinsLutAddr << ", " << m_dataFormat[chip].nBinsEng << ")";
			break;
		case HEXITEC_HIST_FORMAT_CALIB_SEPARATE:
			sstr << "Calibration by posn (row, col, LUTAddr, Energy) = (80, 80, 256, " << m_dataFormat[chip].nBinsEng << ")";
			break;

		case HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB:
			engShift = 2;	// Override engShift
		case HEXITEC_HIST_FORMAT_ENG_POS_CG12:
		case HEXITEC_HIST_FORMAT_ENG_POS_CG11:
		case HEXITEC_HIST_FORMAT_ENG_POS_CG10:
		case HEXITEC_HIST_FORMAT_ENG_POS_CG9:
		case HEXITEC_HIST_FORMAT_ENG_POS_CG8:
		case HEXITEC_HIST_FORMAT_ENG_POS_CG7:
			sstr << "Eng x posn CusterGrade (Time, ClustGrade, row , col,  Energy) = ("<< m_dataFormat[chip].numTF << ", " << m_dataFormat[chip].nBinsClustClass << ", 80, 80, " << m_dataFormat[chip].nBinsEng << ")";
			break;

		case HEXITEC_HIST_FORMAT_CHARAC2D12:
		case HEXITEC_HIST_FORMAT_CHARAC2D10:
			sstr << "2-d shared characterisation, (neighbour, main) = (" << m_dataFormat[chip].nBins[1] << ", " << m_dataFormat[chip].nBins[0] << ")"; 
			break;
		case HEXITEC_HIST_FORMAT_CHARAC3D:
			sstr << "3-d shared characterisation, (neb, central, neb) = ("<< m_dataFormat[chip].nBins[2] << ", "  << m_dataFormat[chip].nBins[1] << ", " << m_dataFormat[chip].nBins[0] << ")"; 
			break;
		case HEXITEC_HIST_FORMAT_CHARAC4D:
			sstr << "4-d shared characterisation";
		}
		if (mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
			sstr << ", Interleaved mapped numTF=" << m_dataFormat[chip].numTFMapped;
	}
	return sstr.str();
}

/**
	Decode the setup and status of a single datamover context.
	Data mover contexts can be used to send data by UDP and when combined with the QDMA, can send data into QDMA queues. Currently the UDP option is used of HexitecMHz.
	For normal mode of operation for HexitecMHz is streaming to UDP, with auto-triggering of the output and hence time frame being loaded from firmware.
	

@param qid			Queue id to be inspected.
@param force		Build the setup string even if the setup has not changed.
@param setup		A string reference to return a description of the configuration of the context.
@param status		A string reference to return a description of the status of the context.
@param setupChange	A bool reference to return that the setup has changed since last time.
@param statusChange	A bool reference to return that the status has changed since last time.

*/

void XDmaHexitec::decodeDataMoverStream(int qid, bool force, string &setup , string & status, bool &running, bool &setupChange, bool & statusChange)
{
	volatile uint32_t *ptr;
	volatile uint8_t * p8;
	uint32_t readoutMode, dm2, dm3;
	int mappedView;
	bool sixteenBit;
	bool sumChips;
	int farmMask,farmBase;
	int autoMode;
	enum FarmIndexMode farmIndexMode;
	int farmIndex;
	int64_t tFrame;
	stringstream setupSstr, statusSstr;

	ptr = m_dataMoverRegs+ HEXITEC_DM_CONTEXT_OFFSET/sizeof(uint32_t)+qid*8;

	readoutMode = ptr[0];
	dm2 = ptr[2];
	dm3 = ptr[3];
	tFrame = dm2 >> 8;
	tFrame |= static_cast<int64_t>((dm3 & 0xfff)) << 24;
	if (tFrame == 68719476735)
		tFrame = -1L;
	setupChange = false;
	statusChange = false;
	running = !!(dm3 & HEXITEC_DM3_RUN);
	sixteenBit = !!(readoutMode & HEXITEC_DM0_16BIT);
	mappedView = HEXITEC_DM0_GET_MV(readoutMode);
	sumChips = !! (readoutMode & HEXITEC_DM0_SUM_CHIPS);
	autoMode = HEXITEC_DM0_GET_TF_MODE(readoutMode);
	if (force || readoutMode != m_prevDataMoverState[qid].dm0)
	{
		if (readoutMode != m_prevDataMoverState[qid].dm0)
			setupChange = true;
		if (readoutMode & HEXITEC_DM0_EVLIST)
			setupSstr << "Event list output";
		else
		{			
			if (readoutMode & HEXITEC_DM0_UDP_MODE)
				setupSstr << "UDP: ";
			else
				setupSstr << "QDMA: ";

			setupSstr << m_mappedViewNames[mappedView] << ", " << (sixteenBit?"16-bit":"32-bit");
			if (m_numChips > 1)
				setupSstr << ", " << (sumChips?"Summing chips":"Separating chips");
			if (autoMode & HEXITEC_DM0_AUTO_TF)
				setupSstr << ", " << ((autoMode&HEXITEC_DM0_AUTO_TF_CLEAR)?"Auto trigger timeframe and clear":"Auto trigger timeframe, no clear");
			else
				setupSstr << ", Timeframe=" << tFrame;
			farmIndex = HEXITEC_DM0_GET_FARM_INDEX(readoutMode);
			setupSstr << ", farm index=" << m_DMfarmIndexModeNames[farmIndex];
			farmBase = HEXITEC_DM0_GET_FARM_BASE(readoutMode);
			farmMask = HEXITEC_DM0_GET_FARM_MASK(readoutMode);
			setupSstr << hex << ", farm base=0x" << farmBase <<", farmMask=0x" << farmMask <<dec;
		}
	}
	if (dm2 !=  m_prevDataMoverState[qid].dm2 || dm3 !=  m_prevDataMoverState[qid].dm3)
		statusChange = true;
	if (dm3 & HEXITEC_DM3_RUN)
		statusSstr << ((readoutMode & HEXITEC_DM0_UDP_MODE)?"UDP":"QDMA") << qid << ": "<< (mappedView?"Mapped TF=":"Spectrum TF=") << tFrame;
	else
		statusSstr << "Stopped";

	m_prevDataMoverState[qid].dm0 = readoutMode;
	m_prevDataMoverState[qid].dm2 = dm2;
	m_prevDataMoverState[qid].dm3 = dm3;
	setup = setupSstr.str();
	status = statusSstr.str();
}
