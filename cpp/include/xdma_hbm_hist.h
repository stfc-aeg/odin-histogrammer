
#ifndef _XDMA_HBM_HIST_H
#define _XDMA_HBM_HIST_H 1
#include "xdma.h"
#define XDmaHbmHistMaxChannels	4
#include <exception>
#include <memory>
#include <mutex>
#include <atomic>
#include <stdint.h>
#include <stdarg.h>
#include "hbm_hist.h"

//#define HBM_HIST_MEM_BYPASS_SIZE (0x100000000L)
//#define HBM_HIST_USER_SIZE (0x4000000L)
#define HBM_NUM_PORTS (16)
#define HBM_TOTAL_PORTS (32)
#define HBM_MAX_CONT_REGS		16
#define HBM_HIST_REGS_BASE 	 (0x60000)
#define HBM_HIST_REGS_INC 	 (0x1000)
#define HBM_HIST_TESTER_BASE (0x70000)

struct HistTesterRegs
{
	uint32_t histEnable;
	uint32_t control;
	uint32_t spare_rw0[6];
	uint32_t magic;
	uint32_t status;
	uint32_t histBusy;
	uint32_t testPatBusy;
	uint64_t histTimer[32];
};
#define HBM_HT_CONT_USE_LIST 	1
#define HBM_HT_CONT_DIS_UPPER	2
#define HBM_HT_STAT_LIST_BUSY	1

#define HBM_RW_DMA_RESET		1
#define HBM_RW_NO_REORDER		2
#define HBM_REORDER_OFFSET_STREAM		(1L<<40)

using namespace std;

class XDmaHbmException : public exception
{
	string m_errMsg;
public :
	XDmaHbmException(string s) : m_errMsg(s) {}
	// XDmaHbmException(const char *cp) : m_errMsg(cp) {}
	XDmaHbmException(const char * format, ...)
	{
		va_list args, args_copy;
		va_start(args, format);
		va_copy(args_copy, args);

		int len = vsnprintf(nullptr, 0, format, args);
		if (len <= 0)
		{
			m_errMsg = "vsnprintf failed";
		}
		else
		{
			m_errMsg.resize(len);
			vsnprintf(&m_errMsg[0], len+1, format, args_copy); // or m_errMsg.data() in C++17 and later...
		}
		va_end(args_copy);
		va_end(args);
	}

	virtual const char * what() const throw()
	{
		return m_errMsg.c_str(); 
	}
};


class XDmaHbmHist {
	XDma *m_xdma;
	int m_physicalPort[HBM_TOTAL_PORTS];
	std::unique_ptr<mutex> m_clearMutex;
//	std::atomic<int64_t> m_clearStartPolls{0};
//	std::atomic<int64_t> m_clearPolls{0};
	int64_t m_clearStartPolls{0};
	int64_t m_clearPolls{0};
public :
	XDmaHbmHist () { m_xdma= nullptr;}
	XDmaHbmHist (XDma *xdma, int numCont);
	void resetClearPolls() { m_clearStartPolls = 0; m_clearPolls = 0;}
	int64_t getClearStartPolls() { return m_clearStartPolls; }
	int64_t getClearPolls() { return m_clearPolls; }
//	~XDmaHbmHist();  // Nothin to do in the destructor, but definign it provents building of an implict mover operator, which is needed with the unique_ptr
	void startClearStream(uint32_t portMask, uint32_t stream, uint32_t startWord, uint32_t numWords, bool waitFinished=false);
	void startClearAll(uint32_t portMask, uint32_t startWord, uint32_t numWords, bool waitFinished=false);
	void startClear(uint32_t portMask, uint32_t startWord, uint32_t num_words, int allStreams, bool waitFinished=false);
	void startClearTPG(uint32_t portMask, uint32_t startWord, uint32_t num_words, int allStreams, bool waitFinished=false);
	void readStreamDma(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data, int dmaChan = 0, int flags=0);
	void writeStreamDma(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data, int dmaChan = 0, int flags=0);
	void readStreamCpu(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data);
	void writeStreamCpu(int port, int stream, uint32_t offset, uint32_t size, uint32_t *data);
	void startAXISReadStream(uint32_t portMask, uint32_t stream, uint32_t startWord, uint32_t numWords);
	void startAXISReadSeq(uint32_t portMask, uint32_t startWord, uint32_t numWords);
	void readDma(char *buffer, uint64_t base, uint64_t numBytes, int dmaChan=0 );
	void writeDma(char *buffer, uint64_t base, uint64_t numBytes, int dmaChan=0 );

	void waitClear();
	void waitClearStart();
	void startTPG(uint32_t portMask, int tpgType, uint32_t numEvents, uint32_t tpgSize);
	void setTPGPortMask(uint32_t value);
	void setTPGCont(uint32_t value);
	int getNumContRegs() { return m_numContRegs; }
	int getPhysicalPort(int logPort);
	
	HBMHistConfig m_histConf = {0};
	int m_numContRegs;
	int m_axiReorder = 0;
	volatile struct AXIHistRegs *m_histRegs[HBM_MAX_CONT_REGS];
	volatile struct HistTesterRegs *m_histTester;
	uint32_t m_portMask[HBM_MAX_CONT_REGS];
	uint32_t *m_histMemBase=nullptr;
	int m_hasMemBAR;
	};

#endif
