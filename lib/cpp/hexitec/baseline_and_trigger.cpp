/* baseline_and_trigger.cpp
Commands to setup the baseline, trigger and cluster finding blocks
*/


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

/**
	Setup baseline subtraction feedback and tracking features

@param chip			Chip number or -1 to duplicate to all chips.
@param maskMode		The mask mode controls when the error signal from the subtracted baseline is fed back to update the baseline estimate. See HEXITEC_BSUB_MASK_DEFS
@param divideCode 	Sets the scaling (division) applied to the error from 0 applied to adjust the baseline estimate. See HEXITEC_BSUB_DIVIDE_DEFS 
@param enbDither	Enable dither (ramping bits below binary point) used in linearity correction.
*/
void XDmaHexitec::setBaselineMode(int chip, int maskMode, int divideCode, bool enbDither, bool useAbsTrig)
{
	uint32_t value;
	
	if (maskMode<0 || maskMode > 15)
		throw  XDmaHexitecException("setBaselineMode: maskMode=%d not in range 0..15", maskMode);
	if (divideCode<0 || divideCode > 15)
		throw  XDmaHexitecException("setBaselineMode: divideCode=%d not in range 0..15", divideCode);

	value = HEXITEC_BSUB_SET(maskMode, divideCode, enbDither);
	if (useAbsTrig)
		value |= HEXITEC_BSUB_USE_ABS_TRIG;
	setChipReg(chip, HEXITEC_CHIP_BASESUB, value);
}

void XDmaHexitec::loadBaseline(int chip, HexitecLoadSaveBaseLine waitMode )
{
	int firstChip, lastChip;
	uint32_t baselineReg;
	int c;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("loadBaseline: chip=%d not in range 0..%d", chip, m_numChips-1);
	else
		firstChip = lastChip = chip;
	if (waitMode == UseShortBurst)
		setGlobReg(HEXITEC_GLB_RUN_REG,  0);
	for (c=firstChip; c<=lastChip; c++)
	{
		baselineReg = getChipReg(c, HEXITEC_CHIP_BASESUB);
		setChipReg(c, HEXITEC_CHIP_BASESUB, baselineReg & ~HEXITEC_BSUB_LOAD );
		setChipReg(c, HEXITEC_CHIP_BASESUB, baselineReg | HEXITEC_BSUB_LOAD );
	}
	if (waitMode == UseShortBurst)
	{
		uint32_t dataPath = getGlobReg(HEXITEC_GLB_DATA_PATH);
		dataPath |= HEXITEC_DATA_PATH_SHORT_BURST_MODE;
		setGlobReg(HEXITEC_GLB_DATA_PATH, dataPath);
		setGlobReg(HEXITEC_GLB_FRAME_BURST_LENGTH, 2);
		setGlobReg(HEXITEC_GLB_RUN_REG,  1);
		waitLoadBaseline(chip);
		setGlobReg(HEXITEC_GLB_RUN_REG,  0);
		dataPath &= ~HEXITEC_DATA_PATH_SHORT_BURST_MODE;
		setGlobReg(HEXITEC_GLB_DATA_PATH, dataPath);
	}
	else if (waitMode == RequestAndWait)
	{
		waitLoadBaseline(chip);
	}
}	

