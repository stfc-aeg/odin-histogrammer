#include <iostream>
#include <fstream>
#include <cerrno>
#include <iomanip>
#include <chrono>
#include <thread>
#include "xdma_hbm_hist.h"
#include "xdma_hexitec.h"
#include "H5Cpp.h"
#include "hdf5_hl.h"

static const char * thresDataSetNamePos[] = {"absHighThres", "mainPosThres", "lowPosThres"};
static const char * thresDataSetNameNeg[] = {"absLowThres", "mainNegThres", "lowNegThres"};
static const char * trigEnableDataSetName = "TrigEnable";
static const char * linCorDataSetName = "linearityCorr";
static const char * cShareDataSetName[] = {"cShareEdgePos", "cShareNegNeb", "cShareLPos"};
static const char * outputPixelMaskName = "outputPixelMask";
void XDmaHexitec::saveSettingsHdf5(const char *fName, int chip, enum HexitecSaveRestore saveFlags)
{
	int row, col;
	int firstChip, lastChip;
	hid_t fileId, *det=NULL;
	char fNameExt[FILENAME_MAX+2];
	bool manyChips=false;
	int len;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		manyChips = m_numChips>1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("saveSettingsHdf5: chip %d is out of range 0...%d", chip, m_numChips-1);
	else
	{
		firstChip = lastChip = chip;
	}

	strcpy(fNameExt, fName);
	if ((len=strlen(fNameExt)) > 3 && strcmp(fNameExt+len-3, ".h5")==0)
		fNameExt[len-3] = 0;
	strcat(fNameExt, ".h5");
	H5::H5File h5File(fNameExt, H5F_ACC_TRUNC);

	if (saveFlags & HexitecSaveRestore::HexitecSaveRestore_AbsThres)
		saveThresholds(h5File, firstChip, lastChip, 0, !!(saveFlags & HexitecSaveRestore_AbsThresPos), !!(saveFlags & HexitecSaveRestore_AbsThresNeg), false);
	if (saveFlags & HexitecSaveRestore_MainThres)
		saveThresholds(h5File, firstChip, lastChip, 1, !!(saveFlags & HexitecSaveRestore_MainThresPos), !!(saveFlags & HexitecSaveRestore_MainThresNeg), !!(saveFlags & HexitecSaveRestore_TrigEnable));
	if (saveFlags & HexitecSaveRestore_LowThres)
		saveThresholds(h5File, firstChip, lastChip, 2, !!(saveFlags & HexitecSaveRestore_LowThresPos), !!(saveFlags & HexitecSaveRestore_LowThresNeg), false);
	if (saveFlags & HexitecSaveRestore_LinCorr)
		saveLinCorr(h5File, firstChip, lastChip);

	if (saveFlags & HexitecSaveRestore_CShareEdgePos)
		saveCShareCorr(h5File, firstChip, lastChip, 0);
	if (saveFlags & HexitecSaveRestore_CShareNegNeb)
		saveCShareCorr(h5File, firstChip, lastChip, 1);
	if (saveFlags & HexitecSaveRestore_CShareLPos)
		saveCShareCorr(h5File, firstChip, lastChip, 2);

	if (saveFlags & HexitecSaveRestore_OutputPixelMask)
		saveOutputPixelMask(h5File, firstChip, lastChip);
	h5File.close();
}

