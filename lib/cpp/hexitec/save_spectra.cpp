#include <iostream>
#include <fstream>
#include <cerrno>
#include <iomanip>
#include <chrono>
#include <thread>
#include "detfile.h"
#include "errors.h"
#include "xdma_hbm_hist.h"
#include "xdma_hexitec.h"
#include "hdf5.h"

using namespace std;

void XDmaHexitec::saveSpectraAsc( char *fName, int chip, int numEng, int firstTF, int numTF, bool mapped, bool sumChips)
{
	int eng, row, col, tf;
	ofstream opFile;
	uint32_t *buff, *ptr;
	int firstChip, lastChip;

	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		for (chip=1 ; chip<m_numChips; chip++)
			if (!(m_dataFormat[0] == m_dataFormat[chip]))
				throw XDmaHexitecException("saveSpectraAsc: Saving all chips, but data format mis-match between chip=0 and chip=%d", chip);
		chip = 0;			
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("saveSpectraAsc: chip %d is out of range 0...%d", chip, m_numChips-1);
	else
	{
		firstChip = lastChip = chip;
	}
	int nBinsClustClass = m_dataFormat[chip].nBinsClustClass;
	if (firstTF < 0)
		throw XDmaHexitecException("saveSpectraAsc: firstTF should be >=0 not %d", firstTF);
		
	if (mapped)
	{
		if (m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
			throw XDmaHexitecException("saveSpectraAsc: Called for mapped spectra, but mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF");
		
		if (firstTF+numTF > m_dataFormat[chip].numTFMapped)
			throw XDmaHexitecException("saveSpectraAsc: Mapped: (firstTF=%d)+(numTf=%d) out of range 1...%d", firstTF, numTF, m_dataFormat[chip].numTFMapped);

		if (numEng < 0)
			numEng = HEXITEC_NBINS_MAPPED;
		else if (numEng > HEXITEC_NBINS_MAPPED)
			throw XDmaHexitecException("saveSpectraAsc: Mapped: numEng=%d out of range 1...%d", numEng, HEXITEC_NBINS_MAPPED);
		numEng = (numEng+7) & 0xFFFFFFF8;		// Round up to 8 word boundary
	}
	else
	{
		if (m_dataFormat[chip].mappedMode== HEXITEC_HIST_MAPPED_MODE_ONLY)
			throw XDmaHexitecException("saveSpectraAsc: Called for non-mapped spectra, but mappedMode == HEXITEC_HIST_MAPPED_MODE_ONLY");
		if (firstTF+numTF > m_dataFormat[chip].numTF)
			throw XDmaHexitecException("saveSpectraAsc: (firstTF=%d)+(numTf=%d) out of range 1...%d", firstTF, numTF, m_dataFormat[chip].numTF);
		if (numEng < 0)
			numEng = m_dataFormat[chip].nBinsEng;
		else if (numEng > m_dataFormat[chip].nBinsEng)
			throw XDmaHexitecException("saveSpectraAsc: numEng=%d out of range 1...%d", numEng, m_dataFormat[chip].nBinsEng);
			
		numEng = (numEng+7) & 0xFFFFFFF8;		// Round up to 8 word boundary
	}
	
	opFile.open(fName);
	if (!opFile.is_open())
		throw XDmaHexitecException("saveSpectraAsc: Cannot open output file %s, errno=%d\n", fName, errno); 

	if (mapped)
	{
		posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
		ptr = buff;
		opFile << "Chip\tTF\tRow\tCol";
		for (eng=0; eng< numEng; eng++)
			opFile << "\t" << eng;
		for (chip=firstChip;chip<=lastChip; chip++)
		{
			for (tf=0; tf<numTF; tf++)
			{
				readMappedEngColRowTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, tf+firstTF, 1, chip, -1, buff);
				for (row=0; row<HEXITEC_NUM_ROWS; row++)
				{
					for (col=0;col<HEXITEC_NUM_COLS; col++)
					{
						opFile << endl << chip << "\t" << tf <<"\t" << row << "\t" << col;
						for (eng=0; eng< numEng; eng++)
							opFile << "\t" << *ptr++;
					}
				}
			}
		}
		opFile << endl;
		opFile.close();
		free(buff);
	}
	else
	{
		switch (m_dataFormat[chip].histMode)
		{
		case HEXITEC_HIST_FORMAT_RUN12:
		case HEXITEC_HIST_FORMAT_RUN11:		
		case HEXITEC_HIST_FORMAT_RUN10:			
		case HEXITEC_HIST_FORMAT_RUN9:		
		case HEXITEC_HIST_FORMAT_RUN8:		
		case HEXITEC_HIST_FORMAT_RUN7:		
		case HEXITEC_HIST_FORMAT_RUN10LSB:			

			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
			ptr = buff;
			opFile << "Chip\tTF\tRow\tCol";
			for (eng=0; eng< numEng; eng++)
				opFile << "\t" << eng;
			for (chip=firstChip;chip<=lastChip; chip++)
			{
				for (tf=0; tf<numTF; tf++)
				{
					readHistEngColRowTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, tf+firstTF, 1, chip, -1, buff);
					for (row=0; row<HEXITEC_NUM_ROWS; row++)
					{
						for (col=0;col<HEXITEC_NUM_COLS; col++)
						{
							opFile << endl << chip << "\t" << tf+firstTF <<"\t" << row << "\t" << col;
							for (eng=0; eng< numEng; eng++)
								opFile << "\t" << *ptr++;
						}
					}
				}
			}
			opFile << endl;
			opFile.close();
			free(buff);
			break;
			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC12:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC11:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC9:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC8:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC7:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:		

			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
			ptr = buff;
			opFile << "Chip\tTF\tCC\tRow\tCol";
			for (eng=0; eng< numEng; eng++)
				opFile << "\t" << eng;
			for (chip=firstChip;chip<=lastChip; chip++)
			{
				for (tf=0; tf<numTF; tf++)
				{
					for (int cc=0; cc<nBinsClustClass; cc++)
					{
						readHistEngColRowCCTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, cc, 1, tf+firstTF, 1, chip, -1, buff);
						for (row=0; row<HEXITEC_NUM_ROWS; row++)
						{
							for (col=0;col<HEXITEC_NUM_COLS; col++)
							{
								opFile << endl << chip << "\t" << tf+firstTF << "\t" << cc <<"\t" << row << "\t" << col;
								for (eng=0; eng< numEng; eng++)
									opFile << "\t" << *ptr++;
							}
						}
					}
				}
			}
			opFile << endl;
			opFile.close();
			free(buff);
			break;

		case HEXITEC_HIST_FORMAT_ENG_ONLY12:	
		case HEXITEC_HIST_FORMAT_ENG_ONLY11:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY10:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY9:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY8:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY7:	
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numTF);
			ptr = buff;
			if (sumChips)
				firstChip = lastChip = -1;
			for (chip=firstChip;chip<=lastChip; chip++)
			{
				if (sumChips)
					opFile << "ChipsSummed" << endl;
				else
					opFile << "Chip"<<chip << endl;
				opFile << "Eng";
				for (tf=0; tf< numTF; tf++)
					opFile << "\tTF" << tf+firstTF;
				readHistEngTime(numEng, firstTF, numTF,  chip, -1, buff);
				for (eng=0; eng< numEng; eng++)
				{
					opFile << endl << eng;
					for (tf=0; tf<numTF; tf++)
						opFile << "\t" << ptr[eng+tf*numEng];
				}
				opFile << endl;
			}
			opFile.close();
			free(buff);
			break;


		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:	
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:	
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numTF*nBinsClustClass);
			ptr = buff;
			if (sumChips)
				firstChip = lastChip = -1;
			for (chip=firstChip;chip<=lastChip; chip++)
			{
				if (sumChips)
					opFile << "ChipsSummed" << endl;
				else
					opFile << "Chip"<<chip << endl;
				opFile << "Eng";
				for (tf=0; tf< numTF; tf++)
				{
					for (int cc=0; cc<nBinsClustClass; cc++)
						opFile << "\tTF" << tf << "CC" << cc;
				}
				readHistEngCCTime(numEng, 0, nBinsClustClass, firstTF, numTF,  chip, -1, buff);
				for (eng=0; eng< numEng; eng++)
				{
					opFile << endl << eng;
					for (tf=0; tf<numTF; tf++)
					{
						for (int cc=0; cc<nBinsClustClass; cc++)
							opFile << "\t" << ptr[eng+(tf*nBinsClustClass+cc)*numEng];
					}
				}
				opFile << endl;
			}
			opFile.close();
			free(buff);
			break;

		
		case HEXITEC_HIST_FORMAT_CALIB_COMB12:	
		case HEXITEC_HIST_FORMAT_CALIB_COMB11:	
		case HEXITEC_HIST_FORMAT_CALIB_COMB10:	
		case HEXITEC_HIST_FORMAT_CALIB_COMBTYPE:	
		case HEXITEC_HIST_FORMAT_CALIB_SEPARATE:	
		case HEXITEC_HIST_FORMAT_CHARAC2D12:			
		case HEXITEC_HIST_FORMAT_CHARAC2D10:			
		case HEXITEC_HIST_FORMAT_CHARAC3D:			
		case HEXITEC_HIST_FORMAT_CHARAC4D:			
			printf("saveSpectraAsc: Format %d not supoorted yet\n", m_dataFormat[chip].histMode);
			break;
		}
	}
}

