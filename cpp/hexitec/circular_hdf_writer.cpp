#include <iostream>
#include <iostream>
#include <fstream>
#include <cerrno>
#include <iomanip>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include "H5Cpp.h"
#include "hdf5_hl.h"
#include "circular_hdf_writer.h"

#include "datamod.h"

#include "xdma_hbm_hist.h"
#include "xdma_hexitec.h"

#include <cxxabi.h>

using namespace std;

CircularHdfWriter::CircularHdfWriter(XDmaHexitec & hexitec, const char *fName, int numEng, bool enbSpectra, bool enbMapped, bool sumChips) : m_hexitec(hexitec)
{
	int chip;
	strncpy(m_fName, fName, FILENAME_MAX);
	m_fName[FILENAME_MAX] = 0;	
	m_numEng = numEng;
	m_enbSpectra = enbSpectra;
	m_enbMapped = enbMapped;
	m_sumChips = sumChips;
	m_progressNumFrames = 4096;
}


CircularHdfWriter::~CircularHdfWriter()
{
	int i;
	printf("Entered : ~CircularHdfWriter destructor\n");
	stopReadoutThreads();
	
	for (i=0; i<HEXITEC_CIRC_WR_MAX_RX_THREADS; i++)
	{
		free(m_spectraThread[i].m_rBuf);
		m_spectraThread[i].m_rBuf = nullptr;
		free(m_spectraThread[i].m_mappedBuf);
		m_spectraThread[i].m_mappedBuf = nullptr;
	}
	for (i=0;i<m_numThreadsRequested; i++)
	{
		if (m_enbMapped)
			m_spectraThread[i].m_dataSetMapped.close();
		if (m_enbSpectra)
			m_spectraThread[i].m_dataSetSpectra.close();
		m_spectraThread[i].m_h5File.close();
	}
}


void CircularHdfWriter::setupReadoutMode(CircWriterReadoutMode readoutMode, int numSpectraThreadsReq, CircWriterUdpTxOnlyMode txOnly)
{	
	stopReadoutThreads();
	m_readoutMode = readoutMode;
	if (numSpectraThreadsReq > HEXITEC_CIRC_WR_MAX_RX_THREADS)
		throw XDmaHexitecException ("CircularHdfWriter: setupReadoutMode  numSpectraThreadsReq=%d  > %d\n", numSpectraThreadsReq, HEXITEC_CIRC_WR_MAX_RX_THREADS);
	m_numThreadsRequested = numSpectraThreadsReq; // Dont use this to say how many threads are actually running, just how many are to be created
	m_udpTxOnly = txOnly;
}	

void CircularHdfWriter::start()
{
	int numChips = m_hexitec.getNumChips();
	int chip=0;
	bool engOnly;
	int numCC;
	int i, t;
	uint32_t dataPath;

	stopReadoutThreads();
	dataPath = m_hexitec.getGlobReg(HEXITEC_GLB_DATA_PATH);
	dataPath |= HEXITEC_DATA_PATH_ENB_FLUSH;
	m_hexitec.setGlobReg(HEXITEC_GLB_DATA_PATH, dataPath); // Next start will send flush tokens to trigger readout

	m_hexitec.getHistFormat(0, &m_histFormat, &m_mappedMode, nullptr);

	for (chip=1; chip<numChips; chip++)
	{
		int histFormat, mappedMode;
		m_hexitec.getHistFormat(chip, &histFormat, &mappedMode, nullptr);
		if (mappedMode != m_mappedMode || histFormat != m_histFormat)
			throw XDmaHexitecException ("CircularHdfWriter: start: setup mismatch, histFormat[0]=%d, histFormat[%d], mappedMode[0]=%d, mappedMode[%d]=%d", 
					m_histFormat, chip, histFormat, m_mappedMode, chip, mappedMode);
	}	
	if (m_numEng < 0)
		m_numEng = m_hexitec.getnBinsEng(0);
	else if (m_numEng > m_hexitec.getnBinsEng(0))
		throw XDmaHexitecException("CircularHdfWriter: numEng=%d out of range 1...%d", m_numEng, m_hexitec.getnBinsEng(0));
		
	m_numEng = (m_numEng+7) & 0xFFFFFFF8;		// Round up to 8 word boundary

	m_errors = 0;
	m_mappedOverRuns = 0;
	m_spectraOverRuns = 0;
	m_readoutEnabled = false;
	switch(m_readoutMode)
	{
	case PolledMemMapped:
		createFiles(0);
		m_firmwareFrame = -1L;
		for (i=0; i<m_numThreadsRequested; i++)
		{
			m_spectraThread[i].m_finishedFrame = std::numeric_limits<int64_t>::min();  // So that mmappedReadout will trigger the first time.
			m_spectraThread[i].m_stop = false;
			m_spectraThread[i].m_thread = thread(&CircularHdfWriter::mmappedReadout, this, i);
			m_spectraThread[i].m_spectraUseCancel = false;
		}
		m_numSpectraThreads = m_numThreadsRequested;
		m_stopReadout = false;
		m_readoutUseCancel = false;
		m_readoutThread = thread(&CircularHdfWriter::mmappedControl, this, 0);
		m_readoutThreadValid = true;
		break;

	case IrqMemMapped:
		createFiles(0);
		m_firmwareFrame = -1L;
		for (i=0; i<m_numThreadsRequested; i++)
		{
			m_spectraThread[i].m_finishedFrame = std::numeric_limits<int64_t>::min();  // So that mmappedReadout will trigger the first time.
			m_spectraThread[i].m_stop = false;
			m_spectraThread[i].m_thread = thread(&CircularHdfWriter::mmappedReadout, this, i);
			m_spectraThread[i].m_spectraUseCancel = false;
		}
		m_numSpectraThreads = m_numThreadsRequested;
		m_stopReadout = false;
		m_readoutUseCancel = false;
		m_readoutThread = thread(&CircularHdfWriter::irqControl, this, 0);
		m_readoutThreadValid = true;
		break;

#if 0
	case AutoUDPThreadPerFrame:
		m_firmwareFrame = -1L;
		t = 0;
		m_numSpectraThreads = 0;
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_ONLY)
		{
			for (i=0; i<m_numThreadsRequested; i++, t++)
			{
				m_spectraThread[t].m_finishedFrame = -1L; // was std::numeric_limits<int64_t>::min();  .. Don't know why I thought I needed this?
				m_spectraThread[t].m_stop = false;
				m_spectraThread[t].m_spectraUseCancel = true;
				m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpReadoutSpectra, this, t);
			}
		}
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF)
		{
			for (i=0; i<m_numThreadsRequested; i++, t++)
			{
				m_spectraThread[t].m_finishedFrame = -1L; // was std::numeric_limits<int64_t>::min();  .. Don't know why I thought I needed this?
				m_spectraThread[t].m_stop = false;
				m_spectraThread[t].m_spectraUseCancel = true;
				m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpReadoutMapped, this, t);
			}
		}
		m_numSpectraThreads = t;
		break;
		
	case AutoUDPThreadPerPacket:
	case AutoUDPNoTrailer:
		m_firmwareFrame = -1L;
		t = 0;
		m_numSpectraThreads = 0;
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_ONLY)
		{
			for (i=0; i<m_numThreadsRequested; i++, t++)
			{
				m_spectraThread[t].m_finishedFrame = -1L; // was std::numeric_limits<int64_t>::min();  .. Don't know why I thought I needed this?
    	 		m_spectraThread[t].m_packetNum = -1; 
				m_spectraThread[t].m_stop = false;
				m_spectraThread[t].m_spectraUseCancel = true;
				if (m_readoutMode == AutoUDPNoTrailer)
					m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpReadoutNoTrailer, this, t);
				else
					m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpReadoutDistributed, this, t);
			}
		}
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF)
		{
			for (i=0; i<m_numThreadsRequested; i++, t++)
			{
				m_spectraThread[t].m_finishedFrame = -1L; // was std::numeric_limits<int64_t>::min();  .. Don't know why I thought I needed this?
    	 		m_spectraThread[t].m_packetNum = -1; 
				m_spectraThread[t].m_stop = false;
				m_spectraThread[t].m_spectraUseCancel = true;
				if (m_readoutMode == AutoUDPNoTrailer)
					m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpReadoutNoTrailer, this, t);
				else
					m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpReadoutDistributed, this, t);
			}
		}
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_ONLY)
		{
			for (i=0; i<m_numThreadsRequested; i++, t++)
			{
				m_spectraThread[t].m_finishedFrame = std::numeric_limits<int64_t>::min();  // So that udpCheckDistributed will trigger the first time.
    	 		m_spectraThread[t].m_packetNum = -1; 
				m_spectraThread[t].m_stop = false;
				m_spectraThread[t].m_spectraUseCancel = false;
				m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpCheckDistributed, this, t);
			}
		}
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF)
		{
			for (i=0; i<m_numThreadsRequested; i++, t++)
			{
				m_spectraThread[t].m_finishedFrame = std::numeric_limits<int64_t>::min();  // So that udpCheckDistributed will trigger the first time.
    	 		m_spectraThread[t].m_packetNum = -1; 
				m_spectraThread[t].m_stop = false;
				m_spectraThread[t].m_spectraUseCancel = false;
				m_spectraThread[t].m_thread = thread(&CircularHdfWriter::udpCheckDistributed, this, t);
			}
		}
		m_numSpectraThreads = t;
		break;
#endif
	default:
		printf("Unsupported readoutMode at %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}	
	m_errors = 0;
	createCircularProgress();
	waitThreadsStarted();
}

