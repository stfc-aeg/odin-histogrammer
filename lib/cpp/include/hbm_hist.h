/********************************************************************************************************************************************************
* 
* hbm_hist.h:	Defines for control of the AXI histogram block via its AXI Lite slave interface
*
*	Author:		W. Helsby
*	Date:	15/2/2021
*
********************************************************************************************************************************************************/

#ifndef HBM_HIST_H
#define HBM_HIST_H


#define AXI_HIST_IDENT_REG		0
#define AXI_HIST_CONFIG0		1
#define AXI_HIST_CONFIG1		2
#define AXI_HIST_CONFIG2		3
#define AXI_HIST_HBM_PORTS		4

#define AXI_HIST_CLEAR_BUSY		5
#define AXI_HIST_TEST_STATUS	6		//!< Test feature status

#define AXI_HIST_CLEAR_MASK		16
#define AXI_HIST_CLEAR_CONT		17
#define AXI_HIST_CLEAR_START	18
#define AXI_HIST_CLEAR_NUM		19
#define AXI_HIST_READ_MASK		20
#define AXI_HIST_READ_CONT		21
#define AXI_HIST_READ_START		22
#define AXI_HIST_READ_NUM		23
#define AXI_HIST_TESTPAT_MASK	24
#define AXI_HIST_TESTPAT_CONT	25
#define AXI_HIST_TESTPAT_LENGTH	26
#define AXI_HIST_TESTPAT_SIZE	27

struct AXIHistRegs
{
	uint32_t magic;		// Hist
	uint32_t conf0;
	uint32_t conf1;
	uint32_t conf2;
	uint32_t spare0;
	uint32_t clearBusy;
	uint32_t status;
	uint32_t spare1[16-7];
	uint32_t clearPortMask;
	uint32_t clearCont;
	uint32_t clearStartWord;
	uint32_t clearNumWords;
	uint32_t readPortMask;
	uint32_t readCont;
	uint32_t readStartWord;
	uint32_t readNumWords;
	uint32_t TPGPortMask;
	uint32_t TPGCont;
	uint32_t TPGNumEvents;
	uint32_t TPGSize;
};

#define AXI_HIST_IDENTIFIER	('H' | 'i'<<8 | 's' <<16 | 't' <<24)
#define AXI_HIST_CG0_NBITS_DATA(x)		((x)&0xF)
#define AXI_HIST_CG0_NBITS_DATA_AXI(x)	(((x)>>4)&0xF)
#define AXI_HIST_CG0_NUM_STREAMS(x)		(((x)>>8)&0xFF)
#define AXI_HIST_CG0_MAJOR(x)			(((x)>>24)&0xFF)
#define AXI_HIST_CG0_MINOR(x)			(((x)>>16)&0xFF)

#define AXI_HIST_CG1_NBITS_ADDR_TOP(x)	(((x)>>0)&0xF)
#define AXI_HIST_CG1_NBITS_STREAM(x)	(((x)>>4)&0xF)
#define AXI_HIST_CG1_GET_NBITS_BANKANDGROUP(x)	(((x)>>8)&0xF)
#define AXI_HIST_CG1_GET_NBITS_ADDR(x)			(((x)>>12)&0xFF)
#define AXI_HIST_CG1_GET_NBITS_ADDR_IN(x)		(((x)>>20)&0xFF)
//#define AXI_HIST_CG1_GET_BG0_AT_BOTTOM(x)		(((x)>>30)&1)
//#define AXI_HIST_CG1_GET_STREAM_AT_BOTTOM(x)	(((x)>>31)&1)
#define AXI_HIST_CG1_GET_BANK_POSN(x)	(((x)>>30)&3)

#define AXI_HIST_CG2_GET_NBITS_SUBSTREAM(x)		(((x)>>0)&0xF)
#define AXI_HIST_CG2_GET_TPG_PRESENT(x)			(((x)>>8)&0xF)
#define AXI_HIST_CG2_GET_CACHE(x)				(((x)>>16)&0xF)
#define AXI_HIST_CG2_GET_READWAITWRITE(x)		(((x)>>20)&0xF)
#define AXI_HIST_CG2_GET_BANK_START_BIT(x)		(((x)>>24)&0x3F)
#define AXI_HIST_CG2_GET_CLEAR_FIFO(x)			(((x)>>30)&1)

#define AXI_HIST_STATUS_READ_BUSY				(1<<0)	
#define AXI_HIST_STATUS_TPG_BUSY				(1<<1)	//!< Test pattern generator is running.
#define AXI_HIST_STATUS_CLEAR_BUSY				(1<<2)	//!< When clear FIFO is configures, show that FIFO is full so cannot write another clear. Without FIFO i show Any clear (including siblings) is busy