void XDmaHexitec::waitLoadBaseline(int chip)
{
	uint64_t status, mask;
	int timeout=20000;
	
	if (chip < 0)
		mask = 0xFFFFFFFFFFFFFFFL;
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("waitLoadBaseline: chip=%d not in range 0..%d", chip, m_numChips-1);
	else
		mask = 1L << chip;
	
	do
	{
		status = getGlobReg64(HEXITEC_GLB_LOADING_BL);
		if ((status & mask) == 0)
			break;
		this_thread::sleep_for (chrono::milliseconds(1));	

	} while (--timeout > 0);
	if (timeout == 0)
		cout << "waitLoadBaseline: timeout waiting for load Baseline to run on chip=" << chip << ", Status=" << status << endl;

//	throw  XDmaHexitecException("waitLoadBaseline: timeout waiting for load Baseline to run on chip=%d, Status = 0x%lX", chip, status);
}	

		
void XDmaHexitec::saveBaseline(int chip, HexitecLoadSaveBaseLine waitMode  )
{
	int firstChip, lastChip;
	uint32_t baselineReg;
	int c;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("saveBaseline: chip=%d not in range 0..%d", chip, m_numChips-1);
	else
		firstChip = lastChip = chip;
	
	if (waitMode == UseShortBurst)
		setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_DIS_RESET_FRAME_COUNT);
	for (c=firstChip; c<=lastChip; c++)
	{
		baselineReg = getChipReg(c, HEXITEC_CHIP_BASESUB);
		setChipReg(c, HEXITEC_CHIP_BASESUB, baselineReg & ~HEXITEC_BSUB_SAVE );
		setChipReg(c, HEXITEC_CHIP_BASESUB, baselineReg | HEXITEC_BSUB_SAVE );
	}
	if (waitMode == UseShortBurst)
	{
		uint32_t dataPath = getGlobReg(HEXITEC_GLB_DATA_PATH);
		dataPath |= HEXITEC_DATA_PATH_SHORT_BURST_MODE;
		setGlobReg(HEXITEC_GLB_DATA_PATH, dataPath);
		setGlobReg(HEXITEC_GLB_FRAME_BURST_LENGTH, 2);
		setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_RUN | HEXITEC_RUN_DIS_RESET_FRAME_COUNT);
		waitSaveBaseline(chip);
		setGlobReg(HEXITEC_GLB_RUN_REG,  HEXITEC_RUN_DIS_RESET_FRAME_COUNT);
		dataPath &= ~HEXITEC_DATA_PATH_SHORT_BURST_MODE;
		setGlobReg(HEXITEC_GLB_DATA_PATH, dataPath);
	}
	else if (waitMode == RequestAndWait)
	{
		waitSaveBaseline(chip);
	}
}	

void XDmaHexitec::waitSaveBaseline(int chip)
{
	uint64_t status, mask;
	int timeout=1000000;
	
	if (chip < 0)
		mask = 0xFFFFFFFFFFFFFFFL;
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("waitSaveBaseline: chip=%d not in range 0..%d", chip, m_numChips-1);
	else
		mask = 1L << chip;
	
	do
	{
		status = getGlobReg64(HEXITEC_GLB_SAVING_BL);
		if ((status & mask) == 0)
			break;
	} while (timeout-- > 0);
	if (timeout == 0)
		throw  XDmaHexitecException("waitSaveBaseline: timeout waiting for save Baseline to run on chip=%d, Status = 0x%lX", chip, status);
}	


/**
	Write a fixed value to Hexitec Absolute trigger thresholds.
	This threshold is in ADUs and is compared against the ADC value before it has had baseline subtraction. It is used before the baseline has settled or after an errant
	event which causes the baseline to get lost such that the normal update does not occur.

@param chip			Chip number  or -1 to duplicate to all chips.
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param thres		Positive going threshold in ADC units.
*/

