#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <chrono>
#include <thread>

#include "xdma.h"
#include "qdma.h"
#include "qdma_nl.h"
#include "dmautils.h"

#include <linux/ioctl.h>
#define IOCTL_XDMA_ALIGN_GET    _IOR('q', 6, int)
//#define RW_MAX_SIZE	0x7ffff000
// 13/2/2023 writeDAMAligned m was  failing .. trying forcing smaller chunks
//#define RW_MAX_SIZE	0x1ffff000
// 12/5/2023  writeDAMAligned was still failing (note 3 instances of vivadi were running?)
#define RW_MAX_SIZE	0x07fff000


/** Memory ordering
The mmapped pointers are mapped 	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
From https://www.amd.com/system/files/TechDocs/24593.pdf ... It appears is sufficient to ensure strict memory ordering.
Also pointers are declared as volatile.
So hopefully don't need any more fence instructions.
*/

using namespace std;

static void qdma_dump_response(const char *resp)
{
	printf("%s", resp);
}

QDma :: QDma()
{
	m_devName= "Uninitialised QDma";
	m_hasMemBAR = 0;
	m_histMemBase = nullptr;
	for (int i =0; i<HEXITEC_MAX_EVLIST_Q; i++)
		m_evListFd[i] = -1;
	for (int i=0;i<XDMA_NUM_EVENT_IRQS; i++)
		m_eventFd[i] = -1;
	m_isQdma = true;
	cout << "XDma() making blank QDma, m_devName=" << m_devName <<endl; 
}
QDma :: QDma(int busNum, int devNum, int funcNum, size_t userSize)
{
	char deviceName[100];
	volatile uint32_t *p;
	uint32_t regs[5];
	int i;
	int qid;
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparam = &xcmd.req.qparm;
	int rv = 0;

	for (i =0; i<HEXITEC_MAX_EVLIST_Q; i++)
		m_evListFd[i] = -1;
	for (i=0;i<XDMA_NUM_EVENT_IRQS; i++)
		m_eventFd[i] = -1;

	m_isQdma = true;

	m_busNum = busNum;
	m_devNum = devNum;
	m_funcNum = funcNum;
	m_devName = "bus"+to_string(busNum)+"dev"+to_string(devNum);
	
	m_num_h2c = 1;
	m_num_c2h = 2;
	for (i=0;i<XDmaHbmHistMaxChannels;i++)
		m_h2c_fd[i] = m_c2h_fd[i] = -1;
	
	sprintf(deviceName, "/sys/bus/pci/devices/0000:%02x:%02x.%x/resource%u", busNum, devNum, funcNum, 2);
	printf("QDma() trying file %s\n", deviceName);

	m_regsBAR = move(XDmaMMap(deviceName , userSize)) ;
	sprintf(deviceName, "/sys/bus/pci/devices/0000:%02x:%02x.%x/resource%u", busNum, devNum, funcNum, 4);
	try {
		m_memBAR = move(XDmaMMap(deviceName, HEXITEC_MEM_BYPASS_SIZE)) ;
		m_hasMemBAR=1;
		m_histMemBase = m_memBAR.m_base;
	}
	catch(exception & e)
	{
		cout << "Cannot access mem Bypass BAR, continuing without" << endl;
		m_hasMemBAR=0;
		m_histMemBase = nullptr;
	}

	for (i=0; i<m_num_h2c; i++) 
	{
		m_h2c_alignment[i] = 1;
	}
	for (i=0; i<m_num_c2h; i++)
	{
		m_c2h_alignment[i] = 1;
	}

	/* Try to make a queue for MM transfer, bidirectional */	
	memset(&xcmd, 0, sizeof(xcmd));
	xcmd.log_msg_dump = qdma_dump_response;
	for (qid=256; qid<511; qid++)
	{
		printf("Trying to build queue id %d\n", qid);
		xcmd.op = XNL_CMD_Q_ADD;

		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = qid;
		qparam->num_q = 1;
		qparam->sflags |= 1 << QPARM_MODE;
		qparam->flags |= XNL_F_QMODE_MM;
		qparam->flags |= XNL_F_QDIR_BOTH;
		qparam->sflags |= 1 << QPARM_DIR;
		if ((rv=qdma_q_add(&xcmd)) >= 0)
		{
			printf("... Success rv=%d\n", rv);
			m_mmQid = qid;
			break;
		}
	}
	if (qid==512)
		printf("Cannot create a QDMA queue to use for memory mapped IO\n");
	/* now start the MM queue */

	memset(&xcmd, 0, sizeof(xcmd));

	xcmd.log_msg_dump = qdma_dump_response;
	xcmd.op = XNL_CMD_Q_START;

	xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
	qparam->idx = qid;
	qparam->num_q = 1;
	qparam->flags |= XNL_F_QDIR_BOTH;
	qparam->sflags |= 1 << QPARM_DIR;
	qparam->fetch_credit = Q_ENABLE_C2H_FETCH_CREDIT;
	qparam->flags |= (XNL_F_CMPL_STATUS_EN | XNL_F_CMPL_STATUS_ACC_EN | XNL_F_CMPL_STATUS_PEND_CHK | XNL_F_CMPL_STATUS_DESC_EN | XNL_F_FETCH_CREDIT);

	qparam->qrngsz_idx = 15;  // Ring size 16384
	qparam->sflags |= 1 << QPARM_RNGSZ_IDX;
	qparam->c2h_bufsz_idx = 0; // Buf size 4096 = 1 page so scatter gather can always work
	qparam->sflags |= 1 << QPARM_C2H_BUFSZ_IDX;

	printf("qdma_q_start returned %d\n", qdma_q_start(&xcmd));

	for (i=0; i<m_num_h2c; i++) 
	{
		m_h2c_alignment[i] = 1;
	}
	for (i=0; i<m_num_c2h; i++)
	{
		m_c2h_alignment[i] = 1;
	}
		
	sprintf(deviceName, "/dev/qdma%02X%02X%X-MM-%d", m_busNum, m_devNum, m_funcNum, m_mmQid);
	m_c2h_fd[0] = open(deviceName, O_RDWR);
	if (m_c2h_fd[0] <0)
		throw XDmaException("Cannot open device %s, errno=%d", deviceName, errno);
	m_h2c_fd[0] = m_c2h_fd[0];

}

