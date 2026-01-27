#include <iostream>
#include <fstream>
#include <cerrno>
#include <iomanip>
#include <chrono>
#include <thread>
#include "xdma_hbm_hist.h"
#include "xdma_hexitec.h"
#include "hdf5.h"

using namespace std;


void XDmaHexitec::loadLinearityGainHDF5(char *fullName, double scale)
{
	hid_t fileId;
	hid_t dataset, dataspace, datatype, dataClass;
	hid_t memspace;
	hsize_t dimsAll[2], dimsChunk[2];
	hsize_t start[2], rdCount[2];
    herr_t      status;
	int rank;
	hsize_t npts;
	size_t sizeBytes;
	double buff[HEXITEC_NUM_ROWS][HEXITEC_NUM_COLS];
	double min, max;
	int row, col;
	int i;
	uint32_t b[HEXITEC_NUM_ROWS*HEXITEC_NUM_COLS];
	int chip=0;
	
	fileId = H5Fopen (fullName, H5F_ACC_RDONLY, H5P_DEFAULT);
	if (fileId < 0)
	{
		throw  XDmaHexitecException("loadLinearityGainHDF5: Cannot open file %s, errno=%d", fullName, errno);
	}

	dataset = H5Dopen (fileId, "/GainMap", H5P_DEFAULT);
	if (dataset < 0)
	{
		throw  XDmaHexitecException("loadLinearityGainHDF5: Cannot open data set DataCube");
	}
	dataspace = H5Dget_space(dataset);
	rank = H5Sget_simple_extent_ndims(dataspace);
	if (rank != 2)
	{
		throw  XDmaHexitecException("loadLinearityGainHDF5: rank of data set =%d, expected rank=2\n", rank);
	}
	rank = H5Sget_simple_extent_dims(dataspace, dimsAll, NULL);
	datatype = H5Dget_type(dataset);
	dataClass =  H5Tget_class( datatype );
	sizeBytes =  H5Tget_size(datatype); 
	H5Tclose(datatype);
	printf("Found dataset dims (%ld, %ld), dataClass=%ld, sizeBytes=%zu\n", dimsAll[0], dimsAll[1], dataClass, sizeBytes);
	if (dataClass != H5T_FLOAT || sizeBytes != 8)
	{
		throw  XDmaHexitecException("loadLinearityGainHDF5: Unexpected class (%d) or size (%d)", dataClass, sizeBytes);
	}

	dimsChunk[0] = HEXITEC_NUM_ROWS;
	dimsChunk[1] = HEXITEC_NUM_COLS;
	start[0] = 0;
	start[1] = 0;

	memspace = H5Screate_simple(2, dimsChunk, NULL);
	status = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, NULL, dimsChunk, NULL);
	if (status < 0)
	{
		throw  XDmaHexitecException("loadLinearityGainHDF5: H5Sselect_hyperslab returns %d selecting scaling data\n", status);
	}
	status =  H5Dread (dataset, H5T_NATIVE_DOUBLE, memspace, dataspace, H5P_DEFAULT, buff);
	if (status < 0)
	{
		throw  XDmaHexitecException("loadLinearityGainHDF5: Error reading scaling data, status=%d", status);
	}
	H5Sclose(memspace);

	for (row=0; row<HEXITEC_NUM_ROWS; row++)
	{
		for (col=0; col<HEXITEC_NUM_COLS; col++)
		{
			double x = buff[row][col];
			int y = scale*x*(double)m_linScaleB;
			if (y < m_linABMin || y > m_linABMax)
			{
				H5Sclose(dataspace);
				H5Dclose(dataset);
				H5Fclose(fileId);
				throw  XDmaHexitecException("loadLinearityGainHDF5: At row=%d, col=%d, gain value=%g scales to %d which is out of range %d to %d", row, col, x, y, m_linABMin, m_linABMax);
			}

			b[col+HEXITEC_NUM_COLS*row] = y;
		}
	}

	H5Sclose(dataspace);
	H5Dclose(dataset);
	H5Fclose(fileId);
	setPixelLin(chip, HEXITEC_REGION_LIN_A, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, -1, 0);
	writePixelLin(chip, HEXITEC_REGION_LIN_B, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, true, b);
	setPixelLin(chip, HEXITEC_REGION_LIN_C, 0, HEXITEC_NUM_COLS, 0, HEXITEC_NUM_ROWS, -1, 0);
}