void XDmaHexitec::saveThresholds(H5::H5File &h5File, int firstChip, int lastChip, int which, bool doPos, bool doNeg, bool doEnable)
{
	int regionNum[] = {HEXITEC_REGION_ABS_THRES, HEXITEC_REGION_MTHRES, HEXITEC_REGION_LTHRES};
	hsize_t dims[3];
	int numChips = 1+lastChip-firstChip;

	if (which < 0 || which >= 3)
		throw XDmaHexitecException("saveSettingsHdf5: which threshold %d is out of range 0...2", which);
	
	auto posThres = new uint16_t[numChips][HEXITEC_NUM_ROWS][HEXITEC_NUM_COLS];
	auto negThres = new uint16_t[numChips][HEXITEC_NUM_ROWS][HEXITEC_NUM_COLS];
	auto enable = new uint16_t[numChips][HEXITEC_NUM_ROWS][HEXITEC_NUM_COLS];

	for (int chip=firstChip; chip<=lastChip; chip++)
	{
		uint32_t combined[HEXITEC_NUM_ROWS][HEXITEC_NUM_COLS];
		readPixelLUT(chip, regionNum[which], 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, reinterpret_cast<uint32_t *>(combined));
		int dstChip = chip-firstChip;
		for (int row=0; row<HEXITEC_NUM_ROWS; row++)
		{
			for (int col=0; col<HEXITEC_NUM_COLS; col++)
			{
				posThres[dstChip][row][col] = static_cast<uint16_t>(HEXITEC_GET_THRES_POS(combined[row][col]));
				negThres[dstChip][row][col] = static_cast<uint16_t>(HEXITEC_GET_THRES_NEG(combined[row][col]));
				enable[dstChip][row][col]   = static_cast<uint16_t>(HEXITEC_GET_TRIG_ENB(combined[row][col]));
			}
		}
	}
		
	dims[0] = 1+lastChip-firstChip;
	dims[1] = HEXITEC_NUM_ROWS;
	dims[2] = HEXITEC_NUM_COLS;
	const char *labels[]= {"chip", "row", "col" , nullptr};
	if (doPos)
		saveDataSetUInt16(h5File, thresDataSetNamePos[which], 3, dims, labels, reinterpret_cast<uint16_t *>(posThres));
	if (doNeg)
		saveDataSetUInt16(h5File, thresDataSetNameNeg[which], 3, dims, labels, reinterpret_cast<uint16_t *>(negThres));
	if (which == 1 && doEnable)
		saveDataSetUInt16(h5File, trigEnableDataSetName, 3, dims, labels, reinterpret_cast<uint16_t *>(enable));
	delete [] posThres;
	delete [] negThres;
	delete [] enable;
}

void XDmaHexitec::saveLinCorr(H5::H5File &h5File, int firstChip, int lastChip)
{
	int numChips = 1+lastChip-firstChip;
	int numPWL = 1<<m_nBitsAddrPWLin;
	hsize_t dims[5];
	const char *labels[]= {"chip", "row", "col", "piece", "abc", nullptr};

	auto linCorr = new uint32_t[numChips*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*3*numPWL];
	auto a = new uint32_t[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*numPWL];
	auto b = new uint32_t[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*numPWL];
	auto c = new uint32_t[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*numPWL];
	dims[0] = numChips;
	dims[1] = HEXITEC_NUM_ROWS;
	dims[2] = HEXITEC_NUM_COLS;
	dims[3] = numPWL;
	dims[4] = 3;

	for (int chip=firstChip; chip <= lastChip; chip++)
	{
		int dstChip = chip-firstChip;
		
		readPixelLin(chip, HEXITEC_REGION_LIN_A, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, a);
		readPixelLin(chip, HEXITEC_REGION_LIN_B, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, b);
		readPixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, c);
		uint32_t *dst = linCorr+dstChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*3*numPWL;
		for(int i=0; i<HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS*numPWL; i++)
		{
			dst[0] = a[i];
			dst[1] = b[i];
			dst[2] = c[i];
			dst += 3;
		}
	}
	saveDataSetUInt32(h5File, linCorDataSetName, 5, dims, labels, linCorr);
	delete [] linCorr;
	delete [] a;
	delete [] b;
	delete [] c;
}

void XDmaHexitec::saveCShareCorr(H5::H5File &h5File, int firstChip, int lastChip, int which)
{
	int numChips = 1+lastChip-firstChip;
	hsize_t dims[3];
	const char *labels[]= {"chip", "lutAddr", "mc", nullptr};
	int regionNumM[] = {HEXITEC_REGION_EDGE_POS_M, HEXITEC_REGION_NEG_NEB_M, HEXITEC_REGION_L_POS_M};
	int regionNumC[] = {HEXITEC_REGION_EDGE_POS_C, HEXITEC_REGION_NEG_NEB_C, HEXITEC_REGION_L_POS_C};
	int regionSize = m_regionSize[regionNumM[which]];

	dims[0] = numChips;
	dims[1] = regionSize;
	dims[2] = 2;

	auto cShareCorr = new uint32_t[numChips*regionSize*2];
	auto m = new uint32_t[regionSize];
	auto c = new uint32_t[regionSize];

	for (int chip=firstChip; chip <= lastChip; chip++)
	{
		int dstChip = chip-firstChip;
		readSharedLUT(chip, regionNumM[which], 0, 0, regionSize, m);
		readSharedLUT(chip, regionNumC[which], 0, 0, regionSize, c);
		uint32_t *dst = cShareCorr+dstChip*regionSize*2;
		for(int i=0; i<regionSize; i++)
		{
			dst[0] = m[i];
			dst[1] = c[i];
			dst += 2;
		}
	}
	saveDataSetUInt32(h5File, cShareDataSetName[which], 3, dims, labels, cShareCorr);
	delete [] cShareCorr;
	delete [] m;
	delete [] c;
}

