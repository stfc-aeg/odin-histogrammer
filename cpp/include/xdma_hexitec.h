
#ifndef _XDMA_HEXITEC_H
#define _XDMA_HEXITEC_H 1
#ifdef __cplusplus
#include <exception>
#include <stdint.h>
#include <stdarg.h>
#include <cstring>
#include <thread>

#include "xdma.h"
#include "qdma.h"
#include "xdma_hbm_hist.h"
#include "udp_core.h"
#include "H5Cpp.h"
#endif

#define HEXITEC_ETH_CORE_BASE	(0x100000)
#define HEXITEC_ETH_CORE_STRIDE	(0x10000)
#define HEXITEC_UDP_CORE_BASE	(0x200000)
#define HEXITEC_UDP_CORE_STRIDE	 (0x40000)
#define HEXITEC_MAX_NUM_UDP_CORES	3			//!< Current max is 3, 2 for 10 G RX, + 1 x 100 G TX
#define HEXITEC_REGS_BASE 	 (0x10000000)		//!< Byte offset to start of Hexitec register and lookup tables.
#define HEXITEC_REGS_BASE_WITH_CHIP_SEL 	 (0x1000000)		//!< Byte offset to start of Hexitec register and lookup tables.
#define HEXITEC_USER_SIZE 					(0x20000000L)
#define HEXITEC_USER_SIZE_WITH_CHIP_SEL 	(0x02000000L)

#define HEXITEC_IRQ_SUPPORT					1
#define HEXITEC_NUM_IRQS					5
#define HEXITEC_MAX_PROC_COL				20				//!< Number of parallel processing column used in HexitecMHz
#define HEXITEC_GLB_OFFSET 	 				(0xF800000)		//!< Byte offset from start of hexitec register to global registers, Chip 31.
#define HEXITEC_GLB_OFFSET_WITH_CHIP_SEL 	(0x0800000)		//!< Byte offset from start of hexitec register to global registers, Chip 1.

#define HEXITEC_GLB_RD_REVISION				0x100	//!< Word offset of revision register from HEXITEC_GLB_OFFSET
#define HEXITEC_GLB_RD_FEATURES				0x101	//!< Word offset of start of features registers.

#define HEXITEC_GLB_LOADING_BL				0x10C	//!< Bitwise mask of chip=0..NumChips-1 of Loading Baseline status bit
#define HEXITEC_GLB_SAVING_BL				0x10E	//!< Bitwise mask of chip=0..NumChips-1 of Saving Baseline status bit
#define HEXITEC_GLB_USING_REF_BRAM			0x110	//!< Bitwise mask of chip=0..NumChips-1 of Using Baseline reference BRAM as baseline update is "lost"
#define HEXITEC_GLB_USING_ABS_TRIG			0x112	//!< Bitwise mask of chip=0..NumChips-1 of Using absolute trigger as baseline update is "lost"

#define HEXITEC_GLB_SCOPE_STATUS			0x114	//!< Status (over run) of scope mode FIFOs/DMAs and Burst running 
#define HEXITEC_GLB_RD_ITFG_STATUS			0x116	//!< Word offset of Integrated time frame generator status register
#define HEXITEC_GLB_RD_ITFG_INP_FRAME		0x117	//!< Monitor Count DOWN of specified number of detector frames to accumulate into current output frame
#define HEXITEC_GLB_RD_ITFG_TIME_FRAME		0x118	//!< Monitor Count UP of current output time frame.
#define HEXITEC_GLB_RD_ITFG_CYCLES			0x119	//!< Monitor Count UP of current output time frame.

#define HEXITEC_GLB_REORDER_STATUS			0x11A	//!< Hexitec 6x2: Reorder block status
#define HEXITEC_GLB_FIRST_FRAME				0x11C	//!< Hexitec 6x2: First readout frame to be received in this run
#define HEXITEC_GLB_REORDER_TLAST_ERR_NUM	0x11E	//!< Hexitec 6x2: Reorder block status Tlast error frame number
#define HEXITEC_GLB_REORDER_FRAME_ERR_NUM	0x120	//!< Hexitec 6x2: Reorder block status Framing error frame number

#define HEXITEC_GLB_UDP_STATUS				0x122	//!< Hexitec MHz and 6x2: UDP RX Status register, OverRun. FramingError, FrameNumErr,and Packet Error
#define HEXITEC_GLB_UDP_ERROR_FRAME0		0x124	//!< Hexitec MHz and 6x2: UDP RX Status register, Frame number of first error frame for UDP RX 0
#define HEXITEC_GLB_UDP_ERROR_FRAME0_H		0x125	//!< Hexitec MHz and 6x2: UDP RX Status register, MSBits of Frame number of first error frame
#define HEXITEC_GLB_UDP_ERROR_FRAME1		0x126	//!< Hexitec 6x2: UDP RX Status register, Frame number of first error frame for UDP RX 0
#define HEXITEC_GLB_UDP_ERROR_FRAME1_H		0x127	//!< Hexitec 6x2: UDP RX Status register, MSBits of Frame number of first error frame
#define HEXITEC_GLB_UDP_ERROR_PACKET0		0x128	//!< Hexitec MHz and 6x2: UDP RX Status register, First Error packet number, Bits 31..0, UDP RX 0
#define HEXITEC_GLB_UDP_ERROR_PACKET0_H		0x129	//!< Hexitec MHz and 6x2: UDP RX Status register, Error packet number, Ms bit (unused)
#define HEXITEC_GLB_UDP_ERROR_PACKET1		0x12A	//!< Hexitec 6x2: UDP RX Status register, First Error packet number, Bits 31..0, UDP RX 0
#define HEXITEC_GLB_UDP_ERROR_PACKET1_H		0x12B	//!< Hexitec 6x2: UDP RX Status register, Error packet number, Ms bit (unused)

#define HEXITEC_GLB_FLUSHED_FRAME			0x12C	//!< Hexitec MHZ (and future Hexitec 6x2)

#define HEXITEC_GLB_FRAME_COUNT0			0x130	//!< Hexitec MHz Frame Count (Chip 0)
#define HEXITEC_GLB_RAW_HIT_COUNT0			0x131	//!< Hexitec MHz Raw Hit Count  (Chip 0)
#define HEXITEC_GLB_FRAME_COUNT1			0x132	//!< Hexitec 6x2 (future) Frame Count (Chip 1)
#define HEXITEC_GLB_RAW_HIT_COUNT1			0x133	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 1)
#define HEXITEC_GLB_FRAME_COUNT2			0x134	//!< Hexitec 6x2 (future) Frame Count (Chip 2)
#define HEXITEC_GLB_RAW_HIT_COUNT2			0x135	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 2)
#define HEXITEC_GLB_FRAME_COUNT3			0x136	//!< Hexitec 6x2 (future) Frame Count (Chip 3)
#define HEXITEC_GLB_RAW_HIT_COUNT3			0x137	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 3)
#define HEXITEC_GLB_FRAME_COUNT4			0x138	//!< Hexitec 6x2 (future) Frame Count (Chip 4)
#define HEXITEC_GLB_RAW_HIT_COUNT4			0x139	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 4)
#define HEXITEC_GLB_FRAME_COUNT5			0x13A	//!< Hexitec 6x2 (future) Frame Count (Chip 5)
#define HEXITEC_GLB_RAW_HIT_COUNT5			0x13B	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 5)
#define HEXITEC_GLB_FRAME_COUNT6			0x13C	//!< Hexitec 6x2 (future) Frame Count (Chip 6)
#define HEXITEC_GLB_RAW_HIT_COUNT6			0x13D	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 6)
#define HEXITEC_GLB_FRAME_COUNT7			0x13E	//!< Hexitec 6x2 (future) Frame Count (Chip 7)
#define HEXITEC_GLB_RAW_HIT_COUNT7			0x13F	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 7)
#define HEXITEC_GLB_FRAME_COUNT8			0x140	//!< Hexitec 6x2 (future) Frame Count (Chip 8)
#define HEXITEC_GLB_RAW_HIT_COUNT8			0x142	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 8)
#define HEXITEC_GLB_FRAME_COUNT9			0x142	//!< Hexitec 6x2 (future) Frame Count (Chip 9)
#define HEXITEC_GLB_RAW_HIT_COUNT9			0x143	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 9)
#define HEXITEC_GLB_FRAME_COUNT10			0x144	//!< Hexitec 6x2 (future) Frame Count (Chip 10)
#define HEXITEC_GLB_RAW_HIT_COUNT10			0x145	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 10)
#define HEXITEC_GLB_FRAME_COUNT11			0x146	//!< Hexitec 6x2 (future) Frame Count (Chip 11)
#define HEXITEC_GLB_RAW_HIT_COUNT11			0x147	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 11)
#define HEXITEC_GLB_FRAME_COUNT12			0x148	//!< Hexitec 6x2 (future) Frame Count (Chip 12)
#define HEXITEC_GLB_RAW_HIT_COUNT12			0x149	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 12)
#define HEXITEC_GLB_FRAME_COUNT13			0x14A	//!< Hexitec 6x2 (future) Frame Count (Chip 13)
#define HEXITEC_GLB_RAW_HIT_COUNT13			0x14B	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 13)
#define HEXITEC_GLB_FRAME_COUNT14			0x14C	//!< Hexitec 6x2 (future) Frame Count (Chip 14)
#define HEXITEC_GLB_RAW_HIT_COUNT14			0x14D	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 14)
#define HEXITEC_GLB_FRAME_COUNT15			0x14E	//!< Hexitec 6x2 (future) Frame Count (Chip 15)
#define HEXITEC_GLB_RAW_HIT_COUNT15			0x14F	//!< Hexitec 6x2 (future) Raw Hit Count (Chip 15)

#define HEXITEC_GLB_INP_TIME_FRAME0			0x150	//!< Hexitec MHz: Input time frame for chip 0 (only chip)

#define HEXITEC_GLB_TX_USR_CLK0_FREQ		0x170	//!< Hexitec 6x2: Clock frequency measure 0 = tx_usr_clk0
#define HEXITEC_GLB_TX_USR_CLK1_FREQ		0x171	//!< Hexitec 6x2: Clock frequency measure 1 
#define HEXITEC_GLB_RX_USR_CLK0_FREQ		0x172	//!< Hexitec 6x2: Clock frequency measure 2
#define HEXITEC_GLB_RX_USR_CLK1_FREQ		0x173	//!< Hexitec 6x2: Clock frequency measure 3 
#define HEXITEC_GLB_REF_CLK_FREQ			0x174	//!< Hexitec 6x2: Clock frequency measure 4

#define HEXITEC_GLB_RD_FEATURES				0x101	//!< Word offset of start of features registers.

#define HEXITEC_GLB_DATA_PATH				0
#define HEXITEC_GLB_RUN_REG					1
#define HEXITEC_GLB_SCOPE_NUM_WORDS			2
#define HEXITEC_GLB_SCOPE_GLOB_SRC			3
#define HEXITEC_GLB_SCOPE_CHIP_SEL			4
#define HEXITEC_GLB_FRAME_BURST_LENGTH		5		//!< Frame busrt length register used to process a limited number of frames, generally to monitor baseline tracking.
#define HEXITEC_GLB_ITFG_CONTROL			8		//!< Integrated time frame generator control register.
#define HEXITEC_GLB_ITFG_INP_PER_TF			9		//!< Integrated time frame input detector frames per output time frame.
#define HEXITEC_GLB_ITFG_NUM_TF				10		//!< Integrated time frame generator, number of output time frames
#define HEXITEC_GLB_ITFG_NUM_CYCLES			12		//!< Integrated time frame generator, number of time to cycle over all output time frames
#define HEXITEC_NUM_TFG_REGS				5
#define HEXITEC_GLB_SOFTWARE_FLUSH			14		//!< Performing a 64 bit write of the 35 bit time frame triggers a flush. To be used for cyclic experiments without HEXITEC_DATA_PATH_ENB_FLUSH set. 
#define HEXITEC_GLB_IRQ_ENB_RW				16		//!< Read/write access to the IRQ enable register
#define HEXITEC_GLB_IRQ_ENB_SET				17		//!< Write 1 to set access to the IRQ enable register
#define HEXITEC_GLB_IRQ_ENB_CLR				18		//!< Write 1 to clear access to the IRQ enable register

#define HEXITEC_NUM_GLB_REGS				8
#define HEXITEC_NUM_ROWS					80
#define HEXITEC_NUM_COLS					80
#define HEXITEC_NUM_STREAMS_BLTR			40		//!< Number of parallel processing streams in Baseline, Linearity, Trigger section
#define HEXITEC_NUM_STREAMS_CSHARE			20		//!< Number of parallel processing streams in Charge sharing and format section
#define HEXITEC_MHZ_NUM_CHUNKS_PER_ROW		2
#define HEXITEC_MHZ_SCOPE_WORDS_PER_CHUNK	2		// Scope DMA is 256 bits wide, so takes 2 off 128bit works  per output word.
#define HEXITEC_NUM_AXI_DMA					6		//!< Number of AXI DMA IP block to control.
#define HEXITEC_NUM_PB_DMA					2		//!< Number of AXI DMAs used to make playback data.
#define HEXITEC_NUM_SCOPE_DMA				4		//!< Maximum number of AXI DMAs used to record scope mode data.
#define HEXITEC_DMA_PB0					0
#define HEXITEC_DMA_PB1					1
#define HEXITEC_DMA_SC0					2
#define HEXITEC_DMA_SC1					3
#define HEXITEC_DMA_SC2					4
#define HEXITEC_DMA_SC3					5
#define HEXITEC_DMA_PB0_BASE			0x80000			//!< Base address of DMA engine.
#define HEXITEC_DMA_PB1_BASE			0x81000			//!< Base address of DMA engine.

#define HEXITEC_DMA_SC1_BASE			0x82000			//!< Base address of DMA engine.
#define HEXITEC_DMA_SC3_BASE			0x83000			//!< Base address of DMA engine.

#define HEXITEC_DMA_MAX_BYTES_DESC		0x3FFFFC0		//!< Maximum Bytes in any one DMA descriptor.

#define HEXITEC_NBINS_ENG_MAX			4096			//!< Number of bins of energy at highest number of bins, used to address energy MAP LUT.

#define HEXITEC_MAX_CHIPS				16				//!< For future expansion

#define HEXITEC_DMA_START_CIRCULAR		1

