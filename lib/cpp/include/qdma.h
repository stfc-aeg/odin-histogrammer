
#ifndef _QDMA_H
#define _QDMA_H 1
#include <exception>
#include <stdint.h>
#include <stdarg.h>
#include "xdma.h"

#define HEXITEC_MEM_BYPASS_SIZE (0x100000000L)
//#define HEXITEC_USER_SIZE (0x20000000L)
/**
@defgroup HEXITEC_DATA_MOVER_DEFS		Macros to define DATA mover control table entries.
@ingroup HEXITEC_DEFS
@{
*/
#define HEXITEC_DATA_MOVER_BASE 		(0x30000)
#define HEXITEC_DATA_MOVER_CONTROL		0	
#define HEXITEC_DATA_MOVER_OVERRUN_CONT	1
#define HEXITEC_DATA_MOVER_REVISION		(0x100)
#define HEXITEC_DATA_MOVER_OVERRUN_STAT	(0x101)

#define HEXITEC_DMC_ENABLE				1	
#define HEXITEC_DMC_DISABLE_TRAILER		(1<<1)	
#define HEXITEC_DMC_STOP_REQUEST		(1<<2)			//!< Request that all Data mover queue stop (politely)	
#define HEXITEC_DMC_SET_PACKET_SHIFT(x)	(((x)&7)<<8)	//!< Set right shift of PacketIndex into tuser and hence src port in trailer-less UDP mode/
#define HEXITEC_DMC_RESET				(1<<30)			//!< Reset Data mover cmds FSM, AXI_data_mover and clear FIFOs
#define HEXITEC_DMC_DM_HALT_REQ			(1<<31)			//!< Request polite stop of all ax_data_mover.

#define HEXITEC_DM_ORC_CLEAR				1			//!< Write 1 to HEXITEC_DATA_MOVER_OVERRUN_CONT to clear any over run flags.

#define HEXITEC_DM_STAT_RUNNING				1			//!< The datamover FSM is not in the Idle state. 
#define HEXITEC_DM_STAT_OVERRUN_SPECTRA		2			//!< Detected overrun when using autonomous read and clear for spectra from HEXITEC_DATA_MOVER_OVERRUN_STAT
#define HEXITEC_DM_STAT_OVERRUN_MAPPED		4			//!< Detected overrun when using autonomous read and clear for spectra from HEXITEC_DATA_MOVER_OVERRUN_STAT

#define HEXITEC_DM_STAT_DM_ERROR			(1<<30)		//!< axi_data_mover detected and error
#define HEXITEC_DM_STAT_DM_HALT_COMPLETE	(1<<31)		//!< axi_data_mover Halt request complete.


#define HEXITEC_DM_CONTEXT_OFFSET		(0x8000)
#define HEXITEC_DM_NUM_QUEUES			256

#define HEXITEC_DM0_16BIT				(1<<0)
#define HEXITEC_DM0_MV_FULL				(0<<1)			//!< Read normal spectra.
#define HEXITEC_DM0_MV_MAPPED8			(2<<1)			//!< Read mapped spectra, first 8 mapped bins
#define HEXITEC_DM0_MV_MAPPED16			(3<<1)			//!< Read mapped spectra, all 16 mapped bins
#define HEXITEC_DM0_GET_MV(x)			(((x)>>1)&3)

#define HEXITEC_DM0_SUM_CHIPS			(1<<3)

#define HEXITEC_DM0_AUTO_TF_CLEAR		(1<<4)				//!< Also clear time frame in autonomously triggered mode.
#define HEXITEC_DM0_AUTO_STOP			(1<<5)				//!< Request that Datamover stops at the end of the next frame. 
#define HEXITEC_DM0_AUTO_TF				(1<<7)				//!< Use TimeframeToken  output from histogrammer to trigger autonomous output of data.
#define HEXITEC_DM0_GET_TF_MODE(x)		(((x)>>4)&15)