void XDmaHexitec::setAbsTriggerThres(int chip, int firstCol, int numCols, int firstRow, int numRows, int highThres, int lowThres)
{
	uint32_t value;
	int firstChip, lastChip;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("setAbsTriggerThres: chip=%d out of range 0 to %d", chip, m_numChips-1);		
	else
		firstChip = lastChip = chip;

	if (highThres > HEXITEC_ABS_THRES_MAX || highThres < 0 || lowThres > HEXITEC_ABS_THRES_MAX || lowThres <0 || lowThres > highThres)
		throw  XDmaHexitecException("setAbsTriggerThres: highThres=%d or lowThres=%d out of range %d to %d", highThres, lowThres, 0, HEXITEC_ABS_THRES_MAX);

	if (m_generation == HexitecGenMHz)
		value = HEXITEC_ABS_THRES_MHZ(highThres, lowThres);
	else
		value = HEXITEC_ABS_THRES_HXT(highThres, lowThres);
	setPixelLUT(chip, HEXITEC_REGION_ABS_THRES, firstCol, numCols, firstRow, numRows, value);
	
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (numCols == HEXITEC_NUM_COLS && numRows == HEXITEC_NUM_ROWS)
		{
			m_absTrigHighName[chip] = std::string("fixed value=")+to_string(highThres);
			m_absTrigLowName[chip] = std::string("fixed value=")+to_string(lowThres);
		}
		else 
		{
			char newValues[100];
			sprintf(newValues, ", (%d, %d) for (%d,%d) = %d", firstCol, firstRow, numCols, numRows, highThres);
			m_absTrigHighName[chip] += std::string(newValues);
			sprintf(newValues, ", (%d, %d) for (%d,%d) = %d", firstCol, firstRow, numCols, numRows, lowThres);
			m_absTrigLowName[chip] += std::string(newValues);
		}
	}
}
/**
	Write a fixed value to Hexitec Main trigger thresholds

@param chip			Chip number  or -1 to duplicate to all chips.
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param pos			Positive going threshold (0...HEXITEC_THRES_MAX)
@param neg			Negative going threshold (HEXITEC_THRES_MIN...0)
@param enable		Enable trigger on this pixel
*/

void XDmaHexitec::setMainTriggerThres(int chip, int firstCol, int numCols, int firstRow, int numRows, int pos, int neg, bool enable)
{
	uint32_t value;
	int firstChip, lastChip;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("setMainTriggerThres: chip=%d out of range 0 to %d", chip, m_numChips-1);		
	else
		firstChip = lastChip = chip;
	
	if (pos > HEXITEC_THRES_MAX || pos < 0 || neg >0 || neg < HEXITEC_THRES_MIN)
		throw  XDmaHexitecException("setMainTriggerThres: Threshold pos=%d or neg=%d out of range %d to %d", pos, neg, HEXITEC_THRES_MIN, HEXITEC_THRES_MAX);

	if (m_generation == HexitecGenMHz)
		value = HEXITEC_MTHRES_MHZ(enable, pos, neg);
	else
		value = HEXITEC_MTHRES_HXT(enable, pos, neg);
	printf("Setting main thres = 0x%08X\n", value);
	setPixelLUT(chip, HEXITEC_REGION_MTHRES, firstCol, numCols, firstRow, numRows, value);
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (numCols == HEXITEC_NUM_COLS && numRows == HEXITEC_NUM_ROWS)
		{
			m_mainTrigPosName[chip] = std::string("fixed value=")+to_string(pos);
			m_mainTrigNegName[chip] = std::string("fixed value=")+to_string(neg);
			m_trigEnableName[chip] =  std::string("fixed value=")+to_string(enable);
		}
		else 
		{
			char newValues[100];
			sprintf(newValues, ", (%d, %d) for (%d,%d) = %d", firstCol, firstRow, numCols, numRows, pos);
			m_mainTrigPosName[chip] += std::string(newValues);
			sprintf(newValues, ", (%d, %d) for (%d,%d) = %d", firstCol, firstRow, numCols, numRows, neg);
			m_mainTrigNegName[chip] += std::string(newValues);
			sprintf(newValues, ", (%d, %d) for (%d,%d) = %d", firstCol, firstRow, numCols, numRows, enable);
			m_trigEnableName[chip] += std::string(newValues);
		}
	}
}
/**
	Write a fixed value to Hexitec Lower level trigger thresholds

@param chip			Chip number  or -1 to duplicate to all chips.
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param pos			Positive going threshold (0...HEXITEC_THRES_MAX)
@param neg			Negative going threshold (HEXITEC_THRES_MIN...0)
*/
void XDmaHexitec::setLowerTriggerThres(int chip, int firstCol, int numCols, int firstRow, int numRows, int pos, int neg)
{
	uint32_t value;
	int firstChip, lastChip;
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("setLowerTriggerThres: chip=%d out of range 0 to %d", chip, m_numChips-1);		
	else
		firstChip = lastChip = chip;
	
	if (pos > HEXITEC_THRES_MAX || pos < 0 || neg >0 || neg < HEXITEC_THRES_MIN)
		throw  XDmaHexitecException("setLowerTriggerThres: Threshold pos=%d or neg=%d out of range %d to %d", pos, neg, HEXITEC_THRES_MIN, HEXITEC_THRES_MAX);

	if (m_generation == HexitecGenMHz)
		value = HEXITEC_LTHRES_MHZ(pos, neg);
	else
		value = HEXITEC_LTHRES_HXT(pos, neg);
	printf("Setting lower thres = 0x%08X\n", value);
	setPixelLUT(chip, HEXITEC_REGION_LTHRES, firstCol, numCols, firstRow, numRows, value);

	for (chip=firstChip; chip<=lastChip; chip++)
	{
		if (numCols == HEXITEC_NUM_COLS && numRows == HEXITEC_NUM_ROWS)
		{
			m_lowTrigPosName[chip] = std::string("fixed value=")+to_string(pos);
			m_lowTrigNegName[chip] = std::string("fixed value=")+to_string(neg);
		}
		else 
		{
			char newValues[100];
			sprintf(newValues, ", (%d, %d) for (%d,%d) = %d", firstCol, firstRow, numCols, numRows, pos);
			m_lowTrigPosName[chip] += std::string(newValues);
			sprintf(newValues, ", (%d, %d) for (%d,%d) = %d", firstCol, firstRow, numCols, numRows, neg);
			m_lowTrigNegName[chip] += std::string(newValues);
		}
	}
}