#define AXI_HIST_CC_CLEAR_ALL	(1<<0)			//!< Clear all streams
#define AXI_HIST_CC_USE_TPG		(1<<1)			//!< Use write/clear test pattern data instead of clearing to 0.
#define AXI_HIST_RC_DISCARD_RD	(1<<31)			//!< Discard Read data when reading.
#define AXI_HIST_RC_ACROSS_STREAMS	(1<<0)		//!< When configured for stream bits near bottom of address, allow burst access to read across streams. If StreamAtBottom==0 this is ignored.

#define AXI_HIST_TC_HIST_TPG(x) (((x)&3)<<0)	//!< Select Histogram event data test pattern generator source

#define AXI_HIST_CACHE_NONE					0	//!< No Cache or data sorter
#define AXI_HIST_CACHE_INC_CACHE_SINGLE		1	//!< Perform increments into same and neighbouring locations in BRAM then accumulate into DRAM. Suits small spectra
#define AXI_HIST_CACHE_INC_CACHE_MULTIPLE	2	//!< Perform increments into same and neighbouring locations in BRAM then accumulate into DRAM. Multiple table for marge spectra, not yet implemented
#define AXI_HIST_CACHE_SORTER				3	//!< Sort events to group events in same HBM page, to use with sub-streams master. Supports larger spectra
#define AXI_HIST_CACHE_SORTER_NEB			4	//!< Sort events to group events in same HBM page and coalesce neighouring hists, to use with sub-streams master. Supports small and larger spectra

#define AXI_HIST_READ_WAIT_WRITES_NONE		0	//!< No additional delays, start writes as soon as data available from read, start next read as soon as write tot the bank is done.
#define AXI_HIST_READ_WAIT_WRITES_SENT		1	//!< Start writes as soon as data available from read, delay next reads until all write have been sent. (wvalid=1, wready=1)
#define AXI_HIST_READ_WAIT_WRITES_DONE		2	//!< Start writes as soon as data available from read, delay next reads until all write have been finished. (bvalid=1, bready=1)
#define AXI_HIST_BUFFER_READ_WAIT_SENT		3	//!< Buffer read data to delay writes until all reads sent (arvalid=1, arready=1), delay next reads until all write have been finished. (bvalid=1, bready=1)
#define AXI_HIST_BUFFER_READ_WAIT_DONE		4	//!< Buffer read data to delay writes until all reads done (rvalid=1, rready=1), delay next reads until all write have been finished. (bvalid=1, bready=1)

#define AXI_HIST_TPG_NONE		0
#define AXI_HIST_TPG_RAMP		1
#define AXI_HIST_TPG_RAND		2
#define AXI_HIST_TPG_ROLLING	3

#define AXI_HIST_TPG_ROLL_SET_SIZE(x)	(((x)&3)<<4)		//!< Size 0..3 codes Energy part of Test pattern to be 64, 256, 1024 or 4096
#define AXI_HIST_TPG_ROLL_CALC_SIZE(x)	(1<<(((x)&3)*2+6))	//!< Size 0..3 codes Energy part of Test pattern to be 64, 256, 1024 or 4096
#define AXI_HIST_TPG_ROLL_SET_OCC(x)	((x)&7)				//!< Occupancy 0..6 1/255 ... 1/3 emulated Hixtec occupancy
#define AXI_HIST_TPG_ROLL_NPIXELS		256					//!< Number of pixels per test patter nstream.


#define AXI_HIST_TPG_HAS_RAMP		(1<<0)
#define AXI_HIST_TPG_HAS_RAND		(1<<1)
#define AXI_HIST_TPG_HAS_ROLLING	(1<<2)

#define AXI_HIST_TSTATUS_READ_BUSY 	(1<<0)
#define AXI_HIST_TSTATUS_TPG_BUSY 	(1<<1)

#define AXI_HIST_MAX_STREAMS 256
#define AXI_HIST_TEST_NUM_STREAMS 8

#define AXI_HIST_TEST_VALID (1<<31)
#define AXI_HIST_TEST_DELAY (1<<30)
#define AXI_HIST_TEST_FIRST (1<<29)
#define AXI_HIST_TEST_GET_OFFSET(x) ((x)&0x1FFFFFFF)

#define AXI_HIST_TP_MAX_PEAKS	4