#define HEXITEC_DM0_FARM_INC_EOF		0					//!< Increment farm index for this entry at the end of each finished frame.
#define HEXITEC_DM0_FARM_INC_EOP		(1<<12)				//!< Increment farm index at the end of every packet and reset at the end of frame
#define HEXITEC_DM0_FARM_FROM_TF		(2<<12)				//!< Farm index is set from the the LSbits of the time frame.
#define HEXITEC_DM0_GET_FARM_INDEX(x)	(((x)>>12)&3)		//!< Get farm index control mode 
#define HEXITEC_DM0_EVLIST				(1<<14)				//!< input data from Event list stream and send to QDMA.
#define HEXITEC_DM0_UDP_MODE			(1<<15)				//!< Send data from this queue to the 100 G UDP port instead of the QDMA.
#define HEXITEC_DM0_FARM_MASK(x)		(((x)&0xFF)<<16)	//!< Bit mask, 0, 1, 3, 7, 15...255 to pass lower bits of farm index to UDP core farm LUT addr
#define HEXITEC_DM0_FARM_BASE(x)		(((x)&0xFF)<<24)	//!< Base address within Farm mode LUT, ored with mask farm index.
#define HEXITEC_DM0_GET_FARM_MASK(x)	(((x)>>16)&0xFF)
#define HEXITEC_DM0_GET_FARM_BASE(x)	(((x)>>24)&0xFF)

#define HEXITEC_DM2BYTE0_RESERVED		1					//!< Don't touch this byte.
#define HEXITEC_DM2BYTE1_TF0(x)			((x)&0xFF)			//!< Bottom 8 bits time frame written here using char * operation.
#define HEXITEC_DM2BYTE2_TF1(x)			(((x)>>8)&0xFF)		//!< Bits 15:8 of time frame written here using char * operation.
#define HEXITEC_DM2BYTE3_TF2(x)			(((x)>>16)&0xFF)	//!< Bits 23:16 of time frame written here using char * operation.
#define HEXITEC_DM3_TF3(x)				(((x)>>24)&0xFFF)	//!< Bits 35:24  of time frame written here using uintg32_t * operation.
#define HEXITEC_DM2_3_GET_TF(r2,r3)		((((r2)>>8)&0xFFFFFF)|(((r3)&0xFFF)<<24))

#define HEXITEC_DM3_RUN					(1<<31)		//!< Run bit enables activity on this Queue
#define HEXITEC_MAX_EVLIST_Q			128			// Plan to use 128 to up to 255 for event list queues.
#define HEXITEC_EVLIST_Q_BASE			128			// Plan to use 128 to up to 255 for event list queues.

#define HEXITEC_UDP_SRC_PORT_GET_FRAME(x)	((x)&15)			//!< Bottom 4 bits of src port are bottom 4 bits of time frame
#define HEXITEC_UDP_SRC_PORT_GET_PACKET(x)	(((x)>>4)&0x7FF)	//!< Next 11 bits of src port are bottom 11 bits of packet number (shifted see HEXITEC_DMC_SET_PACKET_SHIFT).
#define HEXITEC_UDP_SRC_PORT_FRAME_MASK		0xF				
#define HEXITEC_UDP_SRC_PORT_EXP_PACKET(p,s)	(((p)>>((s)&7))&0x7FF)	//!< Extract expected packet from full packet

/** 
@}
*/

using namespace std;


class QDma : public XDma
{
	int m_mmQid=-1;			// QDMA QID to be used for memory mapped IO using read/write
	int m_stQid=-1;			// QDMA QID to be used for stream IO using read.
	int m_numEvListQid=0;
	int m_evListFd[HEXITEC_MAX_EVLIST_Q];
	
public :
	string m_devName;
	QDma();
	QDma (int, int, int, size_t userSize);
	virtual ~QDma();
	virtual QDma & operator= (QDma && src);
	virtual int getStQid() { return m_stQid; }
	virtual void readStream(char *buffer, uint64_t numBytes, int dmaChan=1);
	virtual bool supportsStream() { return true; }
	virtual void createStreamQueues();
	virtual void createEvListQueues(int num);

	virtual int getEvListFd(int qid) 
	{
		if (qid <0 || qid >= HEXITEC_MAX_EVLIST_Q)
			throw XDmaException("getEvListFd: qid %D out of range 0...%d", qid, HEXITEC_MAX_EVLIST_Q-1); 
		return m_evListFd[qid];
	}
};

#endif