/**
	Write a fixed values to Hexitec Linearity correction tables.
	The correction is quadratic   correct = a*x^2+bx+c
	Fixed point arithmetic is used so the raw 
	A is code coded as a 25 bit signed and could be used positive or negative.
	When A=2^24-1 a is scaled so that 4095*a=3.9999
	B is coded as a 25 bit signed number (though usually positive) so that b_int = b_float*2^22
	This means that the largest value of b_int  = 2^24-1 => b_real = (2^24-1)/2^22 = 3.99999976
	C is coded as an 18 bit signed number with 5 bits below the binary point.  c_int = c_float*32.
The larger value of c is c_float=(2^17-1)/32 = 4095.97

@param chip			Chip number  or -1 to duplicate to all chips.
@param firstCol		First column of sensor 0..HEXITEC_NUM_COLS-1
@param numCols		Number of columns of sensor 1..HEXITEC_NUM_COLS.
@param firstRow		First row of sensor 0..HEXITEC_NUM_ROWS-1
@param numRows		Number of rows of sensor 1..HEXITEC_NUM_ROWS.
@param a			Linearity scaling a 
@param b			Linearity scaling b 
@param c			Linearity scaling c 
*/
void XDmaHexitec::setLinearityRaw(int chip, int firstCol, int numCols, int firstRow, int numRows, int a, int b, int c)
{
	uint32_t value;
	int firstChip, lastChip;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("setLinearityRaw: chip=%d out of range 0 to %d", chip, m_numChips-1);		
	else
		firstChip = lastChip = chip;
	
	if (a > m_linABMax || a < m_linABMin || b > m_linABMax || b < m_linABMin)
		throw  XDmaHexitecException("setLinearityRaw: linearity a=%d or b=%d out of range %d to %d", a, b, m_linABMin, m_linABMax);
	if (c > HEXITEC_LINEARITY_C_MAX || c < HEXITEC_LINEARITY_C_MIN)
		throw  XDmaHexitecException("setLinearityRaw: linearity c=%d out of range %d to %d", c, HEXITEC_LINEARITY_C_MIN, HEXITEC_LINEARITY_C_MAX);
		
	setPixelLin(chip, HEXITEC_REGION_LIN_A, firstCol, numCols, firstRow, numRows, -1, a);
	setPixelLin(chip, HEXITEC_REGION_LIN_B, firstCol, numCols, firstRow, numRows, -1, b);
	setPixelLin(chip, HEXITEC_REGION_LIN_C, firstCol, numCols, firstRow, numRows, -1, c);
	for (chip=firstChip; chip<=lastChip; chip++)
	{
		char newValues[200];
		if (numCols == HEXITEC_NUM_COLS && numRows == HEXITEC_NUM_ROWS)
		{
			sprintf(newValues, "All fixed: a=%d, b=%d, c=%d", a, b, c);
			m_linCorrName[chip] = std::string(newValues);
		}
		else 
		{
			sprintf(newValues, ", (%d, %d) for (%d,%d): a=%d, b=%d, c=%d", firstCol, firstRow, numCols, numRows, a, b, c);
			m_linCorrName[chip] += std::string(newValues);
		}
	}
}