void XDmaHexitec::saveOutputPixelMask(H5::H5File &h5File, int firstChip, int lastChip)
{
	int numChips = 1+lastChip-firstChip;
	hsize_t dims[3];
	dims[0] = 1+lastChip-firstChip;
	dims[1] = HEXITEC_NUM_ROWS;
	dims[2] = HEXITEC_NUM_COLS;
	const char *labels[]= {"chip", "row", "col" , nullptr};

	auto pixelMask = new uint8_t [numChips*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS];
	
	for (int chip=firstChip; chip<=lastChip; chip++)
	{
		int dstChip = chip-firstChip;
		readPixelMask(chip, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, pixelMask+dstChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
	}
	saveDataSetUInt8(h5File, outputPixelMaskName, 3, dims, labels, pixelMask);
}

void XDmaHexitec::saveDataSetUInt32(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], const char * labels[], uint32_t *buffer)
{

	hsize_t start[5], count[5];
	hsize_t dimsMem[3];              // dataset dimensions

	H5::DataSpace dataSpaceFile( rank, dims);

	H5::IntType dataType(H5::PredType::NATIVE_UINT32);
	dataType.setOrder(H5T_ORDER_LE);
	H5::DataSet dataSet = h5File.createDataSet(dataSetName, dataType, dataSpaceFile);
	if (labels != nullptr)
	{
		for (int i = 0; i<rank; i++)
			H5DSset_label(dataSet.getId(), i, labels[i]);
	}
	dataSet.write(buffer, H5::PredType::NATIVE_UINT32);
}
void XDmaHexitec::saveDataSetUInt16(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], const char * labels[], uint16_t *buffer)
{

	hsize_t start[5], count[5];
	hsize_t dimsMem[3];              // dataset dimensions

	H5::DataSpace dataSpaceFile( rank, dims);

	H5::IntType dataType(H5::PredType::NATIVE_UINT16);
	dataType.setOrder(H5T_ORDER_LE);
	H5::DataSet dataSet = h5File.createDataSet(dataSetName, dataType, dataSpaceFile);
	if (labels != nullptr)
	{
		for (int i=0; i<rank; i++)
			H5DSset_label(dataSet.getId(), i, labels[i]);
	}
	dataSet.write(buffer, H5::PredType::NATIVE_UINT16);
}
void XDmaHexitec::saveDataSetUInt8(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], const char * labels[], uint8_t *buffer)
{

	hsize_t start[5], count[5];
	hsize_t dimsMem[3];              // dataset dimensions

	H5::DataSpace dataSpaceFile( rank, dims);

	H5::IntType dataType(H5::PredType::NATIVE_UINT8);
	dataType.setOrder(H5T_ORDER_LE);
	H5::DataSet dataSet = h5File.createDataSet(dataSetName, dataType, dataSpaceFile);
	if (labels != nullptr)
	{
		for (int i=0; i<rank; i++)
			H5DSset_label(dataSet.getId(), i, labels[i]);
	}
	dataSet.write(buffer, H5::PredType::NATIVE_UINT8);
}