void CircularHdfWriter::stopReadoutThreads()
{
	int i;
	if (m_readoutThreadValid)
	{
		if (m_readoutUseCancel)
			pthread_cancel(m_readoutThread.native_handle());
		else
			m_stopReadout = true;
		m_readoutThread.join();
	}
	m_readoutThreadValid = false;
	m_readoutStarted = false;
	for (i=0;i<m_numSpectraThreads; i++)
	{
		if (m_spectraThread[i].m_spectraUseCancel)
		{
				pthread_cancel(m_spectraThread[i].m_thread.native_handle());
		}
		else
		{
			lock_guard<mutex> lk(m_curFrameMutex);
			m_spectraThread[i].m_stop = true;
			m_cv.notify_all();
		}
	}
	for (i=0;i<m_numSpectraThreads; i++)
	{
		m_spectraThread[i].m_thread.join();
		m_spectraThread[i].m_started = false;
	}

	m_numSpectraThreads = 0;

}
void CircularHdfWriter::waitThreadsStarted()
{
	if (m_readoutThreadValid)
	{
		unique_lock<mutex> lk(m_threadsStartedMutex);
		while (!m_readoutStarted)
			m_threadsStartedCv.wait(lk);
		lk.unlock();
	}
	do
	{
		bool allStarted=true;
		unique_lock<mutex> lk(m_threadsStartedMutex);
		for (int i=0;i <m_numSpectraThreads; i++)
			allStarted &= m_spectraThread[i].m_started ;
		if (allStarted)
		{
			lk.unlock();
			break;
		}
		m_threadsStartedCv.wait(lk);
		lk.unlock();
	} while (1);	
}	
#if 0
void CircularHdfWriter::allocateReadBuffers()
{
	int numChips = m_hexitec.getNumChips();
	int numEng=0;
	int chip=0;
	bool engOnly;
	int numCC;
	size_t bufSize;
	size_t mappedSize;
	int i;
	int numThreads = 0;
	
	engOnly = m_hexitec.getEngOnly(chip);
	numEng = m_hexitec.getnBinsEng(chip);
	numCC = m_hexitec.getnBinsClustClass(chip);
	stopReadoutThreads();

	if (engOnly)
		bufSize = numEng*sizeof(uint32_t)*(size_t)numCC;
	else
		bufSize = numEng*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*sizeof(uint32_t)*(size_t)numCC;
	mappedSize = HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*HEXITEC_NBINS_MAPPED*sizeof(uint32_t);
	
	switch (m_readoutMode)
	{
	case PolledMemMapped:
	case IrqMemMapped:
		numThreads = m_numThreadsRequested*numChips;
		break;
	case AutoUDPThreadPerFrame:
		numThreads = m_numThreadsRequested;
		if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
			numThreads *= 2;
		bufSize *= numChips;	// FixMe: If summing chips, we don't need to do this in engOnly modes.
		mappedSize *= numChips;
		break;
		
	case AutoUDPThreadPerPacket:
	case AutoUDPNoTrailer:

		numThreads = m_numThreadsRequested;
		if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
			numThreads *= 2;
		bufSize *= numChips;	// FixMe: If summing chips, we don't need to do this in engOnly modes.
		mappedSize *= numChips;
		break;

	default:
		throw XDmaHexitecException ("CircularHdfWriter: allocateReadBuffers  missing coding for readout mode =%d\n", m_readoutMode);
	}
	for (i=numThreads; i<HEXITEC_CIRC_WR_MAX_RX_THREADS; i++)
	{
		lock_guard<mutex> lk(m_spectraThread[i].m_mutex);
		free(m_spectraThread[i].m_rBuf);
		m_spectraThread[i].m_rBuf = nullptr;
		free(m_spectraThread[i].m_mappedBuf);
		m_spectraThread[i].m_mappedBuf = nullptr;
	}

	for (i=0; i<numThreads; i++)
	{
		bool needMapped = false, needSpectra = false;
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_ONLY)
			needSpectra = true;
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF)
			needMapped = true;
		switch (m_readoutMode)
		{
		case PolledMemMapped:
		case IrqMemMapped:
			break;
		case AutoUDPThreadPerFrame:
		case AutoUDPThreadPerPacket:
		case AutoUDPNoTrailer:
			if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
			{
				if (i < numThreads/2)
					needMapped = false;		// Use first half only for full spectra
				else
					needSpectra = false;
			}
			break;
		default:
			throw XDmaHexitecException ("CircularHdfWriter: allocateReadBuffers  missing coding for readout mode =%d\n", m_readoutMode);
		}
		
		if (m_rBufSize != bufSize || !needSpectra )
		{
			free(m_spectraThread[i].m_rBuf);
			m_spectraThread[i].m_rBuf = nullptr;
		}
		if (!needMapped)
		{
			free(m_spectraThread[i].m_mappedBuf);
			m_spectraThread[i].m_mappedBuf = nullptr;
		}
		if (needSpectra && m_spectraThread[i].m_rBuf == nullptr)
		{
			posix_memalign((void **)&m_spectraThread[i].m_rBuf, 4096, bufSize);
			if (m_spectraThread[i].m_rBuf == nullptr)
				throw XDmaHexitecException ("CircularHdfWriter::allocateReadBuffers: Out of memory");
		}
		if (needMapped && m_spectraThread[i].m_mappedBuf == nullptr)
		{
			posix_memalign((void **)&m_spectraThread[i].m_mappedBuf, 4096, mappedSize);
			if (m_spectraThread[i].m_mappedBuf == nullptr)
				throw XDmaHexitecException ("CircularHdfWriter::allocateReadBuffers: Out of memory");
		}
	}
	m_rBufSize = bufSize;
}
#endif