void XDmaHexitec::setLinearityOne(int chip, double offsetADUs)
{
	setLinearityRaw(chip, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, 0, m_linScaleB, (int)(offsetADUs*m_linScaleC));
}

/**
	Setup cluster recognition mode.

@param chip			Chip number or -1 to duplicate to all chips.
@param clusterMode	Cluster recognition mode as specified by HEXITEC_CLUSTER_MODE_DEFS
*/
void XDmaHexitec::setClusterMode(int chip, int clusterMode, int autoTrigRate)
{
	uint32_t value;
	
	if (clusterMode<0 || clusterMode > 7)
		throw  XDmaHexitecException("setClusterMode: clusterMode=%d not in range 0..7", clusterMode);

	if (autoTrigRate<0 || autoTrigRate > 7)
		throw  XDmaHexitecException("setClusterMode: autoTrigRate=%d not in range 0..3", autoTrigRate);

	value = HEXITEC_CLUSTER_MODE_SET(clusterMode) | HEXITEC_CLUSTER_AUTO_RATE(autoTrigRate);
	setChipReg(chip, HEXITEC_CHIP_CLUSTER, value);
}
/** Load linearity correction set to be a simple gain scaling.
The values in the file must read row 0,: col 0...79, row 1: col 0...79 etc.
The values are ASCII doubles around 1.0, typically 1.2 for Hexitec
@param chip The chip number
@param fullName, the full path name to the file
*/
void XDmaHexitec::loadLinearityGainAscii(int chip, char *fullName, double scaleLinearity, double offsetADUs)
{
	int row, col;
	int i;
	uint32_t b[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS];
	FILE *inpf;
	int firstChip, lastChip;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("loadLinearityGainAscii: chip=%d out of range 0 to %d", chip, m_numChips-1);		
	else
		firstChip = lastChip = chip;
	
	inpf = fopen (fullName, "r");
	if (inpf == nullptr)
	{
		throw  XDmaHexitecException("loadLinearityGainAscii: Cannot open file %s, errno=%d", fullName, errno);
	}

	for (row=0; row<HEXITEC_NUM_ROWS; row++)
	{
		for (col=0; col<HEXITEC_NUM_COLS; col++)
		{
			double x;
			int y;
			if (fscanf(inpf, "%lg", &x) != 1)
			{
				fclose(inpf);
				throw  XDmaHexitecException("loadLinearityGainAscii: Cannot open read data at row=%d, col=%d", row, col);
			}
			y = x*(double)m_linScaleB*scaleLinearity;
			if (y < m_linABMin || y > m_linABMax)
			{
				fclose(inpf);
				throw  XDmaHexitecException("loadLinearityGainAscii: At row=%d, col=%d, gain value=%g scales to %d which is out of range %d to %d", row, col, x, y, m_linABMin, m_linABMax);
			}
			b[col+HEXITEC_NUM_COLS*row] = y;
		}
	}
	fclose(inpf);
	setPixelLin(chip, HEXITEC_REGION_LIN_A, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, -1, 0);
	writePixelLin(chip, HEXITEC_REGION_LIN_B, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, true, b);
	setPixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, -1, (int)(offsetADUs*m_linScaleC));

	for (chip=firstChip; chip<=lastChip; chip++)
		m_linCorrName[chip] = std::string(fullName);
}