void XDmaHexitec::loadSettingsHdf5(const char *fName, int chip, enum HexitecSaveRestore loadFlags, double scaleLinearity)
{
	int row, col;
	int firstChip, lastChip;
	hid_t fileId, *det=NULL;
	char fNameExt[FILENAME_MAX+2];
	bool manyChips=false;
	bool requireAll = (loadFlags & HexitecSaveRestore_RequireAll) != 0; 
	int len;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		manyChips = m_numChips>1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("loadSettingsHdf5: chip %d is out of range 0...%d", chip, m_numChips-1);
	else
	{
		firstChip = lastChip = chip;
	}

	strcpy(fNameExt, fName);
	if ((len=strlen(fNameExt)) > 3 && strcmp(fNameExt+len-3, ".h5")==0)
		fNameExt[len-3] = 0;
	strcat(fNameExt, ".h5");
	H5::H5File h5File(fNameExt, H5F_ACC_RDONLY);

	if (loadFlags & HexitecSaveRestore::HexitecSaveRestore_AbsThres)
		loadThresholds(h5File, firstChip, lastChip, 0, !!(loadFlags & HexitecSaveRestore_AbsThresPos), !!(loadFlags & HexitecSaveRestore_AbsThresNeg), false, requireAll, fNameExt);
	if (loadFlags & HexitecSaveRestore_MainThres)
		loadThresholds(h5File, firstChip, lastChip, 1, !!(loadFlags & HexitecSaveRestore_MainThresPos), !!(loadFlags & HexitecSaveRestore_MainThresNeg), !!(loadFlags & HexitecSaveRestore_TrigEnable), requireAll, fNameExt);
	if (loadFlags & HexitecSaveRestore_LowThres)
		loadThresholds(h5File, firstChip, lastChip, 2, !!(loadFlags & HexitecSaveRestore_LowThresPos), !!(loadFlags & HexitecSaveRestore_LowThresNeg), false, requireAll, fNameExt);

	if (loadFlags & HexitecSaveRestore_LinCorr)
		loadLinCorr(h5File, firstChip, lastChip, requireAll, fNameExt, scaleLinearity);

	if (loadFlags & HexitecSaveRestore_CShareEdgePos)
		loadCShareCorr(h5File, firstChip, lastChip, 0, requireAll, fNameExt);
	if (loadFlags & HexitecSaveRestore_CShareNegNeb)
		loadCShareCorr(h5File, firstChip, lastChip, 1, requireAll, fNameExt);
	if (loadFlags & HexitecSaveRestore_CShareLPos)
		loadCShareCorr(h5File, firstChip, lastChip, 2, requireAll, fNameExt);

	if (loadFlags & HexitecSaveRestore_OutputPixelMask)
		loadOutputPixelMask(h5File, firstChip, lastChip, requireAll, fNameExt);

	h5File.close();
}

void XDmaHexitec::loadThresholds(H5::H5File &h5File, int firstChip, int lastChip, int which, bool doPos, bool doNeg, bool doEnable, bool requireAll, char *fName)
{
	int numChips = 1+lastChip-firstChip;
	uint16_t *posThres=nullptr;
	uint16_t *negThres=nullptr;
	uint16_t *trigEnable=nullptr;
	hsize_t dims[5];
	int regionNum[] = {HEXITEC_REGION_ABS_THRES, HEXITEC_REGION_MTHRES, HEXITEC_REGION_LTHRES};
	enum RequiredDataType requiredType=RequiredUInt16;
	std::unique_ptr<uint8_t []> posBase, negBase, trigBase;
	dims[0] = 0;
	dims[1] = HEXITEC_NUM_ROWS;
	dims[2] = HEXITEC_NUM_COLS;
	
	if (doPos)
	{
		if (h5File.nameExists(thresDataSetNamePos[which]))
		{
			posBase = readDataSet(h5File, thresDataSetNamePos[which], 3, dims, requiredType);
			posThres = reinterpret_cast<uint16_t *>(posBase.get());
		}
		else if (requireAll)
		{
			h5File.close();
			throw XDmaHexitecException("loadThresholds: DataSet %d is not present and loading all sections is required", thresDataSetNamePos[which]);
		}
	}
	if (doNeg)
	{
		if (h5File.nameExists(thresDataSetNameNeg[which]))
		{
			negBase = readDataSet(h5File, thresDataSetNameNeg[which], 3, dims, requiredType);
			negThres = reinterpret_cast<uint16_t *>(negBase.get());
		}
		else if (requireAll)
		{
			h5File.close();
			throw XDmaHexitecException("loadThresholds: DataSet %d is not present and loading all sections is required", thresDataSetNameNeg[which]);
		}
	}
	if (which == 1 && doEnable)
	{
		if (h5File.nameExists(trigEnableDataSetName))
		{
			trigBase = readDataSet(h5File, trigEnableDataSetName, 3, dims, requiredType);
			trigEnable = reinterpret_cast<uint16_t *>(trigBase.get());
		}
		else if (requireAll)
		{
			h5File.close();
			throw XDmaHexitecException("loadThresholds: DataSet %d is not present and loading all sections is required", trigEnableDataSetName);
		}
	}
	if (posThres==nullptr && negThres==nullptr && trigEnable == nullptr)
		return;
	if (dims[0] != numChips)
		throw XDmaHexitecException("loadThresholds: Expected data for %d ASICS but found dataSets contain data for %d", numChips, dims[0]);
		
	for(int chip=firstChip; chip<=lastChip; chip++)
	{
		uint32_t combined[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS];
		int srcChip = chip-firstChip;
		memset (combined, 0, sizeof(combined));
		if (posThres == nullptr || negThres == nullptr || (which == 1 && trigEnable == nullptr))
			readPixelLUT(chip, regionNum[which], 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, combined);

		uint16_t *posPtr = posThres+srcChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS;
		uint16_t *negPtr = negThres+srcChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS;
		uint16_t *enbPtr = trigEnable+srcChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS;

		for (int i=0; i<HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS; i++)
		{
			uint32_t pos = (posThres==nullptr)?HEXITEC_GET_THRES_POS(combined[i]):posPtr[i];
			uint32_t neg = (negThres==nullptr)?HEXITEC_GET_THRES_NEG(combined[i]):negPtr[i];
			uint32_t enb = (trigEnable==nullptr)?HEXITEC_GET_TRIG_ENB(combined[i]):enbPtr[i];
			if (m_generation == HexitecGenMHz)
			{
				switch(which)
				{
				case 0: combined[i] = HEXITEC_ABS_THRES_MHZ(pos, neg); break;
				case 1: combined[i] = HEXITEC_MTHRES_MHZ(enb, pos, neg); break;
				case 2: combined[i] = HEXITEC_LTHRES_MHZ(pos, neg); break;
				}
			}
			else
			{
				switch(which)
				{
				case 0: combined[i] = HEXITEC_ABS_THRES_HXT(pos, neg); break;
				case 1: combined[i] = HEXITEC_MTHRES_HXT(pos, neg, enb); break;
				case 2: combined[i] = HEXITEC_LTHRES_HXT(pos, neg);
				}
			}
		}
		writePixelLUT(chip, regionNum[which], 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, combined);
		switch(which)
		{
		case 0:
			if (posThres!=nullptr)
				m_absTrigHighName[chip] = std::string(fName);
			if (negThres!=nullptr)
				m_absTrigLowName[chip] = std::string(fName);
			break;
		case 1:
			if (posThres!=nullptr)
				m_mainTrigPosName[chip] = std::string(fName);
			if (negThres!=nullptr)
				m_mainTrigNegName[chip] = std::string(fName);
			if (trigEnable!=nullptr)
				m_trigEnableName[chip] = std::string(fName);
			break;
		case 2:
			if (posThres!=nullptr)
				m_lowTrigPosName[chip] = std::string(fName);
			if (negThres!=nullptr)
				m_lowTrigNegName[chip] = std::string(fName);
			break;
		}				
	}
}