void CircularHdfWriter::createFiles(int firstIndex)
{
	char fNameExt[FILENAME_MAX+2];
	int len;
	int i, j;
	hsize_t maxDimsFile[HEXITEC_CIRC_WR_MAX_RANK], dimsChunk[HEXITEC_CIRC_WR_MAX_RANK];
	int numRows, numCols;
	size_t totalRowBytes;
	const char *labelsEngPosTime[HEXITEC_CIRC_WR_MAX_RANK] = {"TimeFrame", "Row", "Col", "Energy"};
	const char *labelsEngPosCCTime[HEXITEC_CIRC_WR_MAX_RANK] = {"TimeFrame", "ClusterClass", "Row", "Col", "Energy"};
	const char *labelsEngOnlyTime[HEXITEC_CIRC_WR_MAX_RANK] = {"TimeFrame", "Energy"};
	const char *labelsEngOnlyChipTime[HEXITEC_CIRC_WR_MAX_RANK] = {"TimeFrame", "Chip", "Energy"};
	const char *labelsEngOnlyCCTime[HEXITEC_CIRC_WR_MAX_RANK] = {"TimeFrame", "ClusterClass", "Energy"};
	const char *labelsEngOnlyCCChipTime[HEXITEC_CIRC_WR_MAX_RANK] = {"TimeFrame", "Chip", "ClusterClass", "Energy"};
	const char **labels=nullptr;
	
	numRows = m_hexitec.getNumChipRows()*HEXITEC_NUM_ROWS;
	numCols = m_hexitec.getNumChipCols()*HEXITEC_NUM_COLS;
	if ((len=strlen(m_fName)) > 3 && strcmp(m_fName+len-3, ".h5")==0)
		m_fName[len-3] = 0;
	
	for (i=0; i<m_numThreadsRequested; i++)
	{
		len = strlen(m_fName);
		if (m_numThreadsRequested > 1)
		{
			if (len >= FILENAME_MAX-9)
				throw XDmaHexitecException ("CircularHdfWriter::createFiles: File name %s is too long to append _tfXX.h5", m_fName);
			snprintf(fNameExt, FILENAME_MAX,"%s_tf%02d.h5", m_fName, i);
			fNameExt[FILENAME_MAX] = 0;
		}
		else
		{
			if (len >= FILENAME_MAX-4)
				throw XDmaHexitecException ("CircularHdfWriter::createFiles: File name %s is too long to append .h5", m_fName);
			strcpy(fNameExt, m_fName);
			strcat(fNameExt, ".h5");
		}
		printf("CircularHdfWriter::createFiles: creating file '%s'\n", fNameExt);
		H5::H5File tmpH5File(fNameExt, H5F_ACC_TRUNC); 	// Create the file so I can open it, because H5CC seems not to support std::move?
//		std::swap(m_spectraThread[i+firstIndex].m_h5File, tmpH5File);
		tmpH5File.close();
//		m_spectraThread[i+firstIndex].m_h5File = H5::H5File(fNameExt, H5F_ACC_TRUNC);
		m_spectraThread[i+firstIndex].m_h5File.openFile(fNameExt, H5F_ACC_RDWR);
		
		if (m_enbMapped && m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF && m_hexitec.getNumTFMapped(0) > 0)
		{
			m_spectraThread[i+firstIndex].m_dimsFileMapped[0] = 0;
			m_spectraThread[i+firstIndex].m_dimsFileMapped[1] = numRows;
			m_spectraThread[i+firstIndex].m_dimsFileMapped[2] = numCols;
			m_spectraThread[i+firstIndex].m_dimsFileMapped[3] = HEXITEC_NBINS_MAPPED;
			
			maxDimsFile[0] = H5S_UNLIMITED;
			maxDimsFile[1] = numRows;
			maxDimsFile[2] = numCols;
			maxDimsFile[3] = HEXITEC_NBINS_MAPPED;
	 
			H5::DataSpace dataSpaceFile(4, m_spectraThread[i+firstIndex].m_dimsFileMapped, maxDimsFile);
			H5::DSetCreatPropList pList(H5P_DATASET_CREATE);
			pList.setLayout(H5D_CHUNKED);
			dimsChunk[0] = 1;
			dimsChunk[1] = numRows;
			dimsChunk[2] = numCols;
			dimsChunk[3] = HEXITEC_NBINS_MAPPED;
			pList.setChunk(4, dimsChunk);
			m_spectraThread[i+firstIndex].m_dataSetMapped = m_spectraThread[i+firstIndex].m_h5File.createDataSet("/mapped", H5::PredType::NATIVE_UINT32, dataSpaceFile, pList);
			pList.close();
			dataSpaceFile.close();
			H5DSset_label(m_spectraThread[i+firstIndex].m_dataSetMapped.getId(), 0, "TimeFrame");
			H5DSset_label(m_spectraThread[i+firstIndex].m_dataSetMapped.getId(), 1, "Row");
			H5DSset_label(m_spectraThread[i+firstIndex].m_dataSetMapped.getId(), 2, "Column");
			H5DSset_label(m_spectraThread[i+firstIndex].m_dataSetMapped.getId(), 3, "MappedEng");

			posix_memalign((void **)&(m_spectraThread[i+firstIndex].m_mappedBuf), 4096 /*alignment */,  sizeof(uint32_t)*HEXITEC_NBINS_MAPPED*numRows*numCols);
			if (m_spectraThread[i+firstIndex].m_mappedBuf == nullptr)
				throw XDmaHexitecException ("CircularHdfWriter::createFiles: Out of memory");
		}
		else
			m_enbMapped = false;

		if (m_enbSpectra && m_mappedMode != HEXITEC_HIST_MAPPED_MODE_ONLY && m_hexitec.getNumTF(0) > 0)
		{
			int numCC = m_hexitec.getnBinsClustClass(0);
			switch (m_histFormat)
			{
			case HEXITEC_HIST_FORMAT_RUN12:
			case HEXITEC_HIST_FORMAT_RUN11:		
			case HEXITEC_HIST_FORMAT_RUN10:			
			case HEXITEC_HIST_FORMAT_RUN9:		
			case HEXITEC_HIST_FORMAT_RUN8:		
			case HEXITEC_HIST_FORMAT_RUN7:
			case HEXITEC_HIST_FORMAT_RUN10LSB:			
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[0] = 0;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[1] = numRows;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[2] = numCols;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[3] = m_numEng;
				m_spectraThread[i+firstIndex].m_rankFile = 4;

				totalRowBytes = sizeof(uint32_t)*numCols*m_numEng;
				m_readNumRows =  m_maxReadBytes/totalRowBytes;
				if (m_readNumRows > numRows)
					m_readNumRows = numRows;
				
				posix_memalign((void **)&m_spectraThread[i+firstIndex].m_rBuf, 4096 /*alignment */,  sizeof(uint32_t)*m_numEng*m_readNumRows*numCols);
				labels = labelsEngPosTime;
				break;	

			case HEXITEC_HIST_FORMAT_ENG_POS_CC12:		
			case HEXITEC_HIST_FORMAT_ENG_POS_CC11:		
			case HEXITEC_HIST_FORMAT_ENG_POS_CC10:		
			case HEXITEC_HIST_FORMAT_ENG_POS_CC9:			
			case HEXITEC_HIST_FORMAT_ENG_POS_CC8:			
			case HEXITEC_HIST_FORMAT_ENG_POS_CC7:			
			case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:		
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[0] = 0;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[1] = numCC;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[2] = numRows;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[3] = numCols;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[4] = m_numEng;
				m_spectraThread[i+firstIndex].m_rankFile = 5;
				
				totalRowBytes = sizeof(uint32_t)*numCols*m_numEng;
				m_readNumRows =  m_maxReadBytes/totalRowBytes;
				if (m_readNumRows > numRows)
					m_readNumRows = numRows;
				
				posix_memalign((void **)&m_spectraThread[i+firstIndex].m_rBuf, 4096 /*alignment */,  sizeof(uint32_t)*m_numEng*m_readNumRows*numCols);
				labels = labelsEngPosCCTime;
				break;

			case HEXITEC_HIST_FORMAT_ENG_ONLY12:	
			case HEXITEC_HIST_FORMAT_ENG_ONLY11:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY10:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY9:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY8:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY7:		
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[0] = 0;
				j=1;
				if (!m_sumChips && m_hexitec.getNumChips() > 1)
				{
					labels = labelsEngOnlyChipTime;
					m_spectraThread[i+firstIndex].m_dimsFileSpectra[j++] = m_hexitec.getNumChips();
				}
				else
					labels = labelsEngOnlyTime;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[j++] = m_numEng;
				m_spectraThread[i+firstIndex].m_rankFile = j;
	
				posix_memalign((void **)&m_spectraThread[i+firstIndex].m_rBuf, 4096 /*alignment */,  sizeof(uint32_t)*m_numEng);
				break;
				
			case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:	
			case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:		
			case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:		
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[0] = 0;
				j=1;
				if (!m_sumChips && m_hexitec.getNumChips() > 1)
				{
					m_spectraThread[i+firstIndex].m_dimsFileSpectra[j++] = m_hexitec.getNumChips();
					labels = labelsEngOnlyCCChipTime;
				}
				else
					labels = labelsEngOnlyCCTime;

				m_spectraThread[i+firstIndex].m_dimsFileSpectra[j++] = numCC;
				m_spectraThread[i+firstIndex].m_dimsFileSpectra[j++] = m_numEng;
				m_spectraThread[i+firstIndex].m_rankFile = j;
				
				posix_memalign((void **)&m_spectraThread[i+firstIndex].m_rBuf, 4096 /*alignment */,  sizeof(uint32_t)*m_numEng*numCC);

				break;
#if 0							
			case HEXITEC_HIST_FORMAT_CALIB_COMB12:	
			case HEXITEC_HIST_FORMAT_CALIB_COMB11:	
			case HEXITEC_HIST_FORMAT_CALIB_COMB10:	
			case HEXITEC_HIST_FORMAT_CALIB_COMBTYPE:
				{
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
					H5::DataSpace dataSpaceFile(4, dimsFile);
					dimsMem[0] = HEXITEC_RECIP_SIZE;
					dimsMem[1] = numEng;
					H5::DataSpace dataSpaceMem(2, dimsMem );
					dataSpaceMem.selectAll();
					H5::DataSet dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
					
					posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_RECIP_SIZE);

					count[0] = 1;
					count[1] = 1;
					count[2] = HEXITEC_RECIP_SIZE;
					count[3] = numEng;

					start[2] = 0;
					start[3] = 0;
					for (chip=firstChip; chip<=lastChip; chip++)
					{
						for (clustClass=0; clustClass<nBinsClustClass; clustClass++)
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
				}				
				break;
				
			case HEXITEC_HIST_FORMAT_CHARAC2D12:
			case HEXITEC_HIST_FORMAT_CHARAC2D10:
				{
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
					H5::DataSpace dataSpaceFile(4, dimsFile);
					dimsMem[0] = nBins[1];
					dimsMem[1] = nBins[0];
					H5::DataSpace dataSpaceMem(2, dimsMem );
					dataSpaceMem.selectAll();
					H5::DataSet dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
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
				}
				break;

			case HEXITEC_HIST_FORMAT_CHARAC3D:
				{
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
					H5::DataSpace dataSpaceFile(4, dimsFile);
					dimsMem[0] = nBins[1];
					dimsMem[1] = nBins[0];
					H5::DataSpace dataSpaceMem(2, dimsMem );
					dataSpaceMem.selectAll();
					H5::DataSet dataSet = h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile);
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
				}
				break;
#endif				

			case HEXITEC_HIST_FORMAT_CALIB_SEPARATE:	
			case HEXITEC_HIST_FORMAT_CHARAC4D:			
				throw XDmaHexitecException ("CircularHdfWriter: Format %d not supported yet\n", m_histFormat);
				break;
			default:
				throw XDmaHexitecException ("CircularHdfWriter: Format %d not supported yet", m_histFormat);
				
			}
			for(j=0; j<HEXITEC_CIRC_WR_MAX_RANK; j++)
			{
				maxDimsFile[j] = m_spectraThread[i+firstIndex].m_dimsFileSpectra[j];
				dimsChunk[j] = m_spectraThread[i+firstIndex].m_dimsFileSpectra[j];
			}
			maxDimsFile[0] = H5S_UNLIMITED;
			dimsChunk[0] = 1;
			H5::DSetCreatPropList pList(H5P_DATASET_CREATE);
			pList.setChunk(m_spectraThread[i+firstIndex].m_rankFile, dimsChunk);
			m_spectraThread[i+firstIndex].m_dimsFileSpectra[0] = 0;
			H5::DataSpace dataSpaceFile(m_spectraThread[i+firstIndex].m_rankFile, m_spectraThread[i+firstIndex].m_dimsFileSpectra, maxDimsFile);
			m_spectraThread[i+firstIndex].m_dataSetSpectra = m_spectraThread[i+firstIndex].m_h5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile, pList);
//			m_spectraThread[i+firstIndex].m_dataSetSpectra = tmpH5File.createDataSet("/spectra", H5::PredType::NATIVE_UINT32, dataSpaceFile, pList);
//			m_spectraThread[i+firstIndex].m_dimsFileSpectra[0] = 0;
			if (labels != nullptr)
			{
				for (j=0; j<m_spectraThread[i+firstIndex].m_rankFile; j++)
					H5DSset_label(m_spectraThread[i+firstIndex].m_dataSetSpectra.getId(), j, labels[j]);
			}
			pList.close();
			dataSpaceFile.close();
			if (m_spectraThread[i+firstIndex].m_rBuf == nullptr)
				throw XDmaHexitecException ("CircularHdfWriter::createFiles: Out of memory");
		}
		else
			m_enbSpectra = false;
	}
}

void CircularHdfWriter::writeMapped(int tNum, int bufNum, int tfWrapped)
{
	int chip;
	int i;
	hsize_t start[4], count[5], dimsMemMapped[3];
	int numRows, numCols;
	H5::DataSpace dataSpaceFile;
	
	m_spectraThread[tNum].m_dimsFileMapped[0]++;

	m_spectraThread[tNum].m_dataSetMapped.extend(m_spectraThread[tNum].m_dimsFileMapped);

	dataSpaceFile = m_spectraThread[tNum].m_dataSetMapped.getSpace();
	numRows = m_hexitec.getNumChipRows()*HEXITEC_NUM_ROWS;
	numCols = m_hexitec.getNumChipCols()*HEXITEC_NUM_COLS;

	dimsMemMapped[0] = numRows;
	dimsMemMapped[1] = numCols;
	dimsMemMapped[2] = HEXITEC_NBINS_MAPPED;

	H5::DataSpace dataSpaceMemMapped(3, dimsMemMapped );
	dataSpaceMemMapped.selectAll();

	for (i=0; i<4; i++)
		start[i] = 0;
	count[0] = 1;
	count[1] = numRows;
	count[2] = numCols;
	count[3] = HEXITEC_NBINS_MAPPED;

	start[0] = m_spectraThread[tNum].m_dimsFileMapped[0]-1;
	m_hexitec.readMappedEngGlobColRowTime(HEXITEC_NBINS_MAPPED, 0, numCols, 0, numRows, tfWrapped, 1, -1, m_spectraThread[bufNum].m_mappedBuf);
	dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
	m_spectraThread[tNum].m_dataSetMapped.write(m_spectraThread[bufNum].m_mappedBuf, H5::PredType::NATIVE_UINT32, dataSpaceMemMapped, dataSpaceFile);
}