#define HEXITEC_CHIP_SEL_ADDR			0
#define HEXITEC_CHIP_BASESUB			1
#define HEXITEC_CHIP_CLUSTER			2
#define HEXITEC_CHIP_CORR_A				3
#define HEXITEC_CHIP_ENB_CLUSTER		4
#define HEXITEC_CHIP_FORMAT				5
#define HEXITEC_CHIP_CLUSTER_GRADE		6				//!< Mapping from Cluster class to Cluster grade.

#define HEXITEC_CHIP_NUM_PB				7				//!< Store currently loaded number of playback frames so software can restart and determine current state, not actually needed by firmware

#define HEXITEC_NUM_CHIP_REGS			8

#define HEXITEC_REGION_REGS				0
#define HEXITEC_REGION_ABS_THRES		1
#define HEXITEC_REGION_BASELINE			2
#define HEXITEC_REGION_MTHRES			3
#define HEXITEC_REGION_LTHRES			4
#define HEXITEC_REGION_LIN_A			5
#define HEXITEC_REGION_LIN_B			6
#define HEXITEC_REGION_LIN_C			7
#define HEXITEC_REGION_EDGE_POS_RECIP	8
#define HEXITEC_REGION_EDGE_POS_M		9
#define HEXITEC_REGION_EDGE_POS_C		10
#define HEXITEC_REGION_NEG_NEB_RECIP	11
#define HEXITEC_REGION_NEG_NEB_M		12
#define HEXITEC_REGION_NEG_NEB_C		13
#define HEXITEC_REGION_ENG_MAP			14
#define HEXITEC_REGION_PIX_MASK			15
#define HEXITEC_REGION_L_POS_RECIP		16
#define HEXITEC_REGION_L_POS_M	 		17
#define HEXITEC_REGION_L_POS_C	 		18
#define HEXITEC_REGION_ENABLED_FRAMES	30
#define HEXITEC_REGION_FIFO_COUNTS		31

#define HEXITEC_RECIP_SIZE				1024
#define HEXITEC_EDGE_POS_SIZE			1024
#define HEXITEC_L_POS_SIZE				1024
#define HEXITEC_NEG_NEB_SIZE			2048
#define HEXITEC_ENG_MAP_SIZE			4096
#define HEXITEC_PIX_MASK_SIZE			(80*4)
#define HEXITEC_MAX_REGION_SIZE			2048

#define HEXITEC_EDGE_POS_SCALE_M		(1<<17)
#define HEXITEC_EDGE_POS_SCALE_C		(1<<0)
#define HEXITEC_NEG_NEB_SCALE_M			(1<<17)
#define HEXITEC_NEG_NEB_SCALE_C			(1<<0)
#define HEXITEC_L_POS_SCALE_M			(1<<17)
#define HEXITEC_L_POS_SCALE_C			(1<<0)

#define HEXITEC_NBINS_MAPPED			16
#define HEXITEC_NBINS_MAPPED_HALF		8
#define HEXITEC_NUM_TF_MAPPED_INTL		2048		//!< Number of time frames of mapped energy with interleaved map and full spectra. Use {@link getNumTFMapped()} to allow for alternate memory sizes
#define HEXITEC_NUM_TF_MAPPED_ONLY		8192		//!< Number of time frames of mapped energy when using mapped energy only. Use {@link getNumTFMapped()} to allow for alternate memory sizes

#define HEXITEC_NUM_REGIONS				32
#define HEXITEC_NUM_REGIONS_LIN			3		//!< Number of linearity correction regions.
#define HEXITEC_REGION_OFFSET			0x10000 	//!< WORD offset between starts of regions	

#define HEXITEC_SEL_ADDR_BROADCAST		63			//!< Write to same BRAM in call processing streams (so all columns) at the same time.
#define HEXITEC_ENABLED_FRAMES_SIZE		4096		//!< Size of ring buffer used to store enabled Frames in timeframe counter
#define HEXITEC_ENB_FRMS0_GET_FRAME(x)	((x)&0x7FFFFFFFFFL)	//!< Get the number of enabled frames (into a uint64_t) from the retuned inp frames uint64_t
#define HEXITEC_ENB_FRMS0_GET_EXT_TRIG_LCH(x)	(((x)>>56)&0xF)	//!< Get the latched version of the ext trigger signal number from the retuned inp frames uint64_t. This is probably the easiest to use.
#define HEXITEC_ENB_FRMS0_GET_EXT_TRIG(x)		(((x)>>60)&0xF)	//!< Get the most recent version of the ext trigger signal number from teh last detector frame of the time frame from the retuned inp frames uint64_t
#define HEXITEC_ENB_FRMS1_GET_RAW_HITS(x)	((x)&0xFFFFFFFFFFFFFL)	//!< Get the number of raw hits (into a uint64_t) from the returned inp frames uint64_t

#define HEXITEC_SCOPE_STAT_RUNNING		(1<<31)		//!< Sysytem is running and has not reached the end of a short busrt.
/*

#define HEXITEC_
*/
#define HEXITEC_MHZ_PB_BYTES_PER_HALF_LINE	32			//!< Each 1/2 of PB DMA transfer 256 bits, 32 bytes per beat
#define HEXITEC_MHZ_PB_BYTES_PER_BEAT		64			//!< Each pair of 1/2 line PB DMA beats transfer a total of 512 bits, 64 bytes per beat
#define HEXITEC_MHZ_PB_HEADER_BEATS			1			//!< Number of 256 bit data beat of header in playback frame.
#define HEXITEC_MHZ_PB_BYTES_HALF_FRAME		((HEXITEC_NUM_ROWS*HEXITEC_MHZ_NUM_CHUNKS_PER_ROW+HEXITEC_MHZ_PB_HEADER_BEATS)*HEXITEC_MHZ_PB_BYTES_PER_HALF_LINE)			//!< Number of bytes in each playback (1/2) frame


#define HEXITEC_PB_SOACQ				(1L<<63)
#define HEXITEC_PB_EOACQ				(1L<<62)
#define HEXITEC_PB_EXT_TRIG				(1L<<61)
#define HEXITEC_PB_FRAME_NUM(x)			((x)&0x1FFFFFFFFFFFFFFFL)


#define HEXITEC_UDP_SOF					(1L<<63)
#define HEXITEC_UDP_EOF					(1L<<62)
#define HEXITEC_UDP_SOP					(1L<<61)
#define HEXITEC_UDP_EOP					(1L<<60)

#define HEXITEC_UDP_COUNT_ENG			(1L<<58)
#define HEXITEC_UDP_EOACQ				(1L<<57)
#define HEXITEC_UDP_SOACQ				(1L<<56)
#define HEXITEC_UDP_EXTTRIG(val)		((((uint64_t)(val))&0xF)<<48)


#define HEXITEC_MHZ_UDP_SOACQ			(1L<<63)
#define HEXITEC_MHZ_UDP_EOACQ			(1L<<62)
#define HEXITEC_MHZ_UDP_SOF				(1L<<61)
#define HEXITEC_MHZ_UDP_EOF				(1L<<60)
#define HEXITEC_MHZ_UDP_SOP				(0L<<61)		// Note 0 unused at moment.
#define HEXITEC_MHZ_UDP_EOP				(0L<<60)		// Note 0 unused at moment.

#define HEXITEC_MHZ_UDP0_DET_FRAME(x)		((x)&0xFFFFFFFFFFFFL)		//!< Get detector frame number from UDP header 64 bit word 0
#define HEXITEC_MHZ_UDP01_TIMEFRAME(d0,d1)	(((d0)>>48)&0xFFFFL |((d1)<<16)&0x7FFFF0000L)		//!< Get Alpha Data Time frame from UDP header 64 bit words 0 and 1

#define HEXITEC_MHZ_UDP7_COUNT_DIS		(1L<<56)
#define HEXITEC_MHZ_UDP7_EXTTRIG(val)	((((uint64_t)(val))&0xF)<<48)

#define HEXITEC_SC_BYTES_PER_QUARTER_LINE	16		//!< Each 1/4 of Scope DMA transfers 128 bits, 16 bytes per beat
#define HEXITEC_SC_BYTES_PER_BEAT		64			//!< Each quad of 1/4 line Scope DMA beats transfer a total of 512 bits, 64 bytes per beat
#define HEXITEC_SC_HEADER_BEATS			1			//!< Number of 128 bit data beats of header in scopemode frame.
#define HEXITEC_SC_BYTES_QUARTER_FRAME		((HEXITEC_NUM_ROWS*HEXITEC_MHZ_NUM_CHUNKS_PER_ROW+HEXITEC_SC_HEADER_BEATS)*HEXITEC_SC_BYTES_PER_QUARTER_LINE)	//!< Number of bytes in each scopemode (1/4) frame
#define HEXITEC_MHZ_SCOPE_FRAME_SIZE_BYTES ((HEXITEC_NUM_ROWS*HEXITEC_MHZ_NUM_CHUNKS_PER_ROW+HEXITEC_SC_HEADER_BEATS)*HEXITEC_SC_BYTES_PER_BEAT)

#define HEXITEC_DMA_STATE_DESC_CONF		(1<<0)
#define HEXITEC_DMA_STATE_BUFFER_CONF	(1<<1)
#define HEXITEC_DMA_STATE_DESC_BUILT	(1<<2)
#define HEXITEC_DMA_STATE_DESC_DEBUG	(1<<3)


#define HEXITEC_DATA_PATH_PLAYBACK			(1<<0)		//!< Select playback data into the processing block, otherwise take data from UDP core RX 
#define HEXITEC_DATA_PATH_DISABLE_HIST		(1<<1)		//!< Disable histogramming into HBM to allow saving of list (scope) data into HBM.
#define HEXITEC_DATA_PATH_TRANSMIT_UDP		(1<<2)		//!< Enable sending copy of data sent to processing block to UDP output.
#define HEXITEC_DATA_PATH_TRANSMIT_PB		(1<<3)		//!< Enable sending playback data to UDP TX, UDP RX can then be sent to processing block (or scope mode)
#define HEXITEC_DATA_PATH_SCOPEMODE			(1<<4)		//!< Enable saving of list data into HBM. Currently not connected, for future expansion?
#define HEXITEC_DATA_PATH_EVLIST_ENB		(1<<5)		//!< Enable event list data output, (which can coexist with histogramming)
#define HEXITEC_DATA_PATH_EVLIST_UDP		(1<<6)		//!< Direct event list output to 100G UDP port rather than QDMA queues.
#define HEXITEC_DATA_PATH_EVLIST_FIXED4K	(1<<7)		//!< Direct event list output to 100G UDP port rather than QDMA queues.
#define HEXITEC_DATA_PATH_100G_LOOPBACK(x)	(((x)&7)<<8) //!< Enable loopback of 100G UDP data in the MGTs
#define HEXITEC_DATA_PATH_EVLIST_FARM_MASK(x)	(((x)&0xFF)<<16)	//!< Mask to select how many (lowest) bits of frame number are used to select UDP farm lut entry or QDMA Qid
#define HEXITEC_DATA_PATH_EVLIST_FARM_BASE(x)	(((x)&0xFF)<<24)	//!< Base offset to add to selected bit of frame number to make address for UDP farm mode or QDMA Qid
#define HEXITEC_DATA_PATH_EVLIST_DETAILED		(1<<11)	//!< Send for the full 128 bits of full cluster information rather than just the calculated position and energy.
#define HEXITEC_DATA_PATH_ENB_FLUSH				(1<<12)	//!< Enable firmware initiate histogram cache flush at end of frame. Use for circular buffer mode. Do not use if cycling over frames multiple times.
#define HEXITEC_DATA_PATH_ENB_FLUSH_ALL			(1<<13)	//!< Enable firmware initiate histogram cache flush at end of frame. Use for circular buffer mode. Do not use if cycling over frames multiple times.

#define HEXITEC_DATA_PATH_SHORT_BURST_MODE		(1<<14)	//!< Enable (short) burst mode where only the number of frames specified by HEXITEC_GLB_FRAME_BURST_LENGTH are processed
#define HEXITEC_DATA_PATH_REPLICATE_UDP			(1<<15)	//!< Replicate Raw UDP data from the head onto the UDP output port.
#define HEXITEC_RUN_RUN						(1<<0)		//!< System Run bit, asserted to start system with normal (UDP) or placback data
#define HEXITEC_RUN_DIS_RESET_FRAME_COUNT	(1<<1)		//!< Disabel reset of fraem count at start of run, particularly for use with short bursts when debugging baseline settling

#define HEXITEC_SCOPE_GLOB_SRC_SET(x)		((x)*0xF)	//!< Set global data source control in scope mode data source register

#define HEXITEC_SCOPE_GLOB_SRC_INPUT			0		//!< Scope capture frame data from input multiplexer, either UDP or from playback
#define HEXITEC_SCOPE_GLOB_SRC_TPG				1		//!< Scope capture data from test pattern generator (counters) in scope mode block
#define HEXITEC_SCOPE_GLOB_SRC_WAIT_COUNT_ENB	(1<<31)	//!< Scope waits for CountEnb (not UDP_BIT_HIST_DISABLE) before starting scope mode capture.


#define HEXITEC_IRQ_FLUSHED_FRAME			(0)		//!< Bit and IRQ number to enable and clear new Flush Frame token IRQ
#define HEXITEC_IRQ_ITFG_FINISHED			(1)		//!< Bit and IRQ number  to enable and clear ITFG Finished IRQ
#define HEXITEC_IRQ_ITFG_PAUSED				(2)		//!< Bit and IRQ number  to enable and clear ITFG Paused IRQ
#define HEXITEC_IRQ_ITFG_FRAME				(3)		//!< Bit and IRQ number  to enable and clear ITFG frame changed IRQ
#define HEXITEC_IRQ_CLEAR_DONE				(4)		//!< Bit and IRQ number  to enable and clear HBM clear finished IRQ (not fully implemented)


#define HEXITEC_RX_STAT_UNEXPECTED_SOF_MHZ(x, i)    (((x)>>(10*i+1))&1)	//!< 2nd Packet has SOF
#define HEXITEC_RX_STAT_FRAME_NUM_ERROR_MHZ(x, i)	(((x)>>(10*i+2))&1)	//!< Frame number changes between first and 2nd packet
#define HEXITEC_RX_STAT_BAD_PACKET_NUM_MHZ(x, i)    (((x)>>(10*i+3))&1)	//!< Packet with SOF is not 0, or packet with EOF in not 1
#define HEXITEC_RX_STAT_MISSING_EOF_MHZ(x, i)    	(((x)>>(10*i+4))&1)	//!< 2nd packet does not have EOF set
#define HEXITEC_RX_STAT_FLAGS_MHZ(x, i)    			(((x)>>(10*i+5))&15)			//!< SOA/EOA/SOF/EOF flags from first errant frame

