
#ifndef _XDMA_H
#define _XDMA_H 1
#define XDmaHbmHistMaxChannels	4
#define XDMA_NUM_EVENT_IRQS		16
#include <exception>
#include <stdint.h>
#include <stdarg.h>

#define HEXITEC_MEM_BYPASS_SIZE (0x100000000L)
// #define HEXITEC_USER_SIZE (0x20000000L)

using namespace std;

class XDmaException : public exception
{
	string m_errMsg;
public :
	XDmaException(string s) : m_errMsg(s) {}
	// XDmaHbmException(const char *cp) : m_errMsg(cp) {}
	XDmaException(const char * format, ...)
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

class XDmaMMap {
		int m_fd;
		size_t m_len;
	public :
		string m_devName;
		uint32_t *m_base = (uint32_t *)(-1L);
		XDmaMMap();
		XDmaMMap(const char *deviceName, size_t size);
		~XDmaMMap();
		XDmaMMap & operator= (XDmaMMap && src); 
};
struct DataMoverContext 
{
	uint32_t raw[8];		//!< Raw context copied from BRAM
	uint64_t readCredit;	//!< Unused readCredit issued by QDMA
	uint32_t sixteenBitMode;	//!< 16 bit mode specified by calling SW
	uint32_t mappeView;			//!< Specify whether to read main spectra, first 8 bins of mapped spectra or all 16 bins of mapped spectra when using mapped mode.
	uint32_t sumChips;			//!< Specify whetehr to sum the EngOnly spectra over all 12 chips in Hexitec 6x2
	uint32_t tfMode;			//!< Specify where time frame is supplied from (user SW or firmware in future)
	uint32_t farmIndexMode;		//!< 
	uint32_t farmBase;			//!< First farm index to use (actually Ored with incrementing index)
	uint32_t farmMask;			//!< Mask to select which (usually bottom few) bits of incrementing index are used in output farm idnex.
	uint32_t timeFrame;			//!< Timeframe specified by software or determined by firmware.
	uint32_t run;				//!< Run mode to enable activity. Set after all other bits are specified.
	uint32_t pixelColEng;		//!< Index incrementing through energy bin and pixel column
	uint32_t pixelRow;			//!< Index of pixel row within chip. 0..79
	uint32_t chipCol;			//!< Index of chip column 0..5
	uint32_t chipRow;			//!< Index of chip row 0..1
	uint32_t packetIndex;		//!< Pack index supplied to the QDMA.
};


class XDma {
protected:
	int m_num_h2c=0;
	int m_h2c_fd[XDmaHbmHistMaxChannels];
	int m_h2c_alignment[XDmaHbmHistMaxChannels];
	int m_num_c2h=0;
	int m_c2h_fd[XDmaHbmHistMaxChannels];
	int m_c2h_alignment[XDmaHbmHistMaxChannels];
	int m_busNum=-1;
	int m_devNum=-1;
	int m_funcNum=-1;
	bool m_isQdma = false;
	int m_eventFd[XDMA_NUM_EVENT_IRQS];
public :
	string m_devName;
	XDma();
	XDma (int, size_t userSize);
	virtual ~XDma();
	virtual XDma & operator= (XDma && src);
	void readDma(char *buffer, uint64_t base, uint64_t numBytes,  int dmaChan=0 );
	void writeDma(char *buffer, uint64_t base, uint64_t numBytes, int dmaChan=0 );
	bool isQdma() { return m_isQdma; }
	virtual int getStQid() { return -1; }
	virtual void readStream(char *buffer, uint64_t numBytes, int dmaChan=1);
	virtual bool supportsStream() { return false; }
	virtual void createStreamQueues() { throw XDmaException("createStreamQueues: Not supported on XDMA version"); }
	virtual void createEvListQueues(int num) { throw XDmaException("createEvListQueues: Not supported on XDMA version"); }
	virtual int getEvListFd(int qid) {  throw XDmaException("getEvListFd: Not supported on XDMA version"); } 
	XDmaMMap m_regsBAR;
	XDmaMMap m_memBAR;
	uint32_t *m_histMemBase;
	int m_hasMemBAR;
	int getDevNum() { return m_devNum;}
	virtual int getEventFd(int irqNum);
private :
	void readAlignedDma(char *buffer, uint64_t base, uint64_t numBytes, int dmaChan=0 );
	void writeAlignedDma(char *buffer, uint64_t base, uint64_t numBytes,  int dmaChan=0 );
	};

#endif