void CircularHdfWriter::writeSpectra(int tNum, int bufNum, int tfWrapped)
{
//	H5:DataSpace oldSize[HEXITEC_CIRC_WR_MAX_RANK], newSize[HEXITEC_CIRC_WR_MAX_RANK];
	int chip;
	int firstChip=0, lastChip=m_hexitec.getNumChips()-1;
	int i;
	hsize_t start[HEXITEC_CIRC_WR_MAX_RANK], count[HEXITEC_CIRC_WR_MAX_RANK], dimsMemSpectra[HEXITEC_CIRC_WR_MAX_RANK];
	hsize_t dimsFile[HEXITEC_CIRC_WR_MAX_RANK], maxDimsFile[HEXITEC_CIRC_WR_MAX_RANK];
	int numRows, numCols;

	H5::DataSpace dataSpaceFile;
	int row, thisNumRows;
	int numCC=1;

//	printf("CircularHdfWriter::writeSpectra: tNum=%d, budNum=%d, tfWrapped=%d\n", tNum, bufNum, tfWrapped);

	m_spectraThread[tNum].m_dimsFileSpectra[0]++;
	m_spectraThread[tNum].m_dataSetSpectra.extend(m_spectraThread[tNum].m_dimsFileSpectra);

	dataSpaceFile = m_spectraThread[tNum].m_dataSetSpectra.getSpace();
	numRows = m_hexitec.getNumChipRows()*HEXITEC_NUM_ROWS;
	numCols = m_hexitec.getNumChipCols()*HEXITEC_NUM_COLS;


	for (i=0; i<HEXITEC_CIRC_WR_MAX_RANK; i++)
		start[i] = 0;

	start[0] = m_spectraThread[tNum].m_dimsFileSpectra[0]-1;

	switch (m_histFormat)
	{
	case HEXITEC_HIST_FORMAT_RUN12:
	case HEXITEC_HIST_FORMAT_RUN11:		
	case HEXITEC_HIST_FORMAT_RUN10:			
	case HEXITEC_HIST_FORMAT_RUN9:		
	case HEXITEC_HIST_FORMAT_RUN8:		
	case HEXITEC_HIST_FORMAT_RUN7:
	case HEXITEC_HIST_FORMAT_RUN10LSB:			
		row = 0;
		while (row < numRows)
		{
			thisNumRows = numRows-row;
			if (thisNumRows > m_readNumRows)
				thisNumRows = m_readNumRows;
			
			dimsMemSpectra[0] = thisNumRows;
			dimsMemSpectra[1] = numCols;
			dimsMemSpectra[2] = m_numEng;
			
			H5::DataSpace dataSpaceMem(3, dimsMemSpectra);
			dataSpaceMem.selectAll();
			count[0] = 1;
			count[1] = thisNumRows;
			count[2] = numCols;
			count[3] = m_numEng;
			start[1] = row;
			m_hexitec.readHistEngGlobColRowTime(m_numEng, 0, numCols, row, thisNumRows, tfWrapped, 1, -1, m_spectraThread[bufNum].m_rBuf);
			dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
			m_spectraThread[tNum].m_dataSetSpectra.write(m_spectraThread[bufNum].m_rBuf, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
			dataSpaceMem.close();
			row += thisNumRows;
		}
		break;
		
	case HEXITEC_HIST_FORMAT_ENG_POS_CC12:		
	case HEXITEC_HIST_FORMAT_ENG_POS_CC11:		
	case HEXITEC_HIST_FORMAT_ENG_POS_CC10:		
	case HEXITEC_HIST_FORMAT_ENG_POS_CC9:			
	case HEXITEC_HIST_FORMAT_ENG_POS_CC8:			
	case HEXITEC_HIST_FORMAT_ENG_POS_CC7:			
	case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:		
		numCC = m_hexitec.getnBinsClustClass(0);
		for (int cc=0; cc<numCC; cc++)
		{
			row = 0;
			while (row < numRows)
			{
				thisNumRows = numRows-row;
				if (thisNumRows > m_readNumRows)
					thisNumRows = m_readNumRows;
				
				dimsMemSpectra[0] = thisNumRows;
				dimsMemSpectra[1] = numCols;
				dimsMemSpectra[2] = m_numEng;
				
				H5::DataSpace dataSpaceMem(3, dimsMemSpectra);
				dataSpaceMem.selectAll();
				count[0] = 1;
				count[1] = 1;
				count[2] = thisNumRows;
				count[3] = numCols;
				count[4] = m_numEng;
				start[1] = cc;
				start[2] = row;
				m_hexitec.readHistEngGlobColRowCCTime(m_numEng, 0, numCols, row, thisNumRows, cc, 1, tfWrapped, 1, -1, m_spectraThread[bufNum].m_rBuf);
				dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
				m_spectraThread[tNum].m_dataSetSpectra.write(m_spectraThread[bufNum].m_rBuf, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
				dataSpaceMem.close();
				row += thisNumRows;
			}
		}		
		break;
		
	case HEXITEC_HIST_FORMAT_ENG_ONLY12:	
	case HEXITEC_HIST_FORMAT_ENG_ONLY11:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY10:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY9:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY8:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY7:		
		{ // New scope for dataSpaceMem
			dimsMemSpectra[0] = m_numEng;
			H5::DataSpace dataSpaceMem(1, dimsMemSpectra);
			dataSpaceMem.selectAll();
	
			if (m_sumChips)
				firstChip = lastChip = -1;
			i=0;
			count[i++] = 1;
			if (!m_sumChips && lastChip>firstChip)
				count[i++] = 1;
			count[i++] = m_numEng;
			for (chip=firstChip; chip<=lastChip; chip++)
			{
				if (!m_sumChips && lastChip>firstChip)
					start[1] = chip-firstChip;
					
				m_hexitec.readHistEngTime(m_numEng, tfWrapped, 1,  chip, -1, m_spectraThread[bufNum].m_rBuf);
				dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
				m_spectraThread[tNum].m_dataSetSpectra.write(m_spectraThread[bufNum].m_rBuf, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
			}
			dataSpaceMem.close();
		}
		break;
				
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:	
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:		
	case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:		
		{
			numCC = m_hexitec.getnBinsClustClass(firstChip);
			if (m_sumChips)
				firstChip = lastChip = -1;

			dimsMemSpectra[0] = numCC;
			dimsMemSpectra[1] = m_numEng;
			H5::DataSpace dataSpaceMem(2, dimsMemSpectra );
			dataSpaceMem.selectAll();
	
			i=0;
			count[i++] = 1;
			if (!m_sumChips && lastChip>firstChip)
				count[i++] = 1;
			count[i++] = numCC;
			count[i++] = m_numEng;
			for (chip=firstChip; chip<=lastChip; chip++)
			{
				if (!m_sumChips && lastChip>firstChip)
					start[1] = chip-firstChip;
				m_hexitec.readHistEngCCTime(m_numEng, 0, numCC, tfWrapped, 1,  chip, -1, m_spectraThread[bufNum].m_rBuf);
				dataSpaceFile.selectHyperslab( H5S_SELECT_SET, count, start, nullptr, nullptr);
				m_spectraThread[tNum].m_dataSetSpectra.write(m_spectraThread[bufNum].m_rBuf, H5::PredType::NATIVE_UINT32, dataSpaceMem, dataSpaceFile);
			}
			dataSpaceMem.close();
		}
		break;

	default:
		throw XDmaHexitecException ("CircularHdfWriter: writeSpectra: Format %d not supported yet", m_histFormat);
		break;
				
	}
}



void CircularHdfWriter::createCircularProgress()
{
	int numX = 16384;
	int numY = (m_progressNumFrames+numX-1)/numX;
	int numT = m_numSpectraThreads+2;
	char modName[100];
	
	if (m_progMod == nullptr || m_progMod->head.num_x != numX || m_progMod->head.num_y != numY || m_progMod->head.num_t == numT)
	{
		sprintf(modName, "hxt_circ_writer%dx%d", numY, numT);
		m_progMod = id_mkmod3d (modName, numX, numY, numT, "Det frame low", "Det frame hi", "chip/Thread", nullptr, DATA_LONG, nullptr);
		if (m_progMod != nullptr)
			id_clear_mod(m_progMod);
	}
}
void CircularHdfWriter::updateProgress(int tNum, int64_t firmwareFrame, int64_t finishedFrame)
{
	if (m_progMod == nullptr)
		return;
	int x, y;
	int numX = m_progMod->head.num_x;
	int numY = m_progMod->head.num_y;
	int fw = (int)m_itfgFrame;
//	int fw = (int)firmwareFrame;
	int finished= (int)finishedFrame;
	int32_t *ptr;	
	if (fw < 0)
		fw = 0;
	x = fw % numX;
	y = fw / numX;
//	printf("Update progress (%d, %d, %d)=%d\n", x, y, tNum, finished);
	if (tNum >= m_progMod->head.num_t)
		return;
	if ( y < numY)
	{
		ptr = (int32_t *)id_get_ptr_safe(m_progMod, x, y, tNum);
		*ptr = (int32_t)finished;
	}
}
#if 0
void CircularHdfWriter::run(int options, int rawFramesPerTF, int64_t numTF)
{
	uint32_t dataPath=0;
	int numEng, numCC;
	bool engOnly;
	int quickNumFrames;
	int i;
	int farmMask=0;
	std::chrono::time_point<std::chrono::steady_clock> start, now, startEthernet, startHist, endHistFlush, stopItfg;
	std::chrono::duration<double> elapsedDur;
	std::chrono::duration<double> itfgDur;
	char fName[FILENAME_MAX+10];
	int64_t extTimeframe=0L;
	options |= TEST_HIT_OPT_READOUT_ECR;	// Always work in Eng Col row mode as this is all new firmware will support.
	
	engOnly = m_hexitec.getEngOnly(0);
	numEng = m_hexitec.getnBinsEng(0);
	numCC = m_hexitec.getnBinsClustClass(0);
	mprintf(1, "pbRunCircular: %s\n", m_description.c_str());
	mprintf(1, "test options = 0x%08X, clusterMode=%d, histFormat=%d, mappedMode=%d => numEng=%d, numClusterClass=%d, engOnly=%d\n", m_options,
		m_clusterMode, m_histFormat, m_mappedMode, numEng, numCC, engOnly);
	
	if (m_logFile != nullptr)
		fprintf(m_logFile, "%s\t%04X\t%d\t%d\t%d\t%d\t%d", m_readoutName[m_readoutMode], m_histFormat, m_mappedMode, numEng, numCC, engOnly, rawFramesPerTF);
	mprintf(2, ".. Making test patterns\n");
	
	m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  0);		// Turn off Run bit to stop any current patterns.
	m_hexitec.stopDataMoverStreamUDP(0);	// Stop any running data move r(UDP) TX
	m_hexitec.stopDataMoverStreamUDP(1);	// Stop any running data move r(UDP) TX
	buildTestPatterns();
	m_packetShift = 0;
	if (m_readoutMode == AutoUDPThreadPerFrame || m_readoutMode == AutoUDPThreadPerPacket)
	{
		for (i=0;i<8; i++)
		{
			if (m_numThreadsRequested == 1<<i)
				break;
		}
		if (i==8)
		{
			printf("ERROR: Unsupported number of UDP RX threads %d, must be power of 2\n", m_numThreadsRequested);
			return;
		}
		farmMask = (1<<i)-1;	// In mapp interleaved mode, with e.g. m_numThreadsRequested==8, 8 threads are used fro spectra and 8 for mappped
								// Spectra datamover context used base=0, mask=7, mapped uses base=8, mask=7
		int numThreads = m_numThreadsRequested;
		if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
			numThreads *=2;
		if (m_udpTxOnly == TxNormal)
			m_hexitec.udpTxTestCreateSockets(&m_accelIpAddr, &m_serverIpAddr, UDP_TX_ACCEL_PORT, UDP_TX_SERVER_PORT, numThreads, true);
		m_hexitec.udpTxSetup(&m_accelIpAddr, &m_serverIpAddr, 0, 0, 0, numThreads, true, test_inter_frame_gap, test_use_arp);
		
	}
	else if (m_readoutMode == AutoUDPNoTrailer)
	{
		for (i=0;i<8; i++)
		{
			if (m_numThreadsRequested == 1<<i)
				break;
		}
		if (i==8)
		{
			printf("ERROR: Unsupported number of UDP RX threads %d, must be power of 2\n", m_numThreadsRequested);
			return;
		}
		m_packetShift = i;

		farmMask = (1<<i)-1;	// In mapp interleaved mode, with e.g. m_numThreadsRequested==8, 8 threads are used fro spectra and 8 for mappped
								// Spectra datamover context used base=0, mask=7, mapped uses base=8, mask=7
		int numThreads = m_numThreadsRequested;
		if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
			numThreads *=2;
		// Note accelPort is set to -1 to disable the use of connect and send the src port from the firmware.
		if (m_udpTxOnly==TxNormal)
			m_hexitec.udpTxTestCreateSockets(&m_accelIpAddr, &m_serverIpAddr, -1, UDP_TX_SERVER_PORT, numThreads, true);
		m_hexitec.udpTxSetup(&test_accel_tx_ip_addr, &test_server_ip_addr, -1, 0, 0, numThreads, true, test_inter_frame_gap, test_use_arp);
		
	}
	
	mprintf(2, ".. Allocating read buffers for %d thread per chip\n", m_numThreadsRequested);
	if (m_udpTxOnly==TxNormal)
		allocateReadBuffers();


	if (m_udpTxOnly == TxNormal)
	{
		startReadoutThreads();
		createCircularProgress();
	}
	
	XDmaHexitec::AutonomousMode autoMode=XDmaHexitec::AutoTriggerReadAndClear;
	if (m_udpTxOnly == TxOnly1Pass)
		autoMode = XDmaHexitec::AutoTriggerRead;	// Disable auto clear in 1 pass mode
	if (m_readoutMode == AutoUDPThreadPerFrame)
	{
		int farmBase=0;
		m_hexitec.disableDataMoverUDPTrailer(false, 0);
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_ONLY)
		{
			mprintf(2, "... Setting up farm mode (port from TF) for spectra, base=0x%02X, mask= 0x%02X\n", farmBase, farmMask); // AutoTriggerReadAndClear
			m_hexitec.startDataMoverStreamUDP(-1L, XDmaHexitec::MappedViewSpectra, m_sixteenBit, m_sumChips, 0, farmMask, farmBase, autoMode, XDmaHexitec::FarmIndexFromTF);
			farmBase += farmMask+1;
		}
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF)
		{
			mprintf(2, "... Setting up farm mode (port from TF) for mapped, base=0x%02X, mask= 0x%02X\n", farmBase, farmMask);
			m_hexitec.startDataMoverStreamUDP(-1L, XDmaHexitec::MappedViewMapped16, m_sixteenBit, m_sumChips, 1, farmMask, farmBase, autoMode, XDmaHexitec::FarmIndexFromTF);
			farmBase += farmMask+1;
		}
	}
	else if (m_readoutMode == AutoUDPThreadPerPacket || m_readoutMode == AutoUDPNoTrailer)
	{
		int farmBase=0;
		if (m_readoutMode == AutoUDPNoTrailer)
			m_hexitec.disableDataMoverUDPTrailer(true, 	m_packetShift);
		else
			m_hexitec.disableDataMoverUDPTrailer(false, 0);
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_ONLY)
		{
			mprintf(2, "... Setting up farm mode (distribute each packet) for spectra, base=0x%02X, mask= 0x%02X\n", farmBase, farmMask); 
			m_hexitec.startDataMoverStreamUDP(-1L, XDmaHexitec::MappedViewSpectra, m_sixteenBit, m_sumChips, 0, farmMask, farmBase, autoMode, XDmaHexitec::FarmIndexIncEOP);
			farmBase += farmMask+1;
		}
		if (m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF)
		{
			mprintf(2, "... Setting up farm mode (distribute each packet) for mapped, base=0x%02X, mask= 0x%02X\n", farmBase, farmMask);
			m_hexitec.startDataMoverStreamUDP(-1L, XDmaHexitec::MappedViewMapped16, m_sixteenBit, m_sumChips, 1, farmMask, farmBase, autoMode, XDmaHexitec::FarmIndexIncEOP);
			farmBase += farmMask+1;
		}
	}
	dataPath |= HEXITEC_DATA_PATH_ENB_FLUSH;
	m_hexitec.setGlobReg(HEXITEC_GLB_DATA_PATH, dataPath); // Next start will send flush tokens to trigger readout

	if (m_udpTxOnly==TxOnlyLoop)
	{
		if (options & TEST_HIT_OPT_UDP_FROM_HOST)
		{
			mprintf(2, ".. Starting UDP transmit real data run for ever\n", m_numFramesPerRaw*rawFramesPerTF*numTF);
			m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_RUN);	
			m_pb.sendFramesUDP(m_hexitec, 0x7FFFFFFFFFFFFFFFL, , 0, extTimeframe); // This will take a very very long time
			return;
		}
		else
		{
			int64_t curTF;
			m_hexitec.dmaBuildPBDesc(m_numFrames);
			mprintf(2, ".. Starting DMA for real run\n");
			m_hexitec.dmaStart(1<<HEXITEC_DMA_PB0 | 1<<HEXITEC_DMA_PB1, 0, 0, HEXITEC_DMA_START_CIRCULAR);
			m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_RUN);
			printf("Press return to stop sending of frames\n");
			getchar();
			m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  0);		// Turn off Run bit to force flush of hist caches.
			return;
		}
	}
	if (m_udpTxOnly==TxOnly1Pass)
	{
		if (options & TEST_HIT_OPT_UDP_FROM_HOST)
		{
			// FixMe: Needs work here
			mprintf(2, ".. Starting UDP transmit real data run for ever\n", m_numFramesPerRaw*rawFramesPerTF*numTF);
			m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_RUN);	
			m_pb.sendFramesUDP(m_hexitec, 0x7FFFFFFFFFFFFFFFL, 0, extTimeframe); // This will take a very very long time
			return;
		}
		else
		{
			int64_t curTF;
			m_hexitec.dmaBuildPBDesc(m_numFrames);
			mprintf(2, ".. Starting DMA for real run\n");
			m_hexitec.dmaStart(1<<HEXITEC_DMA_PB0 | 1<<HEXITEC_DMA_PB1, 0, 0, HEXITEC_DMA_START_CIRCULAR);
			m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_RUN);
			HexitecITfgStat iTfgStat;
			do
			{
				m_hexitec.iTfgReadStatus(iTfgStat);
				printf("ITFG Status=%u, ITFG TF=%6u\r",  iTfgStat.status, iTfgStat.timeFrame);
				fflush(stdout);
				this_thread::sleep_for (chrono::milliseconds(50));
			} while (!(iTfgStat.status & HEXITEC_ITFG_STAT_FINISHED));
			m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  0);		// Turn off Run bit to force flush of hist caches.
			this_thread::sleep_for(std::chrono::milliseconds(100));
			if (strlen(save_hdf_fname) > 0)
				m_hexitec.saveSpectraHdf5(save_hdf_fname, 0, -1, 0, numTF, numTF, true, m_mappedMode != HEXITEC_HIST_MAPPED_MODE_OFF, true);			
			return;
		}
	}
	if (options & TEST_HIT_OPT_UDP_FROM_HOST)
	{
		mprintf(2, ".. Starting UDP transmit real data run for %d frames\n", m_numFramesPerRaw*rawFramesPerTF*numTF);
		m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_RUN);	
		m_pb.sendFramesUDP(m_hexitec, (int64_t)m_numFramesPerRaw*rawFramesPerTF*numTF, 0, extTimeframe);
		startHist = std::chrono::steady_clock::now(); 
		this_thread::sleep_for (chrono::microseconds(1000));	
	}
	else
	{
		int64_t curTF;
		m_hexitec.dmaBuildPBDesc(m_numFrames);
		mprintf(2, ".. Starting DMA for real run\n");
		if (manual_test) { printf ("Press return to start DMA for real run\n"); getchar(); }
		m_hexitec.dmaStart(1<<HEXITEC_DMA_PB0 | 1<<HEXITEC_DMA_PB1, 0, 0, HEXITEC_DMA_START_CIRCULAR);
		m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_RUN);
		startHist = std::chrono::steady_clock::now(); 
		HexitecITfgStat stat;
		bool gotItfgTime=false;
		while ((curTF=checkProgress(numTF)) < numTF-1)
		{
			m_hexitec.iTfgReadStatus(stat);
			if (!gotItfgTime && (stat.status & HEXITEC_ITFG_STAT_FINISHED))
			{
				DataMoverContext dm;
				stopItfg = std::chrono::steady_clock::now();
				gotItfgTime = true;
				printf("At End of ITFG run note data moveer\n");
				m_hexitec.readDataMoverStream(&dm, 0);
				printDataMoverContext(dm);
//				exit(1);
			}
			m_itfgFrame = stat.timeFrame;	// Update here for when we are not using mmappedControl()
			updateProgress(m_numSpectraThreads+1, m_itfgFrame, checkProgress(m_numTF));
			printf("ITFG Status=%u, ITFG TF=%6u, DMA desc= %6d, HW frame: %6ld,  SW frame=%6ld.  Sent=%ld, received=%ld    \r",  stat.status, stat.timeFrame,
						m_hexitec.dmaReadCurrDescNum(HEXITEC_DMA_PB0), (int64_t)m_firmwareFrame, curTF, (int64_t)m_spectraSent, (int64_t)m_spectraReceived);
			fflush(stdout);
			this_thread::sleep_for (chrono::milliseconds(50));
			if (gotItfgTime)
			{
				now = std::chrono::steady_clock::now();
				itfgDur = now-stopItfg;
				if (itfgDur.count() > 30.0)
				{
					printf("\n .. Appears to have locked out\n");
					m_errors++;
					break;
				}
			}
		}
		if (!gotItfgTime)
		{
			stopItfg = std::chrono::steady_clock::now();
		}

		printf("\nFinished at CurTF=%ld\n", curTF);
		m_hexitec.iTfgReadStatus(stat);
		printf("After finish: ITFG Status=%u, ITFG TF=%6u, ITFG cycles=%u\n",  stat.status, stat.timeFrame, stat.cycles);
		now = std::chrono::steady_clock::now();
		elapsedDur = now-startHist;
		itfgDur = stopItfg-startHist;
		mprintf(1, "iTFG finished after %g s, finished run in %g s\n", itfgDur.count(), elapsedDur.count());
		mprintf(1, "wait clear start polls = %ld, wait clearpolls=%ld\n", m_hexitec.m_hbmHist.getClearStartPolls(), m_hexitec.m_hbmHist.getClearPolls());
	}
	printResults();
	m_hexitec.setGlobReg(HEXITEC_GLB_RUN_REG,  0);		// Turn off Run bit to force flush of hist caches.
	this_thread::sleep_for (chrono::milliseconds(100));
	stopReadoutThreads();

	if (options & (TEST_HIT_OPT_EVLIST_UDP | TEST_HIT_OPT_EVLIST_QDMA))
	{
		int timeout=0;
		int64_t framesStarted, framesFinished, prevStarted=-1, prevFinished=-1;
		do
		{
			framesStarted = m_eventListTester->getLatestFrameStarted();
			framesFinished = m_eventListTester->getLatestFrameFinished();
			if (framesStarted != prevStarted ||  framesFinished != prevFinished)
			{
				printf("Started=%ld, finished=%ld\r", framesStarted, framesFinished);
				prevStarted = framesStarted;
				prevFinished = framesFinished; 
			}
			if (framesFinished == (int64_t)m_numFramesPerRaw*(int64_t)rawFramesPerTF*(int64_t)numTF-1)
				break;
			this_thread::sleep_for (chrono::milliseconds(10));
		} while (++timeout < 5000);
		printf("\n");
		printf("Note : DATA_PATH=%08X\n", m_hexitec.getGlobReg(HEXITEC_GLB_DATA_PATH));
	}				
	if (options & (TEST_HIT_OPT_EVLIST_UDP | TEST_HIT_OPT_EVLIST_QDMA))
	{
		m_eventListTester->waitIdle();
		m_eventListTester->flushHist();
		endHistFlush = std::chrono::steady_clock::now(); 
		elapsedDur = endHistFlush-startHist;
		mprintf(2, "... Total software Histogram took %g seconds\n", elapsedDur.count());
	}
}
#endif