#define HEXITEC_INP_TF_GET_TIME_FRAME(x)		((x)&0x7FFFFFFFFL)		//!< Get time frame in UDP header from {@link HEXITEC_GLB_INP_TIME_FRAME0}
#define HEXITEC_INP_TF_GET_EXT_TRIG(x)			(((x)>>59)&0xFL)		//!< Get up to 4 bits of external trigger from {@link HEXITEC_GLB_INP_TIME_FRAME0}
#define HEXITEC_INP_TF_GET_COUNT_ENG(x)			(((x)>>63)&1L)			//!< Get Count Enb from UDP header from {@link HEXITEC_GLB_INP_TIME_FRAME0}

/** @defgroup HEXITEC_FEATURES_DEFS		Macros to describe control of how the baseline estimate is updated.
	@ingroup HEXITEC_REGS_DEFS
	@{
*/
#define HEXITEC_NUM_FEATURE_REGS				11
#define HEXITEC_FEATURE_PROC_SIZES			0			//!< Number of Chips, baseline, trigger and linearity stream and charge sharing streams
#define HEXITEC_FEATURE_TRIGGERS			1			//!< Info on the triggers and baseline subtraction
#define HEXITEC_FEATURE_LINEARITY			2			//!< Detail of the Linearity correction. 
#define HEXITEC_FEATURE_OUTPUT_FORMATS		3			//!< Detail of the outout formats available.
#define HEXITEC_FEATURE_MONITORS			4			//!< Detail of debug and monitoring features 
#define HEXITEC_FEATURE_REG_HISTA			5
#define HEXITEC_FEATURE_REG_HISTB			6
#define HEXITEC_FEATURE_REG_PBA				7			//!<  Offset within features of Playback features A
#define HEXITEC_FEATURE_REG_PBB				8			//!<  Offset within features of Playback features B
#define HEXITEC_FEATURE_REG_SCA				9			//!<  Offset within features of Scopemode features A
#define HEXITEC_FEATURE_REG_SCB				10			//!<  Offset within features of Scopemode features B

#define HEXITEC_FEATURE_PS_GENERATION(x)	((x)&0xF)	//!< Get Hexitec generation from register HEXITEC_FEATURE_PROC_SIZES
#define HEXITEC_FEATURE_PS_NUM_CHIPS(x)		(((x)>>8)&0xFF)	//!< Get Number of hexitech chips supported by this accelerator board.

#define HEXITEC_FEATURE_MONITOR_FIFOS(x)	((x)&3)
#define HEXITEC_FEAT_LIN_NBITS_PWL(x)		(((x)>>4)&0xF)	//!< Get the number of bits of Piecewise linearity.
#define HEXITEC_FEAT_LIN_SCALE_C1POSN(x)		(((x)>>8)&0x7)	//!< Get the bit position for lineariy C value to make am offset of 1 output bin of 4096
#define HEXITEC_FEAT_LIN_SCALE_B1POSN(x)		(((x)>>11)&0x1F)	//!< Get the bit position for lineariy B value to make a scaling of 1

#define HEXITEC_FEATURE_HISTA_DATA_MOVER(x)		(((x)>>1)&1)		//!< Firmware contains a data mover to generate reads to stream data to (Q)DMA to PCIe or UDP
#define HEXITEC_FEATURE_HISTA_UDP_OUTPUT(x)		(((x)>>2)&1)		//!< Data mover includes output over UDP

#define HEXITEC_FEATURE_HISTB_NUM_HIST(x)		(((x))&0xF)			//!< Number of HB< hist control reg + read/clear blocks
#define HEXITEC_FEATURE_HISTB_REORDER(x)		(((x)>>4)&0x3F)		//!< AXI address reorder block version (0 for none)
#define HEXITEC_FEATURE_PBA_NUM_PB(x)			(((x))&0xF)			//!< Number of playback DMAs
#define HEXITEC_FEATURE_PBB_PB0_START(x)		(((x))&0x1F)		//!< Start HBM port number of buffers of Playback 0
#define HEXITEC_FEATURE_PBB_PB0_NUM(x)			(((x)>>5)&0x7)		//!< Number of HBM ports for buffers of Playback 0
#define HEXITEC_FEATURE_PBB_PB1_START(x)		(((x)>>8)&0x1F)		//!< Start HBM port number of buffers of Playback 1
#define HEXITEC_FEATURE_PBB_PB1_NUM(x)			(((x)>>13)&0x7)		//!< Number of HBM ports for buffers of Playback 1
#define HEXITEC_FEATURE_PBB_PB2_START(x)		(((x)>>16)&0x1F)	//!< Start HBM port number of buffers of Playback 2
#define HEXITEC_FEATURE_PBB_PB2_NUM(x)			(((x)>>21)&0x7)		//!< Number of HBM ports for buffers of Playback 2
#define HEXITEC_FEATURE_PBB_PB3_START(x)		(((x)>>24)&0x1F)	//!< Start HBM port number of buffers of Playback 3
#define HEXITEC_FEATURE_PBB_PB3_NUM(x)			(((x)>>29)&0x7)		//!< Number of HBM ports for buffers of Playback 4

#define HEXITEC_FEATURE_PBB_PBI_START(i,x)		(((x)>>((i)*8))&0x1F)		//!< Start HBM port number of buffers of Playback i
#define HEXITEC_FEATURE_PBB_PBI_NUM(i,x)		(((x)>>((i)*8+5))&0x7)		//!< Number of HBM ports for buffers of Playback i

#define HEXITEC_FEATURE_SCA_NUM_SCOPE(x)		(((x))&0xF)			//!< Number of scope mode recorder DMAs
#define HEXITEC_FEATURE_SCB_SC0_START(x)		(((x))&0x1F)		//!< Start HBM port number of buffers of scope 0
#define HEXITEC_FEATURE_SCB_SC0_NUM(x)			(((x)>>5)&0x7)		//!< Number of HBM ports for buffers of scope 0
#define HEXITEC_FEATURE_SCB_SC1_START(x)		(((x)>>8)&0x1F)		//!< Start HBM port number of buffers of scope 1
#define HEXITEC_FEATURE_SCB_SC1_NUM(x)			(((x)>>13)&0x7)		//!< Number of HBM ports for buffers of scope 1
#define HEXITEC_FEATURE_SCB_SC2_START(x)		(((x)>>16)&0x1F)	//!< Start HBM port number of buffers of scope 2
#define HEXITEC_FEATURE_SCB_SC2_NUM(x)			(((x)>>21)&0x7)		//!< Number of HBM ports for buffers of scope 2
#define HEXITEC_FEATURE_SCB_SC3_START(x)		(((x)>>24)&0x1F)	//!< Start HBM port number of buffers of scope 3
#define HEXITEC_FEATURE_SCB_SC3_NUM(x)			(((x)>>29)&0x7)		//!< Number of HBM ports for buffers of scope 4

#define HEXITEC_FEATURE_SCB_SCI_START(i,x)		(((x)>>((i)*8))&0x1F)		//!< Start HBM port number of buffers of scope i
#define HEXITEC_FEATURE_SCB_SCI_NUM(i,x)		(((x)>>((i)*8+5))&0x7)		//!< Number of HBM ports for buffers of scope i

#define HEXITEC_FEATURE_OF_CLUSTER_GRADE(x)	((x)&0xF)				//!< Get the number of cluster grades supported
#define HEXITEC_REORDER_ENGROWCOL				(1<<0)					//!< Reorder engine supports reading energy, row, col (Down cols first, then across rows)
#define HEXITEC_REORDER_ENGCOLROW_GAPS			(1<<1)					//!< Reorder engine supports reading energy, col, row (Across rows first, then down col to next row, but with gaps between rows
#define HEXITEC_REORDER_ENGCOLROW_CONT			(1<<2)					//!< Reorder engine supports reading energy, col, row (Across rows first, then down col to next row, col and row contiguous for 6400 pixels
#define HEXITEC_REORDER_SUMMED					(1<<3)					//!< Reorder engine supports summing across chips and stream for Energy Only and calibration images

	/** @} */
/** @defgroup HEXITEC_BSUB_MASK_DEFS		Macros to describe control of how the baseline estimate is updated.
	@ingroup HEXITEC_REGS_DEFS
	@{
*/
#define HEXITEC_BSUB_MASK_NONE					(0)		//!< Never mask update, always apply feedback adjustment
#define HEXITEC_BSUB_MASK_MAIN					(1)		//!< Mask update when this pixel main trigger pos or neg over thres.
#define HEXITEC_BSUB_MASK_LOW					(2)		//!< Mask update when this pixel Main or lower trigger pos or neg over thres.
#define HEXITEC_BSUB_MASK_FIXED					(3)		//!< Never update, used Fixed baseline value with no adjustment
#define HEXITEC_BSUB_MASK_MAIN_OR4NEB			(5)		//!< Mask update when this pixel main or any of 4 neighbours  main triggers pos.
#define HEXITEC_BSUB_MASK_MAIN_OR8NEB			(0xD)	//!< Mask update when this pixel main or any of 8 neighbours  main triggers pos.
#define HEXITEC_BSUB_MASK_LOW_OR4NEB			(0x6)	//!< Mask update when this pixel main or Low  or any of 4 neighbours  main triggers pos.
#define HEXITEC_BSUB_MASK_LOW_OR8NEB			(0xE)	//!< Mask update when this pixel main or Low  or any of 8 neighbours  main triggers pos.
#define HEXITEC_BSUB_MASK_MAIN_OR_LOW_AND4NEB	(0x7)	//!< Mask update when this pixel main or (low and any of 4 neighbours  main triggers pos).
#define HEXITEC_BSUB_MASK_MAIN_OR_LOW_AND8NEB	(0xF)	//!< Mask update when this pixel main or (low and any of 8 neighbours  main triggers pos)
/** @} */
/** @defgroup HEXITEC_BSUB_DIVIDE_DEFS		Macros to describe control of how the baseline error is scaled to update baseline estimate.
	@ingroup HEXITEC_REGS_DEFS
	@{
*/

#define HEXITEC_BSUB_DIVIDE256				0			//!< Divide error feedback by 256, shift 8
#define HEXITEC_BSUB_DIVIDE512				1
#define HEXITEC_BSUB_DIVIDE1024				2
#define HEXITEC_BSUB_DIVIDE2048				3
#define HEXITEC_BSUB_DIVIDE4096				4
#define HEXITEC_BSUB_DIVIDE8192				5
#define HEXITEC_BSUB_DIVIDE16384			6
#define HEXITEC_BSUB_DIVIDE32768			7
#define HEXITEC_BSUB_DIVIDE65536			8

#define HEXITEC_BSUB_SET(maskMode, divideCode, enbDither) (((divideCode)&0xF)|(((maskMode)&0xF)<<4)| ((!!(enbDither))<<12))
#define HEXITEC_BSUB_LOAD					(1<<8)		//!< Trigger transfer of (fixed) baseline values from Read/write table to active baseline table. Needs data to be running to complete
#define HEXITEC_BSUB_SAVE					(1<<9)		//!< Trigger transfer of baseline values from active baseline value to Read/write table. Needs data to be running to complete

#define HEXITEC_BSUB_GET_MODE(x)			(((x)>>4)&0xF)		//!< Get baseline update masking mode
#define HEXITEC_BSUB_GET_DIVIDE(x)			(((x)>>0)&0xF)		//!< Get baseline update divide ratio code
#define HEXITEC_BSUB_GET_DITHER(x)			(((x)>>12)&1)		//!< Get linearity correct enable dither
#define HEXITEC_BSUB_GET_ABS_TRIG(x)		(((x)>>13)&1)		//!< Get use Absolute trigger mode

#define HEXITEC_BSUB_USE_ABS_TRIG			(1<<13)		//!< Enable use of absolute trigger to recover baseline when lost and hence too few updates.

#define HEXITEC_BSUB_REF_SCALE_MHZ				(1<<16)		//!< Scaling to allow for 16 bits of fractional precision below the binary point.
#define HEXITEC_BSUB_REF_SCALE_HXT				(1<<14)		//!< Scaling to allow for 14 bits of fractional precision below the binary point.
/** @} */

/** @defgroup HEXITEC_CLUSTER_MODE_DEFS		Macros to describe how/which clusters of hist are chosen.
	@ingroup HEXITEC_REGS_DEFS
	@{
*/

#define HEXITEC_CLUSTER_MODE_INDEP			0		//!< Each pixel trigger independently, Charge sharing correction is bypassed.		 
#define HEXITEC_CLUSTER_MODE_LONE_POS		1		//!< Trigger only on lone positive trigger, with no nearest neighbours		 
#define HEXITEC_CLUSTER_MODE_LONE_POSNEG	2		//!< Trigger only on lone positive or negative trigger, with no nearest neighbours		 
#define HEXITEC_CLUSTER_MODE_POS			3		//!< Trigger on all recognised types of clusters with 1 to 4 positive main triggers
#define HEXITEC_CLUSTER_MODE_POSNEG			4		//!< Trigger on all recognised types of clusters with at least 1 positive trigger with positive or negative neighbours
#define HEXITEC_CLUSTER_MODE_POSNEG_LOW		5		//!< In addition to all POSNEG trigger, for single positive trigger, use lower threshold to look for one significant	neighbour
#define HEXITEC_CLUSTER_MODE_AUTO_OR_POS	6		//!< Pseduo randomly trigger to allow histogram of noise but also trigger using normal Positive event trigger to see events.
#define HEXITEC_CLUSTER_MODE_AUTO			7		//!< Pseduo randomly trigger to allow histogram of noise.		 

#define HEXITEC_CLUSTER_MODE_SET(x)			((x)&7)	//!< Build Cluster mode register, from cluster mode
#define HEXITEC_AUTOTRIG_1IN16				0		//!< Auto trigger mode triggers each pixel 1 frame in 16
#define HEXITEC_AUTOTRIG_1IN8				1		//!< Auto trigger mode triggers each pixel 1 frame in 8
#define HEXITEC_AUTOTRIG_1IN4				2		//!< Auto trigger mode triggers each pixel 1 frame in 4
#define HEXITEC_AUTOTRIG_1IN2				3		//!< Auto trigger mode triggers each pixel 1 frame in 2, very fast for Hexitec MHz histogramming, probably OK for 6x2
#define HEXITEC_CLUSTER_AUTO_RATE(x)	(((x)&3)<<8)//!< Build auto trigger rate field of HEXITEC_CLUSTER_MODE from HEXITEC_AUTOTRIG_1IN16 etc.
#define HEXITEC_CLUSTER_MODE_GET(x)			((x)&7)	//!< Get Cluster mode from register
#define HEXITEC_CLUSTER_AUTO_RATE_GET(x)	(((x)>>8)&3)//!< Get Auto trigger rate from Cluster mode from register