void XDmaHexitec::saveSpectraDet(char *fName, int chip, int numEng, int firstTF, int numTF, bool mapped, bool sumChips)
{
	int eng, row, col, tf, clustClass;
	ofstream opFile;
	uint32_t *buff, *ptr;
	DETFILE *det=NULL;
	char fNameExt[FILENAME_MAX+2];
	char comment[100];
	char title[100];
	int len;
	int nBins[4];
	int corner;
	int firstChip, lastChip;
	bool manyChips=false;
	
	if (chip < 0)
	{
		firstChip = 0;
		lastChip = m_numChips-1;
		for (chip=1 ; chip<m_numChips; chip++)
			if (!(m_dataFormat[0] == m_dataFormat[chip]))
				throw XDmaHexitecException("saveSpectraDet: Saving all chips, but data format mis-match between chip=0 and chip=%d", chip);
		chip = 0;
		manyChips = m_numChips>1;
	}
	else if (chip >= m_numChips)
		throw XDmaHexitecException("saveSpectraDet: chip %d is outof range 0...%d", chip, m_numChips-1);
	else
	{
		firstChip = lastChip = chip;
	}
	if (firstTF < 0)
		throw XDmaHexitecException("saveSpectraDet: firstTF should be >=0 not %d", firstTF);

	int nBinsClustClass = m_dataFormat[chip].nBinsClustClass;
	if (mapped)
	{
		if (m_dataFormat[chip].mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF)
			throw XDmaHexitecException("saveSpectraDet: Called for mapped spectra, but mappedMode == HEXITEC_HIST_MAPPED_MODE_OFF");
		
		if (numTF+firstTF > m_dataFormat[chip].numTFMapped)
			throw XDmaHexitecException("saveSpectraDet: Mapped: (firstTF=%d)+(numTf=%d) out of range 1...%d", firstTF, numTF, m_dataFormat[chip].numTFMapped);

		if (numEng < 0)
			numEng = HEXITEC_NBINS_MAPPED;
		else if (numEng > HEXITEC_NBINS_MAPPED)
			throw XDmaHexitecException("saveSpectraDet: Mapped: numEng=%d out of range 1...%d", numEng, HEXITEC_NBINS_MAPPED);
		numEng = (numEng+7) & 0xFFFFFFF8;		// Round up to 8 word boundary
	}
	else
	{
		if (m_dataFormat[chip].mappedMode== HEXITEC_HIST_MAPPED_MODE_ONLY)
			throw XDmaHexitecException("saveSpectraDet: Called for non-mapped spectra, but mappedMode == HEXITEC_HIST_MAPPED_MODE_ONLY");
		if (firstTF+numTF > m_dataFormat[chip].numTF)
			throw XDmaHexitecException("saveSpectraDet: (firstTF=%d)+numTf=%d out of range 1...%d", firstTF, numTF, m_dataFormat[chip].numTF);
		if (numEng < 0)
			numEng = m_dataFormat[chip].nBinsEng;
		else if (numEng > m_dataFormat[chip].nBinsEng)
			throw XDmaHexitecException("saveSpectraDet: numEng=%d out of range 1...%d", numEng, m_dataFormat[chip].nBinsEng);
			
		numEng = (numEng+7) & 0xFFFFFFF8;		// Round up to 8 word boundary
	}

	if (mapped)
	{
		posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
		ptr = buff;
		for (chip=firstChip;chip<=lastChip; chip++)
		{
			for (tf=0; tf<numTF; tf++)
			{
				strcpy(fNameExt, fName);
				if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
					fNameExt[len-4] = 0;
				if (manyChips)
				{
					if (numTF > 1)
						sprintf(fNameExt+len-4, "_ch%02dtf%03d.det", chip, tf+firstTF);
					else
						sprintf(fNameExt+len-4, "_ch%02d.det", chip);
				}
				else
				{
					if (numTF > 1)
						sprintf(fNameExt+len-4, "%03d.det", tf+firstTF);
					else
						strcat(fNameExt, ".det");
				}
				det = det_open (fNameExt, "w");
				if (det == NULL)
				{
					free(buff);
					throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
				}
				det->width = numEng;
				det->height = HEXITEC_NUM_COLS;
				det->num_t = HEXITEC_NUM_ROWS;
				det->data_type = DET_LOCAL_INT32;
				det->title = strdup ("Hexitec Mapped");
				det_set_date_time (det);
				sprintf(comment, "Time Frame %d", tf);
				det->comment[0] = strdup (comment);
				if (manyChips)
				{
					sprintf(comment, "Chip %d", chip);
					det->comment[1] = strdup (comment);
				}
				if (det_write_header (det) != 0)
					exit (errno);
				readMappedEngColRowTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, tf+firstTF, 1, chip, -1, buff);
				det_write3d (det, 0, 0, 0, numEng, HEXITEC_NUM_COLS, HEXITEC_NUM_ROWS, buff, DET_LOCAL_INT32);
				det_close(det);
			}
		}
		free(buff);
	}
	else
	{
		switch (m_dataFormat[chip].histMode)
		{
		case HEXITEC_HIST_FORMAT_RUN12:
		case HEXITEC_HIST_FORMAT_RUN11:		
		case HEXITEC_HIST_FORMAT_RUN10:			
		case HEXITEC_HIST_FORMAT_RUN9:		
		case HEXITEC_HIST_FORMAT_RUN8:		
		case HEXITEC_HIST_FORMAT_RUN7:		
		case HEXITEC_HIST_FORMAT_RUN10LSB:			

			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
			for (chip=firstChip;chip<=lastChip; chip++)
			{
				for (tf=0; tf<numTF; tf++)
				{
					strcpy(fNameExt, fName);
					if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
						fNameExt[len-4] = 0;
					if (manyChips)
					{
						if (numTF > 1)
							sprintf(fNameExt+len-4, "_ch%02dtf%03d.det", chip, tf+firstTF);
						else
							sprintf(fNameExt+len-4, "_ch%02d.det", chip);
					}
					else
					{
						if (numTF > 1)
							sprintf(fNameExt+len-4, "%03d.det", tf+firstTF);
						else
							strcat(fNameExt, ".det");
					}
					det = det_open (fNameExt, "w");
					if (det == NULL)
					{
						free(buff);
						throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
					}
					det->width = numEng;
					det->height = HEXITEC_NUM_COLS;
					det->num_t = HEXITEC_NUM_ROWS;
					det->data_type = DET_LOCAL_INT32;
					det->title = strdup ("Hexitec Spectra by position");
					det_set_date_time (det);
					sprintf(comment, "Time Frame %d", tf);
					det->comment[0] = strdup (comment);
					if (det_write_header (det) != 0)
						exit (errno);
					readHistEngColRowTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, tf+firstTF, 1, chip, -1, buff);
					det_write3d (det, 0, 0, 0, numEng, HEXITEC_NUM_COLS, HEXITEC_NUM_ROWS, buff, DET_LOCAL_INT32);
					det_close(det);
				}
			}
			free(buff);
			break;

		case HEXITEC_HIST_FORMAT_ENG_POS_CC12:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC11:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10:		
		case HEXITEC_HIST_FORMAT_ENG_POS_CC9:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC8:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC7:			
		case HEXITEC_HIST_FORMAT_ENG_POS_CC10LSB:		

			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS);
			for (chip=firstChip;chip<=lastChip; chip++)
			{
				for (tf=0; tf<numTF; tf++)
				{
					for (int cc=0; cc<nBinsClustClass; cc++)
					{
						strcpy(fNameExt, fName);
						if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
							fNameExt[len-4] = 0;
						if(manyChips)
						{
							if (numTF > 1)
								sprintf(fNameExt+len-4, "_ch%02dtf%03dcc%02d.det", chip, tf+firstTF, cc);
							else
								sprintf(fNameExt+len-4, "_ch%02dcc%02d.det", chip, cc);
						}
						else
						{
							if (numTF > 1)
								sprintf(fNameExt+len-4, "%03dcc%02d.det", tf+firstTF, cc);
							else
								sprintf(fNameExt+len-4, "_cc%02d.det", cc);
						}
						det = det_open (fNameExt, "w");
						if (det == NULL)
						{
							free(buff);
							throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
						}
						det->width = numEng;
						det->height = HEXITEC_NUM_COLS;
						det->num_t = HEXITEC_NUM_ROWS;
						det->data_type = DET_LOCAL_INT32;
						det->title = strdup ("Hexitec Spectra by position and cluster Class");
						det_set_date_time (det);
						sprintf(comment, "Time Frame=%d Cluster Class %d", tf+firstTF, cc);
						det->comment[0] = strdup (comment);
						if (det_write_header (det) != 0)
							exit (errno);
						readHistEngColRowCCTime(numEng, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, cc, 1, tf+firstTF, 1, chip, -1, buff);
						det_write3d (det, 0, 0, 0, numEng, HEXITEC_NUM_COLS, HEXITEC_NUM_ROWS, buff, DET_LOCAL_INT32);
						det_close(det);
					}
				}
			}
			free(buff);
			break;

			
		case HEXITEC_HIST_FORMAT_CALIB_COMB12:	
		case HEXITEC_HIST_FORMAT_CALIB_COMB11:	
		case HEXITEC_HIST_FORMAT_CALIB_COMB10:	
		case HEXITEC_HIST_FORMAT_CALIB_COMBTYPE:
		
			if (sumChips || firstChip == lastChip)
			{
				if (sumChips)
					chip = -1;
				strcpy(fNameExt, fName);
				if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
					fNameExt[len-4] = 0;
				strcat(fNameExt, ".det");
				posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_RECIP_SIZE);
				det = det_open (fNameExt, "w");
				if (det == NULL)
				{
					free(buff);
					throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
				}
				det->width = numEng;
				det->height = HEXITEC_RECIP_SIZE;
				det->num_t = nBinsClustClass;
				det->data_type = DET_LOCAL_INT32;
				det->title = strdup (sumChips?"Hexitec Spectra by LUT address, summed chips":"Hexitec Spectra by LUT address, single chip");
				det_set_date_time (det);
				if (det_write_header (det) != 0)
					exit (errno);

				for (clustClass=0; clustClass<nBinsClustClass; clustClass++)
				{
					
					readHistEngCalibClass(0, numEng, 0, HEXITEC_RECIP_SIZE, clustClass, 1, chip, -1, buff);
					det_write3d (det, 0, 0, clustClass, numEng, HEXITEC_RECIP_SIZE, 1, buff, DET_LOCAL_INT32);
				}
				det_close(det);
				free(buff);
			}
			else
			{
				posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*HEXITEC_RECIP_SIZE);
				for (chip=firstChip; chip<=lastChip; chip++)
				{
					strcpy(fNameExt, fName);
					if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
						fNameExt[len-4] = 0;
					sprintf(fNameExt+len-4, "_ch%02d.det", chip);
					det = det_open (fNameExt, "w");
					if (det == NULL)
					{
						free(buff);
						throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
					}
					det->width = numEng;
					det->height = HEXITEC_RECIP_SIZE;
					det->num_t = nBinsClustClass;
					det->data_type = DET_LOCAL_INT32;
					sprintf(title, "Hexitec Spectra by LUT address, single chip=%d", chip);
					det->title = strdup (title);
					det_set_date_time (det);
					if (det_write_header (det) != 0)
						exit (errno);

					for (clustClass=0; clustClass<nBinsClustClass; clustClass++)
					{
						readHistEngCalibClass(0, numEng, 0, HEXITEC_RECIP_SIZE, clustClass, 1, chip, -1, buff);
						det_write3d (det, 0, 0, clustClass, numEng, HEXITEC_RECIP_SIZE, 1, buff, DET_LOCAL_INT32);
					}
					det_close(det);
				}
				free(buff);
			}
				
			break;
			
		case HEXITEC_HIST_FORMAT_CHARAC2D12:
		case HEXITEC_HIST_FORMAT_CHARAC2D10:
			getnBinsCharac(firstChip, nBins);
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*nBins[0]*nBins[1]);
			if (sumChips)
				firstChip = lastChip = -1;

			for (chip=firstChip; chip<=lastChip; chip++)
			{
				strcpy(fNameExt, fName);
				if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
					fNameExt[len-4] = 0;
				if (sumChips || firstChip == lastChip)
					strcat(fNameExt, ".det");
				else
					sprintf(fNameExt+len-4, "_ch%02d.det", chip);

				det = det_open (fNameExt, "w");
				if (det == NULL)
				{
					free(buff);
					throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
				}
				det->width = nBins[0];
				det->height = nBins[1];
				det->num_t = 2;
				det->data_type = DET_LOCAL_INT32;
				if (sumChips)
					det->title = strdup ("Hexitec Characterise 2-d image, summed chips");
				else
				{
					sprintf(title, "Hexitec Characterise 2-d image,  chip=%d", chip);
					det->title = strdup (title);
				}
				det_set_date_time (det);
				if (det_write_header (det) != 0)
					exit (errno);

				for (int diag=0; diag<2; diag++)
				{
					readHistCharac2d(0, nBins[0], 0, nBins[1], diag, chip, -1, buff);
					det_write3d (det, 0, 0, diag, nBins[0], nBins[1], 1, buff, DET_LOCAL_INT32);
				}
				det_close(det);
			}
			free(buff);
			break;


		case HEXITEC_HIST_FORMAT_CHARAC3D:
			getnBinsCharac(firstChip, nBins);
			posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*nBins[0]*nBins[1]);
			if (sumChips)
			{
				firstChip = lastChip = -1;
				for (chip=firstChip; chip<=lastChip; chip++)
				{
					strcpy(fNameExt, fName);
					if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
						fNameExt[len-4] = 0;
					if (sumChips || firstChip == lastChip)
						strcat(fNameExt, ".det");
					else
						sprintf(fNameExt+len-4, "_ch%02d.det", chip);
				
					det = det_open (fNameExt, "w");
					if (det == NULL)
					{
						free(buff);
						throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
					}
					det->width = nBins[0];
					det->height = nBins[1];
					det->num_t = nBins[2];
					det->data_type = DET_LOCAL_INT32;
					if (sumChips)
						det->title = strdup ("Hexitec Characterise 3-d image, summing chips");
					else
					{
						sprintf(title, "Hexitec Characterise 3-d image,  chip=%d", chip);
						det->title = strdup (title);
					}					
					det_set_date_time (det);
					if (det_write_header (det) != 0)
						exit (errno);

					for (corner=0; corner<nBins[2]; corner++)
					{		
						readHistCharac3d(0, nBins[0], 0, nBins[1], corner, 1, chip, -1, buff);
						det_write3d (det, 0, 0, corner, nBins[0], nBins[1], 1, buff, DET_LOCAL_INT32);
					}
					det_close(det);
				}
			}
			free(buff);
			break;
			
		case HEXITEC_HIST_FORMAT_ENG_ONLY12:	
		case HEXITEC_HIST_FORMAT_ENG_ONLY11:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY10:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY9:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY8:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY7:		
			if (sumChips || firstChip == lastChip)
			{
				if (sumChips)
					chip = -1;
				strcpy(fNameExt, fName);
				if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
					fNameExt[len-4] = 0;
				strcat(fNameExt, ".det");
				posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numTF);
				det = det_open (fNameExt, "w");
				if (det == NULL)
				{
					free(buff);
					throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
				}
				det->width = numEng;
				det->height = numTF;
				det->num_t = 1;
				det->data_type = DET_LOCAL_INT32;
				det->title = strdup (sumChips?"Hexitec Eng Only Spectra Summed over chips":"Hexitec Eng Only Spectra, Single chip");
				det_set_date_time (det);
				if (det_write_header (det) != 0)
					exit (errno);
				readHistEngTime(numEng, firstTF, numTF,  chip, -1, buff);
				det_write3d (det, 0, 0, 0, numEng, numTF, 1, buff, DET_LOCAL_INT32);
				det_close(det);
				free(buff);
			}
			else
			{
				strcpy(fNameExt, fName);
				if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
					fNameExt[len-4] = 0;
				strcat(fNameExt, ".det");
				posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*numTF);
				det = det_open (fNameExt, "w");
				if (det == NULL)
				{
					free(buff);
					throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
				}
				det->width = numEng;
				det->height = numTF;
				det->num_t = m_numChips;
				det->data_type = DET_LOCAL_INT32;
				det->title = strdup ("Hexitec Eng Only Spectra, multiple chips");
				det_set_date_time (det);
				if (det_write_header (det) != 0)
					exit (errno);
				for (chip=firstChip; chip<=lastChip; chip++)
				{
					readHistEngTime(numEng, firstTF, numTF,  chip, -1, buff);
					det_write3d (det, 0, 0, chip, numEng, numTF, 1, buff, DET_LOCAL_INT32);
				}
				det_close(det);
				free(buff);
			}
			break;
			
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC12:	
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC11:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC10:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC9:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC8:		
		case HEXITEC_HIST_FORMAT_ENG_ONLY_CC7:		
			if (sumChips || firstChip == lastChip)
			{
				if (sumChips)
					chip = -1;
				strcpy(fNameExt, fName);
				if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
					fNameExt[len-4] = 0;
				strcat(fNameExt, ".det");
				posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*nBinsClustClass*numTF);
				det = det_open (fNameExt, "w");
				if (det == NULL)
				{
					free(buff);
					throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
				}
				det->width = numEng;
				det->height = nBinsClustClass;
				det->num_t = numTF;
				det->data_type = DET_LOCAL_INT32;
				det->title = strdup (sumChips?"Hexitec Eng Only/CC Spectra summed over chips":"Hexitec Eng Only/CC Spectra, single chip");
				det_set_date_time (det);
				if (det_write_header (det) != 0)
					exit (errno);
				readHistEngCCTime(numEng, 0, nBinsClustClass, firstTF, numTF,  chip, -1, buff);
				det_write3d (det, 0, 0, 0, numEng, numTF, 1, buff, DET_LOCAL_INT32);
				det_close(det);
				free(buff);
			}
			else
			{
				posix_memalign((void **)&buff, 4096 /*alignment */,  sizeof(uint32_t)*numEng*nBinsClustClass*numTF);
				for (chip=firstChip; chip<=lastChip; chip++)
				{
					strcpy(fNameExt, fName);
					if ((len=strlen(fNameExt)) > 4 && strcmp(fNameExt+len-4, ".det")==0)
						fNameExt[len-4] = 0;
					sprintf(fNameExt+len-4, "_ch%02d.det", chip);
					det = det_open (fNameExt, "w");
					if (det == NULL)
					{
						free(buff);
						throw XDmaHexitecException("saveSpectraDet: Cannot open output file %s, errno=%d\n", fNameExt, errno); 
					}
					det->width = numEng;
					det->height = nBinsClustClass;
					det->num_t = numTF;
					det->data_type = DET_LOCAL_INT32;
					sprintf(title, "Hexitec Eng Only/CC Spectra, single chip=%d", chip);
					det->title = strdup (title);
					det_set_date_time (det);
					if (det_write_header (det) != 0)
						exit (errno);
					readHistEngCCTime(numEng, 0, nBinsClustClass, firstTF, numTF,  chip, -1, buff);
					det_write3d (det, 0, 0, 0, numEng, numTF, 1, buff, DET_LOCAL_INT32);
					det_close(det);
				}
				free(buff);
			}
			break;

		case HEXITEC_HIST_FORMAT_CALIB_SEPARATE:	
		case HEXITEC_HIST_FORMAT_CHARAC4D:			
			printf("saveSpectraDet: Format %d not supported yet\n", m_dataFormat[chip].histMode);
			break;
		}
	}
}

