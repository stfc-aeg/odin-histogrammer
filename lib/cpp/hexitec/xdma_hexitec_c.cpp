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

using namespace std;
#define HEXITEC_MAX_ERROR_MSG 1024

static char errMsg[HEXITEC_MAX_ERROR_MSG+2];

extern "C" void * hexitec_open(int useQDma, int busNum, int devNum, int funcNum)
{
	XDmaHexitec *handle;
	try 
	{
		handle = new XDmaHexitec(useQDma, busNum, devNum, funcNum);
	} 
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return NULL;
	}
	return (void *)handle;
}
extern "C" void hexitec_close(void *handle)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	
	delete hexitec;
}

extern "C" int hexitec_getHistFormat(void *handle, int chip, int *histFormatP, int *mappedModeP, int *histShiftP)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->getHistFormat(chip, histFormatP, mappedModeP, histShiftP);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}	


extern "C" int hexitec_readHistEngRowColTime(void *handle, int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->readHistEngRowColTime(numEng, firstRow, numRows, firstCol, numCols, firstTF, numTF, chip, dmaChan, data);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}
extern "C" int hexitec_readHistEngColRowTime(void *handle, int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->readHistEngColRowTime(numEng, firstCol, numCols, firstRow, numRows, firstTF, numTF, chip, dmaChan, data);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}

extern "C"  char * hexitec_get_error_message()
{
	return errMsg;
}
extern "C" int hexitec_setDefaultXDmaChan(void *handle, int dmaChan)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->setDefaultXDmaChan(dmaChan);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}
extern "C" int hexitec_setDmaDescRWChan(void *handle, int dmaChan)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->setDmaDescRWChan(dmaChan);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}

extern "C" int hexitec_getNumChips(void *handle)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	return hexitec->getNumChips();
}
extern "C" int hexitec_getnBinsEng(void *handle, int chip)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	int nbins;
	try 
	{
		nbins = hexitec->getnBinsEng(chip);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return nbins;
}
extern "C" int hexitec_getNumTF(void *handle, int chip)
{
	int numTF;
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		numTF = hexitec->getNumTF(chip);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return numTF;
}
extern "C" int hexitec_getNumTFMapped(void *handle, int chip)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	int numTFMapped;
	try 
	{
		numTFMapped = hexitec->getNumTFMapped(chip);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return numTFMapped;
}
extern "C" int hexitec_getnBinsClustClass(void *handle, int chip)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	int numCC;
	try 
	{
		numCC = hexitec->getnBinsClustClass(chip);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return numCC;
}

extern "C" int hexitec_getUsePosn(void *handle, int chip)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	bool usePosn;
	try 
	{
		usePosn = hexitec->getUsePosn(chip);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return (int)usePosn;
}

extern "C" int hexitec_readHistEngCalibClass(void *handle, int firstEng, int numEng, int firstLutAddr, int numLutAddr, int firstClusterClass, int numClusterClass, int chip, int dmaChan, uint32_t *data)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->readHistEngCalibClass(firstEng, numEng, firstLutAddr, numLutAddr, firstClusterClass, numClusterClass, chip, dmaChan, data);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}

extern "C" int hexitec_readHistCharac2d(void *handle, int firstMain, int numMain, int firstNeb, int numNeb, int diag, int chip, int dmaChan, uint32_t *data)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->XDmaHexitec::readHistCharac2d(firstMain, numMain, firstNeb, numNeb, diag, chip, dmaChan, data);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}
extern "C" int hexitec_getnBinsCharac(void *handle, int chip, int nBins[4])
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->XDmaHexitec::getnBinsCharac(chip, nBins);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}


extern "C" int hexitec_getNumChipCols(void *handle)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	int numChipCols;
	try 
	{
		numChipCols =  hexitec->getNumChipCols();
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return numChipCols;
}
extern "C" int hexitec_getNumChipRows(void *handle)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	int numChipRows;
	try 
	{
		numChipRows = hexitec->getNumChipRows();
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return numChipRows;
}
extern "C" int hexitec_readMappedEngColRowTime(void *handle, int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data, int useReorder)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->readMappedEngColRowTime(numEng, firstCol, numCols, firstRow, numRows, firstTF, numTF, chip, dmaChan, data, useReorder!=0);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}