/** @} */
/** @} */

/** @defgroup HEXITEC_CSHARE_MODE_DEFS		Macros to describe charge sharing corrections.
	@ingroup HEXITEC_REGS_DEFS
	@{
*/
#define HEXITEC_CSHARE_ENB_EDGE_POS_CORR		(1<<0)		
#define HEXITEC_CSHARE_ENB_NEG_NEB_CORR			(1<<1)
#define HEXITEC_CSHARE_ENB_L_POS_CORR			(1<<2)

#define HEXITEC_CSHARE_DIS_SUMMING				(1<<8)	//!< Disable charge summing, particularly for the special case of isolating the Fluorescence peaks
#define HEXITEC_CSHARE_DIS_ADJUST_POSN			(1<<9)	//!< Disable the adjustment of position again particularly for the special case of isolating the Fluorescence peaks

/** @} */
/** @defgroup HEXITEC_ENB_CLUSTER_DEFS		Macros to enable various cluster types into the output data
	@ingroup HEXITEC_REGS_DEFS
	@{
*/
#define HEXITEC_CLUSTER_ENB_LONE	(1<<0)		 
#define HEXITEC_CLUSTER_ENB_HOZ		(1<<1)	 
#define HEXITEC_CLUSTER_ENB_HOZ_NL	(1<<2)	 
#define HEXITEC_CLUSTER_ENB_HOZ_NR	(1<<3)	 
#define HEXITEC_CLUSTER_ENB_VERT	(1<<4) 
#define HEXITEC_CLUSTER_ENB_VERT_NA	(1<<5)	 
#define HEXITEC_CLUSTER_ENB_VERT_NB	(1<<6)	 
#define HEXITEC_CLUSTER_ENB_DIAG1	(1<<7)	 
#define HEXITEC_CLUSTER_ENB_DIAG1NL	(1<<8)	 
#define HEXITEC_CLUSTER_ENB_DIAG1NR	(1<<9)	 
#define HEXITEC_CLUSTER_ENB_DIAG2	(1<<10)	 
#define HEXITEC_CLUSTER_ENB_DIAG2NL	(1<<11)	 
#define HEXITEC_CLUSTER_ENB_DIAG2NR	(1<<12)	 
#define HEXITEC_CLUSTER_ENB_L1		(1<<13)	 
#define HEXITEC_CLUSTER_ENB_L2		(1<<14)	 
#define HEXITEC_CLUSTER_ENB_L3		(1<<15)	 
#define HEXITEC_CLUSTER_ENB_L4		(1<<16)	 
#define HEXITEC_CLUSTER_ENB_QUAD	(1<<17) 
#define HEXITEC_CLUSTER_ENB_ALL		(0x3FFFF)

#define HEXITEC_CLUSTER_ENB_EDGE_POS	(HEXITEC_CLUSTER_ENB_HOZ | HEXITEC_CLUSTER_ENB_VERT )	 
#define HEXITEC_CLUSTER_ENB_DIAG_POS	(HEXITEC_CLUSTER_ENB_DIAG1 | HEXITEC_CLUSTER_ENB_DIAG2)
#define HEXITEC_CLUSTER_ENB_EDGE_NEG	(HEXITEC_CLUSTER_ENB_HOZ_NL | HEXITEC_CLUSTER_ENB_HOZ_NR  | HEXITEC_CLUSTER_ENB_VERT_NA | HEXITEC_CLUSTER_ENB_VERT_NB)	 
#define HEXITEC_CLUSTER_ENB_DIAG_NEG	(HEXITEC_CLUSTER_ENB_DIAG1NL | HEXITEC_CLUSTER_ENB_DIAG1NR | HEXITEC_CLUSTER_ENB_DIAG2NL | HEXITEC_CLUSTER_ENB_DIAG2NR)

#define HEXITEC_CLUSTER_ENB_CLUST2		(HEXITEC_CLUSTER_ENB_EDGE_POS | HEXITEC_CLUSTER_ENB_DIAG_POS | HEXITEC_CLUSTER_ENB_EDGE_NEG | HEXITEC_CLUSTER_ENB_DIAG_NEG)
#define HEXITEC_CLUSTER_ENB_CLUST3		(HEXITEC_CLUSTER_ENB_L1 | HEXITEC_CLUSTER_ENB_L2 | HEXITEC_CLUSTER_ENB_L3 | HEXITEC_CLUSTER_ENB_L4)

/** @} */

/** @defgroup HEXITEC_CLUSTER_TYPE_DEFS		Macros to control list the full cluster type details used in firmware
	@ingroup HEXITEC_REGS_DEFS
	@{
*/
#define HEXITEC_CLUSTER_TYPE_NONE		0x00
#define HEXITEC_CLUSTER_TYPE_LONE		0x01
#define HEXITEC_CLUSTER_TYPE_HOZ		0x02
#define HEXITEC_CLUSTER_TYPE_HOZNL		0x0A
#define HEXITEC_CLUSTER_TYPE_HOZNR		0x12
#define HEXITEC_CLUSTER_TYPE_VERT		0x03
#define HEXITEC_CLUSTER_TYPE_VERTNA		0x0B
#define HEXITEC_CLUSTER_TYPE_VERTNB		0x13
#define HEXITEC_CLUSTER_TYPE_DIAG1		0x04
#define HEXITEC_CLUSTER_TYPE_DIAG1NL	0x0C
#define HEXITEC_CLUSTER_TYPE_DIAG1NR	0x14
#define HEXITEC_CLUSTER_TYPE_DIAG2		0x05
#define HEXITEC_CLUSTER_TYPE_DIAG2NL	0x0D
#define HEXITEC_CLUSTER_TYPE_DIAG2NR	0x15
#define HEXITEC_CLUSTER_TYPE_L1			0x06
#define HEXITEC_CLUSTER_TYPE_L2			0x0E
#define HEXITEC_CLUSTER_TYPE_L3			0x16
#define HEXITEC_CLUSTER_TYPE_L4			0x1E
#define HEXITEC_CLUSTER_TYPE_QUAD		0x07


/** @} */

/** @defgroup HEXITEC_CLUSTER_CLASS_DEFS		Macros to control describe the 4 bits of cluster class used to give a reduced number of cluster types
	@ingroup HEXITEC_REGS_DEFS
	@{
*/
#define HEXITEC_CLUSTER_CLASS_LONE		0x0
#define HEXITEC_CLUSTER_CLASS_EDGE_POS	0x1
#define HEXITEC_CLUSTER_CLASS_EDGE_NEG	0x2
#define HEXITEC_CLUSTER_CLASS_DIAG_POS	0x3
#define HEXITEC_CLUSTER_CLASS_DIAG_NEG	0x4
#define HEXITEC_CLUSTER_CLASS_L			0x5
#define HEXITEC_CLUSTER_CLASS_QUAD		0x6
#define HEXITEC_CLUSTER_CLASS_UNKNOWN	0xE
#define HEXITEC_CLUSTER_CLASS_PADDING	0xF

#define HEXITEC_CLUSTER_CLASS_NUMBER	0x10
/** @} */


/** @defgroup HEXITEC_FORMAT_DEFS		Macros to control format of data work sent to histogrammer.
	@ingroup HEXITEC_REGS_DEFS
	@{
*/
#define HEXITEC_HIST_SHIFT_ENG12			0	//!< Make histograms with 4096 energy bins 
#define HEXITEC_HIST_SHIFT_ENG11			1	//!< Make histograms with 2048 energy bins 
#define HEXITEC_HIST_SHIFT_ENG10			2	//!< Make histograms with 1024 energy bins 
#define HEXITEC_HIST_SHIFT_ENG9				3 	//!< Make histograms with 512  energy bins 
#define HEXITEC_HIST_SHIFT_ENG8				4 	//!< Make histograms with 256  energy bins 
#define HEXITEC_HIST_SHIFT_ENG7				5	//!< Make histograms with 128  energy bins
#define HEXITEC_HIST_SHIFT_ENG10LSB			6	//!< Make histograms with 128  energy bins

#define HEXITEC_HIST_FORMAT_RUN12			((0<<3) | HEXITEC_HIST_SHIFT_ENG12)			//!< Normal run mode, Row, column and 4096 energy bins
#define HEXITEC_HIST_FORMAT_RUN11			((0<<3) | HEXITEC_HIST_SHIFT_ENG11)			//!< Normal run mode, Row, column and 2048 energy bins		 
#define HEXITEC_HIST_FORMAT_RUN10			((0<<3) | HEXITEC_HIST_SHIFT_ENG10)			//!< Normal run mode, Row, column and 1024 energy bins		 
#define HEXITEC_HIST_FORMAT_RUN9			((0<<3) | HEXITEC_HIST_SHIFT_ENG9)			//!< Normal run mode, Row, column and 512  energy bins		 
#define HEXITEC_HIST_FORMAT_RUN8			((0<<3) | HEXITEC_HIST_SHIFT_ENG8)			//!< Normal run mode, Row, column and 256  energy bins		 
#define HEXITEC_HIST_FORMAT_RUN7			((0<<3) | HEXITEC_HIST_SHIFT_ENG7)			//!< Normal run mode, Row, column and 128  energy bins		 
#define HEXITEC_HIST_FORMAT_RUN10LSB		((0<<3) | HEXITEC_HIST_SHIFT_ENG10LSB)		//!< Normal run mode, Row, column and bottom 10 bits of Energy (for lower energy experiments)

#define HEXITEC_HIST_FORMAT_ENG_ONLY12		((1<<3) | HEXITEC_HIST_SHIFT_ENG12)			//!< 0-d energy only run mode with row and column removed and 4096 energy bins		 
#define HEXITEC_HIST_FORMAT_ENG_ONLY11		((1<<3) | HEXITEC_HIST_SHIFT_ENG11)			//!< 0-d energy only run mode with row and column removed and 2048 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY10		((1<<3) | HEXITEC_HIST_SHIFT_ENG10)			//!< 0-d energy only run mode with row and column removed and 1024 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY9		((1<<3) | HEXITEC_HIST_SHIFT_ENG9)			//!< 0-d energy only run mode with row and column removed and 512  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY8		((1<<3) | HEXITEC_HIST_SHIFT_ENG8)			//!< 0-d energy only run mode with row and column removed and 256  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY7		((1<<3) | HEXITEC_HIST_SHIFT_ENG7)			//!< 0-d energy only run mode with row and column removed and 128  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CC12	((2<<3) | HEXITEC_HIST_SHIFT_ENG12)			//!< Debug mode with Cluster Class (4 bits) position and 4096 energy bins		 
#define HEXITEC_HIST_FORMAT_ENG_POS_CC11	((2<<3) | HEXITEC_HIST_SHIFT_ENG11)			//!< Debug mode with Cluster Class (4 bits) position and 2048 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CC10	((2<<3) | HEXITEC_HIST_SHIFT_ENG10)			//!< Debug mode with Cluster Class (4 bits) position and 1024 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CC9		((2<<3) | HEXITEC_HIST_SHIFT_ENG9)			//!< Debug mode with Cluster Class (4 bits) position and 512  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CC8		((2<<3) | HEXITEC_HIST_SHIFT_ENG8)			//!< Debug mode with Cluster Class (4 bits) position and 256  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CC7		((2<<3) | HEXITEC_HIST_SHIFT_ENG7)			//!< Debug mode with Cluster Class (4 bits) position and 128  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB	((2<<3) | HEXITEC_HIST_SHIFT_ENG10LSB)		//!< Debug mode with Cluster Class (4 bits) position and bottom 10 bits of Energy				 
											
#define HEXITEC_HIST_FORMAT_ENG_ONLY_CC12	((3<<3) | HEXITEC_HIST_SHIFT_ENG12)			//!< Debug mode with Cluster Class (4 bits) and 4096 energy bins		 
#define HEXITEC_HIST_FORMAT_ENG_ONLY_CC11	((3<<3) | HEXITEC_HIST_SHIFT_ENG11)			//!< Debug mode with Cluster Class (4 bits) and 2048 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY_CC10	((3<<3) | HEXITEC_HIST_SHIFT_ENG10)			//!< Debug mode with Cluster Class (4 bits) and 1024 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY_CC9	((3<<3) | HEXITEC_HIST_SHIFT_ENG9)			//!< Debug mode with Cluster Class (4 bits) and 512  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY_CC8	((3<<3) | HEXITEC_HIST_SHIFT_ENG8)			//!< Debug mode with Cluster Class (4 bits) and 256  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_ONLY_CC7	((3<<3) | HEXITEC_HIST_SHIFT_ENG7)			//!< Debug mode with Cluster Class (4 bits) and 128  energy bins				 
																							 

#define HEXITEC_HIST_FORMAT_CALIB_COMB12	((4<<3) | HEXITEC_HIST_SHIFT_ENG12)		 	//!< Calibration mode overlaying all pixels, 4 bits of Cluster class, 1024 bins of LUT address and 4096 energy bins
#define HEXITEC_HIST_FORMAT_CALIB_COMB11	((4<<3) | HEXITEC_HIST_SHIFT_ENG11)		 	//!< Calibration mode overlaying all pixels, 4 bits of Cluster class, 1024 bins of LUT address and 2048 energy bins
#define HEXITEC_HIST_FORMAT_CALIB_COMB10	((4<<3) | HEXITEC_HIST_SHIFT_ENG10)		 	//!< Calibration mode overlaying all pixels, 4 bits of Cluster class, 1024 bins of LUT address and 1024 energy bins
#define HEXITEC_HIST_FORMAT_CALIB_COMBTYPE	((5<<3) | HEXITEC_HIST_SHIFT_ENG10)		 	//!< Calibration mode overlaying all pixels, all 6 Cluster type bits, 1024 bins of LUT address and 1024 energy bins 		 
#define HEXITEC_HIST_FORMAT_CALIB_SEPARATE	((5<<3) | HEXITEC_HIST_SHIFT_ENG9)			//!< Calibration mode separating pixels, 256 bins of LUT address and 512 energy bins 		  