void QDma :: createStreamQueues()
{	
	char deviceName[100];
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparam = &xcmd.req.qparm;
	int rv = 0;
	int qid;
	
	/* Try to make a queue for ST transfer, Card to Host only */	
	memset(&xcmd, 0, sizeof(xcmd));
	xcmd.log_msg_dump = qdma_dump_response;
	for (qid=4; qid<HEXITEC_DM_NUM_QUEUES; qid++)		// Changed to start at 4 to leave 0..3 for UDP mode.
	{
		printf("Trying to build queue id %d\n", qid);
		xcmd.op = XNL_CMD_Q_ADD;

		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = qid;
		qparam->num_q = 1;
		qparam->sflags |= 1 << QPARM_MODE;
		qparam->flags |= XNL_F_QMODE_ST;
		qparam->flags |= XNL_F_QDIR_C2H;
		qparam->sflags |= 1 << QPARM_DIR;
		if ((rv=qdma_q_add(&xcmd)) >= 0)
		{
			printf("... Success rv=%d\n", rv);
			m_stQid = qid;
			break;
		}
	}
	if (qid==HEXITEC_DM_NUM_QUEUES)
		printf("Cannot create a QDMA queue to use for stream IO\n");
	/* now start the ST queue */

	memset(&xcmd, 0, sizeof(xcmd));

	xcmd.log_msg_dump = qdma_dump_response;
	xcmd.op = XNL_CMD_Q_START;

/*
	if ((dir == QDMA_Q_DIR_C2H) && (mode == QDMA_Q_MODE_ST)) {
		if (cmptsz)
			qparm->cmpt_entry_size = cmptsz;
		else
			qparm->cmpt_entry_size = XNL_ST_C2H_CMPT_DESC_SIZE_8B;
		qparm->cmpt_tmr_idx = idx_tmr;
		qparm->cmpt_cntr_idx = idx_cnt;
		qparm->cmpt_trig_mode = trig_mode;
		if (pfetch_en)
			qparm->flags |= XNL_F_PFETCH_EN;
	}

	qparm->flags |= (XNL_F_CMPL_STATUS_EN | XNL_F_CMPL_STATUS_ACC_EN |
			XNL_F_CMPL_STATUS_PEND_CHK | XNL_F_CMPL_STATUS_DESC_EN |
			XNL_F_FETCH_CREDIT);

*/

	xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
	qparam->idx = qid;
	qparam->num_q = 1;
	qparam->flags |= XNL_F_QDIR_C2H;
	qparam->sflags |= 1 << QPARM_DIR;
	qparam->fetch_credit = Q_ENABLE_C2H_FETCH_CREDIT;
	qparam->flags |= (XNL_F_CMPL_STATUS_EN | XNL_F_CMPL_STATUS_ACC_EN | XNL_F_CMPL_STATUS_PEND_CHK | XNL_F_CMPL_STATUS_DESC_EN | XNL_F_FETCH_CREDIT | XNL_F_PFETCH_EN);
	qparam->cmpt_entry_size = XNL_ST_C2H_CMPT_DESC_SIZE_16B; // 128 bit completion
	qparam->cmpt_trig_mode = 1; // Trig every desc. Perhaps can change this to use timeout and/or packet count.
	qparam->qrngsz_idx = 15;  // Ring size 16384
	qparam->sflags |= 1 << QPARM_RNGSZ_IDX;
	qparam->c2h_bufsz_idx = 0; // Buf size 4096 = 1 page so scatter gather can always work
	qparam->sflags |= 1 << QPARM_C2H_BUFSZ_IDX;

	printf("qdma_q_start returned %d\n", qdma_q_start(&xcmd));

	sprintf(deviceName, "/dev/qdma%02X%02X%X-ST-%d", m_busNum, m_devNum, m_funcNum, m_stQid);
	m_c2h_fd[1] = open(deviceName, O_RDWR);
	printf("Opened stream device %s as path %d\n", deviceName, m_c2h_fd[1]);
	if (m_c2h_fd[1] <0)
		throw XDmaException("Cannot open device %s, errno=%d", deviceName, errno);
}
QDma :: ~QDma()
{
	int i;
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparam = &xcmd.req.qparm;
	for (i=0;i<m_num_c2h; i++)
	{
		if (m_c2h_fd[i] >= 0)
		{
			close (m_c2h_fd[i]);
			m_c2h_fd[i] = -1;
		}
	}
	
	if (m_mmQid >= 0)
	{
		int rv = 0;
		printf ("~QDma: stopping QDMA queue %d\n", m_mmQid);

		memset(&xcmd, 0, sizeof(xcmd));

		xcmd.op = XNL_CMD_Q_STOP;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = m_mmQid;
		qparam->num_q = 1;
		qparam->flags |= XNL_F_QDIR_BOTH;
		qparam->sflags |= 1 << QPARM_DIR;
		qdma_q_stop(&xcmd);

		memset(&xcmd, 0, sizeof(xcmd));

		xcmd.op = XNL_CMD_Q_DEL;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = m_mmQid;
		qparam->num_q = 1;
		qparam->flags |= XNL_F_QDIR_BOTH;
		qparam->sflags |= 1 << QPARM_DIR;
		
		qdma_q_del(&xcmd);
	}

	if (m_stQid >= 0)
	{
		int rv = 0;
		printf ("~QDma: stopping QDMA queue %d\n", m_stQid);

		memset(&xcmd, 0, sizeof(xcmd));

		xcmd.op = XNL_CMD_Q_STOP;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = m_stQid;
		qparam->num_q = 1;
		qparam->flags |= XNL_F_QDIR_C2H;
		qparam->sflags |= 1 << QPARM_DIR;
		qdma_q_stop(&xcmd);

		memset(&xcmd, 0, sizeof(xcmd));

		xcmd.op = XNL_CMD_Q_DEL;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = m_stQid;
		qparam->num_q = 1;
		qparam->flags |= XNL_F_QDIR_C2H;
		qparam->sflags |= 1 << QPARM_DIR;
		
		qdma_q_del(&xcmd);
	}

	for (int i =0; i<m_numEvListQid; i++)
	{
		if (m_evListFd[i] >= 0)
		{
			close(m_evListFd[i]);
			m_evListFd[i] = -1;
		}
	}
	
	if (m_numEvListQid > 0)
	{			
		memset(&xcmd, 0, sizeof(xcmd));
		xcmd.op = XNL_CMD_Q_STOP;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = HEXITEC_EVLIST_Q_BASE;
		qparam->num_q = m_numEvListQid;
		qparam->flags |= XNL_F_QDIR_C2H;
		qparam->sflags |= 1 << QPARM_DIR;
		qdma_q_stop(&xcmd);

		memset(&xcmd, 0, sizeof(xcmd));

		xcmd.op = XNL_CMD_Q_DEL;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = HEXITEC_EVLIST_Q_BASE;
		qparam->num_q = m_numEvListQid;
		qparam->flags |= XNL_F_QDIR_C2H;
		qparam->sflags |= 1 << QPARM_DIR;
		
		qdma_q_del(&xcmd);
		m_numEvListQid = 0;
	}


/*	for (i=0;i<m_num_h2c; i++)
	{
		if (m_h2c_fd[i] >= 0)
			close (m_h2c_fd[i]);
	}
*/
}