void CircularHdfWriter::mmappedControl(int tNum)
{	
	int64_t finishedFrame;
	int64_t prevFinished=-1;
	{
		lock_guard<mutex> lk(m_threadsStartedMutex);
		m_readoutStarted=true;
		m_threadsStartedCv.notify_all();
	}
	do
	{
		HexitecITfgStat itfgStat;
		uint64_t frameToken = m_hexitec.getFlushedFrame();
		m_hexitec.iTfgReadStatus(itfgStat);
		m_itfgFrame = itfgStat.timeFrame;
		if (frameToken & HEXITEC_FLUSHED_FRAME_VALID)
			finishedFrame = (int64_t)HEXITEC_FLUSHED_FRAME_GET(frameToken);
		else 
			finishedFrame= -1;
		if (finishedFrame == prevFinished)
			this_thread::sleep_for (chrono::milliseconds(1));	
		else
		{
			{
				lock_guard<mutex> lk(m_curFrameMutex);
				m_firmwareFrame = finishedFrame;
			}
			m_cv.notify_all();
			prevFinished = finishedFrame;
			updateProgress(m_numSpectraThreads, finishedFrame, finishedFrame);
		}
	} while (!m_stopReadout);
}

void CircularHdfWriter::irqControl(int tNum)
{	
	int64_t finishedFrame;
	int64_t prevFinished=-1;
	ssize_t rsize;
	int nFd;
	{
		lock_guard<mutex> lk(m_threadsStartedMutex);
		m_readoutStarted=true;
		m_threadsStartedCv.notify_all();
	}
	m_hexitec.setIrqEnable(1<<HEXITEC_IRQ_FLUSHED_FRAME);
	do
	{
		struct pollfd pollFd[2];
		
		pollFd[0].fd = m_hexitec.getEventFd(HEXITEC_IRQ_FLUSHED_FRAME);
		pollFd[0].events = POLLIN | POLLRDNORM ;
		pollFd[0].revents = 0;
		
		nFd = poll(pollFd, 1, 100);
		if (nFd < 0)
			printf("irqControl: poll returned nFd=%d, errno=%d\n", nFd, errno);
		if (pollFd[0].revents)
		{
			uint32_t sync;
			rsize= read(m_hexitec.getEventFd(HEXITEC_IRQ_FLUSHED_FRAME), (char *)&sync, 4);
			if (rsize != 4)
				printf("Error reading eventFd=%d, returned %d, errno=%d\n", m_hexitec.getEventFd(HEXITEC_IRQ_FLUSHED_FRAME), (int)rsize, errno);
			
			m_hexitec.clearIrqEnable(1<<HEXITEC_IRQ_FLUSHED_FRAME);
			m_hexitec.setIrqEnable(1<<HEXITEC_IRQ_FLUSHED_FRAME);
		}
		/* Even if this was a timeout rather than a IRQ evemt, check flushedToken just in case an event was lost, as they are flagged but not queued */
		HexitecITfgStat itfgStat;
		uint64_t frameToken = m_hexitec.getFlushedFrame();
		m_hexitec.iTfgReadStatus(itfgStat);
		m_itfgFrame = itfgStat.timeFrame;
		if (frameToken & HEXITEC_FLUSHED_FRAME_VALID)
			finishedFrame = (int64_t)HEXITEC_FLUSHED_FRAME_GET(frameToken);
		else 
			finishedFrame= -1;
		if (finishedFrame > prevFinished)
		{
			{
				lock_guard<mutex> lk(m_curFrameMutex);
				m_firmwareFrame = finishedFrame;
			}
			m_cv.notify_all();
			prevFinished = finishedFrame;
			updateProgress(m_numSpectraThreads, finishedFrame, finishedFrame);
		}
	} while (!m_stopReadout);
	m_hexitec.clearIrqEnable(1<<HEXITEC_IRQ_FLUSHED_FRAME);
}