#define HEXITEC_HIST_FORMAT_ENG_POS_CG12	((6<<3) | HEXITEC_HIST_SHIFT_ENG12)			//!< Run mode with Cluster Grade (1 bit) position and 4096 energy bins		 
#define HEXITEC_HIST_FORMAT_ENG_POS_CG11	((6<<3) | HEXITEC_HIST_SHIFT_ENG11)			//!< Run mode with Cluster Grade (1 bit) position and 2048 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CG10	((6<<3) | HEXITEC_HIST_SHIFT_ENG10)			//!< Run mode with Cluster Grade (1 bit) position and 1024 energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CG9		((6<<3) | HEXITEC_HIST_SHIFT_ENG9)			//!< Run mode with Cluster Grade (1 bit) position and 512  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CG8		((6<<3) | HEXITEC_HIST_SHIFT_ENG8)			//!< Run mode with Cluster Grade (1 bit) position and 256  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CG7		((6<<3) | HEXITEC_HIST_SHIFT_ENG7)			//!< Run mode with Cluster Grade (1 bit) position and 128  energy bins				 
#define HEXITEC_HIST_FORMAT_ENG_POS_CG10LSB	((6<<3) | HEXITEC_HIST_SHIFT_ENG10LSB)		//!< Run mode with Cluster Grade (1 bit) position and bottom 10 bits of Energy				 

#define HEXITEC_HIST_FORMAT_CHARAC2D12		((7<<3) | 0)								//!< Characterisation plot for up to 2 pixel clusters, 12 bits resolution fo main peak
#define HEXITEC_HIST_FORMAT_CHARAC3D		((7<<3) | 1)								//!< Characterisation plot for up to 3 pixel clusters
#define HEXITEC_HIST_FORMAT_CHARAC4D		((7<<3) | 2)								//!< Characterisation plot for up to 4 pixel clusters
#define HEXITEC_HIST_FORMAT_CHARAC2D10		((7<<3) | 3)								//!< Characterisation plot for up to 2 pixel clusters, 10 bits resolution for main peak

#define HEXITEC_CHARAC2_NUM_MAIN				4096
#define HEXITEC_CHARAC2_NUM_NEB					8192

/** @} */

/**
@defgroup HEXITEC_THRESHOLD_DEFS		Macros to describe mapped (scalar) energy mode.
@ingroup HEXITEC_REGS_DEFS
@{
*/
#define HEXITEC_HIST_MAPPED_MODE_OFF		0
#define HEXITEC_HIST_MAPPED_MODE_ONLY		1
#define HEXITEC_HIST_MAPPED_MODE_INTL		2

#define HEXITEC_HIST_FORMAT_SET(x)			((x)&0x3F)
#define HEXITEC_HIST_MAPPED_MODE_SET(x)		(((x)&0x7)<<8)
#define HEXITEC_HIST_FORMAT_SHIFT_SET(x)	(((x)&0x3)<<12)

#define HEXITEC_HIST_FORMAT_GET(x)			((x)&0x3F)
#define HEXITEC_HIST_MAPPED_MODE_GET(x)		(((x)>>8)&0x7)
#define HEXITEC_HIST_FORMAT_SHIFT_GET(x)	(((x)>>12)&0x3)

/** @} */

/**
@defgroup HEXITEC_THRESHOLD_DEFS		Macros to describe the trigger thresholds.
@ingroup HEXITEC_LUT_DEFS
@{
*/
#define HEXITEC_ABS_THRES_MAX		(4095)
#define HEXITEC_THRES_MAX			(4095)
#define HEXITEC_THRES_MIN			(-4096)
#define HEXITEC_THRES_ENABLE			0x80000000

#define HEXITEC_MTHRES_MHZ(enb, pos, neg) 	(((enb)?HEXITEC_THRES_ENABLE:0)|((neg)&0x1FFF)<<16 | ((pos)&0x1FFF))
#define HEXITEC_LTHRES_MHZ(pos, neg) 		(((neg)&0x1FFF)<<16 | ((pos)&0x1FFF))
#define HEXITEC_MTHRES_HXT(enb, pos, neg) 	(((enb)?HEXITEC_THRES_ENABLE:0)|((neg)&0x7FFF)<<16 | ((pos)&0x7FFF))
#define HEXITEC_LTHRES_HXT(pos, neg) 		(((neg)&0x7FFF)<<16 | ((pos)&0x7FFF))
#define HEXITEC_ABS_THRES_MHZ(high, low) 			(((low)&0x0FFF)<<16 | ((high)&0x0FFF))
#define HEXITEC_ABS_THRES_HXT(high, low) 			(((low)&0x3FFF)<<16 | ((high)&0x3FFF))
#define HEXITEC_GET_THRES_POS(x)			(((x))&0x7FFF)
#define HEXITEC_GET_THRES_NEG(x)			(((x)>>16)&0x7FFF)
#define HEXITEC_GET_TRIG_ENB(x)				(((x)>>31)&0x1)

/** @} */

/**
@defgroup HEXITEC_LINEARITY_DEFS		Macros to describe the linearity corrections.
@ingroup HEXITEC_LUT_DEFS
@{
*/
/* See m_linABMax, m_linABMin
#define HEXITEC_LINEARITY_AB_MAX		0XFFFFFF
#define HEXITEC_LINEARITY_AB_MIN		(-0X1000000)
*/
#define HEXITEC_LINEARITY_C_MAX			0X01FFFF
#define HEXITEC_LINEARITY_C_MIN			(-0X020000)

/* See m_linScaleB 
#define HEXITEC_LINEARITY_B_SCALE		0x400000
*/
#define HEXITEC_LINEARITY_C_SCALE			0x20

#define HEXITEC_LINEARITY_MAX_PIECES		16		//!< Maximum number of pieces of PW linearity (current build is actually 8)
/** @} */


#define HEXITEC_NUM_CLUSTER_CLASS		16	//!< Number of Cluster classes

#define HEXITEC_FLUSHED_FRAME_VALID (1L<<63)				//!< mask to determine if any time frame token has reached the histogram output this run
#define HEXITEC_FLUSHED_FRAME_GET(x)  ((x)&0xFFFFFFFFFL)	//!< Extract flushed frame token from the output word

#define MSG_ERROR	0
#define MSG_QUIET	0
#define MSG_SPARSE	1
#define MSG_NORMAL	2
#define MSG_VERBOSE	3

#define HBM_HT_CONT_USE_LIST 	1
#define HBM_HT_CONT_DIS_UPPER	2
#define HBM_HT_STAT_LIST_BUSY	1

#define HEXITEC_MHZ_ROW_STRIDE	128
#define HEXITEC_MHZ_COL_STRIDE	128
#define HEXITEC_MHZ_ROWCOL_SHIFT_EO	 (25-2)		//!< Shift for combined Row and COl when working with Energy only (discarding most of position bits)
#define HEXITEC_MHZ_PORT_SHIFT	 	 29		//!< Shift for column bits for HBM port only for calib modes which discard position
#define HEXITEC_T_STRIDE			8192		//!< For Hexitech (original) with contiguous rowCOl address (0...6399) the jump to the next time frame 8192

#define HEXITEC_REORDER_OFFSET_STREAM		(1L<<40)
#define HEXITEC_REORDER_OFFSET_ROWCOL		(2L<<40)
#define HEXITEC_REORDER_OFFSET_COLROW_GAPS	(3L<<40)
#define HEXITEC_REORDER_OFFSET_COLROW_CONT	(4L<<40)
#define HEXITEC_REORDER_OFFSET_SUMMED		(5L<<40)

#define HEXITEC_REORDER_OFFSET_MAPPED_ALL	(15L<<40)		//!< Access all 16 bins of mapped energy
#define HEXITEC_REORDER_OFFSET_MAPPED_HALF	(14L<<40)		//!< Acces mapped bins 0..7 or mapped energy

#define HEXITEC_REORDER_CHIP_SUM_ALL_HXT	15				//!< Sum all 12 chip in Hexitec 6x2 and others TBD.

#define HEXITEC_NUM_ROWCOL_SUM		16		//!< Number of row and Col address bins to sum over to get all events in Energy Only modes.
#define HEXITEC_ROWCOL_SHIFT_EO	 	26		//!< Shift for combined Row and Col when working with Energy only (discarding most of position bits)
#define HEXITEC_CHIP_SHIFT	 		30		//!< Shift for chip number for accessing reordered addresses, most formats

#define UDP_RX_HEAD_PORT			61648
#define UDP_RX_ACCEL_PORT			61649
#define UDP_TX_ACCEL_PORT			40100
#define UDP_TX_SERVER_PORT			40101

//#define UDP_EVLIST_ACCEL_PORT			40200  Removed as Accel port is a single value.
#define UDP_EVLIST_SERVER_PORT			40201

#define MAX_UDP_PACKET_BYTES		8900

#define ETHERNET_GT_RESET_REG		0x0000
#define ETHERNET_RESET_REG			0x0004
#define ETHERNET_MODE_REG			0x0008
#define ETHERNET_RX_CONF_REG1		0x0014
#define ETHERNET_PM_TICK_REG		0x0020

#define ETHERNET_MODE_LOOPBACK		(1<<31)		//!< Enable loopback at the GTs in the ethernet core

#define ETHERNET_REVISION_REG		0x0024
#define ETHERNET_TX_STATUS_REG		0x0400
#define ETHERNET_RX_STATUS_REG		0x0404
#define ETHERNET_RX_BLOCK_LOCK_REG	0x040C		//!< Not clear if this is connected in the Xilinx Core
#define ETHERNET_RX_VALID_CTRL_REG	0x0494		//!< Not clear if this is connected in the Xilinx Core
#define ETHERNET_SPEED_REG			0x0498

#define ETHERNET_STAT_CYCLE_COUNT_LSB_REG		0x0500
#define ETHERNET_STAT_CYCLE_COUNT_MSB_REG		0x0504
#define ETHERNET_STAT_BAD_CODE_LSB_REG			0x0660
#define ETHERNET_STAT_BAD_CODE_MSB_REG			0x0660
#define ETHERNET_STAT_RX_ERROR_LSB_REG			0x0668
#define ETHERNET_STAT_RX_ERROR_MSB_REG			0x0668
//#define ETHERNET_STAT_TX_FRAME_ERROR_LSB_REG	0x06A0   // This appears not to exist in the PCS/PMA only core AND reading it cause AER ERRORS. DO not try to access.
//#define ETHERNET_STAT_TX_FRAME_ERROR_MSB_REG	0x06A4

#define HEXITEC_CLOCK_MEASURE_BOTH_VALID		(1<<31)		//!< Frequency measurement valid from both reference (100 Hz side) and measured side.

#define HEXITEC_MAX_FARM_SOCKETS		256			//!< Maximum number sockets in test mode == of entries in Farm mode socket LUT
#define HEXITEC_UDP_TRAILER_BYTES		64			//!< 512 bit = 64 byte trailer for UDP 100G data TX
#define HEXITEC_UDP_MAX_FRAME_BYTES		8192		//!< Maximum payload size of frame is 8192 bytes, though can be less
#define HEXITEC_UDP_TRAILER3_RAW_HITS(x)			((x)&0xFFFFFFFFFFFFFL)		//!< Get 52 bit input frame count from 64 bit word 3 (from bit 256) of UDP trailer
#define HEXITEC_UDP_TRAILER4_INP_FRAMES(x)			((x)&0x7FFFFFFFFFL)		//!< Get 39 bit input frame count from 64 bit word 4 (from bit 256) of UDP trailer
#define HEXITEC_UDP_TRAILER4_EXT_TRIG_LCH(x)	(((x)>>56)&0xF)	//!< Get the latched version of the ext trigger signal number from from 64 bit word 4 (from bit 256) of UDP trailer. This is probably the easiest to use.
#define HEXITEC_UDP_TRAILER4_EXT_TRIG(x)		(((x)>>60)&0xF)	//!< Get the most recent version of the ext trigger signal number from 64 bit word 4 (from bit 256) of UDP trailer.

#define HEXITEC_UDP_TRAILER5_PROCSSED_FRAME(x)		(((x))&0x7FFFFFFFFL)	//!< Get 35 bit current processed time frame number from UDP trailer word 5, which should be ahead of this time frame
#define HEXITEC_UDP_TRAILER5_OVERRUN(x)		(((x)>>63)&0x1L)		//!< Get Overrun flag from Trailer word 5
#define HEXITEC_UDP_TRAILER7_PACKET(x)		((x)&0x3FFFFF)		//!< Get packet number from UDP trailer 64 bit word 7 (last 64 bits)
#define HEXITEC_UDP_TRAILER7_FRAME(x)		(((x)>>24)&0x7FFFFFFFFL)		//!< Get 35 bit extended time frame number from UDP trailer word 7 (last 64 bits)
/**
@defgroup HEXITEC_ITFG_DEFS		Macros to describe the linearity corrections.
@ingroup HEXITEC_DEFS
@{
*/


#define HEXITEC_ITFG_CONT_ENB			(1<<0)			//!< Enable ITFG, otherwise time frame will come from header if/when defined.
#define HEXITEC_ITFG_CONT_SET_MODE(x) 	(((x)&0xF)<<1)	//!< Set ITFG mode bits
#define HEXITEC_ITFG_CONT_GET_MODE(x) 	(((x)>>1)&0xF)	//!< Get ITFG mode bits
#define HEXITEC_ITFG_CONT_SET_SRC(x) 	(((x)&0xF)<<8)	//!< Set ITFG source selection field to select the require External trigger
#define HEXITEC_ITFG_CONT_GET_SRC(x) 	(((x)>>8)&0xF)	//!< Get ITFG source selection field 
#define HEXITEC_ITFG_CONT_FALLING 		(1<<12)			//!< Invert the external trigger so the falling edge triggers the system.
#define HEXITEC_ITFG_CONT_GET_FALLING(x) (((x)>>12)&1)	//!< Get invert the external trigger so the falling edge triggers the system.

#define HEXITEC_ITFG_CONT_TRIG		(1<<31)				//!< Trigger ITFG, triggers on rising edge.

