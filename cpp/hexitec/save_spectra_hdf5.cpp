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


void XDmaHexitec::saveSpectraHdf5(char *fName, int chip, int numEng, int firstTF, int numTFSpectra, int numTFMapped, bool enbSpectra, bool enbMapped, bool sumChips, const char **comments)
{
	int eng, row, col, tf, clustClass;
	ofstream opFile;
	uint32_t *buff, *ptr;
	hid_t fileId, *det=NULL;
	char fNameExt[FILENAME_MAX+2];
	char comment[100];
	char title[100];
	int len;
	int nBins[4];
	int corner;
	int firstChip, lastChip;
	bool manyChips=false;
	hsize_t start[5], count[5];
	hsize_t dimsFile[6], dimsMem[3];              // dataset dimensions
	int rank;
	int numCC=1;
	int numRows, numCols;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		for (int c=1 ; c<m_numChips; c++)
			if (!(m_dataFormat[0] == m_dataFormat[c]))
				throw XDmaHexitecException("saveSpectraHdf5: Saving all chips, but data format mis-match between chip=0 and chip=%d", c);
		manyChips = m_numChips>1;
		numCols = m_numChipCols*HEXITEC_NUM_COLS;
		numRows = m_numChipRows*HEXITEC_NUM_ROWS;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("saveSpectraHdf5: chip %d is out of range 0...%d", chip, m_numChips-1);
	else
	{
		firstChip = lastChip = chip;
		numCols = HEXITEC_NUM_COLS;
		numRows = HEXITEC_NUM_ROWS;
	}
	int nBinsClustClass = m_dataFormat[firstChip].nBinsClustClass;
	strcpy(fNameExt, fName);
	if ((len=strlen(fNameExt)) > 3 && strcmp(fNameExt+len-3, ".h5")==0)
		fNameExt[len-3] = 0;
	strcat(fNameExt, ".h5");
	H5::H5File h5File(fNameExt, H5F_ACC_TRUNC);
//	H5:: Group settingsGroup(h5File.createGroup("/settings"));
	
	printf("saveSpectraHdf5: chip=%d, histMode=%d, mappedMode=%d, enbMapped=%d\n", chip, m_dataFormat[firstChip].histMode, m_dataFormat[firstChip].mappedMode, enbMapped);
	if (enbMapped)
	{
		if (m_dataFormat[firstChip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
			throw XDmaHexitecException("saveSpectraHdf5: Called for mapped spectra, but mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF");
		
		if (firstTF+numTFMapped > m_dataFormat[firstChip].numTFMapped)
			throw XDmaHexitecException("saveSpectraHdf5: Mapped: (firstTF=%d) +(numTFMapped=%d) out of range 1...%d", firstTF, numTFMapped, m_dataFormat[firstChip].numTFMapped);

		hsize_t dimsMapped[4], dimsMem[3];              // dataset dimensions
		dimsMapped[0] = numTFMapped;
		dimsMapped[1] = numRows;
		dimsMapped[2] = numCols;
		dimsMapped[3] = HEXITEC_NBINS_MAPPED;
		
		H5::DataSpace dataSpaceMapped( 4, dimsMapped );

		dimsMem[0] = numRows;
		dimsMem[1] = numCols;
		dimsMem[2] = HEXITEC_NBINS_MAPPED;

		H5::DataSpace dataSpaceMem( 3, dimsMem );
		dataSpaceMem.selectAll();
		H5::IntType dataType(H5::PredType::NATIVE_UINT32);
		dataType.setOrder(H5T_ORDER_LE);
		H5::DataSet dataSet = h5File.createDataSet("/mapped", dataType, dataSpaceMapped);
		H5DSset_label(dataSet.getId(), 3, "MappedEng");
		H5DSset_label(dataSet.getId(), 2, "Column");
		H5DSset_label(dataSet.getId(), 1, "Row");
		H5DSset_label(dataSet.getId(), 0, "TimeFrame");

		posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*HEXITEC_NBINS_MAPPED*numCols*numRows);
		count[0] = 1;
		count[1] = numRows;
		count[2] = numCols;
		count[3] = HEXITEC_NBINS_MAPPED;
		start[1] = 0;
		start[2] = 0;
		start[3] = 0;
		for (tf=0;tf<numTFMapped; tf++)
		{
			start[0] = tf;
			if (chip >= 0)
				readMappedEngColRowTime(HEXITEC_NBINS_MAPPED, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, tf+firstTF, 1, chip, -1, buff);
			else
				readMappedEngGlobColRowTime(HEXITEC_NBINS_MAPPED, 0, numCols, 0, numRows, tf+firstTF, 1, -1, buff);
			dataSpaceMapped.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
			dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceMapped);
		}
		if (!enbSpectra)
			hdf5AddAttributes(dataSet, comments);
	}
	if (enbSpectra)
	{
		H5::DataSpace dataSpaceFile, dataSpaceMem;
		H5::DataSet dataSet;
		if (m_dataFormat[firstChip].mappedMode== HEXITEC_HIST_MAPPED_MODE_ONLY)
			throw XDmaHexitecException("saveSpectraHdf5: Called for non-mapped spectra, but mappedMode == HEXITEC_HIST_MAPPED_MODE_ONLY");
		if (numTFSpectra+firstTF > m_dataFormat[firstChip].numTF)
			throw XDmaHexitecException("saveSpectraHdf5: Spectra: (firstTF=%d)+(numTFSpectra=%d) out of range 1...%d", firstTF, numTFSpectra, m_dataFormat[firstChip].numTF);
		if (numEng < 0)
			numEng = m_dataFormat[firstChip].nBinsEng;
		else if (numEng > m_dataFormat[firstChip].nBinsEng)
			throw XDmaHexitecException("saveSpectraHdf5: numEng=%d out of range 1...%d", numEng, m_dataFormat[firstChip].nBinsEng);
			
		numEng = (numEng+7) & 0xFFFFFFF8;		// Round up to 8 word boundary

		switch (m_dataFormat[firstChip].histMode)
		{
		case HEXITEC_HIST_FORMAT_RUN12:
		case HEXITEC_HIST_FORMAT_RUN11:		
		case HEXITEC_HIST_FORMAT_RUN10:			
		case HEXITEC_HIST_FORMAT_RUN9:		
		case HEXITEC_HIST_FORMAT_RUN8:		
		case HEXITEC_HIST_FORMAT_RUN7:
		case HEXITEC_HIST_FORMAT_RUN10LSB:			
			dimsFile[0] = numTFSpectra;
			dimsFile[1] = numRows;
			dimsFile[2] = numCols;
			dimsFile[3] = numEng;
			dataSpaceFile = H5::DataSpace(4, dimsFile);
			dimsMem[0] = numRows;
			dimsMem[1] = numCols;
			dimsMem[2] = numEng;
			dataSpaceMem = H5::DataSpace(3, dimsMem);
			dataSpaceMem.selectAll();
			dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
			H5DSset_label(dataSet.getId(), 0, "TimeFrame");
			H5DSset_label(dataSet.getId(), 1, "Row");
			H5DSset_label(dataSet.getId(), 2, "Column");
			H5DSset_label(dataSet.getId(), 3, "Energy");
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numRows*numCols);
			count[0] = 1;
			count[1] = numCols;
			count[2] = numRows;
			count[3] = numEng;
			start[1] = 0;
			start[2] = 0;
			start[3] = 0;
			for (tf=0; tf<numTFSpectra; tf++)
			{
				start[0] = tf;
				if (chip >= 0)
					readHistEngColRowTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, firstTF+tf, 1, chip, -1, buff);
				else
					readHistEngGlobColRowTime(numEng, 0, numCols, 0, numRows, firstTF+tf, 1, -1, buff);
					
				dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
				dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
			}
			free(buff);
			break;

		case HEXITEC_HIST_FORMAT_ENG_POS_CC12:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC11:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC9:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC8:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC7:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:		
			numCC = getnBinsClustClass(firstChip);
			dimsFile[0] = numTFSpectra;
			dimsFile[1] = numCC;
			dimsFile[2] = numRows;
			dimsFile[3] = numCols;
			dimsFile[4] = numEng;
			dataSpaceFile = H5::DataSpace( 5, dimsFile);
			dimsMem[0] = numRows;
			dimsMem[1] = numCols;
			dimsMem[2] = numEng;
			dataSpaceMem=H5::DataSpace( 3, dimsMem );
			dataSpaceMem.selectAll();
			dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
			H5DSset_label(dataSet.getId(), 0, "TimeFrame");
			H5DSset_label(dataSet.getId(), 1, "ClusterClass");
			H5DSset_label(dataSet.getId(), 2, "Row");
			H5DSset_label(dataSet.getId(), 3, "Column");
			H5DSset_label(dataSet.getId(), 4, "Energy");
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numRows*numCols);
			count[0] = 1;
			count[1] = 1;
			count[2] = numRows;
			count[3] = numCols;
			count[4] = numEng;
			start[2] = 0;
			start[3] = 0;
			start[4] = 0;
			for (tf=0; tf<numTFSpectra; tf++)
			{
				for (int cc=0; cc<numCC; cc++)
				{
					start[1] = cc;
					start[0] = tf;
					if (chip >= 0)
						readHistEngColRowCCTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, cc, 1, firstTF+tf, 1, chip, -1, buff);
					else
						readHistEngGlobColRowCCTime(numEng, 0, numCols, 0, numRows, cc, 1, firstTF+tf, 1, -1, buff);
					dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
					dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
				}
			}
			free(buff);
			break;
			
		case HEXITEC_HIST_FORMAT_CALIB_COMB12:	
		case HEXITEC_HIST_FORMAT_CALIB_COMB11:	
		case HEXITEC_HIST_FORMAT_CALIB_COMB10:	
		case HEXITEC_HIST_FORMAT_CALIB_COMBTYPE:
			numCC = getnBinsClustClass(firstChip);
			if (sumChips)
			{
				dimsFile[0] = 1;
				firstChip = lastChip = -1;
			}
			else
				dimsFile[0] = 1+lastChip-firstChip;

			dimsFile[1] = numCC;
			dimsFile[2] = HEXITEC_RECIP_SIZE;
			dimsFile[3] = numEng;
			dataSpaceFile = H5::DataSpace(4, dimsFile);
			dimsMem[0] = HEXITEC_RECIP_SIZE;
			dimsMem[1] = numEng;
			dataSpaceMem = H5::DataSpace(2, dimsMem );
			dataSpaceMem.selectAll();
			dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
			H5DSset_label(dataSet.getId(), 0, "Chip");
			H5DSset_label(dataSet.getId(), 1, "ClusterClass");
			H5DSset_label(dataSet.getId(), 2, "LutAddr");
			H5DSset_label(dataSet.getId(), 3, "Energy");
			
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_RECIP_SIZE);

			count[0] = 1;
			count[1] = 1;
			count[2] = HEXITEC_RECIP_SIZE;
			count[3] = numEng;

			start[2] = 0;
			start[3] = 0;
			for (chip=firstChip; chip<=lastChip; chip++)
			{
				for (clustClass=0; clustClass<numCC; clustClass++)
				{
					if (sumChips)
						start[0] = 0;
					else
						start[0] = chip-firstChip;
					start[1] = clustClass;
					readHistEngCalibClass(0, numEng, 0, HEXITEC_RECIP_SIZE, clustClass, 1, chip, -1, buff);
					dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
					dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
				}
			}
			free(buff);
			break;
			
		case HEXITEC_HIST_FORMAT_CHARAC2D12:
		case HEXITEC_HIST_FORMAT_CHARAC2D10:
			getnBinsCharac(firstChip, nBins);
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*nBins[0]*nBins[1]);
			if (sumChips)
			{
				dimsFile[0] = 1;
				firstChip = lastChip = -1;
			}
			else
				dimsFile[0] = 1+lastChip-firstChip;
			dimsFile[1] = 2;	
			dimsFile[2] = nBins[1];
			dimsFile[3] = nBins[0];
			dataSpaceFile = H5::DataSpace(4, dimsFile);
			dimsMem[0] = nBins[1];
			dimsMem[1] = nBins[0];
			dataSpaceMem = H5::DataSpace(2, dimsMem );
			dataSpaceMem.selectAll();
			dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
			H5DSset_label(dataSet.getId(), 0, "Chip");
			H5DSset_label(dataSet.getId(), 1, "Diagonal");
			H5DSset_label(dataSet.getId(), 2, "Neighbour");
			H5DSset_label(dataSet.getId(), 3, "PosPixel");
			count[0] = 1;
			count[1] = 1;
			count[2] = nBins[1];
			count[3] = nBins[0];

			start[2] = 0;
			start[3] = 0;

			for (chip=firstChip; chip<=lastChip; chip++)
			{
				start[0] = chip-firstChip; // Also OK with firstCHip == -1 for sumChips
				for (int diag=0; diag<2; diag++)
				{
					start[1] = diag;
					readHistCharac2d(0, nBins[0], 0, nBins[1], diag, chip, -1, buff);
					dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
					dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
				}
			}
			free(buff);
			break;

		case HEXITEC_HIST_FORMAT_CHARAC3D:
			getnBinsCharac(firstChip, nBins);
			if (sumChips)
			{
				dimsFile[0] = 1;
				firstChip = lastChip = -1;
			}
			else
				dimsFile[0] = 1+lastChip-firstChip;
			dimsFile[1] = nBins[2];
			dimsFile[2] = nBins[1];
			dimsFile[3] = nBins[0];
			dataSpaceFile = H5::DataSpace(4, dimsFile);
			dimsMem[0] = nBins[1];
			dimsMem[1] = nBins[0];
			dataSpaceMem = H5::DataSpace(2, dimsMem);
			dataSpaceMem.selectAll();
			dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
			H5DSset_label(dataSet.getId(), 0, "Chip");
			H5DSset_label(dataSet.getId(), 1, "Corner");
			H5DSset_label(dataSet.getId(), 2, "VertNeb");
			H5DSset_label(dataSet.getId(), 3, "HozNeb");
			count[0] = 1;
			count[1] = 1;
			count[2] = nBins[1];
			count[3] = nBins[0];

			start[2] = 0;
			start[3] = 0;
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*nBins[0]*nBins[1]);
			for (chip=firstChip; chip<=lastChip; chip++)
			{
				start[0] = chip-firstChip;
				for (corner=0; corner<nBins[2]; corner++)
				{		
					start[1] = corner;
					readHistCharac3d(0, nBins[0], 0, nBins[1], corner, 1, chip, -1, buff);
					dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
					dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
				}
			}
			free(buff);
			break;
			
		case HEXITEC_HIST_FORMAT_ENG_ONLY12:	
		case HEXITEC_HIST_FORMAT_ENG_ONLY11:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY10:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY9:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY8:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY7:		
			if (sumChips)
				firstChip = lastChip = -1;
			dimsFile[0] = 1+lastChip-firstChip;
			dimsFile[1] = numTFSpectra;
			dimsFile[2] = numEng;
			dataSpaceFile = H5::DataSpace(3, dimsFile);
			dimsMem[0] = numTFSpectra;
			dimsMem[1] = numEng;
			dataSpaceMem = H5::DataSpace(2, dimsMem );
			dataSpaceMem.selectAll();
			dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
			H5DSset_label(dataSet.getId(), 0, "Chip");
			H5DSset_label(dataSet.getId(), 1, "TimeFrame");
			H5DSset_label(dataSet.getId(), 2, "Energy");
			count[0] = 1;
			count[1] = numTFSpectra;
			count[2] = numEng;

			start[1] = 0;
			start[2] = 0;
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numTFSpectra);

			for (chip=firstChip; chip<=lastChip; chip++)
			{
				start[0] = chip-firstChip;
				readHistEngTime(numEng, firstTF, numTFSpectra,  chip, -1, buff);
				dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
				dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
			}
			free(buff);
			break;
			
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:	
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:		
			numCC = getnBinsClustClass(firstChip);
			if (sumChips)
				firstChip = lastChip = -1;
			dimsFile[0] = 1+lastChip-firstChip;
			dimsFile[1] = numTFSpectra;
			dimsFile[2] = numCC;
			dimsFile[3] = numEng;
			dataSpaceFile  =H5::DataSpace(4, dimsFile);
			dimsMem[0] = numCC;
			dimsMem[1] = numEng;
			dataSpaceMem = H5::DataSpace(2, dimsMem );
			dataSpaceMem.selectAll();
			dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
			H5DSset_label(dataSet.getId(), 0, "Chip");
			H5DSset_label(dataSet.getId(), 1, "TimeFrame");
			H5DSset_label(dataSet.getId(), 2, "ClusterClass");
			H5DSset_label(dataSet.getId(), 3, "Energy");
			count[0] = 1;
			count[1] = 1;
			count[2] = numCC;
			count[3] = numEng;

			start[2] = 0;
			start[3] = 0;
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numCC);

			for (chip=firstChip; chip<=lastChip; chip++)
			{
				start[0] = chip-firstChip;
				for (tf=0; tf<numTFSpectra; tf++)
				{
					start[1] = tf;
					readHistEngCCTime(numEng, 0, nBinsClustClass, firstTF+tf, 1,  chip, -1, buff);
					dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
					dataSet.write(buff, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
				}
			}
			free(buff);
			break;

		case HEXITEC_HIST_FORMAT_CALIB_SEPARATE:	
		case HEXITEC_HIST_FORMAT_CHARAC4D:			
			printf("saveSpectraHdf5: Format %d not supported yet\n", m_dataFormat[firstChip].histMode);
			break;
		}
		hdf5AddAttributes(dataSet, comments);
	}
	h5File.close();
}