/** Load full linearity correction.
The file must contain a row per pixel, ordered row 0, col 0...79, row 1, col 0...79 etc.
Each line must contain either 3 ASCII doubles a, b,c of ax^2+bx+c or 8 triples if using Piecewise Linearity.
The a value caling is /...
The b value scaling  is arounf 1, typically 1.2 for hexitec.
The c values is scaled to ADC units. Noite if teh spectrum is collected at e.g .1024 bins, the c value will need scaling by 4 to match to 4096 ADC codes.
@param chip The chip number
@param fullName, the full path name to the file
*/
#define MAX_LINE 1024
void XDmaHexitec::loadLinearityAscii(int chip, char *fullName, double scaleLinearity)
{
	int row, col;
	int i;
	uint32_t *a=nullptr, *b=nullptr, *c=nullptr;
	char lbuf[MAX_LINE+2];
	double data[3*HEXITEC_LINEARITY_MAX_PIECES];
	FILE *inpf= nullptr;
	char *cp, *next;
	int nTriples;
	int firstChip, lastChip;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("loadLinearityAscii: chip=%d out of range 0 to %d", chip, m_numChips-1);		
	else
		firstChip = lastChip = chip;
	
	inpf = fopen (fullName, "r");
	if (inpf == nullptr)
	{
		throw  XDmaHexitecException("loadLinearityAscii: Cannot open file %s, errno=%d", fullName, errno);
	}
	a = new uint32_t [HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*HEXITEC_LINEARITY_MAX_PIECES];
	b = new uint32_t [HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*HEXITEC_LINEARITY_MAX_PIECES];
	c = new uint32_t [HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS*HEXITEC_LINEARITY_MAX_PIECES];

	for (row=0; row<HEXITEC_NUM_ROWS; row++)
	{
		for (col=0; col<HEXITEC_NUM_COLS; col++)
		{
			if (fgets(lbuf, MAX_LINE, inpf) == nullptr)
			{
				fclose(inpf); delete [] a; delete [] b; delete [] c;
				throw  XDmaHexitecException("loadLinearityGainAscii: Cannot read data at row=%d, col=%d", row, col);
			}
			if (strlen(lbuf) >= MAX_LINE-1)
			{
				fclose(inpf); delete [] a; delete [] b; delete [] c;
				throw  XDmaHexitecException("loadLinearityAscii: Input line is too long at row=%d, col=%d", row, col);
			}
			for (i=0, cp=lbuf; 1 ; i++)
			{
				double x;
				x = strtod((const char *)cp, &next);
				if (next == cp)
					break;
				if (i<HEXITEC_LINEARITY_MAX_PIECES)
					data[i] = x;
				cp = next+1;
			}
			if (row ==0 && col == 0)
			{
				nTriples = i/3;
				if (nTriples != 1 && nTriples != 1 << m_nBitsAddrPWLin)
				{
					fclose(inpf); delete [] a; delete [] b; delete [] c;
					throw  XDmaHexitecException("loadLinearityAscii: Line='%s' Unexpected number of values %d at row=%d, col=%d, expected 3 or %d", lbuf, i, row, col, 3 << m_nBitsAddrPWLin);
				}
			}
			if (i != 3*nTriples)
			{
				fclose(inpf); delete [] a; delete [] b; delete [] c;
				throw  XDmaHexitecException("loadLinearityAscii: Line='%s' Unexpected number of values %d at row=%d, col=%d, expected %d", lbuf, i, row, col, 3*nTriples);
			}
			for (i=0; i<nTriples; i++)
			{
				int ia, ib, ic;
				ia = 4096.0*(double)m_linScaleA*data[3*i]*scaleLinearity;		// Scaled to match ADC range
				ib = (double)m_linScaleB*data[3*i+1]*scaleLinearity;
				ic = data[3*i+2]*(double)m_linScaleC*scaleLinearity;
				if (ia < m_linABMin || ia > m_linABMax)
				{
					fclose(inpf);
					throw  XDmaHexitecException("loadLinearityAscii: At row=%d, col=%d, triple=%d, a value=%g scales to %d which is out of range %d to %d", row, col, i, data[3*i], 
									ia, m_linABMin, m_linABMax);
				}
				if (ib < m_linABMin || ib > m_linABMax)
				{
					fclose(inpf);
					throw  XDmaHexitecException("loadLinearityAscii: At row=%d, col=%d, triple=%d, b value=%g scales to %d which is out of range %d to %d", row, col, i, data[3*i+1], 
									ib, m_linABMin, m_linABMax);
				}
				if (ic < HEXITEC_LINEARITY_C_MIN || ic > HEXITEC_LINEARITY_C_MAX)
				{
					fclose(inpf);
					throw  XDmaHexitecException("loadLinearityAscii: At row=%d, col=%d, triple=%d, c value=%g scales to %d which is out of range %d to %d", row, col, i, data[3*i+2], 
									ic, HEXITEC_LINEARITY_C_MIN, HEXITEC_LINEARITY_C_MAX);
				}
				a[i+nTriples*(col+HEXITEC_NUM_COLS*row)] = ia;
				b[i+nTriples*(col+HEXITEC_NUM_COLS*row)] = ib;
				c[i+nTriples*(col+HEXITEC_NUM_COLS*row)] = ic;
			}
		}
	}
	fclose(inpf);
	writePixelLin(chip, HEXITEC_REGION_LIN_A, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, nTriples==1, a);
	writePixelLin(chip, HEXITEC_REGION_LIN_B, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, nTriples==1, b);
	writePixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, nTriples==1, c);
	delete [] a;
	delete [] b;
	delete [] c;
	for (chip=firstChip; chip<=lastChip; chip++)
		m_linCorrName[chip] = std::string(fullName);
 }