void XDmaHexitec::loadLinCorr(H5::H5File &h5File, int firstChip, int lastChip, bool requireAll, char *fName, double scaleLinearity)
{
	int numChips = 1+lastChip-firstChip;
	int numPWL = 1<<m_nBitsAddrPWLin;
	hsize_t dims[5];
	enum RequiredDataType requiredType=RequiredAny;

	dims[0] = numChips;
	dims[1] = HEXITEC_NUM_ROWS;
	dims[2] = HEXITEC_NUM_COLS;
	dims[3] = 0;
	dims[4] = 3;
	
	if (!h5File.nameExists(linCorDataSetName))
	{
		if (requireAll)
		{
			h5File.close();
			throw XDmaHexitecException("loadLinCorr: DataSet %s is not present and loading all sections is required", linCorDataSetName);
		}
		else
			return;
	}
	
	std::unique_ptr<uint8_t[]> linCorrBuffer = readDataSet(h5File, linCorDataSetName, 5, dims, requiredType);

	if (dims[3] > numPWL)
	{
		h5File.close();
		throw XDmaHexitecException("loadLinCorr: DataSet contains %d pieces, but hardware supports only %d", dims[3], numPWL);
	}

	std::unique_ptr<uint32_t []> a {new uint32_t[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*numPWL]};
	std::unique_ptr<uint32_t []> b {new uint32_t[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*numPWL]};
	std::unique_ptr<uint32_t []> c {new uint32_t[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*numPWL]};

	if (requiredType == RequiredUInt32)
	{
		uint32_t *base = reinterpret_cast<uint32_t *>(linCorrBuffer.get());
		for (int chip=firstChip; chip <= lastChip; chip++)
		{
			int srcChip = chip-firstChip;
			uint32_t *src = base + srcChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*dims[3]*3;
			for (int rc=0; rc<HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS; rc++)
			{
				for(int pwl=0; pwl<numPWL; pwl++)
				{
					int i=pwl;
					if (i >= dims[3])
						i = dims[3]-1;
					if (scaleLinearity != 1.0)
					{
						int ia = src[3*i]*scaleLinearity;
						int ib = src[3*i+1]*scaleLinearity;
						int ic = src[3*i+2]*scaleLinearity;
						if (ia < m_linABMin || ia > m_linABMax)
						{
							h5File.close();
							throw  XDmaHexitecException("loadLinCorr: At row=%d, col=%d, triple=%d, a value=%d scales to %d which is out of range %d to %d", 
								rc/HEXITEC_NUM_COLS, rc % HEXITEC_NUM_COLS, i, src[3*i], ia, m_linABMin, m_linABMax);
						}
						if (ib < m_linABMin || ib > m_linABMax)
						{
							h5File.close();
							throw  XDmaHexitecException("loadLinCorr: At row=%d, col=%d, triple=%d, b value=%d scales to %d which is out of range %d to %d", 
								rc/HEXITEC_NUM_COLS, rc % HEXITEC_NUM_COLS, i, src[3*i+1], ib, m_linABMin, m_linABMax);
						}
						if (ic < HEXITEC_LINEARITY_C_MIN || ic > HEXITEC_LINEARITY_C_MAX)
						{
							h5File.close();
							throw  XDmaHexitecException("loadLinCorr: At row=%d, col=%d, triple=%d, c value=%d scales to %d which is out of range %d to %d", 
								rc/HEXITEC_NUM_COLS, rc % HEXITEC_NUM_COLS, i, src[3*2+2], ic, HEXITEC_LINEARITY_C_MIN, HEXITEC_LINEARITY_C_MAX);
						}
						a[rc*numPWL+pwl] = ia;
						b[rc*numPWL+pwl] = ib;
						c[rc*numPWL+pwl] = ic;
					}
					else
					{
						a[rc*numPWL+pwl] = src[3*i];
						b[rc*numPWL+pwl] = src[3*i+1];
						c[rc*numPWL+pwl] = src[3*i+2];
					}
				}
				src += 3*dims[3];
			}
			writePixelLin(chip, HEXITEC_REGION_LIN_A, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, false, a.get());
			writePixelLin(chip, HEXITEC_REGION_LIN_B, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, false, b.get());
			writePixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, false, c.get());
		}
	}
	else
	{
		double *base = reinterpret_cast<double *>(linCorrBuffer.get());
		for (int chip=firstChip; chip <= lastChip; chip++)
		{
			int srcChip = chip-firstChip;
			double *src = base + srcChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*dims[3]*3;
			for (int rc=0; rc<HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS; rc++)
			{
				for(int pwl=0; pwl<numPWL; pwl++)
				{
					int i=pwl;
					if (i >= dims[3])
						i = dims[3]-1;
					int ia, ib, ic;
					ia = 4096.0*(double)m_linScaleA*src[3*i]*scaleLinearity;		// Scaled to match ADC range
					ib = (double)m_linScaleB*src[3*i+1]*scaleLinearity;
					ic = src[3*i+2]*(double)m_linScaleC*scaleLinearity;
					if (ia < m_linABMin || ia > m_linABMax)
					{
						h5File.close();
						throw  XDmaHexitecException("loadLinCorr: At row=%d, col=%d, triple=%d, a value=%g scales to %d which is out of range %d to %d", 
							rc/HEXITEC_NUM_COLS, rc % HEXITEC_NUM_COLS, i, src[3*i], ia, m_linABMin, m_linABMax);
					}
					if (ib < m_linABMin || ib > m_linABMax)
					{
						h5File.close();
						throw  XDmaHexitecException("loadLinCorr: At row=%d, col=%d, triple=%d, b value=%g scales to %d which is out of range %d to %d", 
							rc/HEXITEC_NUM_COLS, rc % HEXITEC_NUM_COLS, i, src[3*i+1], ib, m_linABMin, m_linABMax);
					}
					if (ic < HEXITEC_LINEARITY_C_MIN || ic > HEXITEC_LINEARITY_C_MAX)
					{
						h5File.close();
						throw  XDmaHexitecException("loadLinCorr: At row=%d, col=%d, triple=%d, c value=%g scales to %d which is out of range %d to %d", 
							rc/HEXITEC_NUM_COLS, rc % HEXITEC_NUM_COLS, i, src[3*2+2], ic, HEXITEC_LINEARITY_C_MIN, HEXITEC_LINEARITY_C_MAX);
					}
					
					a[rc*numPWL+pwl] = ia;
					b[rc*numPWL+pwl] = ib;
					c[rc*numPWL+pwl] = ic;
				}
				src += 3*dims[3];
			}
			writePixelLin(chip, HEXITEC_REGION_LIN_A, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, false, a.get());
			writePixelLin(chip, HEXITEC_REGION_LIN_B, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, false, b.get());
			writePixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, false, c.get());
		}
	}
	for (int chip=firstChip; chip <= lastChip; chip++)
		m_linCorrName[chip] = std::string(fName);
}


