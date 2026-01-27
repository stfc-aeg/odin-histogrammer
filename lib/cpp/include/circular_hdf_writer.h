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
#include "datamod.h"

#include "xdma_hbm_hist.h"
#include "xdma_hexitec.h"

#include <cxxabi.h>

#define HEXITEC_CIRC_WR_MAX_RANK 6
#define HEXITEC_CIRC_WR_MAX_RX_THREADS 64

/* Playback testing towards circular buffer mode.
   The playback memory will be loaded with a pattern using most of the memory.
   This is split into an integer number of output frames andthe DMA set to look over multiple passes of memory.
   The ITFG is used to accumulate 1 one more of these frames into each output frame, so that the expected for  given output frame can be summed from 1 or more of the expected frames.

	Consider a fsatest frame rate of 10 ms, 10,000 detector frames.
	Number of pb frames=613800, so number of raw output frames in pb memory = 61 ... Prime, so will beat with powers of 10 or 2 etc.
	
*/

enum CircWriterReadoutMode { Unknown, PolledMemMapped, IrqMemMapped, AutoUDPThreadPerFrame, AutoUDPThreadPerPacket, AutoUDPNoTrailer};
enum CircWriterUdpTxOnlyMode {TxNormal, TxOnlyLoop, TxOnly1Pass};

class CircularHdfWriter
{
public:
	CircularHdfWriter(XDmaHexitec & hexitec, const char *fName, int numEng, bool enbSpectra, bool enbMapped, bool sumChips);
	~CircularHdfWriter();
	void setupReadoutMode(CircWriterReadoutMode readoutMode, int numSpectraThreadsReq, CircWriterUdpTxOnlyMode txOnly=TxNormal);
	inline void setIpAddr(uint32_t accelIpAddr, uint32_t serverIpAddr) {m_accelIpAddr = accelIpAddr; m_serverIpAddr = serverIpAddr; }
	void start();
	void stop();
	int64_t checkProgress(int64_t maxTF);
	int64_t getMappedOverRuns() { return m_mappedOverRuns; }
	int64_t getSpectraOverRuns() { return m_spectraOverRuns; }
private:
	void createCircularProgress();
	void createFiles(int firstIndex);
	void writeMapped(int tNum, int bufNum, int tfWrapped);
	void writeSpectra(int tNum, int bufNum, int tfWrapped);
	void resetReadoutThreads();
	void updateProgress(int tNum, int64_t firmwareFrame, int64_t finishedFrame);
	void mmappedControl(int tNum);
	void irqControl(int tNum);
	void mmappedReadout(int tNum);

/*	void flushUdpBuffers(int tNum);
	void udpReadoutSpectra(int tNum);
	void udpReadoutMapped(int tNum);
	void udpReadoutDistributed(int tNum);
	void udpReadoutNoTrailer(int tNum);
	void udpCheckDistributed(int tNum);
*/
	void waitThreadsStarted();
	void stopReadoutThreads();
	char m_fName[FILENAME_MAX+2];
	XDmaHexitec & m_hexitec;
	int m_numEng;
	int m_histFormat=-1;
	int m_mappedMode=-1;
	mutex m_controlMutex;
	int m_verbose=1;
	int m_progressNumFrames;
//	int m_numFrames=0;
	std::atomic<int>  m_rawFramesPerTF{1};
	bool m_sixteenBit = false;
	bool m_sumChips = false;
	uint32_t m_accelIpAddr=0;
	uint32_t m_serverIpAddr=0;		// Forces use of default Ip addr.
	int m_numSpectraThreads=0;
	int m_numThreadsRequested=0;
	bool m_enbSpectra;
	bool m_enbMapped;
	thread m_readoutThread;
	bool m_readoutThreadValid=false;
	bool m_readoutUseCancel=false;
	std::atomic<bool> m_stopReadout{false};
	std::mutex m_threadsStartedMutex;
	std::condition_variable m_threadsStartedCv;
	bool m_readoutStarted=false;
	mutex m_curFrameMutex;
	std::atomic<int64_t>  m_itfgFrame{0}, m_firmwareFrame{-1};
	std::condition_variable m_cv;
	std::atomic<int64_t> m_errors{0};
	std::atomic<int64_t> m_mappedOverRuns{0}, m_spectraOverRuns{0};
	std::atomic<bool> m_readoutEnabled{false};
	int m_packetShift=0;
	CircWriterUdpTxOnlyMode m_udpTxOnly=TxNormal;
	const char *m_readoutName[6]= { "Unknown", "PolledMemMapped", "IRQMemMapped", "AutoUDPThreadPerFrame", "AutoUDPThreadPerPacket", "AutoUDPNoTrailer" };
	int m_readNumRows;
	size_t m_maxReadBytes=1024*1024;	// Limit read size to help parallelise reading and writing within 1 thread.
	struct SpectraThread
	{
		uint32_t *m_rBuf=nullptr;
		uint32_t *m_mappedBuf=nullptr;
		std::thread m_thread;
		std::mutex m_mutex;
		std::atomic<int64_t> m_finishedFrame;
		bool m_stop=false;
		bool m_spectraUseCancel=false;
		std::atomic<int> m_packetNum{1};
		bool m_started=false;
		H5::H5File m_h5File;
		uint64_t padding[2];
		hsize_t m_dimsFileSpectra[HEXITEC_CIRC_WR_MAX_RANK];
		hsize_t m_dimsFileMapped[HEXITEC_CIRC_WR_MAX_RANK];
		int m_rankFile;
		H5::DataSet m_dataSetSpectra;
		H5::DataSet m_dataSetMapped;
		hsize_t m_count[HEXITEC_CIRC_WR_MAX_RANK];
	} m_spectraThread[HEXITEC_CIRC_WR_MAX_RX_THREADS];
	size_t m_rBufSize = 0;
	CircWriterReadoutMode m_readoutMode = Unknown;
	std::string m_description;
	MOD_IMAGE3D *m_progMod=nullptr;	
};