void CircularHdfWriter::mmappedReadout(int tNum)
{
	int tfWrapped;
	int64_t curFrame;
	int myFrame = tNum/m_hexitec.getNumChips();
	int64_t firmwareFrame;

	if (m_verbose >= 3)
		printf("CircularHdfWriter::mmappedReadout: Starting thread num %d for time frames=%d, note m_firmwareFrame=%ld, finishedFrame=%ld\n", tNum, myFrame, (int64_t)m_firmwareFrame, (int64_t)m_spectraThread[tNum].m_finishedFrame);

	{
		lock_guard<mutex> lk(m_threadsStartedMutex);
		m_spectraThread[tNum].m_started=true;
		m_threadsStartedCv.notify_all();
	}

	do
	{
		unique_lock<mutex> lk(m_curFrameMutex);
		while (!m_spectraThread[tNum].m_stop && m_firmwareFrame < m_spectraThread[tNum].m_finishedFrame+m_numThreadsRequested)
			m_cv.wait(lk);
		firmwareFrame = m_firmwareFrame;
		lk.unlock();
		if (m_spectraThread[tNum].m_stop)
			return;
		if (m_verbose >= 4)
			printf("tnum%d : triggered at m_firmwareFrame=%ld\n", tNum, firmwareFrame);

		for (curFrame=(m_spectraThread[tNum].m_finishedFrame<0)?myFrame:m_spectraThread[tNum].m_finishedFrame+m_numThreadsRequested; curFrame <= firmwareFrame; curFrame+=m_numThreadsRequested)
		{
			lock_guard<mutex> bufLock(m_spectraThread[tNum].m_mutex);
			int chip = 0;
			bool engOnly = m_hexitec.getEngOnly(chip);
			int numEng = m_hexitec.getnBinsEng(chip);
			int numCC = m_hexitec.getnBinsClustClass(chip);
			if (m_verbose >= 4)
				printf("tnum%d : reading frame %ld\n", tNum, curFrame);
			if (m_enbMapped)
			{
				tfWrapped  = curFrame % (int64_t)m_hexitec.getNumTFMapped(chip);
				writeMapped(tNum, tNum, tfWrapped);
				for (chip=0; chip<m_hexitec.getNumChips(); chip++)
					m_hexitec.clearHistTimeframes(tfWrapped, 1, chip, true, false);
			}	
			if (m_enbSpectra)
			{
				tfWrapped  = curFrame % (int64_t)m_hexitec.getNumTF(chip);
				writeSpectra(tNum, tNum, tfWrapped);
				for (chip=0; chip<m_hexitec.getNumChips(); chip++)
					m_hexitec.clearHistTimeframes(tfWrapped, 1, chip, false, false);
			}	
			uint64_t frameToken = m_hexitec.getFlushedFrame();
			int64_t newFinished;
			if (frameToken & HEXITEC_FLUSHED_FRAME_VALID)
				newFinished = (int64_t)HEXITEC_FLUSHED_FRAME_GET(frameToken);
			else 
				newFinished= -1;
			if (m_enbMapped && newFinished-curFrame > m_hexitec.getNumTFMapped(0)-1 && m_mappedOverRuns++ < 2)
				printf("Overrun on mapped: finished writing %ld, firmware has finished %ld\n", curFrame, newFinished);

			if (m_enbSpectra && newFinished-curFrame > m_hexitec.getNumTF(0)-1 && m_spectraOverRuns++ < 2)
				printf("Overrun on spectra: finished writing %ld, firmware has finished %ld\n", curFrame, newFinished);
			updateProgress(tNum, firmwareFrame, curFrame);
			m_spectraThread[tNum].m_finishedFrame = curFrame;
		}			
	} while (1);
}
/*
void CircularHdfWriter::flushUdpBuffers(int tNum)
{
	int numRetries=100;
	do
	{
		int rc;
		char buffer[HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES];
		rc = recv(m_hexitec.getUdpTxTestSocket(tNum), buffer, HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES, MSG_DONTWAIT);
		if (rc <0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				numRetries--;
			else
				break;
		}
	} while (numRetries);
}	
void CircularHdfWriter::udpReadoutSpectra(int tNum)
{
	int tfWrapped;
	int64_t curFrame;
	int myFrame = tNum % m_numThreadsRequested;
	int64_t firmwareFrame;

		
	if (m_verbose >= 3)
		printf("CircularHdfWriter::udpReadoutSpectra: Starting thread num %d for time frames=%d\n", tNum, myFrame);
	flushUdpBuffers(tNum);
	{
		lock_guard<mutex> lk(m_threadsStartedMutex);
		m_spectraThread[tNum].m_started=true;
		m_threadsStartedCv.notify_all();
	}
	do
	{
		if (m_spectraThread[tNum].m_rBuf == nullptr)
			throw XDmaHexitecException ("CircularHdfWriter: udpReadoutSpectra tnum=%d, started with nullptr buffer %d", tNum);

		if (m_spectraThread[tNum].m_finishedFrame<0)
			curFrame = myFrame;
		else
			curFrame = m_spectraThread[tNum].m_finishedFrame+m_numThreadsRequested;
		
		{
			lock_guard<mutex> bufLock(m_spectraThread[tNum].m_mutex);
			int errors = 0;
			if ((errors=m_hexitec.udpTxTestReadFrame(tNum, curFrame, (char *)m_spectraThread[tNum].m_rBuf, m_rBufSize, tNum, !m_readoutEnabled)) < 0)
				return;
			m_errors += errors;
			int chip = 0;
			bool engOnly = m_hexitec.getEngOnly(chip);
			int numEng = m_hexitec.getnBinsEng(chip);
			int numCC = m_hexitec.getnBinsClustClass(chip);
			int numCCToCheck = numCC;
			tfWrapped  = curFrame % (int64_t)m_hexitec.getNumTF(chip);
			if (engOnly)
			{
				checkHistBuffEngOnlyCC(curFrame, tfWrapped, numEng, numCC, m_spectraThread[tNum].m_rBuf, m_spectraThread[tNum].m_chip, "UDP Circ engOnly spectra");
			}
			else	
			{
				checkHistBuffEngColRowCC(curFrame, tfWrapped, numEng, numCC, m_spectraThread[tNum].m_rBuf, false, m_spectraThread[tNum].m_chip, "UDP Circ spectra");
			}	
		}			
		updateProgress(tNum, firmwareFrame, curFrame);
		m_spectraThread[tNum].m_finishedFrame = curFrame;
	} while (1);
}
void CircularHdfWriter::udpReadoutMapped(int tNum)
{
	int tfWrapped;
	int64_t curFrame;
	int myFrame = tNum % m_numThreadsRequested;
	int64_t firmwareFrame;
		
	if (m_verbose >= 3)
		printf("CircularHdfWriter::udpReadoutMapped: Starting thread num %d for time frames=%d\n", tNum, myFrame);

	flushUdpBuffers(tNum);
	{
		lock_guard<mutex> lk(m_threadsStartedMutex);
		m_spectraThread[tNum].m_started=true;
		m_threadsStartedCv.notify_all();
	}
	do
	{
		if (m_spectraThread[tNum].m_mappedBuf == nullptr)
			throw XDmaHexitecException ("CircularHdfWriter: udpReadoutMapped tnum=%d, started with nullptr buffer", tNum);

		if (m_spectraThread[tNum].m_finishedFrame<0)
			curFrame = myFrame;
		else
			curFrame = m_spectraThread[tNum].m_finishedFrame+m_numThreadsRequested;
		
		{
			lock_guard<mutex> bufLock(m_spectraThread[tNum].m_mutex);
			int errors = 0;
			if ((errors=m_hexitec.udpTxTestReadFrame(tNum, curFrame, (char *)m_spectraThread[tNum].m_mappedBuf, 
											m_hexitec.getNumChips()*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*HEXITEC_NBINS_MAPPED*sizeof(uint32_t), tNum, !m_readoutEnabled)) < 0)
				return;
			m_errors += errors;
			int chip = m_spectraThread[tNum].m_chip;
			bool engOnly = m_hexitec.getEngOnly(chip);
			int numEng = m_hexitec.getnBinsEng(chip);
			int numCC = m_hexitec.getnBinsClustClass(chip);
			int numCCToCheck = numCC;
			tfWrapped  = curFrame % (int64_t)m_hexitec.getNumTFMapped(chip);
			checkHistBuffEngColRowCC(curFrame, tfWrapped, HEXITEC_NBINS_MAPPED, numCC, m_spectraThread[tNum].m_mappedBuf, true, m_spectraThread[tNum].m_chip, "UDP Circ mapped");
		}	
		updateProgress(tNum, firmwareFrame, curFrame);
		m_spectraThread[tNum].m_finishedFrame = curFrame;
	} while (1);
}
*/
/* These versions set the UDP core to send each packet to a different udp port and hence thread
	There are numThreadsRequested UDP RX threads and numThreadsRequested Checking threads for both spectra and mapped views.
	There is a ring buffer of software RX buffers using the m_rBuf and m_mappedBuf of the first numThreadsRequested m_spectraThread threads
	These are shared between all the threads.
*/
/*
void CircularHdfWriter::udpReadoutDistributed(int tNum)
{
	int tfWrapped;
	int64_t curFrame;
	int64_t firmwareFrame;
	bool doingMapped = 	m_mappedMode==HEXITEC_HIST_MAPPED_MODE_ONLY || (m_mappedMode==HEXITEC_HIST_MAPPED_MODE_INTL && tNum>=m_numThreadsRequested);
	char *buffer=nullptr;
	size_t bufSize;
	ptrdiff_t offset = 0;
	char sideBuffer[HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES];
	char *p;
	bool useSideBuffer;
	size_t payloadBytes = 0;
	int expPacket;
	uint64_t *tptr;
	ssize_t rc;
	int recvPacket;
	int64_t recvFrame;
	size_t frameSizeBytes=0;
	size_t elementSize = sizeof(uint32_t);
	int expNumPackets;
	
	if (m_verbose >= 3)
		printf("CircularHdfWriter::udpReadoutDistributed: Starting thread num %d, doingMapped=%d\n", tNum, doingMapped);
	flushUdpBuffers(tNum);
	{
		lock_guard<mutex> lk(m_threadsStartedMutex);
		m_spectraThread[tNum].m_started=true;
		m_threadsStartedCv.notify_all();
	}
	do
	{
		int chip = m_spectraThread[tNum].m_chip;
		bool engOnly = m_hexitec.getEngOnly(chip);
		int numEng = m_hexitec.getnBinsEng(chip);
		int numCC = m_hexitec.getnBinsClustClass(chip);
		int numCCToCheck = numCC;
		
		if (doingMapped)
		{
			frameSizeBytes = HEXITEC_NBINS_MAPPED*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS*m_hexitec.getNumChips()*elementSize;			
		}
		else
		{
			frameSizeBytes = numEng*numCC*m_hexitec.getNumChips()*elementSize;
			if (!engOnly)
				frameSizeBytes *= HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS;
		}
		
		if (m_spectraThread[tNum].m_packetNum == -1)
			expPacket = tNum % m_numThreadsRequested  + 1;
		else 
			expPacket = m_spectraThread[tNum].m_packetNum;

		// Cannot recv data straight into buffer as the trailer will overlap the next packet, but we don't know what order the packets will arrive.
		try 
		{
			rc = recv(m_hexitec.getUdpTxTestSocket(tNum), sideBuffer, HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES, 0);
		} 
		catch (abi::__forced_unwind&)
		{  // handle pthread_cancel stack unwinding exception
			throw;
		}
		if (m_spectraThread[tNum].m_finishedFrame<0)
			curFrame = 0;
		else
			curFrame = m_spectraThread[tNum].m_finishedFrame+1;

		if (rc < 0)
		{
			if (errno == EINTR)
				continue;
			else
				throw XDmaException("udpReadoutDistributed: recv returnd error, errno=%d", errno);
		}
		if (rc < HEXITEC_UDP_TRAILER_BYTES)
		{
			printf("CircularHdfWriter::udpReadoutDistributed:tNum=%d, doingMapped=%d Received only %zu bytes, which is less than the trailer ... WHY?\n", tNum, doingMapped, rc);
			continue;
		}
		payloadBytes = rc-HEXITEC_UDP_TRAILER_BYTES;
		tptr = (uint64_t *)(sideBuffer+rc-HEXITEC_UDP_TRAILER_BYTES);
		recvPacket = HEXITEC_UDP_TRAILER7_PACKET(tptr[7]);
		recvFrame  = HEXITEC_UDP_TRAILER7_FRAME(tptr[7]);
		if (!m_readoutEnabled)
		{
			printf("CircularHdfWriter::udpReadoutDistributed:tNum=%d, doingMapped=%d: discarding old data at Time frame  %ld at packet=%d\n", tNum, recvFrame, recvPacket);
		}
		else
		{
			if (curFrame != recvFrame || expPacket != recvPacket)
			{
				printf("CircularHdfWriter::udpReadoutDistributed:tNum=%d, doingMapped=%d Time frame or packet mismatch expecting TF=%ld, received TF=%ld, Expected packet=%d, received=%d, expNumPackets=%d\n", 
								tNum, doingMapped, curFrame, recvFrame, expPacket, recvPacket, expNumPackets);
				curFrame = recvFrame;
				m_spectraThread[tNum].m_finishedFrame = curFrame-1;
				expPacket = recvPacket;
			}
			int bufNum = curFrame % m_numThreadsRequested;
			if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL && tNum >= m_numThreadsRequested)
				bufNum += m_numThreadsRequested;
			buffer = (char*)(doingMapped?m_spectraThread[bufNum].m_mappedBuf:m_spectraThread[bufNum].m_rBuf);
			bufSize = doingMapped?(sizeof(uint32_t)*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS*HEXITEC_NBINS_MAPPED*m_hexitec.getNumChips()):m_rBufSize;
			
			offset = (expPacket-1)*payloadBytes;	// Assuming all packets are same length, which firmware does ensure and must continue to
			if (offset+payloadBytes > bufSize)
				printf("CircularHdfWriter::udpReadoutDistributed:tNum=%d, doingMapped=%d offset=%ld + payLoadBytes=%zd overrun buffer size %zd\n", 
								tNum, doingMapped, offset, payloadBytes, bufSize );
			else
				memcpy(buffer+offset, sideBuffer, payloadBytes);
			updateProgress(tNum, 0, expPacket);
			expPacket += m_numThreadsRequested;
			expNumPackets = frameSizeBytes/payloadBytes;
			if (expPacket > expNumPackets)
			{
				m_spectraThread[tNum].m_finishedFrame = curFrame;
				m_spectraThread[tNum].m_packetNum = -1;
				int j=0;
				if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL && tNum >= m_numThreadsRequested)
					j = m_numThreadsRequested;
				int64_t minFinished = curFrame;
				int nt = m_numThreadsRequested;
				if (m_mappedMode==HEXITEC_HIST_MAPPED_MODE_INTL)
					nt *= 2;		// If interleaved mapped mode, check we have both full and mapped spectra before starting checking (of both)
				for (int i=0; i<nt; i++, j++)
				{
					if (minFinished > m_spectraThread[j].m_finishedFrame)
						minFinished = m_spectraThread[j].m_finishedFrame;
				}
				if (minFinished > m_firmwareFrame)
				{
					if (m_verbose >= 4)
						printf("\nudpReadoutDistributed: tNum=%d: doingMapped=%d: Triggering check of time frame %ld\n", tNum, doingMapped, minFinished);
					{
						lock_guard<mutex> lk(m_curFrameMutex);
						m_firmwareFrame = minFinished;
					}
					m_cv.notify_all();
					
				}
			}
			else
				m_spectraThread[tNum].m_packetNum = expPacket;
		}
	} while (1);
}

void CircularHdfWriter::udpReadoutNoTrailer(int tNum)
{
	int tfWrapped;
	int64_t curFrame;
	int64_t firmwareFrame;
	bool doingMapped = 	m_mappedMode==HEXITEC_HIST_MAPPED_MODE_ONLY || (m_mappedMode==HEXITEC_HIST_MAPPED_MODE_INTL && tNum>=m_numThreadsRequested);
	char *buffer=nullptr;
	size_t bufSize;
	ptrdiff_t offset = 0;
//	char sideBuffer[HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES];
	char *p;
	bool useSideBuffer;
	size_t payloadBytes = HEXITEC_UDP_MAX_FRAME_BYTES;
	int curPacket;
	uint64_t *tptr;
	ssize_t rc;
	int recvPacket;
	int64_t recvFrame;
	size_t frameSizeBytes=0;
	size_t elementSize = sizeof(uint32_t);
	int expNumPackets;
	struct sockaddr_in src_addr;
	socklen_t addrlen;
	
	if (m_verbose >= 3)
		printf("CircularHdfWriter::udpReadoutNoTrailer: Starting thread num %d, doingMapped=%d\n", tNum, doingMapped);
	flushUdpBuffers(tNum);

	{
		lock_guard<mutex> lk(m_threadsStartedMutex);
		m_spectraThread[tNum].m_started=true;
		m_threadsStartedCv.notify_all();
	}
	do
	{
		int chip = m_spectraThread[tNum].m_chip;
		bool engOnly = m_hexitec.getEngOnly(chip);
		int numEng = m_hexitec.getnBinsEng(chip);
		int numCC = m_hexitec.getnBinsClustClass(chip);
		int numCCToCheck = numCC;
		
		if (doingMapped)
		{
			frameSizeBytes = HEXITEC_NBINS_MAPPED*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS*m_hexitec.getNumChips()*elementSize;			
		}
		else
		{
			frameSizeBytes = numEng*numCC*m_hexitec.getNumChips()*elementSize;
			if (!engOnly)
				frameSizeBytes *= HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS;
		}
		
		if (m_spectraThread[tNum].m_packetNum == -1)
			curPacket = tNum % m_numThreadsRequested  + 1;
		else 
			curPacket = m_spectraThread[tNum].m_packetNum;

		if (m_spectraThread[tNum].m_finishedFrame<0)
			curFrame = 0;
		else
			curFrame = m_spectraThread[tNum].m_finishedFrame+1;

		int bufNum = curFrame % m_numThreadsRequested;
		if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL && tNum >= m_numThreadsRequested)
			bufNum += m_numThreadsRequested;
		buffer = (char*)(doingMapped?m_spectraThread[bufNum].m_mappedBuf:m_spectraThread[bufNum].m_rBuf);
		bufSize = doingMapped?(sizeof(uint32_t)*HEXITEC_NUM_COLS*HEXITEC_NUM_ROWS*HEXITEC_NBINS_MAPPED*m_hexitec.getNumChips()):m_rBufSize;
		
		offset = (curPacket-1)*payloadBytes;	// Assuming all packets are same length, which firmware does ensure and must continue to
		if (offset+payloadBytes > bufSize)
		{
			printf("CircularHdfWriter::udpReadoutNoTrailer:tNum=%d, doingMapped=%d offset=%ld + payLoadBytes=%zd overrun buffer size %zd\n", 
								tNum, doingMapped, offset, payloadBytes, bufSize );
			offset = 0;
		}
		addrlen = sizeof(src_addr);
		// Cannot recv data straight into buffer as the trailer will overlap the next packet, but we don't know what order the packets will arrive.
		try 
		{
			rc = recvfrom(m_hexitec.getUdpTxTestSocket(tNum), buffer+offset, HEXITEC_UDP_TRAILER_BYTES+HEXITEC_UDP_MAX_FRAME_BYTES, 0, (struct sockaddr *)&src_addr, &addrlen);
		} 
		catch (abi::__forced_unwind&)
		{  // handle pthread_cancel stack unwinding exception
			throw;
		}
	
		if (rc < 0)
		{
			if (errno == EINTR)
				continue;
			else
				throw XDmaException("udpReadoutNoTrailer: recv returnd error, errno=%d", errno);
		}
		if (rc < HEXITEC_UDP_TRAILER_BYTES)
		{
			printf("CircularHdfWriter::udpReadoutNoTrailer:tNum=%d, doingMapped=%d Received only %zu bytes, which is less than the trailer ... WHY?\n", tNum, doingMapped, rc);
			continue;
		}
		uint16_t srcPort = ntohs(src_addr.sin_port);
		payloadBytes = rc;

		recvPacket = HEXITEC_UDP_SRC_PORT_GET_PACKET(srcPort);
		recvFrame = HEXITEC_UDP_SRC_PORT_GET_FRAME(srcPort);
		printf("CircularHdfWriter::udpReadoutNoTrailer:tNum=%d, doingMapped=%d: packetsize=%zd, src Port = 0x%04X. packet=%d, curPacket=%d, frame=%d, expFrame=%d\n", 
			tNum, doingMapped, rc, srcPort, recvPacket, curPacket, recvFrame, curFrame); 
		if (!m_readoutEnabled)
		{
			printf("CircularHdfWriter::udpReadoutNoTrailer:tNum=%d, doingMapped=%d: discarding old data at Time frame  %ld at packet=%d\n", tNum, recvFrame, recvPacket);
		}
		else
		{
			int expPacket = HEXITEC_UDP_SRC_PORT_EXP_PACKET(curPacket, m_packetShift);
			if ((curFrame & HEXITEC_UDP_SRC_PORT_FRAME_MASK) != recvFrame || expPacket != recvPacket)
			{
				printf("CircularHdfWriter::udpReadoutNoTrailer:tNum=%d, doingMapped=%d Time frame or packet mismatch expecting TF=%ld, received TF=%ld, curPacket=%d, expPacket=%d, received=%d, expNumPackets=%d,src_port=0x%04X=%d\n", 
								tNum, doingMapped, curFrame, recvFrame, curPacket, expPacket, recvPacket, expNumPackets, src_addr.sin_port, srcPort);
#if 0
				curFrame = recvFrame;
				m_spectraThread[tNum].m_finishedFrame = curFrame-1;
				curPacket = recvPacket;
				bufNum = curFrame % m_numThreadsRequested;
				if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL && tNum >= m_numThreadsRequested)
					bufNum += m_numThreadsRequested;
				char *newBuffer = (char*)(doingMapped?m_spectraThread[bufNum].m_mappedBuf:m_spectraThread[bufNum].m_rBuf);
			
				ptrdiff_t newOffset = (curPacket-1)*payloadBytes;	// Assuming all packets are same length, which firmware does ensure and must continue to
				if (newOffset+payloadBytes > bufSize)
					printf("CircularHdfWriter::udpReadoutNoTrailer:tNum=%d, doingMapped=%d offset=%ld + payLoadBytes=%zd overrun buffer size %zd\n", 
									tNum, doingMapped, offset, payloadBytes, bufSize );
				else
					memcpy(newBuffer+newOffset, buffer+offset, sideBuffer, payloadBytes);
#endif
			}

			updateProgress(tNum, 0, curPacket);
			curPacket += m_numThreadsRequested;
			expNumPackets = frameSizeBytes/payloadBytes;
			if (curPacket > expNumPackets)
			{
				m_spectraThread[tNum].m_finishedFrame = curFrame;
				m_spectraThread[tNum].m_packetNum = -1;
				int j=0;
				if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL && tNum >= m_numThreadsRequested)
					j = m_numThreadsRequested;
				int64_t minFinished = curFrame;
				int nt = m_numThreadsRequested;
				if (m_mappedMode==HEXITEC_HIST_MAPPED_MODE_INTL)
					nt *= 2;		// If interleaved mapped mode, check we have both full and mapped spectra before starting checking (of both)
				for (int i=0; i<nt; i++, j++)
				{
					if (minFinished > m_spectraThread[j].m_finishedFrame)
						minFinished = m_spectraThread[j].m_finishedFrame;
				}
				if (minFinished > m_firmwareFrame)
				{
					if (m_verbose >= 4)
						printf("\nudpReadoutNoTrailer: tNum=%d: doingMapped=%d: Triggering check of time frame %ld\n", tNum, doingMapped, minFinished);
					{
						lock_guard<mutex> lk(m_curFrameMutex);
						m_firmwareFrame = minFinished;
					}
					m_cv.notify_all();
					
				}
			}
			else
				m_spectraThread[tNum].m_packetNum = curPacket;
		}
	} while (1);
}
*/