#if 1
QDma & QDma::operator= (QDma && src)
{
	int i;
	printf ("QDma move operator this=%p, src=%p\n", this, &src);
	if (this != &src)
	{
		this->m_devName = move(src.m_devName); // Don't why this does not work when called from XDmaHexitec constructor
		this->m_busNum = src.m_busNum;
		this->m_devNum = src.m_devNum;
		this->m_funcNum = src.m_funcNum;
		this->m_num_h2c = src.m_num_h2c;
		this->m_mmQid = src.m_mmQid;
		printf("sett m_mmQid=%d in move operator\n", src.m_mmQid);
		src.m_mmQid = -1;
		for (i=0; i<src.m_num_h2c; i++) 
		{
			this->m_h2c_fd[i] = src.m_h2c_fd[i];
			src.m_h2c_fd[i] = -1;
			this->m_h2c_alignment[i] = src.m_h2c_alignment[i];
		}
		this->m_num_c2h = src.m_num_c2h;
		for (i=0; i<src.m_num_c2h; i++)
		{
			this->m_c2h_fd[i] = src.m_c2h_fd[i];
			src.m_c2h_fd[i] = -1;
			this->m_c2h_alignment[i] = src.m_c2h_alignment[i];
		}
		this->m_regsBAR = move(src.m_regsBAR);
		this->m_memBAR = move(src.m_memBAR);
		this->m_histMemBase = src.m_histMemBase;
		this->m_hasMemBAR = src.m_hasMemBAR;
		this->m_numEvListQid = src.m_numEvListQid;
		for (i=0; i<m_numEvListQid; i++)
		{
			this->m_evListFd[i] = src.m_evListFd[i];
			src.m_evListFd[i] = -1;
		}
		src.m_numEvListQid = 0;
	}
	return *this;
}
#endif