void XDmaHexitec::loadCShareCorr(H5::H5File &h5File, int firstChip, int lastChip, int which, bool requireAll, char *fName)
{
	int numChips = 1+lastChip-firstChip;
	hsize_t dims[3];
	int regionNumM[] = {HEXITEC_REGION_EDGE_POS_M, HEXITEC_REGION_NEG_NEB_M, HEXITEC_REGION_L_POS_M};
	int regionNumC[] = {HEXITEC_REGION_EDGE_POS_C, HEXITEC_REGION_NEG_NEB_C, HEXITEC_REGION_L_POS_C};
	int regionSize = m_regionSize[regionNumM[which]];
	enum RequiredDataType requiredType=RequiredAny;

	dims[0] = 0;
	dims[1] = regionSize;
	dims[2] = 2;

	if (!h5File.nameExists(cShareDataSetName[which]))
	{
		if (requireAll)
		{
			h5File.close();
			throw XDmaHexitecException("loadCShareCorr: DataSet %s is not present and loading all sections is required", cShareDataSetName[which]);
		}
		else
			return;
	}
	
	std::unique_ptr<uint8_t[]> cShareBuffer = readDataSet(h5File, cShareDataSetName[which], 3, dims, requiredType);

	if (dims[0] != 1 && dims[0] != numChips)
	{
		h5File.close();
		throw XDmaHexitecException("loadCShareCorr: DataSet %d has numChips=%d but firmware expects %d", cShareDataSetName[which], dims[0], numChips);
	}
	std::unique_ptr<uint32_t []>  m {new uint32_t[regionSize]};
	std::unique_ptr<uint32_t []>  c {new uint32_t[regionSize]};


	for (int chip=firstChip; chip <= lastChip; chip++)
	{
		int srcChip = chip-firstChip;
		if (dims[0] == 0)
			srcChip = 0;		// Repeat same charge sharing correction to all chips.
		if (requiredType == RequiredUInt32)
		{
			uint32_t *src = reinterpret_cast<uint32_t *>(cShareBuffer.get())+2*regionSize*srcChip;
			for(int i=0; i<regionSize; i++)
			{
				m[i] = src[0];
				c[i] = src[1];
				src += 2;
			}
		}
		else
		{
			double *src = reinterpret_cast<double *>(cShareBuffer.get())+2*regionSize*srcChip;
			double scaleM = 1.0;
			double scaleC = 1.0;
			switch (which)
			{
			case 0:
				scaleM = static_cast<double>(HEXITEC_EDGE_POS_SCALE_M);
				scaleC = static_cast<double>(HEXITEC_EDGE_POS_SCALE_C);
				break;
			case 1:
				scaleM = static_cast<double>(HEXITEC_NEG_NEB_SCALE_M);
				scaleC = static_cast<double>(HEXITEC_NEG_NEB_SCALE_C);
				break;
			case 2:
				scaleM = static_cast<double>(HEXITEC_L_POS_SCALE_M);
				scaleC = static_cast<double>(HEXITEC_L_POS_SCALE_C);
				break;
			default:
				throw XDmaHexitecException("loadCShareCorr: which=%d, is out of range0..2", which);
			}

			for(int i=0; i<regionSize; i++)
			{
				m[i] = src[0]*scaleM;
				c[i] = src[1]*scaleC;
				src += 2;
			}
		}
		writeSharedLUT(chip, regionNumM[which], -1, 0, regionSize, m.get());
		writeSharedLUT(chip, regionNumC[which], -1, 0, regionSize, c.get());
		switch(which)
		{
		case 0:
			m_cShareEdgePosName[chip] = std::string(fName);
			break;
		case 1:
			m_cShareNegNeb[chip] = std::string(fName);
			break;
		case 2:
			m_cShareLPos[chip] = std::string(fName);
			break;
		}
	}
}