#define HEXITEC_ITFG_TRIG_MODE_IMMEDIATE	0			//!< Start immediately on run, count frames
#define HEXITEC_ITFG_TRIG_MODE_SWFIRST		1			//!< Wait SW Trig then run burst of nTF x nDetFrames 
#define HEXITEC_ITFG_TRIG_MODE_SWEACH		2			//!< Wait for SW trig to start each burst of nDetFrames
#define HEXITEC_ITFG_TRIG_MODE_SWINC		3			//!< Wait for SW trig to start first then inc frame on each trig
#define HEXITEC_ITFG_TRIG_MODE_SWENB		4			//!< Count while SW Enb high, stop when low, inc TF on falling edge
#define HEXITEC_ITFG_TRIG_MODE_HWFIRST		9			//!< Wait HW Trig then run burst of nTF x nDetFrames 
#define HEXITEC_ITFG_TRIG_MODE_HWEACH		10			//!< Wait for HW trig to start each burst of nDetFrames
#define HEXITEC_ITFG_TRIG_MODE_HWINC		11			//!< Wait for HW trig to start first then inc frame on each trig
#define HEXITEC_ITFG_TRIG_MODE_HWENB		12			//!< Count while HW Enb high, stop when low, inc TF on falling

#define HEXITEC_ITFG_STAT_RUNNING	(1<<0)				//!< ITFG is running (including paused waiting for triggers if used)
#define HEXITEC_ITFG_STAT_PAUSED	(1<<1)				//!< ITFG is paused, waiting for trigger.
#define HEXITEC_ITFG_STAT_FINISHED	(1<<2)				//!< ITFG is has finished.

/** 
@}
*/
/**
@defgroup HEXITEC_EVLIST_DEFS		Macros to describe the Event list data output mode
@ingroup HEXITEC_DEFS
@{
*/

#define HEXITEC_EVLIST_MAX_SOCKETS	256					//!< Maximum number of Farm Mode LUT entries of IPAddr/sockets pairs
#define HEXITEC_EVLIST_GET_TAGS(x)	(((x)>>30)&3)		//!< Get Tags bits from th top 2 bits of each event list word
#define HEXITEC_EVLIST_TAG_HEADER		1				//!< First Word of header, lower bits contain the detector frame number
#define HEXITEC_EVLIST_TAG_HEADER_CONT	2				//!< Following words of header.
#define HEXITEC_EVLIST_TAG_DATA			0				//!< Event data, Cluster class, position and energy
#define HEXITEC_EVLIST_TAG_TRAILER		3				//!< Last word of packet, may contain read event data or dummy (cluster class 15) padding event data.

#define HEXITEC_EVLIST_H0_GET_FNUM(x)	((x)&0x3FFFFFFF)	//!< Get lower bits of detector frame number
#define HEXITEC_EVLIST_H1_GET_FNUM(x)	((x)&0xFFFF)		//!< Get upper bits of detector frame number
#define HEXITEC_EVLIST_H1_GET_CHIP(x)	(((x)>>26)&0xF)		//!< Get chip number
#define HEXITEC_EVLIST_H2_GET_TF(x)		(((x))&0x1FFFF)		//!< Get ITFG Time Frame
#define HEXITEC_EVLIST_H3_GET_EXTTRIG(x)	(((x))&0x1)		//!< Get External trigger value
#define HEXITEC_EVLIST_D_GET_ENG(x)		(((x))&0xFFF)		//!< Get 12 bits of energy from event data
#define HEXITEC_EVLIST_D_GET_POSN(x)	(((x)>>12)&0x1FFF)	//!< Get Row and Column positions (13 bits, 0...6399) from event data
#define HEXITEC_EVLIST_D_GET_SPARE(x)	(((x)>>25)&0x1)		//!< For future expansion
#define HEXITEC_EVLIST_D_GET_CC(x)		(((x)>>26)&0xF)		//!< Get 4 bits of cluster class from event data

#define HEXITEC_EVLIST_T_GET_RAW(x)		(((x))&0x3FFFFFFF)	//!< Get Raw hit count from the trailer.

#define HEXITEC_EVLIST_D1_GET_RAWPOSN(x)	(((x))&0x1FFF)		//!< Get Raw position from data offset 1
#define HEXITEC_EVLIST_D1_GET_CTYPE(x)		(((x)>>16)&0x3F)		//!< Get 6 but cluster type from data offset 1

#define HEXITEC_EVLIST_D2_GET_PIX00(x)		(((x))&0x1FFF)		//!< Get signed 13 bits of baseline subtracted pixel ADC value from data offset 2
#define HEXITEC_EVLIST_D2_GET_PIX01(x)		(((x)>>16)&0x1FFF)	//!< Get signed 13 bits of baseline subtracted pixel ADC value from data offset 2
#define HEXITEC_EVLIST_D3_GET_PIX10(x)		(((x))&0x1FFF)		//!< Get signed 13 bits of baseline subtracted pixel ADC value from data offset 3
#define HEXITEC_EVLIST_D3_GET_PIX11(x)		(((x)>>16)&0x1FFF)	//!< Get signed 13 bits of baseline subtracted pixel ADC value from data offset 2
#define HEXITEC_EVLIST_D23_PIX_SIGN			(0x1000)			//!< Pixel value sign bit

/** 
@}
*/

#ifdef __cplusplus
struct HexitecChipRegs
{
	uint32_t sel_addr;
	uint32_t basesub;
	uint32_t cluster;
	uint32_t corr_a;
	uint32_t enb_cluster;
	uint32_t format;
};


struct HexitecITfgStat
{
	uint32_t status;
	uint32_t inpFrame;
	uint32_t timeFrame;
	uint32_t cycles;
};
struct HexitecFIFOCounts
{
	int64_t directDroppedWrite[HEXITEC_MAX_PROC_COL];
	int64_t nebDroppedWrite[HEXITEC_MAX_PROC_COL];
	int64_t mappedDroppedWrite[HEXITEC_MAX_PROC_COL];
	int64_t nebMappedDroppedWrite[HEXITEC_MAX_PROC_COL];
	int64_t directStalledRead[HEXITEC_MAX_PROC_COL];
	int64_t nebStalledRead[HEXITEC_MAX_PROC_COL];
	int64_t mappedStalledRead[HEXITEC_MAX_PROC_COL];
	int64_t nebMappedStalledRead[HEXITEC_MAX_PROC_COL];
	int64_t histFIFOProgFull[HEXITEC_MAX_PROC_COL];
	int64_t histAlmostFullA[HEXITEC_MAX_PROC_COL];
	int64_t forceFlushBC[HEXITEC_MAX_PROC_COL];
	int64_t histFIFOWrite[HEXITEC_MAX_PROC_COL];
	int64_t histFIFORead[HEXITEC_MAX_PROC_COL];

	int64_t directWrite[HEXITEC_MAX_PROC_COL];
	int64_t nebWrite[HEXITEC_MAX_PROC_COL];
	int64_t mappedWrite[HEXITEC_MAX_PROC_COL];
	int64_t nebMappedWrite[HEXITEC_MAX_PROC_COL];

	int64_t totalDirectDroppedWrite=0L;
	int64_t totalNebDroppedWrite=0L;
	int64_t totalMappedDroppedWrite=0L;
	int64_t totalNebMappedDroppedWrite=0L;
	int64_t totalDirectStalledRead=0L;
	int64_t totalNebStalledRead=0L;
	int64_t totalMappedStalledRead=0L;
	int64_t totalNebMappedStalledRead=0L;
	int64_t totalHistFIFOProgFull=0L;
	int64_t totalHistAlmostFullA=0L;
	int64_t totalForceFlushBC=0L;
	int64_t totalHistFIFOWrite=0L;
	int64_t totalHistFIFORead=0L;
	int64_t totalDirectWrite=0L;
	int64_t totalNebWrite=0L;
	int64_t totalMappedWrite=0L;
	int64_t totalNebMappedWrite=0L;
};
enum HexitecITfgMode 
{
	Immediate		= 0, 				//!< Run programmed burst of frames starting immediately, 	
	SWFirst			= 1,				//!< wait for rising edge of SWTrig and then run all the frames with no gaps.
	SWCountedEach	= 2,				//!< Wait for software trigger for each frame, accumulating programmed number of input frames and then discarding unused input frames until next SW trigger.
	SWIncEach		= 3,				//!< Wait for software trigger for start of first frame, accumulating frames until next rising edge of SW Trig.
	SWGated			= 4,				//!< Count While SW trigger is high, disable when increment time frame on falling edge.
	HWFirst			= 9,				//!< Wait HW Trig then run burst of nTF x nDetFrames 
	HWCountedEach	= 10,				//!< Wait for hardware trigger for each frame, accumulating programmed number of input frames and then discarding unused input frames until next trigger.
	HWIncEach		= 11,				//!< Wait for hardware trigger for start of first frame, accumulating frames until next rising edge of Trig.
	HWGated			= 12					//!< Count While HW trigger is high, disable when increment time frame on falling edge.
};
enum HexitecSaveRestore 
{
	HexitecSaveRestore_AbsThresPos 	= 0x00000001,
	HexitecSaveRestore_AbsThresNeg 	= 0x00000002,
	HexitecSaveRestore_AbsThres 	= 0x00000003,
	HexitecSaveRestore_MainThresPos = 0x00000004,
	HexitecSaveRestore_MainThresNeg = 0x00000008,
	HexitecSaveRestore_MainThres 	= 0x0000000C,
	HexitecSaveRestore_TrigEnable 	= 0x00000010,
	HexitecSaveRestore_LowThresPos 	= 0x00000020,
	HexitecSaveRestore_LowThresNeg 	= 0x00000040,
	HexitecSaveRestore_LowThres 	= 0x00000060,
	HexitecSaveRestore_LinCorr		= 0x00000080,
	HexitecSaveRestore_CShareEdgePos= 0x00000100, 
	HexitecSaveRestore_CShareNegNeb = 0x00000200,
	HexitecSaveRestore_CShareLPos	= 0x00000400,
	HexitecSaveRestore_CShareSpares	= 0x0000F800,
	HexitecSaveRestore_CShare 		= 0x0000FF00,
	HexitecSaveRestore_OutputPixelMask = 0x00010000,
	HexitecSaveRestore_RequireAll 	= 0x80000000,
	HexitecSaveRestore_All 			= 0x7FFFFFFF,
};
using namespace std;

struct HexitecTimeFrameInfo
{
	uint64_t enabledFrames;
	uint64_t rawHits;
	int extTrigLast, extTrigLatched;
	inline bool operator==(const HexitecTimeFrameInfo& r) {return enabledFrames == r.enabledFrames && rawHits == r.rawHits && extTrigLast == r.extTrigLast && extTrigLatched == r.extTrigLatched;}
//	inline bool operator==(const HexitecTimeFrameInfo& r) {return enabledFrames == r.enabledFrames && extTrigLast == r.extTrigLast && extTrigLatched == r.extTrigLatched;}
	inline bool operator!=(const HexitecTimeFrameInfo& r) {return ! (*this==r); }
};


class XDmaHexitecException : public exception
{
	string m_errMsg;
public :
	XDmaHexitecException(string s) : m_errMsg(s) {}
	// XDmaHbmException(const char *cp) : m_errMsg(cp) {}
	XDmaHexitecException(const char * format, ...)
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

enum HexitecGeneration {HexitecGenHexitec, HexitecGenMHz};
enum HexitecUdpRxConnection {Normal, Loopback, FromHost};
enum HexitecEventListSizes {
	EventSize32, 	//!< Normal 32 bit event with position and corrected energy
	EventSize128, 	//!< Detailed 128 bit event with position, corrected energy, cluster type and 4 pixel values used
	EventSize256	//!< Possible future expansion 
	};
enum HexitecLoadSaveBaseLine 
{
	Request, 		//!< Just Request the load or Save baseline, calling code must run some frames and wait for finished.
	RequestAndWait,	//!< Request Load/Save Baseline and then wait until finished, must be called with the system running.
	UseShortBurst	//!< Use the short burst feature to enable the system to run for 2 frames at a time to receive and process the request
 };
 enum HexitecScopeMemlayout {
	 HexitecScopeMemDefault,	//!< Default layout from features registers, currently places scope mode over histograms only
	 HexitecScopeMemUsePB,		//!< Reused playback memory for scope mode
	 HexitecScopeMemAll		//!< use All HBM for scope mode data
 };
	 
class XDmaHexitec {
	int m_numChips;
	int m_numChipCols=1;		//!< Number of columns of chips in e.g. 6 x 2 array
	int m_numChipRows=1;		//!< Number of columns of chips in e.g. 6 x 2 array
	volatile uint32_t * m_regs;
	volatile uint32_t * m_dataMoverRegs=nullptr;
	uint32_t m_revision;
	const char * m_regionNames[HEXITEC_NUM_REGIONS];
	uint32_t m_regionMasks[HEXITEC_NUM_REGIONS];
	uint32_t m_features[HEXITEC_NUM_FEATURE_REGS];
	int m_numHBMPorts = 20;
	int m_defaultXDmaChan=0;
	int m_dmaDescRWChan=0;
	int m_numPbDma=0;
	int m_numScopeDma=0;
	int m_nBitsAddrPWLin=0;
	int m_nBitsCoeffLin=0;
	int m_linScaleA=1;
	int m_linScaleB=1;
	int m_linScaleC=HEXITEC_LINEARITY_C_SCALE;
	int m_bsubRefScale;
	int m_linABMax, m_linABMin;
	enum HexitecGeneration m_generation;
	int m_maxAdcValue=4095;
	int m_numRxUdp=0;
	int m_numTxUdp=0;
	int m_udpRxTestSocket[4]={-1, -1, -1, -1};
	int m_udpTxTestSocket[HEXITEC_MAX_FARM_SOCKETS];
	uint16_t m_udpTxTestPort[HEXITEC_MAX_FARM_SOCKETS];
	const int *m_regionSize=nullptr;
	uint32_t m_txAccelIpAddr=0;
	uint32_t m_txServerIpAddr[HEXITEC_MAX_FARM_SOCKETS];
	bool m_useChipSel=true;
	int m_globOffset=0;
	int m_numProcCol=1;
	bool m_hasFIFOMon=false;
	int m_maxBitsClusterGrade=0;
	int64_t m_scopeHistFrame=0L;
public :
//! [AXI_DMA_DESCRIPTOR]
/* This has  to be padded to at least 64 bytes boundaries and the padding space is used to store the virtual addresses of the descriptor and optionally data for ARM/Linux code */
	struct AXIDMADesc
	{
		uint64_t physNext;
		uint64_t physAddr;
		uint32_t reserved1[2];
		uint32_t control;
		uint32_t status;
		uint32_t app[5];
		uint32_t frameNum;
		uint32_t pad1;
		uint32_t pad2;
	};
//! [AXI_DMA_DESCRIPTOR]