void QDma::createEvListQueues(int num)
{
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparam = &xcmd.req.qparm;
	int rv = 0;
	int i;
	int qid;
	char deviceName[100];

	if (num < 0 || num > HEXITEC_MAX_EVLIST_Q)
		throw XDmaException("QDma::createEvListQueues: Number of event list queues should be in range 0 ... %d, not %d", HEXITEC_MAX_EVLIST_Q, num);
	
	if (num < m_numEvListQid)
	{
		// Delete unused queues
		for (i=num; i<m_numEvListQid; i++)
		{
			if (m_evListFd[i] >= 0)
			{
				close(m_evListFd[i]);
				m_evListFd[i] = -1;
			}
		}
			
		memset(&xcmd, 0, sizeof(xcmd));
		xcmd.op = XNL_CMD_Q_STOP;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = HEXITEC_EVLIST_Q_BASE+num;
		qparam->num_q = m_numEvListQid-num;
		qparam->flags |= XNL_F_QDIR_C2H;
		qparam->sflags |= 1 << QPARM_DIR;
		qdma_q_stop(&xcmd);

		memset(&xcmd, 0, sizeof(xcmd));

		xcmd.op = XNL_CMD_Q_DEL;
		xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
		qparam->idx = HEXITEC_EVLIST_Q_BASE+num;
		qparam->num_q = m_numEvListQid-num;
		qparam->flags |= XNL_F_QDIR_C2H;
		qparam->sflags |= 1 << QPARM_DIR;
		
		qdma_q_del(&xcmd);
		m_numEvListQid = num;
	}
	else
	{
	/* Try to make a queue for ST transfer, Card to Host only */	
		for (i=m_numEvListQid; i<num; i++)
		{
			qid = i + HEXITEC_EVLIST_Q_BASE;
			/* Try opening existing queue and if it exists use it. */
			sprintf(deviceName, "/dev/qdma%02X%02X%X-ST-%d", m_busNum, m_devNum, m_funcNum, qid);
			m_evListFd[i] = open(deviceName, O_RDWR);
			if (m_evListFd[i] < 0)
			{
				/* Does not exist, so create it */
				memset(&xcmd, 0, sizeof(xcmd));
				xcmd.log_msg_dump = qdma_dump_response;
				printf("Trying to build queue id %d for %d\n", HEXITEC_EVLIST_Q_BASE+m_numEvListQid, num-m_numEvListQid);
				xcmd.op = XNL_CMD_Q_ADD;

				xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
				qparam->idx = qid;
				qparam->num_q = 1;
				qparam->sflags |= 1 << QPARM_MODE;
				qparam->flags |= XNL_F_QMODE_ST;
				qparam->flags |= XNL_F_QDIR_C2H;
				qparam->sflags |= 1 << QPARM_DIR;
				rv=qdma_q_add(&xcmd);
				if (rv < 0)
					throw XDmaException("QDma::createEvListQueues: Cannot create event list queue %d", qid);

				/* now start the ST queue */
				memset(&xcmd, 0, sizeof(xcmd));

				xcmd.log_msg_dump = qdma_dump_response;
				xcmd.op = XNL_CMD_Q_START;
				xcmd.if_bdf = m_busNum<<12 | m_devNum<<4 | m_funcNum;
				qparam->idx = qid;
				qparam->num_q = 1;
				qparam->flags |= XNL_F_QDIR_C2H;
				qparam->sflags |= 1 << QPARM_DIR;
				qparam->fetch_credit = Q_ENABLE_C2H_FETCH_CREDIT;
				qparam->flags |= (XNL_F_CMPL_STATUS_EN | XNL_F_CMPL_STATUS_ACC_EN | XNL_F_CMPL_STATUS_PEND_CHK | XNL_F_CMPL_STATUS_DESC_EN | XNL_F_FETCH_CREDIT | XNL_F_PFETCH_EN);
				qparam->cmpt_entry_size = XNL_ST_C2H_CMPT_DESC_SIZE_16B; // 128 bit completion
				qparam->cmpt_trig_mode = 1; // Trig every desc. Perhaps can change this to use timeout and/or packet count.
				qparam->qrngsz_idx = 15;  // Ring size 16384
				qparam->sflags |= 1 << QPARM_RNGSZ_IDX;
				qparam->c2h_bufsz_idx = 0; // Buf size 4096 = 1 page so scatter gather can always work
				qparam->sflags |= 1 << QPARM_C2H_BUFSZ_IDX;

				if ((rv=qdma_q_start(&xcmd)) < 0)
					throw XDmaException("QDma::createEvListQueues: Cannot start event list queue %d", qid);
			
				sprintf(deviceName, "/dev/qdma%02X%02X%X-ST-%d", m_busNum, m_devNum, m_funcNum, qid);
				m_evListFd[i] = open(deviceName, O_RDWR);
				if (m_evListFd[i] < 0)
					throw XDmaException("QDma::createEvListQueues: Cannot open path to event list queues %d device name=%d, errno=%d", qid, deviceName, errno);
			}
		}
		m_numEvListQid = num;
	}
}

void QDma::readStream(char *buffer, uint64_t numBytes, int dmaChan)
{
	ssize_t rc;
	uint64_t count = 0;
	char *buf = (char *)buffer;

	while (count < numBytes)
	{
		uint64_t bytes = numBytes - count;

		if (bytes > RW_MAX_SIZE)
			bytes = RW_MAX_SIZE;


		/* read data from file into memory buffer */
		rc = read(m_c2h_fd[1], buf, bytes);
		if (rc < 0)
			throw XDmaException("readStream: read 0x%lx failed %ld.\n", bytes, rc);

		count += rc;
		if (rc != bytes)
			fprintf(stderr, "readStream, read underflow 0x%lx/0x%lx\n", rc, bytes);
		buf += rc;
	}

	if (count != numBytes)
		throw XDmaException("readStream: read  underflow 0x%lx/0x%lx.\n", count, numBytes);
}
