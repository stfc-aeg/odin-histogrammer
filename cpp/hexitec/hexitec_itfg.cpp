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

#include "xdma_hexitec.h"

void XDmaHexitec::iTfgDisable()
{
	setGlobReg(HEXITEC_GLB_ITFG_CONTROL, 0);
}

void XDmaHexitec::iTfgTrigger()
{
	uint32_t reg = getGlobReg(HEXITEC_GLB_ITFG_CONTROL);
	
	setGlobReg(HEXITEC_GLB_ITFG_CONTROL, reg & ~HEXITEC_ITFG_CONT_TRIG);
	setGlobReg(HEXITEC_GLB_ITFG_CONTROL, reg | HEXITEC_ITFG_CONT_TRIG);
}

void XDmaHexitec::iTfgSetup(HexitecITfgMode mode, int extTrigSrc, bool invertExtTrig, uint32_t inpFramesPerTF, uint64_t numTF, uint32_t numCycles)
{
	uint32_t regs[5];
	regs[0] = HEXITEC_ITFG_CONT_SET_MODE(mode) | HEXITEC_ITFG_CONT_ENB | HEXITEC_ITFG_CONT_SET_SRC(extTrigSrc);
	if (invertExtTrig)
		regs[0] |= HEXITEC_ITFG_CONT_FALLING;
	regs[1] = inpFramesPerTF;
	regs[2] = (uint32_t)(numTF & 0xFFFFFFFFL);
	regs[3] = (uint32_t)(numTF >> 32);
	regs[4] = numCycles;
	writeGlobRegs(HEXITEC_GLB_ITFG_CONTROL, 5, regs);
}

void XDmaHexitec::iTfgReadStatus(HexitecITfgStat & stat)
{
	uint32_t regs[4];
	readGlobRegs(HEXITEC_GLB_RD_ITFG_STATUS, 4, regs);

	stat.status    = regs[0];
	stat.inpFrame  = regs[1];
	stat.timeFrame = regs[2];
	stat.cycles    = regs[3];
}