	XDmaHexitec(int useQDma, int busNum, int devNum, int funcNum);
	~XDmaHexitec();
	XDma *m_xdma;		// WIH 28/6/2023 .. For Polymorphism to work this needs to be a pointer.
	XDmaHbmHist m_hbmHist;
	XDmaUDPCore m_udpCore[HEXITEC_MAX_NUM_UDP_CORES];
	XDmaUDPCore m_udpTxCore[HEXITEC_MAX_NUM_UDP_CORES];
	void setPbNumFrames(int chip, uint32_t pbNumFrames);
	uint32_t getPbNumFrames(int chip);
	enum MemReadAccess {UnknownFormat, EngPosTime, EngPosCCTime, EngTime, EngCCTime, EngCalibCC, Linear, Charac2d, Charac3d, Special};
	inline int getDevNum() { return m_xdma->getDevNum(); }
	inline int getNumChips() {return m_numChips; }
	inline int getNumChipCols() {return m_numChipCols; }
	inline int getNumChipRows() {return m_numChipRows;	}
	inline int getMaxAdcValue() { return m_maxAdcValue; }
	inline int getBsubRefScale() { return m_bsubRefScale; }
	inline int getNumHBMPorts() { return m_numHBMPorts;  }
	inline uint32_t getRegionMask(int region) { 
		if (region < 0 || region >=HEXITEC_NUM_REGIONS) throw XDmaHexitecException("getRegionMask: invalid region %d", region);
		else return m_regionMasks[region]; }
	inline int getNumProcCol() { return m_numProcCol; }
	inline bool getHasFIFOMon() { return m_hasFIFOMon; }
	inline enum HexitecGeneration getGeneration() { return m_generation; }
	inline int getMaxBitsClustGrade() { return m_maxBitsClusterGrade; }
	void writeChipRegs(int chip, int region, int offset, int num, uint32_t *data);
	void readChipRegs(int chip, int region, int offset, int num, uint32_t *data);
	void setChipReg(int chip, int offset, uint32_t value);
	uint32_t getChipReg(int chip, int offset);
	void writeGlobRegs(int offset, int num, uint32_t *data);
	void readGlobRegs(int offset, int num, uint32_t *data);
	void setGlobReg(int offset, uint32_t value);
	void setGlobReg64(int offset, uint64_t value);
	uint32_t getGlobReg(int offset);
	uint64_t getGlobReg64(int offset);
	void setPixelLUT(int chip, int region, int firstCol, int numCol, int firstRow, int numRow, uint32_t value);
	void writePixelLUT(int chip, int region, int firstCol, int numCol, int firstRow, int numRow, uint32_t *value);
	void readPixelLUT(int chip, int region, int firstCol, int numCol, int firstRow, int numRow, uint32_t *value);
	void setPixelLin(int chip, int region, int firstCol, int numCols, int firstRow, int numRows, int adcTop, uint32_t value);
	void writePixelLin(int chip, int region,  int firstCol, int numCols, int firstRow, int numRows,bool replicateAdcTop, uint32_t *data);
	void readPixelLin(int chip, int region, int firstCol, int numCols, int firstRow, int numRows, uint32_t *data);
	void setSharedLUT(int chip, int region, int stream, int first, int num, uint32_t value);
	void writeSharedLUT(int chip, int region, int stream, int first, int num, uint32_t *data);
	void readSharedLUT(int chip, int region, int stream, int first, int num, uint32_t *data);
	void initRecipLUT(int chip, int region, int stream);
	void initCShareLUTs(int chip, int stream);
	void initPixelMask(int chip);
	void setPixelMask(int chip, int col, int row, bool disable);

	void loadLinearityGainAscii(int chip, char *fullName, double scaleLinearity=1.0, double offsetADUs=0.0);
	void loadLinearityAscii(int chip, char *fullName, double scaleLinearity=1.0);
	void loadLinearityGainHDF5(char *fullName, double scale);
	void linearityAddOffset(int chip, double offsetADUs);
	void loadCShareAscii(int chip, int region, char *fullName);
	void loadCShareAsciiMC(int chip, int region, char *fullName);
	void loadEngMapAscii(int chip, char *fileName);
	void initEngMapThres(int chip, int thres);
	void loadBadPixelsTrigAscii(char *fullName);
	void loadBadPixelsOutputAscii(char *fullName);
	
	inline int getNumPbDma() { return m_numPbDma;}
	inline int getNumScopeDma() { return m_numScopeDma; } 
	void dmaReset(uint32_t streamMask);
	int dmaBuildDesc(int stream, int srcStream, int flags, uint64_t byteOffset, uint64_t numBytes, uint32_t maxBlockBytes);
	void dmaBuildPBDesc(int numFramesTotal);
	void dmaStart(uint32_t streamMask, int firstDescIn, int numDescIn, int options);
	uint32_t dmaStop(uint32_t streamMask);
	uint32_t dmaReadStatus(int stream);
	uint64_t dmaReadCurrDesc(int stream);
	uint32_t dmaReadCurrDescNum(int stream);
	void dmaWaitIdle(int streamMask, double timeOut);
	bool dmaWaitIdleNoExcept(int streamMask, double timeOut);
	int dmaPrintDesc(int stream, int firstDesc, int numDesc);
	int getMaxPbFrames();
	int getMaxScopeFrames();
	uint32_t getPbFrameBytesAligned();
	void writeDmaBuff(int stream, uint64_t offset, uint64_t numBytes, char * ptr);
	void readDmaBuff(int stream, uint64_t offset, uint64_t numBytes, char * ptr);

	void setBaselineMode(int chip, int maskMode, int divideCode, bool enbDither, bool useAbsTrig);
	void loadBaseline(int chip, HexitecLoadSaveBaseLine waitMode );
	void waitLoadBaseline(int chip);
	void saveBaseline(int chip, HexitecLoadSaveBaseLine waitMode );
	void waitSaveBaseline(int chip);
	void setAbsTriggerThres(int chip,  int firstCol, int numCols, int firstRow, int numRows, int highThres, int lowThres);
	void setMainTriggerThres(int chip, int firstCol, int numCols, int firstRow, int numRows, int pos, int neg, bool enable);
	void setLowerTriggerThres(int chip,int firstCol, int numCols,  int firstRow, int numRows, int pos, int neg);
	void setLinearityRaw(int chip, int firstCol, int numCols, int firstRow, int numRows, int a, int b, int c);
	void setLinearityOne(int chip, double offsetADUs=0.0);
	void setClusterMode(int chip, int clusterMode, int autoTrigRate=0);
	void setCShareMode(int chip, bool enbEdgePos, bool enbNegNeb, bool enbLPos, bool disSum, bool disAdjPosn);
	void getClusterMode(int chip, int *clusterMode, int *autoTrigRate);
	void setClusterTypes(int chip, int enbClusterType);
	void setHistFormat(int chip, int histFormat, int mappedMode, int histShift=0);
	void getHistFormat(int chip, int *histFormatP, int *mappedModeP, int *histShiftP=nullptr);
	int getnBinsEng(int chip);
	bool getEngLsb10(int chip);
	bool getUsePosn(int chip);
	int getnBinsClustClass(int chip);
	int getHistShift(int chip);
	void getnBinsCharac(int chip, int nBins[4]);
	int getNumTF(int chip);
	int getNumTFMapped(int chip);
	bool getEngOnly(int chip);
	bool getUseClustGrade(int chip);
	void enableHist();
	void readHistEngRowColTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder=true);
	void readHistEngColRowTime(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder=true);
	void readMappedEngRowColTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder=true);
	void readMappedEngColRowTime(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder=true);
	void readHistEngRowColCCTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder=true);
	void readHistEngColRowCCTime(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder=true);
	void readMappedEngRowColCCTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorder=true);
	void readHistEngGlobColRowTime(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numGlobRows, int firstTF, int numTF, int dmaChan, uint32_t *data);
	void readHistEngGlobColRowCCTime(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int dmaChan, uint32_t *data);
	void readMappedEngGlobColRowTime(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numGlobRows, int firstTF, int numTF, int dmaChan, uint32_t *data);

	void readHistEngTime(int numEng, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorderSumming=true);
	void readHistEngCCTime(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useReorderSumming=true);
	void readHistEngCalibClass(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data);
	void readHistCharac2d(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data);
	void readHistCharac3d(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data);
	void clearHistAll();
	void clearHistTimeframes(int firstTF, int numTF, int chip, bool mapped, bool wait);
	void setDefaultXDmaChan(int dmaChan);
	void setDmaDescRWChan(int dmaChan);
	inline int getNBitsAddrPWLin() { return m_nBitsAddrPWLin; }
	inline int getNumRxUdp() { return m_numRxUdp; }
	inline int getNumTxUdp() { return m_numTxUdp; }
	uint32_t getRxEthernetReg(int ethNum, int offset);
	uint64_t getRxEthernetReg64(int ethNum, int offset);
	void setRxEthernetReg(int ethNum, int offset, uint32_t value);
	void udpRxTestCreateSockets(uint32_t *serverIpAddrP, uint32_t *accelIpAddrP, int hostPort, int accelPort);
	int getUdpRxTestSocket(int toggle);
	void setRxEthernetLoopback(int flags);
	void udpRxSetup(uint32_t *srcIpAddrP, uint32_t *accelIpAddrP, int headPort, int accelPort, HexitecUdpRxConnection connType);
	uint64_t getMacAddr(uint32_t ipAddr);
	void udpResetCounts(bool tx);
	void udpTxTestCreateSockets(uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, int accelPort, int serverPort, int numSockets, bool sameIpAddr=true);
	void udpTxSetup(uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, int accelPort, int serverPort, int farmBase, int farmNum, bool enbFarmMode, uint16_t interFrameGap, bool useArp);
	int getUdpTxTestSocket(int index);
	int udpTxTestReadFrame(int index, int64_t & timeFrame, char *buf, size_t payloadBytes, int debugTag=0, bool discardStale=false, int64_t *dataMoverOverRunPtr=nullptr, HexitecTimeFrameInfo *tfInfo=nullptr);
	string udpShowRxStatus(bool onlyOnError);

	void iTfgDisable();
	void iTfgTrigger();
	void iTfgSetup(HexitecITfgMode mode, int extTrigSrc, bool invertExtTrig, uint32_t inpFramesPerTF, uint64_t numTF, uint32_t numCycles);
	void iTfgReadStatus(HexitecITfgStat & stat);
	void printClockFrequencies();
	enum MappedView {MappedViewSpectra, MappedViewMapped8, MappedViewMapped16 };
	enum AutonomousMode {AutoOff, AutoTriggerRead, AutoTriggerReadAndClear};
	enum FarmIndexMode { 	FarmIndexIncEOF, //!< Farm Index increments at the end of every complete time frame on that queue.
							FarmIndexIncEOP, //!< Farm Index increments on every UDP packets, so data from a time frame is spread across ports/servers.
							FarmIndexFromTF }; //!< FarmIndex is equal to the LSBits of time frame, so fixed for a complete time frame.
	void startDataMoverStream(int timeFrame, enum MappedView mappedView, bool sixteenBit, bool sumChips, enum AutonomousMode autoMode=AutoOff);
	void waitDataMoverFinished(int64_t numTF, int qid);
	bool isDataMoverFinished(int64_t numTF, int qid, int64_t *curTF=nullptr);
	void stopDataMoverStreamUDP(int qid);
	int  startDataMoverStreamUDP(int64_t timeFrameExt, enum MappedView mappedView, bool sixteenBit, bool sumChips, int qid, int farmMask, int farmBase, enum AutonomousMode autoMode=AutoOff, enum FarmIndexMode farmIndexMode = FarmIndexIncEOF);
	void startDataMoverEvList(int numQueues);
	void disableDataMoverUDPTrailer(bool disable, int packetShift);
	void clearDataMoverOverRun();
	uint32_t getDataMoverStatus();
	uint32_t getDataMoverOverRun();

	void readDataMoverStream(DataMoverContext *dm, int qid=-1);
	int getDataMoverUDPIndex(int qid);
	void saveSpectraAsc(char *fName, int chip, int numEng, int firstTF, int numTF, bool mapped=false, bool sumChips=false);
	void saveSpectraDet(char *fName, int chip, int numEng, int firstTF, int numTF, bool mapped=false, bool sumChips=false);
	void saveSpectraHdf5(char *fName, int chip, int numEng, int firstTF, int numTFFull, int numTFMapped, bool enbSpectra, bool enbMapped, bool sumChips, const char ** comments=nullptr);
	uint64_t getFlushedFrame();
	const char * getBsubMaskName(int bsub) { if (bsub <0 || bsub > 15) throw XDmaHexitecException("getBsubName: invalid bsub %d", bsub); return m_bsubMaskNames[bsub];}
	const char * getAutoTrigName(int rate) { if (rate < 0 || rate > 3) throw XDmaHexitecException("getAutoTrigName: invalid autoTrig %d", rate); return m_autoTrigNames[rate];}
	const char * getClusterModeName(int cm){ if (cm <0 || cm > 7)      throw XDmaHexitecException("getClusterModeName: invalid cluster Mode %d", cm); return m_clusterModeNames[cm];}
	const char * getITfgModeNames(int mode){ if (mode<0 || mode > 12)  throw XDmaHexitecException("getITfgModeNames: invalid iTfg Mode %d", mode); return m_iTfgModeNames[mode]; }
	
	void getDiagnosticCounters(int chip, uint32_t *frameCount, uint32_t *rawHitCount);
	void getDiagnosticCounters(uint32_t *frameCount, uint32_t *rawHitCount);
	void writeIrqEnable(uint32_t irqEnb);
	uint32_t getIrqEnable();
	void setIrqEnable(uint32_t irqSet);
	void clearIrqEnable(uint32_t irqClr);
	int getEventFd(int irqNum);
	int supportsIrqs() { return !(m_xdma->isQdma()); }
	int m_debug;
	int64_t getOneFIFOCounts(int chip, int procCol, int offset);
	HexitecFIFOCounts getAllFIFOCounts(int chip);
	uint64_t getInpTimeFrame(int chip);
	void setClusterGradeReg(int chip, uint32_t value);
	void setClusterGrade(int chip, int gradeMap[HEXITEC_CLUSTER_CLASS_NUMBER]);
	void getClusterGrade(int chip, int gradeMap[HEXITEC_CLUSTER_CLASS_NUMBER]);
	void saveSettingsHdf5(const char *fName, int chip=-1, enum HexitecSaveRestore saveFlags=HexitecSaveRestore_All);
	void loadSettingsHdf5(const char *fName, int chip, enum HexitecSaveRestore loadFlags, double scaleLinearity=1.0);
	void writePixelMask(int chip, int firstCol, int numCols, int firstRow, int numRows, uint8_t *disableMask);
	void readPixelMask(int chip, int firstCol, int numCols, int firstRow, int numRows, uint8_t *disableMask);
	std::string m_absTrigHighName[HEXITEC_MAX_CHIPS];
	std::string m_absTrigLowName[HEXITEC_MAX_CHIPS];
	std::string m_mainTrigPosName[HEXITEC_MAX_CHIPS];
	std::string m_mainTrigNegName[HEXITEC_MAX_CHIPS];
	std::string m_lowTrigPosName[HEXITEC_MAX_CHIPS];
	std::string m_lowTrigNegName[HEXITEC_MAX_CHIPS];
	std::string m_linCorrName[HEXITEC_MAX_CHIPS];
	std::string m_trigEnableName[HEXITEC_MAX_CHIPS];
	std::string m_cShareEdgePosName[HEXITEC_MAX_CHIPS];
	std::string m_cShareNegNeb[HEXITEC_MAX_CHIPS];
	std::string m_cShareLPos[HEXITEC_MAX_CHIPS];
	std::string m_outputPixelMaskName[HEXITEC_MAX_CHIPS];
	void eventListCloseFiles();
	void eventListStop();
	void eventListCalcIpAddrs(uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, bool sameIpAddr, int i, uint32_t &accelIpAddr, uint32_t &serverIpAddr);
	void eventListCreateSockets( uint32_t *accelIpAddrP, uint32_t *serverIpAddrP, int accelPort, int serverPort, int numSockets, bool sameIpAddr);
	void eventListStartQdmaRx(int numQueues);
	void eventListResetFrameCounters();
	bool eventListAnyBusy();
	void eventListOpenFiles(char * rootFname);
	void eventListWaitIdle();
	uint32_t eventListCalcDataPath(int numThreads, bool useQdma, HexitecEventListSizes eventSizes);
	void setScopeMemory(enum HexitecScopeMemlayout layout);
	void dmaBuildScopeDesc(int numFramesTotal);
	void armScopeMode(enum HexitecScopeMemlayout memLayout, int numFrames, bool waitCountEnb);
	void dmaScopeWaitIdle(double timeOut);
	void readDmaDesc(int stream, int first, int num, XDmaHexitec::AXIDMADesc *descBuff);
	void readScopeDataMHz(int firstFrame, int numFrames, void *outputBuffer);
	void readAndHistScope(int numFrames, int absHighThres, int absLowThres, int posThres, uint32_t *rawHist1d, uint32_t *rawHist2d, uint32_t *bsubHist1d, uint32_t *bsubHist2d, uint32_t *bsubHist3d, int maxErrors, int &errors, bool noClear);
	void histScopeDataMHz(int frame, int numFrames, int absHighThres, int absLowThres, int posThres, uint8_t *frameBuffer,  
							uint32_t *rawHist1d, uint32_t *rawHist2d, uint32_t *bsubHist1d, uint32_t *bsubHist2d, uint32_t *bsubHist3d, int maxErrors, int &errors, int64_t &prevExtTF, bool &prevCountEnb);
	string decodeDataPath();
	string decodeDataPath(uint32_t dataPath);
	
	string decodeBaseline(int chip);
	string decodeBaseline(uint32_t baselineReg);
	string decodeClusterMode(int chip);
	string decodeClusterMode(uint32_t clusterMode);
	string decodeClusterEnable(int chip);
	string decodeClusterEnable(uint32_t clusterEnb);
	string decodeHistFormat(int chip);
	string decodeClusterGrade(int chip);
	string decodeClusterGrade(uint32_t clusterGrade);
	void iTfgGetSetup(HexitecITfgMode &mode, int &extTrigSrc, bool &invertExtTrig, uint32_t &inpFramesPerTF, uint64_t &numTF, uint32_t &numCycles);
	const char *getITfgStatusName(uint32_t status);
	void decodeDataMoverStream(int qid, bool force, string &setup , string & status, bool &running, bool &setupChange, bool & statusChange, int64_t *tFramePtr=nullptr);
	void readTimeFrameInfo(int chip, int64_t firstTF, int numTF, HexitecTimeFrameInfo *data);
	void softwareFlush(uint64_t lastFrame);