class AXIHistTPSpectra
{
	public:
	int num_bins;				//!< Number of energy bins, typcially 4096 for Xspress3/4
	int num_peaks;				//!< Number of K-alpha peaks (k-beta infered at 1.1 x position 0.2 * height)
	struct 
	{
		int posn;				//!<	k-alpha peak position (bins)
		double height;
		int fwhm;				//!<	k-alpha peak FWHM (bins)
	} peak[AXI_HIST_TP_MAX_PEAKS];
	double pileup2;				//!<	number of counts in 2x pileup peak compared to k alpha (typically 0.2)
	double background;			//!<	Level of backgound compared to k alpha (typically 0.01)
	int average_events_per_tf;	//!< Average number of events in each time frame. Loops round when reaches end.
	AXIHistTPSpectra()
	{
		int i;
		num_bins = 4096;
		num_peaks = 1;
		peak[0].posn = 589;
		peak[0].height = 1.0;
		for (i=0; i<AXI_HIST_TP_MAX_PEAKS; i++)
			peak[i].fwhm = 20;

		pileup2 = 0.02;
		background = 0.001;
		average_events_per_tf = 1000;
	} ;
	void setupMn()
	{
		num_peaks= 1;
		peak[0].posn = 589;
		peak[0].height = 1.0;
		peak[0].fwhm = 20;
		pileup2 = 0.02;
	}
	void setupZr()
	{
		num_peaks= 1;
		peak[0].posn = 1570;
		peak[0].height = 1.0;
		peak[0].fwhm = 35;
		pileup2 = 0.02;
	}
	void setupMixed()
	{
		num_peaks= 4;
		peak[0].posn = 640; // Fe
		peak[0].height = 1.;
		peak[0].fwhm = 20;
		peak[1].posn = 805;	// Cu
		peak[1].height = 0.5;
		peak[0].fwhm = 25;
		peak[2].posn = 862;	// Zn
		peak[2].height = 0.5;
		peak[2].fwhm = 25;
		peak[3].posn = 1750;	// Mo
		peak[3].height = 2;
		peak[3].fwhm = 35;
		pileup2 = 0.005;
	}
};
#define AXI_TEST_IDENTIFIER	('T' | 'e'<<8 | 's' <<16 | 't' <<24)

class HbmException : public exception
{
	string m_errMsg;
public :
	HbmException(string s) : m_errMsg(s) {}
	// XDmaHbmException(const char *cp) : m_errMsg(cp) {}
	HbmException(const char * format, ...)
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

struct HBMHistConfig
{
	int NBitsDataHist;		//!<	Data width of each histogram element, 8, 16 or 32 
	int NBitsDataAXI;		//!<	Data width if AXI data bus.  To the firmware clears and reads are addressed in term of these words.
	int NumStreams;			//!< 	Number of parallel histogramming streams
	int NBitsAddrTopFixed;	//!< 	Number of fixed bits at top of address space.
	int NBitsStream;		//!<	Number of address bits (at top of used address space) used to identify the histogram stream
	int NBitsSubStream;		//!<	Number of bits of counter used to processes 2**n events per stream in an overall cycle. Addresses must not repeat within a cycle.
	int NBitsBankAndGroup;	//!<  	Total number of bank and bank group select bit at top of use address space. These bits usually come from the stream bits.
	int NBitsAddrMem;		//!<	Total Number of address bits to form the Byte address of the memory, excluding any top fixed bits.
	int	NBitsAddrIn;		//!<	Width of the input addresses used to specify the location to be histogrammed. Address in terms of NBitsDataHist words and excluding the stream select bits
	enum BankPosition_t { BankRowCol, BankRowColBG0, RowColBank, RowBankCol, SidRowBankColBG0} BankPosn;
	int BankStartBit;		//!<   Bit position in Byte address of the bank when using RowBankCol or SidRowBankColBG0
	int HistWordPerAXIWord;	//!< 	Number of histogram words in each AXI word (calculated from NBitsDataAXI/NBitsDataHist)
	int HistTPGPresent;		//!<	Flags to indicate which (if any) Histogram test pattern generators are present.
	int64_t TotalMemWords;	//!< 	Total number of words of size NBitsDataHist 
	int64_t HistWordsPerStream;	//!< Number of words of size NBitsDataHist per stream
	int64_t AXIWordsPerStream;	//!< Num of words of size NBitsDataAXI per stream.
	uint32_t HBMPortMask;		//!< Bit mask showing to which HBM port this master and it siblings are connected.
	int Major, Minor;		//!< AXI hist firmware version
	int Cache;				//!< Cache options configured into build
	int ReadWaitWrite;		//!< Read wait for Write and Write wait for Read options configured in firmware
	std::string CacheName;		//!< Cache type decoded to string
	std::string ReadWriteName;	//!< Read Wait Write type decoded to string
	bool hasClearFIFO;
	