void XDmaHexitec::loadBadPixelsTrigAscii(char *fullName)
{
	int chip;
	int row, col;
	int i;
	uint32_t mainThres;
	char lbuf[MAX_LINE+2];
	FILE *inpf= nullptr;
	
	inpf = fopen (fullName, "r");
	if (inpf == nullptr)
	{
		throw  XDmaHexitecException("loadBadPixelsTrigAscii: Cannot open file %s, errno=%d", fullName, errno);
	}

	i = 1;
	while (fgets(lbuf, MAX_LINE, inpf) != nullptr)
	{
		if (sscanf(lbuf, "%d %d %d", &chip, &row, &col) != 3)
		{
			fclose(inpf);
			throw  XDmaHexitecException("loadBadPixelsTrigAscii: Cannot bad pixel chip, row, col at line %d : '%s'", i, lbuf);
		}
		readPixelLUT(chip, HEXITEC_REGION_MTHRES, col, 1, row, 1, &mainThres);
		mainThres &= ~HEXITEC_THRES_ENABLE;
		writePixelLUT(chip, HEXITEC_REGION_MTHRES, col, 1, row, 1, &mainThres);
		printf("loadBadPixelsTrigAscii: Disabled trigger on row=%d, col=%d, new threshold=%08X\n", row, col, mainThres);
		i++;
	}
	fclose(inpf);
}
/**
	Add an fixed offset to the offset term of the linearity correction. The main use of this will be to use with auto-triggered modes to allow the baseline subtracted data noise level
	to be histogrammed
@param chip			Chip number or -1 to duplicate to all chips.
@param offsetADUs	Offset in ADUs. This is a double an can be fractional as the linearity correction has some bits below the binary point.
*/
void XDmaHexitec::linearityAddOffset(int chip, double offsetADUs)
{
	int i;
	uint32_t *c=nullptr;
	int firstChip, lastChip;
	int offset = (int)(offsetADUs*m_linScaleC);
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
	}
	else if (chip >= m_numChips)
		throw  XDmaHexitecException("linearityAddOffset: Chip %d is out of range 0...%d", chip, m_numChips-1);
	else
		firstChip = lastChip = chip;

	c = new uint32_t [HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS<<m_nBitsAddrPWLin];

	for (chip=firstChip; chip <= lastChip; chip++)
	{
		readPixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, c);
		for (i=0; i<HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS<<m_nBitsAddrPWLin; i++)
		{
			c[i] += offset;
		}
		writePixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, false, c);
		m_linCorrName[chip] += std::string("+offset=")+std::to_string(offsetADUs);
	}
	delete [] c;
 }