int64_t CircularHdfWriter::checkProgress(int64_t maxTF)
{
	int i, j, chip;
	int64_t f, finishedFrame[HEXITEC_CIRC_WR_MAX_RX_THREADS];
	int fisrtPerFrameThreads = 0;

	if (m_numSpectraThreads == 1)
		return m_spectraThread[0].m_finishedFrame;
	
	switch (m_readoutMode)
	{
	case PolledMemMapped:
	case IrqMemMapped:
		for (i=0; i< m_numThreadsRequested; i++)
		{
			finishedFrame[i] = m_spectraThread[i].m_finishedFrame;
		}
		break;

#if 0		
	case AutoUDPThreadPerFrame:
	case AutoUDPThreadPerPacket:
	case AutoUDPNoTrailer:
		fisrtPerFrameThreads = 0; // thread block or 0 & 1 for AutoUDPThreadPerFrame
		if (m_readoutMode == AutoUDPThreadPerPacket || m_readoutMode == AutoUDPNoTrailer)
			fisrtPerFrameThreads = (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)?2:1; 

		for (i=0;i<m_numThreadsRequested; i++) // Copy frame numbers from first set of Checking threads
			finishedFrame[i] = m_spectraThread[i+fisrtPerFrameThreads*m_numThreadsRequested].m_finishedFrame;
		if (m_mappedMode == HEXITEC_HIST_MAPPED_MODE_INTL)
		{
			fisrtPerFrameThreads++;
			// Updated finishedFrame with values from mapped mode view in Mapped Interlaved mode
			for (i=0;i<m_numThreadsRequested; i++)
			{	
				if ((f=m_spectraThread[i+fisrtPerFrameThreads*m_numThreadsRequested].m_finishedFrame) < finishedFrame[i])
					finishedFrame[i] = m_spectraThread[i+fisrtPerFrameThreads*m_numThreadsRequested].m_finishedFrame;
			}
		}
		break;
#endif
	default : 
		printf("Uncoded readoutNum=%d @ %s:%d\n", m_readoutMode, __FILE__, __LINE__);
		exit(1);
	}	
	f=finishedFrame[0];
	for (j=1; j<m_numThreadsRequested && j<=maxTF; j++) // Exclude any unused threads for small number of time frames.
	{
		if (finishedFrame[j] < f)
			f = finishedFrame[j];
	}
	j = (int)((f+1) % m_numThreadsRequested);
	for(i=0; i<m_numThreadsRequested; i++)
	{
		if (finishedFrame[j] >= f+1)
			f++;
		else
			break;
		j = (j+1) % m_numThreadsRequested;
	}
	return f;
}