	bool isSibling(const HBMHistConfig & rhs)
	{
		return NBitsDataHist == rhs.NBitsDataHist && NBitsDataAXI == rhs.NBitsDataAXI && NumStreams == rhs.NumStreams && NBitsAddrTopFixed == rhs.NBitsAddrTopFixed &&	
		       NBitsStream == rhs.NBitsStream && NBitsSubStream == rhs.NBitsSubStream && NBitsBankAndGroup == rhs.NBitsBankAndGroup && NBitsAddrMem == rhs.NBitsAddrMem &&
		       NBitsAddrIn == rhs.NBitsAddrIn && BankPosn == rhs.BankPosn && BankStartBit == rhs.BankStartBit && HistWordPerAXIWord == rhs.HistWordPerAXIWord &&
		       HistTPGPresent == rhs.HistTPGPresent && TotalMemWords == rhs.TotalMemWords && HistWordsPerStream == rhs.HistWordsPerStream && AXIWordsPerStream == rhs.AXIWordsPerStream;	
	}
	void initFromRegs(volatile uint32_t *p)
	{	
		uint32_t regs[5];
		int j;
		const char *cacheNames[] = {"None", "Inc Cache single", "Inc Cache multiple", "Sorter", "Sorter with neighbour coalesce"};
		const char *readWriteNames[] = { "None", "Reads wait for all writes sent", "Reads wait for all writes finished", "Buffer read data until all reads sent", "Buffer read data until all reads finished"};
		int bankPosn;
		for (j=0; j<5; j++)
			regs[j] = *p++;
	
		if (regs[0] != AXI_HIST_IDENTIFIER)
			throw HbmException("XDmaHbmHist: Error reading identifier register: read 0x%08X, expected 0x%08X\n", regs[0], AXI_HIST_IDENTIFIER);

		NBitsDataHist		= 1 << AXI_HIST_CG0_NBITS_DATA(regs[1]);
		NBitsDataAXI		= 1 << AXI_HIST_CG0_NBITS_DATA_AXI(regs[1]);
		NumStreams			= AXI_HIST_CG0_NUM_STREAMS(regs[1]);
		Major 				= AXI_HIST_CG0_MAJOR(regs[1]);
		Minor 				= AXI_HIST_CG0_MINOR(regs[1]);
		NBitsAddrTopFixed 	= AXI_HIST_CG1_NBITS_ADDR_TOP(regs[2]);
		NBitsStream			= AXI_HIST_CG1_NBITS_STREAM(regs[2]);
		NBitsBankAndGroup	= AXI_HIST_CG1_GET_NBITS_BANKANDGROUP(regs[2]);
		NBitsAddrMem		= AXI_HIST_CG1_GET_NBITS_ADDR(regs[2]);
		NBitsAddrIn			= AXI_HIST_CG1_GET_NBITS_ADDR_IN(regs[2]);
		BankStartBit		= AXI_HIST_CG2_GET_BANK_START_BIT(regs[3]);
		hasClearFIFO		= AXI_HIST_CG2_GET_CLEAR_FIFO(regs[3]);
		switch (AXI_HIST_CG1_GET_BANK_POSN(regs[2]))
		{
		case 0:
			if (BankStartBit > 0)
				BankPosn = RowBankCol;
			else
				BankPosn = BankRowCol;
			break;
		case 1:
			BankPosn = BankRowColBG0;
			break;
		case 2:
			BankPosn = RowColBank;
			break;
		case 3:
			BankPosn = SidRowBankColBG0;
		}
		HistWordPerAXIWord 	= NBitsDataAXI/NBitsDataHist;
		TotalMemWords 		= (((u_int64_t)1)<<NBitsAddrMem)/(NBitsDataHist/8);
		HistWordsPerStream 	= TotalMemWords/NumStreams;
		AXIWordsPerStream 	= HistWordsPerStream/HistWordPerAXIWord;
		NBitsSubStream		= AXI_HIST_CG2_GET_NBITS_SUBSTREAM(regs[3]);
		HistTPGPresent		= AXI_HIST_CG2_GET_TPG_PRESENT(regs[3]);
		Cache				= AXI_HIST_CG2_GET_CACHE(regs[3]);
		ReadWaitWrite		= AXI_HIST_CG2_GET_READWAITWRITE(regs[3]);
		HBMPortMask			= regs[4];
		if (Cache <= AXI_HIST_CACHE_SORTER_NEB)
			CacheName = cacheNames[Cache];
		else
			CacheName = "UNKNOWN";
		if (ReadWaitWrite <= AXI_HIST_BUFFER_READ_WAIT_DONE)
			ReadWriteName = readWriteNames[ReadWaitWrite];
		else
			ReadWriteName = "UNKNOWN";
	
		if (HBMPortMask == 0)
		{
			cout << "Cannot find any HBM ports connected" << endl;
		}
		else
		{

		}
	}
} ;

#endif