private:
	void initDMA();
	void initDataMover();
	void interpretHistFormat(int chip, int histFormat, int mappedMode, int histShift);
	void readHistEngRowColTimeInt(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass, bool useReorder);
	void readHistEngRowColTimeReordered(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass);
	void readHistEngTimeInt(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass, bool useReorderSumming);
	void readHistEngTimeReorderedMHz(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClusterClass);
	void readHistEngTimeReorderedHxt(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClusterClass);
//	void readHistEngCalibClassReorderedMHz(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data);
	void readHistEngCalibClassSummedMHz(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data);
	void readHistEngCalibClassReorderedHxt(int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data);
//	void readHistCharac2dReorderedMHz(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data);
	void readHistCharac2dReorderedHxt(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data);
//	void readHistCharac3dReorderedMHz(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data);
	void readHistCharac3dReorderedHxt(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data);
	void readHistCharac2dSummedMHz(int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data);
	void readHistCharac3dSummedMHz(int firstHoz, int numHoz, int firstVert, int numVert, int firstCorn, int numCorn, int chip, int dmaChan, uint32_t *data);
	void readHistEngColRowTimeInt(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass, bool useReorder);
	void readHistEngColRowTimeReorderedMHz(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass);
	void readHistEngColRowTimeReorderedHxt(int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool mapped, bool useClustClass);
	void readHistEngTimeSummedMHz(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass);
	void readHistEngTimeSummedHxt(int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, bool useClustClass);
	void readHistEngGlobColRowTimeInt(int numEng, int firstGlobCol, int numGlobCols, int firstGlobRow, int numGlobRows, int firstCC, int numCC, int firstTF, int numTF, int dmaChan, uint32_t *data, bool mapped, bool useClustClass);
	void checkFormatMatch(const char * funcName);
	void saveThresholds(H5::H5File &h5File, int firstChip, int lastChip, int which, bool doPos, bool doNeg, bool doEnable);
	void saveDataSetUInt32(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], const char * labels[], uint32_t *buffer);
	void saveDataSetUInt16(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], const char * labels[], uint16_t *buffer);
	void saveDataSetUInt8(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], const char * labels[], uint8_t *buffer);
	void saveLinCorr(H5::H5File &h5File, int firstChip, int lastChip);
	void saveCShareCorr(H5::H5File &h5File, int firstChip, int lastChip, int which);
	void saveOutputPixelMask(H5::H5File &h5File, int firstChip, int lastChip);
	enum RequiredDataType {RequiredUInt8, RequiredUInt16, RequiredUInt32, RequiredDouble, RequiredAny};
	void loadThresholds(H5::H5File &h5File, int firstChip, int lastChip, int which, bool doPos, bool doNeg, bool doEnable, bool requireAll, char *fName);
	void loadLinCorr(H5::H5File &h5File, int firstChip, int lastChip, bool requireAll, char *fName, double scaleLinearity);
	void loadCShareCorr(H5::H5File &h5File, int firstChip, int lastChip, int which, bool requireAll, char *fName);
	void loadOutputPixelMask(H5::H5File &h5File, int firstChip, int lastChip, bool requireAll, char *fName);
	std::unique_ptr<uint8_t[]> readDataSet(H5::H5File &h5File, const char * dataSetName, int rank, hsize_t dims[], RequiredDataType &requiredType);
	void hdf5AddAttributes(H5::DataSet &ds, const char ** comments);
	void hdf5AddIntAttribute(H5::DataSet &ds, const char * rootName, int chip, int value);
	void hdf5AddStringAttribute(H5::DataSet &ds, const char * rootName, int chip, const char * cStr);
	void hdf5AddStringAttribute(H5::DataSet &ds, const char * rootName, int chip, std::string str);
	inline void readOneFrameInfo(volatile uint64_t *p64, HexitecTimeFrameInfo *data);
//! [AXI_DMA_STREAM]
	struct DMAStream
	{
		int32_t state=0;
		int32_t isTx=0;
		u_int64_t baseAddr=0;			// Byte offset into AXI LITE space.
		u_int64_t descPhys=0;			// Physical address in AXI space (HBM)
		u_int64_t dataStart=0;		// Byte offset of data buffer in AXI space (HBM)
		u_int64_t dataSize=0;		// Total data size, sum of all segments.
		u_int64_t transferStart=0;
		u_int64_t transferSize=0;
		u_int32_t numDesc=0;
		u_int32_t maxBlockBytes=0;
		u_int32_t validFrameBytes=0;		//!< If >0, this alllows the bytes per frame to be output to be less than the aligned size (maxBlockBytes)
		int32_t definedDesc=0;
		int32_t numFrames=0;
		uint32_t * virtBase=nullptr;		// Virtual base address of the control registers.
		enum {AllOneFrame, FramePerDesc, FrameByBytes } frameRule;
		DMAStream() { memset( this, 0, sizeof(*this) ) ; }; // Don't used any Virtual functions in this inner class, or change this initialisation.
		int readoutCurDesc = 0;
		int readoutErrorFlags = 0;
		int readoutErrorData = 0;

	} ;
//! [AXI_DMA_STREAM]
	struct DataFormat
	{
		int histMode;
		int nBinsEng;
		int nBinsClustClass;
		int numTF;
		int numTFMapped;
		int mappedMode;
		int nBins[4];		// Number of bins for Charactisation modes
		int nBinsLutAddr=1024;	// Currently fixed, but may  change with different firmware
		bool usesPosn;
		int histShift;
		enum MemReadAccess memReadAccess = UnknownFormat;
		inline bool operator==(const DataFormat& rhs)
		{
			return this->histMode == rhs.histMode && this->nBinsEng == rhs.nBinsEng && this->numTF == rhs.numTF && this->numTFMapped == rhs.numTFMapped &&
				this->mappedMode == rhs.mappedMode && this->nBinsLutAddr == rhs.nBinsLutAddr && this->memReadAccess == rhs.memReadAccess && this->histShift == rhs.histShift;
		}
	};
	DataFormat m_dataFormat[HEXITEC_MAX_CHIPS];
	DMAStream m_dmaStream[HEXITEC_NUM_AXI_DMA];
	struct DataMoverState 
	{
		uint32_t dm0=0, dm2=0, dm3=0;
	} m_prevDataMoverState[HEXITEC_DM_NUM_QUEUES];
	const char *m_bsubMaskNames[16] ={"Always Update", "Main Trigger", "Low Trigger", "Fixed Baseline", "Unused4", "Main Or 4 neb", "Low or 3 Neb", "Main or Low And4 neb", 
									"Unused8", "Unused9", "Unused10", "Unused11", "Unused12", "Main or 8 neb", "Low or 8 neb", "Main or low and 8 neb" };
	const char *m_clusterModeNames[8] = {"Independent", "Lone Positive", "Lone Pos & Neg", "Main Positive", "Main Pos & Neg", "Main+low Pos & Neg", "Auto or man pos", "Auto Only"};
	const char *m_autoTrigNames[4]  = {"1 in 16", "1 in 8", "1 in 4", "1 in 2"};
	const char *m_iTfgModeNames[13] =  {"Start immediately", "Wait SW then burst", "Wait SW each frame", "Inc Frame on SW", "CountENb from SW", "5 Unused", "6 Unused", "7 Unused", "8 Unused", 
	"Wait HW then burst", "Wait HW each frame", "Inc Frame from HW", "Use Hw as Count enb" };
	const char * m_mappedViewNames[4] =  {"Spectrum", "Reserved", "Mapped 8", "Mapped 16"};
	const char * m_DMfarmIndexModeNames[4] = {"Inc each Frame", "Inc each Packet", "From timeframe", "Reserved"};

	struct EventListWriter 
	{
		std::mutex m_mutex;
		std::thread m_thread;
		bool m_threadValid = false;
		int m_socketFd=-1;
		int m_fileFd=-1;
		std::atomic<bool> m_busy{false};
		uint32_t m_ipAddr;
		uint16_t m_port;
		std::atomic<int64_t> m_currFrame{-1};
	} m_eventWriter[HEXITEC_EVLIST_MAX_SOCKETS];
	std::atomic<uint64_t> m_eventListFramingErrors{0};
	std::atomic<uint64_t> m_eventListFileErrors{0};
	std::atomic<uint64_t> m_eventListLostFrames{0};
	uint64_t m_eventListMaxPrintErrors=100L;
	int m_numEventWriters=0;
	bool m_eventWriteQdma=false;
	bool m_eventWriteIncludeLength=false;
	HexitecEventListSizes m_eventListEventSize=EventSize32;
	void eventListWrite(int tNum);

	};
#endif

#ifdef __cplusplus
extern "C" {
#endif
void *hexitec_open(int useQDma, int busNum, int devNum, int funcNum);
void hexitec_close(void *handle);
int hexitec_getHistFormat(void *handle, int chip, int *histFormatP, int *mappedModeP, int *histShiftP);
int hexitec_getNumChips(void *handle);
int hexitec_getnBinsEng(void *handle, int chip);
int hexitec_getNumTF(void *handle, int chip);
int hexitec_getNumTFMapped(void *handle, int chip);
int hexitec_getnBinsCharac(void *handle, int chip, int nBins[4]);
int hexitec_getnBinsClustClass(void *handle, int chip);
int hexitec_getNumChipCols(void *handle);
int hexitec_getNumChipRows(void *handle);
int hexitec_getUsePosn(void *handle, int chip);

int hexitec_readHistEngRowColTime(void *handle, int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);
int hexitec_readHistEngColRowTime(void *handle, int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);
int hexitec_readHistEngCalibClass(void *handle, int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data);
int hexitec_readHistCharac2d(void *handle, int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data);
int hexitec_readMappedEngColRowTime(void *handle, int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, int useReorder);
int hexitec_readHistEngTime(void *handle, int numEng, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);
int hexitec_readHistEngCCTime(void *handle, int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);
int hexitec_readHistEngColRowCCTime(void *handle, int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);

char * hexitec_get_error_message();
#ifdef __cplusplus
}
#endif

#endif