extern "C" int hexitec_readHistEngTime(void *handle, int numEng, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->readHistEngTime(numEng, firstTF, numTF, chip, dmaChan, data, true);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}
extern "C" int hexitec_readHistEngCCTime(void *handle, int numEng, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->readHistEngCCTime(numEng, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, true);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}

extern "C" int hexitec_readHistEngColRowCCTime(void *handle, int numEng, int firstCol, int numCols, int firstRow, int numRows, int firstCC, int numCC, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data)
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->readHistEngColRowCCTime(numEng, firstCol, numCols, firstRow, numRows, firstCC, numCC, firstTF, numTF, chip, dmaChan, data, true);
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}

#if 0
extern "C" int hexitec_
{
	XDmaHexitec * hexitec = (XDmaHexitec*) handle;
	try 
	{
		hexitec->
	}
	catch (exception& e)
	{
		strncpy(errMsg, e.what(), HEXITEC_MAX_ERROR_MSG);
		return -1;
	}
	return 0;
}
	
	void readMappedEngRowColTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);

	void writeChipRegs(int chip, int region, int offset, int num, uint32_t *data);
	void readChipRegs(int chip, int region, int offset, int num, uint32_t *data);
	void setChipReg(int chip, int offset, uint32_t value);
	uint32_t getChipReg(int chip, int offset);
	void writeGlobRegs(int offset, int num, uint32_t *data);
	void readGlobRegs(int offset, int num, uint32_t *data);
	void setGlobReg(int offset, uint32_t value);
	uint32_t getGlobReg(int offset);
	void setPixelLUT(int chip, int region, int firstRow, int numRow, int firstCol, int numCol, uint32_t value);
	void writePixelLUT(int chip, int region, int firstRow, int numRow, int firstCol, int numCol, uint32_t *value);
	void readPixelLUT(int chip, int region, int firstRow, int numRow, int firstCol, int numCol, uint32_t *value);
	void writeSharedLUT(int chip, int region, int stream, int first, int num, uint32_t *data);
	void readSharedLUT(int chip, int region, int stream, int first, int num, uint32_t *data);
	
	void dmaReset(uint32_t streamMask);
	int dmaBuildDesc(int stream, int srcStream, int flags, uint64_t byteOffset, uint64_t numBytes, uint32_t maxBlockBytes);
	void dmaStart(uint32_t streamMask, int firstDescIn, int numDescIn, int options);
	uint32_t dmaStop(uint32_t streamMask);
	uint32_t dmaReadStatus(int stream);
	uint64_t dmaReadCurrDesc(int stream);
	void dmaWaitIdle(int streamMask, double timeOut);
	int dmaPrintDesc(int stream, int firstDesc, int numDesc);
	int getMaxPbFrames();
	void writeDmaBuff(int stream, uint64_t offset, uint64_t numBytes, char * ptr);

	void setBaselineMode(int chip, int maskMode, int divideCode, bool enbDither);
	void loadBaseline(int chip, HexitecLoadSaveBaseLine waitMode );
	void waitLoadBaseline(int chip);
	void saveBaseline(int chip, HexitecLoadSaveBaseLine waitMode );
	void setMainTriggerThres(int chip, int firstRow, int numRows, int firstCol, int numCols, int pos, int neg, bool enable);
	void setLowerTriggerThres(int chip, int firstRow, int numRows, int firstCol, int numCols, int pos, int neg);
	void setLinearityRaw(int chip, int firstRow, int numRows, int firstCol, int numCols, int a, int b, int c);
	void setLinearityOne(int chip, double offsetADUs=0.0);
	void setClusterMode(int chip, int clusterMode);
	void setCShareMode(int chip, bool enbEdgePos, bool enbNegNeb, bool disSumming, bool disAdjPosn);
	void setClusterTypes(int chip, int enbClusterType);
	void setHistFormat(int chip, int histFormat, int mappedMode);
	
	void enableHist();
	void readHistEngRowColTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);
	void readMappedEngRowColTime(int numEng, int firstRow, int numRows, int firstCol, int numCols, int firstTF, int numTF, int chip, int dmaChan, uint32_t *data);
	void clearHistAll();

#endif