void XDmaHexitec::loadOutputPixelMask(H5::H5File &h5File, int firstChip, int lastChip, bool requireAll, char *fName)
{
	int numChips = 1+lastChip-firstChip;
	enum RequiredDataType requiredType=RequiredUInt8;

	hsize_t dims[3];
	dims[0] = 1+lastChip-firstChip;
	dims[1] = HEXITEC_NUM_ROWS;
	dims[2] = HEXITEC_NUM_COLS;

	if (!h5File.nameExists(outputPixelMaskName))
	{
		if (requireAll)
		{
			h5File.close();
			throw XDmaHexitecException("loadLinCorr: DataSet %d is not present and loading all sections is required", outputPixelMaskName);
		}
		else
			return;
	}
	
	std::unique_ptr<uint8_t[]> pixelMaskBuffer = readDataSet(h5File, outputPixelMaskName, 3, dims, requiredType);
	uint8_t *pixelMask = pixelMaskBuffer.get();
	
	for (int chip=firstChip; chip<=lastChip; chip++)
	{
		int srcChip = chip-firstChip;
		writePixelMask(chip, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, pixelMask+srcChip*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
		m_outputPixelMaskName[chip] = std::string(fName);
	}
}


std::unique_ptr<uint8_t[]> XDmaHexitec::readDataSet(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], RequiredDataType &requiredType)
{
	H5::DataSet dataSet = h5File.openDataSet(dataSetName);
	hsize_t fileDims[10];
	hsize_t totalSize=1;
	size_t fileDtypeSize;
	size_t memDtypeSize= 0;
	H5::DataType memType;
	
	if (dataSet.getTypeClass() == H5T_INTEGER)
	{
		H5::IntType intType = dataSet.getIntType();
		fileDtypeSize = intType.getSize();
		switch (requiredType)
		{
		case RequiredUInt8:
			if (fileDtypeSize != 1)
				throw XDmaHexitecException("readDataSet: Required size 1 bytes but dataSet=%s size %zd bytes per element", dataSetName, fileDtypeSize);
			memType = H5::PredType::NATIVE_UINT8;
			break;
		case RequiredUInt16:
			if (fileDtypeSize != 2)
				throw XDmaHexitecException("readDataSet: Required size 2 bytes but dataSet=%s size %zd bytes per element", dataSetName, fileDtypeSize);
			memType = H5::PredType::NATIVE_UINT16;
			break;
		case RequiredUInt32:
			if (fileDtypeSize != 4)
				throw XDmaHexitecException("readDataSet: Required size 4 bytes but dataSet=%s size %zd bytes per element", dataSetName, fileDtypeSize);
			memType = H5::PredType::NATIVE_UINT32;
			break;
		case RequiredAny:
			switch (fileDtypeSize)
			{
			case 1: 
				requiredType = RequiredUInt8; 
				memType = H5::PredType::NATIVE_UINT8; 
				break;
			case 2:
				requiredType = RequiredUInt16;
				memType = H5::PredType::NATIVE_UINT16;
				break;
			case 4:
				requiredType = RequiredUInt32;
				memType = H5::PredType::NATIVE_UINT32;
				break;
			default:
				throw XDmaHexitecException("readDataSet: dataSet=%s unsupported size %zd bytes per element", dataSetName, fileDtypeSize);
			}
			break;
		default:
			throw XDmaHexitecException("readDataSet: Required dataClass=INTEGER but dataSet=%s is class %d",  dataSetName, dataSet.getTypeClass());
		}
		memDtypeSize = fileDtypeSize;
	}
	else
	{
		if (requiredType == RequiredAny)
		{
			memType = H5::PredType::NATIVE_DOUBLE;
			requiredType = RequiredDouble;
		}
		else if (requiredType == RequiredDouble)
			memType = H5::PredType::NATIVE_DOUBLE;
		else
			throw XDmaHexitecException("readDataSet: Required dataClass=DOUBLE but dataSet=%s is class %d",  dataSetName, dataSet.getTypeClass());
		memDtypeSize = sizeof(double);
	}
	H5::DataSpace dataSpace = dataSet.getSpace();
	int fileRank = dataSpace.getSimpleExtentNdims();
	if (rank != fileRank)
		throw XDmaHexitecException("readDataSet: Required rank=%d but dataSet=%s is rank %d", rank, dataSetName, fileRank);
	dataSpace.getSimpleExtentDims(fileDims, NULL);

	totalSize = memDtypeSize;
	for (int i=0; i<rank; i++)
	{
		if (dims[i] == 0)
			dims[i] = fileDims[i];
		else if (dims[i] != fileDims[i])
			throw XDmaHexitecException("readDataSet: Required dimension %d  size=%d but dataSet=%s is size=%d", i, dims[i], dataSetName, fileDims[i]);
		totalSize *= dims[i];
	}

	std::unique_ptr<uint8_t []> buffer{new uint8_t[totalSize]};
	dataSet.read(buffer.get(), memType);
	dataSet.close();
	return buffer;
}
