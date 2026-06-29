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

XDmaMMap::XDmaMMap()
{
	m_devName="Unused";
	m_fd = -1;
	m_len = 0;
	m_base = NULL;
}

XDmaMMap::XDmaMMap(const char *deviceName, size_t size)
{
	struct stat sbuf;
	m_devName = deviceName;
	m_fd = -1;
	m_base = (uint32_t *)-1;
	m_len = size;
	if ((m_fd = open(deviceName, O_RDWR | O_SYNC)) == -1)
//		cout << "Cannot open device " << deviceName << ": " << strerror(errno) << endl;
		throw XDmaException("Cannot open device %s, errno=%d", deviceName, errno);

#if 0
// Unfortunately the size returns 0, so I don't know how to determine the size automatically.
	if (fstat(m_fd,&sbuf) < 0)
//		cout << "Cannot stat device " << deviceName << ": " << strerror(errno) << endl;
		throw XDmaException("Cannot stat %s", deviceName);
	cout << "Size=" << sbuf.st_size << endl;
#endif
	m_base = (uint32_t *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
	cout << "Mapped "<<deviceName <<" to address " << m_base << endl;
	if (m_base == (uint32_t *)-1)
//		cout << "Cannot mmap device " << deviceName << ": " << strerror(errno) << endl;
		throw XDmaException("Cannot mmap %s", deviceName);
}
XDmaMMap::~XDmaMMap()
{
	cout << "~XDmaMMap:Closing device " << m_devName << " fd=" << m_fd << endl;
	if (m_base != (uint32_t *)-1 && m_base )
	{
		cout << ".... Un-mapping memory @ " << m_base <<", size=" << m_len << endl;
		munmap(m_base, m_len);
	}
	m_base = (uint32_t *)-1;
	if (m_fd >= 0)
		close(m_fd);
	m_fd = -1;
}

XDmaMMap & XDmaMMap::operator= (XDmaMMap && src)
{
//	cout << "XDmaMMap: using move assignment operator" << endl;
	if (this != &src)
	{
		if (m_base != (uint32_t *)-1)
			munmap(m_base, m_len);
		if (m_fd >= 0)
		{
//			cout << "XDmaMMap: move assignment operator closing path " << m_fd << endl;
			close(m_fd);
		}
		this->m_devName = move(src.m_devName);
		this->m_fd = src.m_fd;
		this->m_len = src.m_len;
		this->m_base = src.m_base;
		src.m_base = (uint32_t *)-1;
		src.m_fd = -1;
	}
	return *this;
}

XDma :: XDma()
{
	int i;
	m_devName= "Uninitialised";
	m_hasMemBAR = 0;
	m_histMemBase = nullptr;
	for (i=0;i<XDMA_NUM_EVENT_IRQS; i++)
		m_eventFd[i] = -1;
//	cout << "XDma() making blank XDma, m_devName=" << m_devName <<endl; 
}
XDma :: XDma(int devNum, size_t userSize)
{
	char deviceName[100];
	volatile uint32_t *p;
	uint32_t regs[5];
	int i;
	m_devNum = devNum;
	m_devName = "/dev/xdma"+to_string(devNum);
	m_num_h2c = XDmaHbmHistMaxChannels;
	m_num_c2h = XDmaHbmHistMaxChannels;
	for (i=0;i<XDmaHbmHistMaxChannels;i++)
		m_h2c_fd[i] = m_c2h_fd[i] = -1;
	
	sprintf(deviceName, "%s_user", m_devName.c_str());
	m_regsBAR = move(XDmaMMap(deviceName, userSize)) ;

	sprintf(deviceName, "%s_bypass", m_devName.c_str());
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
	
	for (i=0;i<m_num_c2h; i++)
	{
		sprintf(deviceName, "%s_c2h_%d", m_devName.c_str(), i);
		m_c2h_fd[i] = open(deviceName, O_RDWR);
		if (m_c2h_fd[i] <0)
			throw XDmaException("Cannot open device %s, errno=%d", deviceName, errno);
		if (ioctl(m_c2h_fd[i], IOCTL_XDMA_ALIGN_GET, m_c2h_alignment+i) < 0)
			throw XDmaException("Cannot ioctl device %s, to get alignment, errno=%d", deviceName, errno);
//		cout << "c2h_aligment["<<i<<"] = " << m_c2h_alignment[i] << endl;
	}
	for (i=0;i<m_num_h2c; i++)
	{
		sprintf(deviceName, "%s_h2c_%d", m_devName.c_str(), i);
		m_h2c_fd[i] = open(deviceName, O_RDWR);
		if (m_h2c_fd[i] <0)
			throw XDmaException("Cannot open device %s, errno=%d", deviceName, errno);
		if (ioctl(m_h2c_fd[i], IOCTL_XDMA_ALIGN_GET, m_h2c_alignment+i) < 0)
			throw XDmaException("Cannot ioctl device %s, to get alignment, errno=%d", deviceName, errno);
		cout << "h2c_aligment["<<i<<"] = " << m_h2c_alignment[i] << endl;
	}
	for (i=0;i<XDMA_NUM_EVENT_IRQS; i++)
	{
		sprintf(deviceName, "%s_events_%d", m_devName.c_str(), i);
		m_eventFd[i] = open(deviceName,  O_RDWR);
	}
}
XDma :: ~XDma()
{
	int i;
	cout <<"~Xdma() : Closing files\n" << endl; 
	for (i=0;i<m_num_c2h; i++)
	{
		if (m_c2h_fd[i] >= 0)
			close (m_c2h_fd[i]);
	}
	for (i=0;i<m_num_h2c; i++)
	{
		if (m_h2c_fd[i] >= 0)
			close (m_h2c_fd[i]);
	}
	for (i=0;i<XDMA_NUM_EVENT_IRQS; i++)
	{
		if (m_eventFd[i] >= 0)
			close(m_eventFd[i]);
	}
}

#if 1
XDma & XDma::operator= (XDma && src)
{
	int i;
	printf ("XDma move operator this=%p, src=%p\n", this, &src);
	if (this != &src)
	{
		/*if (m_base != (uint32_t *)-1)
			munmap(m_base, m_len);
		if (m_fd >= 0)
			close(m_fd); */
		
		this->m_devName = move(src.m_devName); // Don't why this does not work when called from XDmaHexitec constructor
		this->m_busNum = src.m_busNum;
		this->m_devNum = src.m_devNum;
		this->m_funcNum = src.m_funcNum;
		this->m_num_h2c = src.m_num_h2c;
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
		for (i=0;i<XDMA_NUM_EVENT_IRQS; i++)
			this->m_eventFd[i] = src.m_eventFd[i];
	}
	return *this;
}
#endif

void XDma::readDma(char *buffer, uint64_t AXIAddress, uint64_t numBytes, int dmaChan)
{
	if (m_isQdma)
		dmaChan = 0;	// For QDMA, each instance of QDMA makes it own MM Queue and opens as m_c2h_fd[0]
	try
	{
		if (((uintptr_t)buffer) % 4096 == 0 && AXIAddress % m_c2h_alignment[dmaChan] == 0)
		{
			readAlignedDma(buffer, AXIAddress, numBytes, dmaChan);
		}
		else
		{
			char *allocated;
			int misalign = AXIAddress % m_c2h_alignment[dmaChan];
			posix_memalign((void **)&allocated, 4096 /*alignment */ , numBytes + m_c2h_alignment[dmaChan]+4096);
			if (allocated == 0)			
				throw XDmaException("readDma: Out of memory" );
			readAlignedDma(allocated+misalign, AXIAddress, numBytes, dmaChan);
			memcpy(buffer, allocated+misalign, numBytes);
			free(allocated);
		}
	}
	catch (std::exception& e)
	{
		throw XDmaException("readDma: %s", e.what());
	}
}
void XDma::writeDma(char *buffer, uint64_t AXIAddress, uint64_t numBytes, int dmaChan)
{
	if (m_isQdma)
		dmaChan = 0;	// For QDMA, each instance of QDMA makes it own MM Queue and opens as m_c2h_fd[0]
	try
	{
		if (((uintptr_t)buffer) % 4096 == 0 && AXIAddress % m_h2c_alignment[dmaChan] == 0)
			writeAlignedDma(buffer, AXIAddress, numBytes, dmaChan);
		else
		{
			char *allocated;
			int misalign = AXIAddress % m_h2c_alignment[dmaChan];
			posix_memalign((void **)&allocated, 4096 /*alignment */ , numBytes + m_h2c_alignment[dmaChan]+4096);
			if (allocated == 0)			
				throw XDmaException("writeDma: Out of memory" );
			memcpy(allocated+misalign, buffer, numBytes);
			writeAlignedDma(allocated+misalign, AXIAddress, numBytes, dmaChan);
			free(allocated);
		}
	}
	catch (std::exception& e)
	{
		throw XDmaException("writeStreamDma: %s", e.what());
	}
}

void XDma::readAlignedDma(char *buffer, uint64_t AXIAddress, uint64_t numBytes, int dmaChan)
{
	ssize_t rc;
	uint64_t count = 0;
	char *buf = (char *)buffer;
	off_t offset = AXIAddress;

	if (m_isQdma)
		dmaChan = 0;	// For QDMA, each instance of QDMA makes it own MM Queue and opens as m_c2h_fd[0]

	while (count < numBytes)
	{
		uint64_t bytes = numBytes - count;

		if (bytes > RW_MAX_SIZE)
			bytes = RW_MAX_SIZE;

/*		rc = lseek(m_c2h_fd[dmaChan], offset, SEEK_SET);
		if (rc != offset)
			throw XDmaException("readAlignedDma: dmaChan=%d, fd=%d, seek off 0x%lx != 0x%lx. Errno=%d", dmaChan, m_c2h_fd[dmaChan], rc, offset, errno);
*/
		//printf("readAlignedDma: reading from 0x%010lX for 0x%08lx\n", offset, bytes);
		/* read data from file into memory buffer */
		rc = pread(m_c2h_fd[dmaChan], buf, bytes, offset);
		if (rc < 0)
			throw XDmaException("readAlignedDma: read 0x%lx @ 0x%lx failed, errno=%d.\n", bytes, offset, errno);

		count += rc;
		if (rc != bytes)
			fprintf(stderr, "readAlignedDma, read underflow 0x%lx/0x%lx @ 0x%lx.\n", rc, bytes, offset);
		buf += bytes;
		offset += bytes;
	}

	if (count != numBytes)
		throw XDmaException("readAlignedDma: read  underflow 0x%lx/0x%lx.\n", count, numBytes);
}

void XDma::writeAlignedDma(char *buffer,  uint64_t AXIAddress, uint64_t numBytes, int dmaChan)
{
	ssize_t rc;
	uint64_t count = 0;
	char *buf = (char *)buffer;
	off_t offset = AXIAddress;

	if (m_isQdma)
		dmaChan = 0;	// For QDMA, each instance of QDMA makes it own MM Queue and opens as m_c2h_fd[0]

	while (count < numBytes)
	{
		uint64_t bytes = numBytes - count;

		if (bytes > RW_MAX_SIZE)
			bytes = RW_MAX_SIZE;

/*		rc = lseek(m_h2c_fd[dmaChan], offset, SEEK_SET);
		if (rc != offset)
			throw XDmaException("writeAlignedDma: seek off 0x%lx != 0x%lx. Errno=%d", rc, offset, errno);
*/
		/* read data from file into memory buffer */
		int attempt=0;
		do
		{
			rc = pwrite(m_h2c_fd[dmaChan], buf, bytes, offset);
			if (rc < 0)
			{
				printf("writeAlignedDma: write 0x%lx @ 0x%lx of total 0x%lX @ 0x%lx failed, errno=%d, attempt=%d.\n", bytes, offset, numBytes, AXIAddress, errno, attempt);
				if (++attempt > 3)
					throw XDmaException("writeAlignedDma: write 0x%lx @ 0x%lx of total 0x%lX @ 0x%lx failed, errno=%d", bytes, offset, numBytes, AXIAddress, errno);
			}
		} while (rc < 0);

		count += rc;
		if (rc != bytes)
			fprintf(stderr, "writeAlignedDma, write underflow 0x%lx/0x%lx @ 0x%lx.\n", rc, bytes, offset);
		buf += bytes;
		offset += bytes;
	}

	if (count != numBytes)
		throw XDmaException("readAlignedDma: write  underflow 0x%lx/0x%lx.\n", count, numBytes);
}

void XDma::readStream(char *buffer, uint64_t numBytes, int dmaChan)
{
	throw XDmaException("readStream is not supported with XDMA version");
}

int XDma::getEventFd(int irqNum)
{
	if (irqNum < 0 || irqNum >= XDMA_NUM_EVENT_IRQS)
		throw XDmaException("getEventFd: irqNum=%d is out of range 0...%d", irqNum, XDMA_NUM_EVENT_IRQS-1);
	return m_eventFd[irqNum];
}