void XDmaHexitec::hdf5AddAttributes(H5::DataSet &ds, const char **comments)
{
//	printf("hdf5AddAttributes:: Adding all attributes\n");
	for (int chip=0; chip<m_numChips; chip++)
	{
		uint32_t baselineReg = getChipReg(chip, HEXITEC_CHIP_BASESUB);
		hdf5AddIntAttribute(ds, "bSubMode", chip, HEXITEC_BSUB_GET_MODE(baselineReg));
		hdf5AddIntAttribute(ds, "bSubDivide", chip, HEXITEC_BSUB_GET_DIVIDE(baselineReg));
		hdf5AddIntAttribute(ds, "dither", chip, HEXITEC_BSUB_GET_DITHER(baselineReg));
		hdf5AddIntAttribute(ds, "bSubUseAbsTrig", chip, HEXITEC_BSUB_GET_ABS_TRIG(baselineReg));
		hdf5AddStringAttribute(ds, "bSubModeName", chip, m_bsubMaskNames[HEXITEC_BSUB_GET_MODE(baselineReg)]);

		uint32_t clusterMode = getChipReg(chip, HEXITEC_CHIP_CLUSTER);
		hdf5AddIntAttribute(ds, "clusterMode", chip, HEXITEC_CLUSTER_MODE_GET(clusterMode));
		hdf5AddIntAttribute(ds, "clusterAutoTrigRate", chip, HEXITEC_CLUSTER_AUTO_RATE_GET(clusterMode));
		uint32_t clusterEnb = getChipReg(chip, HEXITEC_CHIP_ENB_CLUSTER) & HEXITEC_CLUSTER_ENB_ALL;
		hdf5AddIntAttribute(ds, "clusterEnb", chip, clusterEnb);
		hdf5AddStringAttribute(ds, "clusterModeName", chip, m_clusterModeNames[HEXITEC_CLUSTER_MODE_GET(clusterMode)]);
		hdf5AddStringAttribute(ds, "clusterAutoTrigRateName", chip, m_autoTrigNames[HEXITEC_CLUSTER_AUTO_RATE_GET(clusterMode)]);

		hdf5AddIntAttribute(ds, "histFormat", chip, m_dataFormat[chip].histMode);
		hdf5AddIntAttribute(ds, "histNBinsEng", chip, m_dataFormat[chip].nBinsEng);
		hdf5AddIntAttribute(ds, "histNBinsClustClass", chip, m_dataFormat[chip].nBinsClustClass);
		hdf5AddIntAttribute(ds, "histNumTF", chip, m_dataFormat[chip].numTF);
		hdf5AddIntAttribute(ds, "histNumTFMapped", chip, m_dataFormat[chip].numTFMapped);
		hdf5AddIntAttribute(ds, "histNBinsLutAddr", chip, m_dataFormat[chip].nBinsLutAddr);
		hdf5AddIntAttribute(ds, "histUsesPosn", chip, m_dataFormat[chip].usesPosn);
		hdf5AddIntAttribute(ds, "histMappedMode", chip, m_dataFormat[chip].mappedMode);
		hdf5AddIntAttribute(ds, "histEngOnly", chip, getEngOnly(chip));
		uint32_t clusterGrade = getChipReg(chip, HEXITEC_CHIP_CLUSTER_GRADE);
		hdf5AddIntAttribute(ds, "clusterGrade", chip,clusterGrade);
		hdf5AddStringAttribute(ds, "trigAbsHigh", chip, m_absTrigHighName[chip]);
		hdf5AddStringAttribute(ds, "trigAbsgLow", chip, m_absTrigLowName[chip]);
		hdf5AddStringAttribute(ds, "trigMainPos", chip, m_mainTrigPosName[chip]);
		hdf5AddStringAttribute(ds, "trigMainNeg", chip, m_mainTrigNegName[chip]);
		hdf5AddStringAttribute(ds, "trigLowPos", chip, m_lowTrigPosName[chip]);
		hdf5AddStringAttribute(ds, "trigLowNeg", chip, m_lowTrigNegName[chip]);
		hdf5AddStringAttribute(ds, "trigEnable", chip, m_trigEnableName[chip]);
		hdf5AddStringAttribute(ds, "linCorr", chip, m_linCorrName[chip]);
		hdf5AddStringAttribute(ds, "cShareEdgePos", chip, m_cShareEdgePosName[chip]);
		hdf5AddStringAttribute(ds, "cShareNegNeb", chip, m_cShareNegNeb[chip]);
		hdf5AddStringAttribute(ds, "cShareLPos", chip, m_cShareLPos[chip]);
		hdf5AddStringAttribute(ds, "outputPixelMask", chip, m_outputPixelMaskName[chip]);

	}
	uint32_t iTfgControl = getGlobReg(HEXITEC_GLB_ITFG_CONTROL);

	hdf5AddIntAttribute(ds, "iTfgControl", -1, static_cast<int>(iTfgControl));
	hdf5AddIntAttribute(ds, "useiTfg", -1, static_cast<int>(iTfgControl&HEXITEC_ITFG_CONT_ENB));
	hdf5AddIntAttribute(ds, "iTfgMode", -1, static_cast<int>(HEXITEC_ITFG_CONT_GET_MODE(iTfgControl)));
	hdf5AddIntAttribute(ds, "iTfgSrc", -1, static_cast<int>(HEXITEC_ITFG_CONT_GET_SRC(iTfgControl)));
	hdf5AddIntAttribute(ds, "iTfgFalling", -1, static_cast<int>(HEXITEC_ITFG_CONT_GET_FALLING(iTfgControl)));

	hdf5AddIntAttribute(ds, "iTfgInputFramesPerTF", -1, getGlobReg(HEXITEC_GLB_ITFG_INP_PER_TF));
	hdf5AddIntAttribute(ds, "iTfgNumTf", -1, getGlobReg(HEXITEC_GLB_ITFG_NUM_TF));
	hdf5AddIntAttribute(ds, "iTfgNumCycles", -1, getGlobReg(HEXITEC_GLB_ITFG_NUM_CYCLES));
	if (comments != nullptr)
	{
		for (int i=0; *comments != nullptr; i++, comments++)
			hdf5AddStringAttribute(ds, "comment", i, *comments);
	}
}

void XDmaHexitec::hdf5AddIntAttribute(H5::DataSet &ds, const char * rootName, int chip, int value)
{
	char name[102];
	if (chip >= 0)
		snprintf(name, 100, "%s%d", rootName, chip);
	else
		strncpy(name, rootName, 100);
	name[100] = 0;
//	printf("hdf5AddIntAttribute:: Adding '%s' value=%d\n", name, value);

	H5::IntType intType(H5::PredType::STD_I32LE);
	H5::DataSpace attSpace(H5S_SCALAR);
	H5::Attribute att(ds.createAttribute(name, intType, attSpace));
	att.write(intType, &value);
}

void XDmaHexitec::hdf5AddStringAttribute(H5::DataSet &ds, const char * rootName, int chip, const char * cStr)
{
	char name[102];
	if (chip >= 0)
		snprintf(name, 100, "%s%d", rootName, chip);
	else
		strncpy(name, rootName, 100);
	name[100] = 0;
//	printf("hdf5AddStringAttribute:: Adding '%s' value=%s\n", name, cStr);

	H5::StrType strType(0, H5T_VARIABLE);
	H5::DataSpace attSpace(H5S_SCALAR);
	H5::Attribute att(ds.createAttribute(name, strType, attSpace));
	att.write(strType, std::string(cStr));
}

void XDmaHexitec::hdf5AddStringAttribute(H5::DataSet &ds, const char * rootName, int chip, std::string str)
{
	char name[102];
	if (chip >= 0)
		snprintf(name, 100, "%s%d", rootName, chip);
	else
		strncpy(name, rootName, 100);
	name[100] = 0;
//	printf("hdf5AddStringAttribute:: Adding '%s' value=%s\n", name, str.c_str());

	H5::StrType strType(0, H5T_VARIABLE);
	H5::DataSpace attSpace(H5S_SCALAR);
	H5::Attribute att(ds.createAttribute(name, strType, attSpace));
	att.write(strType, str);
}

/* 
https://stackoverflow.com/questions/5988824/setting-attributes-on-datasets-using-hdf5-c-api



StrType str_type(0, H5T_VARIABLE);
DataSpace att_space(H5S_SCALAR);
Attribute att = ds.createAttribute( "myAttribute", str_type, att_space );
att.write( str_type, "myString" );

*/