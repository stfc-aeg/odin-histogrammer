/*
	Extended DET-file library
	By Richard W.M. Jones
*/


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(_OS9K) || defined(_OS9000)
#include <types.h>
#else
#include "os9types.h"
#endif
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "detfile.h"

#define DEBUG 0
#define CRLF "\015\012"
#define CR '\015'			
#define LF '\012'

#define DETFILE_MAX_MULTI_HEX 8

static void	free_det (DETFILE *);
static int	read_header (char *, DETFILE *);
static int	atodt (char *);
#ifndef DETFILE_FOR_LIBXSPRESS3
static char *dttoa (int);
static void	add_int_field (char *, char *, int);
static void	add_double_field (char *, char *, double);
static void	add_str_field (char *, char *, char *);
static void add_empty_field (char *, char *);
static void	add_end_field (char *);
static void what_conversion (int local_type, int file_type, int *needs_bswap, int *needs_conversion);
static int do_conversion(int src_type, int dest_type, void *dest, void *src, int nwords);
static void memcpyfast (void *, void *, int, int);
static void	itof (float *, int32 *, int);
static void	ftoi (int32 *, float *, int);

void det_bswap(u_int32 *dst, u_int32 *src, int n);
void det_bswap2(u_int16 *dst, u_int16 *src, int n);
void det_bswap8(void *dst, void *src, int n);
#endif

static char *strsave (char *text);

#ifdef DETFILE_FOR_LIBXSPRESS3
DETFILE *xsp3_det_open (const char *filename, const char *mode);
DETFILE *xsp3_det_fpopen (FILE *fp, const char *mode);
void xsp3_det_close (DETFILE *det);
void xsp3_det_fpclose (DETFILE *det);

#define det_open xsp3_det_open
#define det_fpopen xsp3_det_fpopen
#define det_close xsp3_det_close
#define det_fpclose xsp3_det_fpclose
#endif
/*
static void	bswap (int32 *, int32 *, int);
*/

DETFILE *
det_open (const char *filename, const char *mode)
/* If Filename == NULL then just create structes for use with da.servers own writing
*/
{
	DETFILE *det;
	FILE *fp ;
	char *open_mode;

#if defined(__WATCOMC__) || defined(__GNUC__)
	if (mode[0] == 'r')
		open_mode = "rb";
	else if (mode[0] == 'w')
		open_mode = "wb";
	else
    {
		fprintf(stderr, "Detfile now only supports modes r or w\n");
		return NULL;
    }
#else
	open_mode = mode;
#endif
	if (filename == NULL)
		fp = NULL;
	else
	{
#ifdef __CYGWIN__
		printf("detfile.c : Opening file '%s' mode='%s'\n", filename, open_mode);
#endif
		if ((fp = fopen (filename, open_mode)) == NULL)
			return NULL;
	}

	det = det_fpopen (fp, mode);
	if (det == NULL)
	{
		if (fp != NULL)
			fclose (fp);
		return NULL;
	}
	return det;
}

DETFILE *
det_fpopen (FILE *fp, const char *mode)
{
	DETFILE *det = malloc (sizeof (DETFILE));

	if (det == NULL)
		return NULL;
	memset (det, 0, sizeof (DETFILE));
	det->fp = fp;
	det->aspect = -1.0;
	det->num_t = 1;	
	/* Check mode. The following modes are valid:
	 *	r				Open for reading
	 *	w				Create (truncate) new file for writing
	 *	r+				Open old file for reading and writing *
	 *	rb, wb			As above
	 *	rb+				As above
	 *	w+, wb+			Not valid
	 *	a, a+, ab, ab+	Not valid
	 *		* not valid on non-seekable devices
	 * The file pointer passed to det_fpopen must be positioned at the
	 * beginning of the file, as if the file had just been opened.
	 */
	if (strcmp (mode, "r") == 0 || strcmp (mode, "rb") == 0)
	{
		det->mode = 'r';
		if (fp == NULL)
		{
			fprintf(stderr, " Cannot read from NULL file \n");
			return NULL;
		}
	}
	else if (strcmp (mode, "w") == 0 || strcmp (mode, "wb") == 0)
		det->mode = 'w';
	else
	{
		fprintf (stderr, "detfile.c: Invalid mode \"%s\" (must be r, w, r+).\n", mode);
		return NULL;
	}


	/* read the header, and decode it */
	if (det->mode == 'r' || det->mode == 'u')
	{
		if (fread (det->header, DET_HEADER_SIZE, 1, fp) != 1 ||
			read_header (det->header, det) != 0)
			return NULL;
		det->filepos = DET_HEADER_SIZE;
	}
	else
		det->filepos = 0;

	return det;
}

void
det_close (DETFILE *det)
{
	FILE *fp = det->fp;
	det_fpclose (det);
	if (fp != NULL)
		fclose (fp);
}

void
det_fpclose (DETFILE *det)
{
	switch (det->mode)
	{
	case 'r':
		/* Reading the file, and not updating it, so nothing needs to be done
		 * except to release the memory used by DETFILE.
		 */
		free_det (det);
		break;
	case 'w':
#ifdef DETFILE_FOR_LIBXSPRESS3
		fprintf(stderr, "detfile.c libxspress3 veriosn of detfile deos not support writing files\n");	
#else
		/* Writing the file, so if we haven't written the header yet, write it,
		 * then pad out the file to its full size if necessary, before closing
		 * it.
		 */
		if (det->width && det->height && det->data_type)
		{
			int fsize, i;

			if (det->fp != NULL && det->header_state != written)
				det_write_header (det);
			/* Don't see how this can be the wrong size if have used det_write_header */
		}
#endif
		free_det (det);
		break;
	}
}

#ifndef DETFILE_FOR_LIBXSPRESS3
int
det_build_header (DETFILE *det)
{
	int i, sl;
	char *p;

	if (det->mode == 'r')
	{
		fprintf (stderr, "detfile.c: Tried to write to a file opened for reading.\n");
		return -1;
	}

	memset (det->header, 0, DET_HEADER_SIZE);
	strcpy (det->header, DET_MAGIC CRLF);

	/* add the standard fields, which must be set */
	if (det->width && det->height && det->data_type && det->num_t)
	{
		add_int_field (det->header, "NUMX", det->width);
		add_int_field (det->header, "NUMY", det->height);
		add_int_field (det->header, "NUMT", det->num_t);
		add_str_field (det->header, "DATATYPE", dttoa (det->data_type));
	}
	else
	{
		fprintf (stderr, "detfile.c: width, height, data_type fields must be set.\n");
		return -1;
	}

	add_double_field (det->header, "ASPECT", det->aspect);
	/* add optional fields, if set */
	if (det->title)
		add_str_field (det->header, "TITLE", det->title);
	if (det->date)
		add_str_field (det->header, "DATE", det->date);
	if (det->time)
		add_str_field (det->header, "TIME", det->time);

	if (det->x_lab)
		add_str_field (det->header, "X_LAB", det->x_lab);
	if (det->y_lab)
		add_str_field (det->header, "Y_LAB", det->y_lab);
	if (det->z_lab)
		add_str_field (det->header, "Z_LAB", det->z_lab);
		
	/* add comment fields */
	for (i=0; i<DET_MAX_COMMENT; ++i)
		if (det->comment[i])
			add_str_field (det->header, "COMMENT", det->comment[i]);

	/* add unknown fields */
	for (i=0; i<DET_MAX_UNKNOWN; ++i)
		if (det->unknown[i].label && det->unknown[i].value)
			add_str_field (det->header, det->unknown[i].label, det->unknown[i].value);
		else if (det->unknown[i].label)
			add_empty_field (det->header, det->unknown[i].label);

	add_end_field (det->header);

	/* if there is room, add the comment about seeking to offset <nnnn> */
	sl = strlen (det->header);
	if (sl < DET_HEADER_SIZE - 80)
	{
		p = &(det->header[sl]);
		sprintf (p, "Note: Raw data starts at offset %d in the file." CRLF,
			DET_HEADER_SIZE);
	}
	det->header_state = built;
	return 0;
}

int
det_write_header (DETFILE *det)
{
	if (det->mode == 'r')
	{
		fprintf (stderr, "detfile.c: Tried to write to a file opened for reading.\n");
		return -1;
	}

	/* If we aren't at the beginning of the file already, we need to see back
	 * to the start.
	 */
	if (det->fp == NULL)
    {
		/* Dummy file version, so do not try to write header */
		fprintf(stderr, "detfile.c: Tried to write header for a dummy (fp == NULL) detfile\n");
		return 0;
    }

	if (det->filepos != 0)
	{
		fprintf(stderr, "detfile.c: Tried to seek back, cannot in this version\n");
	}

	if (det->header_state != built)
		det_build_header(det);
	/* Write the header. */
	if (fwrite (det->header, DET_HEADER_SIZE, 1, det->fp) != 1)
		return -1;
	det->filepos = DET_HEADER_SIZE;
	det->header_state = written;
	return 0;
}

int
det_set_date_time (DETFILE *det)
{
	char buffer[256];
	struct tm *timeptr;
	time_t t;

	if (det->date)	free (det->date);
	if (det->time)	free (det->time);

	time (&t);
	timeptr = localtime (&t);
	strftime (buffer, 256, "%X", timeptr);
	det->time = strsave (buffer);
	strftime (buffer, 256, "%a %d %b %Y", timeptr);
	det->date = strsave (buffer);

	return 0;
}
int
det_write (DETFILE *det, int x, int y, int width, int height, void *data, int local_type)
{
	return det_write3d (det, x, y, 0 , width, height, 1, data, local_type);
}

int
det_write3d (DETFILE *det, int x, int y, int t, int width, int height, int num_t, void *data, int local_type)
{
	int needs_bswap, needs_conversion, line, frame;
	long filepos;

	u_char *ptr = (u_char *) data;
	size_t elem_size=det_size_of (det->data_type);

	if (!det->data_type || !det->width || !det->height || !det->num_t || det->mode == 'r')
	{
		fprintf (stderr, "detfile.c: width/height/data_type fields must be set, and file must be writable.\n");
		return -1;
	}
	if (x < 0 || y < 0 || t < 0 || width <= 0 || height <= 0 || num_t < 0 ||
		x >= det->width || y >= det->height || t >= det->num_t ||
		x + width > det->width || y + height >  det->height || t+num_t > det->num_t)
	{
		fprintf (stderr, "detfile.c: image size (%d,%d, %d), requested area (%d,%d,%d)-(%d,%d,%d).\n",
			det->width, det->height, det->num_t, x, y, t, x+width-1, y+height-1, t+num_t-1);
		return -1;
	}

	/* Write header first */
	if (det->header_state != written)
		det_write_header(det);

	what_conversion (local_type, det->data_type, &needs_bswap, &needs_conversion);
/*	if (y == 0)
		printf("Detfile needs_bswap=%d, needs_itof=%d, needs_ftoi=%d\n",
				needs_bswap, needs_itof, needs_ftoi);
*/
	if (needs_bswap || needs_conversion)
	{
		if (det->priv == NULL)
			det->priv = malloc (width * elem_size);
		if (det->priv == NULL) return -1;
		for (frame=t; frame<t+num_t; frame++)
		{
			for (line = y; line < y+height; ++line)
			{
				if (needs_conversion)
					do_conversion(local_type, det->data_type, (void *)det->priv, (void *)ptr, width);
				else
					memcpyfast (det->priv, ptr, width, elem_size);

				if (needs_bswap)
				{
					if (elem_size == 2)
						det_bswap2 (det->priv, det->priv, width);
					else if (elem_size == 8)
						det_bswap8 (det->priv, det->priv, width);
					else
						det_bswap (det->priv, det->priv, width);
				}

				filepos = DET_HEADER_SIZE + (frame*det->width*det->height + line * det->width + x) *
											elem_size;
				if (det->filepos != filepos)
				{
					if (fseek (det->fp, filepos, SEEK_SET) != 0)
						{ return -1; }
					det->filepos = filepos;
				}
				if (fwrite (det->priv, width * elem_size, 1, det->fp)
					!= 1)
					{ return -1; }
				det->filepos += width * elem_size;
				ptr += width * elem_size;
			}
		}
	}
	else
	{
		/* No conversion is necessary. Spot the case where we can write the
		 * data in one go. Else write it line-at-a-time.
		 */
		if (width==det->width && (num_t==1 || height==det->height))
		{
			filepos = DET_HEADER_SIZE + (t*det->width*det->height + y * det->width + x) * elem_size;
			if (det->filepos != filepos)
			{
				if (fseek (det->fp, filepos, SEEK_SET) != 0)
					return -1;
				det->filepos = filepos;
			}
			if (fwrite (data, ((long)num_t) * height * width * elem_size,
							1, det->fp) != 1)
				return -1;
			det->filepos += ((long)num_t) * width * height * elem_size;
		}
		else
		{
			for (frame=t; frame<t+num_t; frame++)
			{
				for (line = y; line < y + height; ++line)
				{
					filepos = DET_HEADER_SIZE + (frame*det->width*det->height + line * det->width + x) * elem_size;
					if (det->filepos != filepos)
					{
						if (fseek (det->fp, filepos, SEEK_SET) != 0)
							return -1;
						det->filepos = filepos;
					}
					if (fwrite (ptr, width * elem_size, 1, det->fp)
							!= 1)
						return -1;
					det->filepos += width * elem_size;
					ptr += width * elem_size;
				}
			}
		}
	}

	return 0;
}

int
det_read (DETFILE *det, int x, int y, int width, int height, void *data, int local_type)
{
	return det_read3d (det, x, y, 0, width, height, 1, data, local_type);
}
int
det_read3d (DETFILE *det, int x, int y, int t, int width, int height, int num_t, void *data, int local_type)
{
	int needs_bswap, needs_conversion, frame, line;
	long filepos;
	u_char *ptr = (u_char *) data;
	int rc;
	size_t elem_size = det_size_of(det->data_type);
	if (det->mode == 'w')
	{
		fprintf (stderr, "detfile.c: Cannot read a write-only file.\n");
		return -1;
	}
	if (x < 0 || y < 0 || t < 0 || width <= 0 || height <= 0 || num_t < 0 ||
		x >= det->width || y >= det->height || t >= det->num_t ||
		x + width > det->width || y + height >  det->height || t+num_t > det->num_t)
	{
		fprintf (stderr, "detfile.c: image size (%d,%d, %d), requested area (%d,%d,%d)-(%d,%d,%d).\n",
			det->width, det->height, det->num_t, x, y, t, x+width-1, y+height-1, t+num_t-1);
		return -1;
	}

	what_conversion (det->data_type, local_type, &needs_bswap, &needs_conversion);

	/* Read the data into the memory buffer first, and perform conversion
	 * on it later. We assume here that all data types are the same size.
	 * Spot the case where we can read the whole lot in at once.
	 */
	 if ( det_size_of (local_type) < det_size_of (det->data_type))
	 {
		fprintf(stderr, "det_read3d: Cannot scale down data size on read; File type=%d=%s, size=%d, Local_type=%d=%s, size=%d\n", det->data_type, dttoa(det->data_type), det_size_of(det->data_type),
					local_type, dttoa(local_type), det_size_of(local_type) );
			return -1;
	 }

	if (width == det->width && (num_t == 1 || height == det->height))
	{
		filepos = DET_HEADER_SIZE + (t*det->width*det->height + y * det->width + x) * elem_size;
		if (det->filepos != filepos)
		{
			if (fseek (det->fp, filepos, SEEK_SET) != 0)
            {
				fprintf(stderr,"fseek(..,%d,%d) returns error in det_read (full lines)\n",
						filepos, SEEK_SET );
				return -1;
            }
		}
		if ((rc=fread (data, ((long)num_t) * width * height * elem_size, 1,
					det->fp)) != 1)
        {
			fprintf(stderr,"fread(.., %ld, 1, ..) returns error (%d) in det_read (full lines)\n",
				num_t* width * height * elem_size, rc);
			fprintf(stderr, "Current position = %d\n", ftell(det->fp));
			return -1;
        }
		det->filepos += width * height * elem_size;
	}
	else
	{
		for (frame=t; frame<t+num_t; frame++)
		{
			for (line=y; line<y+height; ++line)
			{
				filepos = DET_HEADER_SIZE + (frame*det->width*det->height + line * det->width + x) * elem_size;
				if (det->filepos != filepos)
				{
					if (fseek (det->fp, filepos, SEEK_SET) != 0)
		            {
						fprintf(stderr,"fseek(..,%ld,%d) returns error in det_read (partial lines)\n",
							filepos, SEEK_SET );
						return -1;
		            }
					det->filepos = filepos;
				}
				if (fread (ptr, width * elem_size, 1, det->fp)!= 1)
	        	{
						fprintf(stderr,"fread(.., %d, 1, ..) returns error in det_read (partial lines)\n",
							width * elem_size);	
					return -1;
	        	}
				det->filepos += width * elem_size;
				ptr += width * elem_size;
			}
		}
	}
/*	printf("Got data, calling conversions\n"); */
	if (needs_bswap)
	{
		if (elem_size == 2)
			det_bswap2 (data, data, width * height);
		else if (elem_size == 8)
			det_bswap8 (data, data, width * height);
		else
			det_bswap (data, data, width * height);
	}
	if (needs_conversion)
		do_conversion(det->data_type, local_type, data, data, width * height);

	return 0;
}

int 
det_size_of (int data_type)
{
	switch (data_type)
	{
	case DET_DATA_INTEL_IEEEFLOAT:
		return 4;
	case DET_DATA_INTEL_INT32:
		return 4;
	case DET_DATA_INTEL_IEEEDOUBLE:
		return 8;
	case DET_DATA_INTEL_INT16:
		return 2;
	case DET_DATA_INTEL_UINT16:
		return 2;
	case DET_DATA_MOTOROLA_IEEEFLOAT:
		return 4;
	case DET_DATA_MOTOROLA_IEEEDOUBLE:
		return 8;
	case DET_DATA_MOTOROLA_INT32:
		return 4;
	case DET_DATA_MOTOROLA_INT16:
		return 2;
	case DET_DATA_MOTOROLA_UINT16:
		return 2;
	case 0:
		return 1;
	default:
		return 1;
	}
	return 4;
}

void
det_dump_header (DETFILE *det, FILE *fp)
{
	int count, i;

	fprintf (fp,
		"DET file header contents:\n"
		"  path = %d  mode = %c  filepos = %d header state= %d\n"
		"  width = %d  height = %d  num_t = %d type = %s\n"
		"  title = %s\n"
		"  date = %s\n"
		"  time = %s\n",
			fileno (det->fp), det->mode, det->filepos,
			det->header_state,
			det->width, det->height, det->num_t,
			dttoa (det->data_type),
			det->title ? det->title : "(none)",
			det->date ? det->date : "(none)",
			det->time ? det->time : "(none)");
	fprintf (fp, "  comments:\n");
	for (count=0, i=0; i<DET_MAX_COMMENT; ++i)
		if (det->comment[i])
			fprintf (fp, "    %s\n", det->comment[i]), count ++;
	if (count == 0)
		fprintf (fp, "    (none)\n");
	fprintf (fp, "  unknown tags:\n");
	for (count=0, i=0; i<DET_MAX_UNKNOWN; ++i)
		if (det->unknown[i].label && det->unknown[i].value)
			fprintf (fp, "    %s=%s\n", det->unknown[i].label, det->unknown[i].value),
			count ++;
		else if (det->unknown[i].label)
			fprintf (fp, "    %s\n", det->unknown[i].label),
			count ++;
	if (count == 0)
		fprintf (fp, "    (none)\n");
}

#endif

static char *
strsave (char *text)
{
	char *p;
	if (text == NULL) return NULL;
	p = malloc (strlen (text) + 1);
	if (p)
		strcpy (p, text);
	return p;
}

/*----- private functions -----*/

static int
atodt (char *s)
{
	while (*s && isspace (*s))
		s++;
	if (!*s) return 0;
	if (strcmp (s, "intel_ieeefloat") == 0 ||
		strcmp (s, "INTEL_IEEEFLOAT") == 0)
		return DET_DATA_INTEL_IEEEFLOAT;
	else if (strcmp (s, "intel_ieeedouble") == 0 ||
		strcmp (s, "INTEL_IEEEDOUBLE") == 0)
		return DET_DATA_INTEL_IEEEDOUBLE;
	else if (strcmp (s, "intel_int32") == 0 ||
			strcmp (s, "INTEL_INT32") == 0)
		return DET_DATA_INTEL_INT32;
	else if (strcmp (s, "intel_int16") == 0 ||
			strcmp (s, "INTEL_INT16") == 0)
		return DET_DATA_INTEL_INT16;
	else if (strcmp (s, "intel_uint16") == 0 ||
			strcmp (s, "INTEL_UINT16") == 0)
		return DET_DATA_INTEL_UINT16;
	else if (strcmp (s, "motorola_ieeefloat") == 0 ||
			 strcmp (s, "MOTOROLA_IEEEFLOAT") == 0)
		return DET_DATA_MOTOROLA_IEEEFLOAT;
	else if (strcmp (s, "motorola_ieeedouble") == 0 ||
			 strcmp (s, "MOTOROLA_IEEEDOUBLE") == 0)
		return DET_DATA_MOTOROLA_IEEEDOUBLE;
	else if (strcmp (s, "motorola_int32") == 0 ||
			strcmp (s, "MOTOROLA_INT32") == 0)
		return DET_DATA_MOTOROLA_INT32;
	else if (strcmp (s, "motorola_int16") == 0 ||
			strcmp (s, "MOTOROLA_INT16") == 0)
		return DET_DATA_MOTOROLA_INT16;
	else if (strcmp (s, "motorola_uint16") == 0 ||
			strcmp (s, "MOTOROLA_UINT16") == 0)
		return DET_DATA_MOTOROLA_UINT16;
	else
		return 0;
}


static void
free_det (DETFILE *det)
{
	int i;

	if (det->title)		free (det->title);
	if (det->date)		free (det->date);
	if (det->time)		free (det->time);
	for (i=0; i<DET_MAX_COMMENT; ++i)
		if (det->comment[i])
			free (det->comment[i]);
	for (i=0; i<DET_MAX_UNKNOWN; ++i)
	{
		if (det->unknown[i].label)
			free (det->unknown[i].label);
		if (det->unknown[i].value)
			free (det->unknown[i].value);
	}
	free (det->priv);		/* Free private buffer if used */
	free (det);
}

static int
read_header (char *header, DETFILE *det)
{
	char *start, *end, *next, *equals;
	char *dummy;
	char label[64], value[256];
	int quit = 0, i, cnum = 0, unum = 0;

	i = strlen (DET_MAGIC);
	if (strncmp (header, DET_MAGIC, i) != 0)
	{
		fprintf (stderr, "detfile.c: Not a DLDGIFF format file (magic incorrect).\n");
		return -1;
	}
	if (header[i] == CR && header[i+1] == LF)
		start = &header[i+2];
	else if (header[i] == CR || header[i] == LF)
		start = &header[i+1];
	else
	{
		fprintf (stderr, "detfile.c: Extra rubbish after DLDGIFF magic identifier.\n");
		return -1;
	}

	while (!quit && start < &header[DET_HEADER_SIZE])
	{
#if DEBUG
		printf ("detfile header offset %d\n", start - header);
#endif

		/* search for the CR-LF at the end of this line */
		end = start;
		while (end[0] && end[0] != CR && end[0] != LF)
			end ++;
		if (end[0] == CR && end[1] == LF)		/* CR-LF ending */
			next = end+2;
		else if (end[0] == CR || end[0] == LF)
			next = end+1;
		else
			return -1;

		/* mark the end of the line */
		end[0] = 0;

#if DEBUG
		printf ("detfile line '%s'\n", start);
#endif

		/* look for the '=' sign, if present */
		equals = strchr (start, '=');

		/* search forwards over the whitespace until we find the label
		 * and copy this label to a private buffer
		 */
		while (*start && isspace (*start))
			start ++;
		for (i=0; !isspace (*start) && *start != '='; ++i)
			label[i] = *start++;
		label[i] = 0;

		/* if there is a value section present, copy this to a private buffer
		 * also
		 */
		if (equals)
			strcpy (value, equals+1);

#if DEBUG
		printf ("detfile read '%s=%s'\n",
					label, value ? value : "(none)");
#endif

		/* see if we recognize this label/value pair */
		if (!equals)
		{
			if (strcmp (label, "end") == 0 || strcmp (label, "END") == 0)
				quit = 1;
			else
			{
				det->unknown[unum].label = strsave (label);
				det->unknown[unum].value = NULL;
				unum ++;
			}
		}
		else
		{
			if (strcmp (label, "numx") == 0 || strcmp (label, "NUMX") == 0)
				det->width = atoi (value);
			else if (strcmp (label, "numy") == 0 || strcmp (label, "NUMY") == 0)
				det->height = atoi (value);
			else if (strcmp (label, "numt") == 0 || strcmp (label, "NUMT") == 0)
				det->num_t = atoi (value);
			else if (strcmp (label, "aspect") == 0 || strcmp (label, "ASPECT") == 0)
				det->aspect = strtod(value, &dummy);
			else if (strcmp (label, "datatype") == 0 || strcmp (label, "DATATYPE") == 0)
				det->data_type = atodt (value);
			else if (strcmp (label, "date") == 0 || strcmp (label, "DATE") == 0)
				det->date = strsave (value);
			else if (strcmp (label, "time") == 0 || strcmp (label, "TIME") == 0)
				det->time = strsave (value);
			else if (strcmp (label, "x_lab") == 0 || strcmp (label, "X_LAB") == 0)
				det->x_lab = strsave (value);
			else if (strcmp (label, "y_lab") == 0 || strcmp (label, "Y_LAB") == 0)
				det->y_lab = strsave (value);
			else if (strcmp (label, "z_lab") == 0 || strcmp (label, "Z_LAB") == 0)
				det->z_lab = strsave (value);
			else if (strcmp (label, "title") == 0 || strcmp (label, "TITLE") == 0)
				det->title = strsave (value);
			else if (strcmp (label, "comment") == 0 || strcmp (label, "COMMENT") == 0)
				det->comment[cnum++] = strsave (value);
			else
			{
				det->unknown[unum].label = strsave (label);
				det->unknown[unum].value = strsave (value);
				unum ++;
			}
		}

		/* move to the start of the next line */
		start = next;
	}

	if (!quit)				/* no 'END' statement found */
	{
		fprintf (stderr, "detfile.c: no 'end' found in header.\n");
		return -1;
	}
	det->header_state = hs_read;
	return 0;
}

#ifndef DETFILE_FOR_LIBXSPRESS3

static char *
dttoa (int dt)
{
	switch (dt)
	{
	case DET_DATA_INTEL_IEEEFLOAT:
		return "INTEL_IEEEFLOAT";
	case DET_DATA_INTEL_INT32:
		return "INTEL_INT32";
	case DET_DATA_INTEL_IEEEDOUBLE:
		return "INTEL_IEEEDOUBLE";
	case DET_DATA_INTEL_INT16:
		return "INTEL_INT16";
	case DET_DATA_INTEL_UINT16:
		return "INTEL_UINT16";
	case DET_DATA_MOTOROLA_IEEEFLOAT:
		return "MOTOROLA_IEEEFLOAT";
	case DET_DATA_MOTOROLA_IEEEDOUBLE:
		return "MOTOROLA_IEEEDOUBLE";
	case DET_DATA_MOTOROLA_INT32:
		return "MOTOROLA_INT32";
	case DET_DATA_MOTOROLA_INT16:
		return "MOTOROLA_INT16";
	case DET_DATA_MOTOROLA_UINT16:
		return "MOTOROLA_UINT16";
	case 0:
		return "(none)";
	default:
		return "(unknown)";
	}
}

static void
add_int_field (char *header, char *label, int value)
{
	int sl = strlen (header);
	char *p = &header[sl];
	char valuestr[32];

	sprintf (valuestr, "%d", value);

	if (sl + strlen (label) + strlen (valuestr) + 3 + 5 <= DET_HEADER_SIZE)
		sprintf (p, "%s=%d" CRLF, label, value);
	else
		fprintf (stderr, "detfile.c: %s=%d not written (header too long).\n",
			label, value);
}

static void
add_double_field (char *header, char *label, double value)
{
	int sl = strlen (header);
	char *p = &header[sl];
	char valuestr[80];

	sprintf (valuestr, "%g", value);

	if (sl + strlen (label) + strlen (valuestr) + 3 + 5 <= DET_HEADER_SIZE)
		sprintf (p, "%s=%g" CRLF, label, value);
	else
		fprintf (stderr, "detfile.c: %s=%g not written (header too long).\n",
			label, value);
}

static void
add_str_field (char *header, char *label, char *value)
{
	int sl = strlen (header);
	char *p = &header[sl];

	if (sl + strlen (label) + strlen (value) + 3 + 5 <= DET_HEADER_SIZE)
		sprintf (p, "%s=%s" CRLF, label, value);
	else
		fprintf (stderr, "detfile.c: \"%s=%s\" not written (header too long).\n",
			label, value);
}

static void
add_multi_hex_field (char *header, char *label, int num, int *value)
{
	int sl = strlen (header);
	char *p;
	char valuestr[9*DETFILE_MAX_MULTI_HEX+2];
	int i;

	if (num < DETFILE_MAX_MULTI_HEX)
	{
		fprintf(stderr, "detfile.c: add_multi_hex_field: num values=%d > maximum=%d\n", num, DETFILE_MAX_MULTI_HEX);
		return;
	}
	p = valuestr;
	for (i=0; i<num; i++)
	{
		if (i == num-1)
			p += sprintf(p, "%X", value[i]);
		else
			p += sprintf(p, "%X,", value[i]);
	}
	p = header+sl;
	if (sl + strlen (label) + strlen (valuestr) + 3 + 5 <= DET_HEADER_SIZE)
		sprintf (p, "%s=%s" CRLF, label, valuestr);
	else
		fprintf (stderr, "detfile.c: %s=%s not written (header too long).\n",
			label, valuestr);
}

static void
add_empty_field (char *header, char *label)
{
	int sl = strlen (header);
	char *p = &header[sl];

	if (sl + strlen (label) + 2 + 5 <= DET_HEADER_SIZE)
		sprintf (p, "%s" CRLF, label);
	else
		fprintf (stderr, "detfile.c: %s not written (header too long).\n",
			label);
}

static void
add_end_field (char *header)
{
	int sl = strlen (header);
	char *p = &header[sl];

	if (sl + 5 <= DET_HEADER_SIZE)
		sprintf (p, "END" CRLF);
	else
		fprintf (stderr, "detfile.c: ** warning ** 'END' record could not be written.\n");
}

static void
what_conversion (int local_type, int file_type, int *needs_bswap, int *needs_conversion)
{
	*needs_bswap = ((local_type ^ file_type) & 16) == 16;

	local_type &= 15;
	file_type &= 15;

	if ((local_type == DET_DATA_INTEL_INT16 || local_type == DET_DATA_INTEL_UINT16) && (file_type == DET_DATA_INTEL_INT16 || file_type == DET_DATA_INTEL_UINT16))
		*needs_conversion = 0;	 /* Cannot do any better than leave alone for 16 bit ints */
	else 
		*needs_conversion = local_type != file_type;
}

/* This version of memcpy is guaranteed to read aligned integers using
 * long reads if the pointers passed are aligned. This lets us read
 * DET files straight from DL200.
 */
static void
memcpyfast (void *dest, void *src, int nwords, int elemsize)
{
	switch (elemsize)
	{
	case 1:
		memcpy (dest, src, nwords);
		break;
	case 2: {
		u_int16 *d = (u_int16 *) dest;
		u_int16 *s = (u_int16 *) src;
		for (; nwords; --nwords)
			*d++ = *s++;
	}	break;
	case 8:
		nwords *= 2;
		/* FALLTHROUGH */
	case 4: {
		u_int32 *d = (u_int32 *) dest;
		u_int32 *s = (u_int32 *) src;
		for (; nwords; --nwords)
			*d++ = *s++;
	}	break;
	default:
		memcpy (dest, src, nwords * elemsize);
		break;
	}
}

#if defined(_MPF68K) || defined(_MPFPOWERPC)
#if defined(_MPF68K)
_asm (
"det_bswap		movem.l a0-a1,-(sp)						\n"
"			move.l d1,a0				Source		\n"
"			move.l d0,a1				Destination	\n"
"			move.l 12(sp),d1			Count in words \n"
"			bra bswap20								\n"
"bswap10		move.l (a0)+,d0				Read	\n"
"			rol.w #8,d0					Swap bytes	\n"
"			swap d0									\n"
"			rol.w #8,d0								\n"
"			move.l d0,(a1)+				Write		\n"
"bswap20		subq.l #1,d1				Loop	\n"
"			bpl.b bswap10							\n"
"			movem.l (sp)+,a0-a1						\n"
"			rts										\n"
);
#else

_asm(

"*	r3		dst							\n"
"*	r4		src							\n"
"*	r5	count							\n"
"*   r6 = 0 for index					\n"
"det_bswap									\n"
"		andi. r6,r6,0					\n"
"		cmplwi cr0,r5,0					\n"
"		beq bswap20						\n"
"bswap10	lwz r0,0(r4)				\n"
"		stwbrx r0,r6,r3					\n"
"		addi r3,r3,4					\n"
"		addi r4,r4,4					\n"
"		addi r5,r5,-1					\n"
"		cmplwi cr0,r5,0					\n"
"		bne bswap10						\n"
"bswap20	blr							\n"
);
#endif
#else
/* Non OS9 , hopefull gcc will work out if it is power pc */
#if defined(__powerpc__)
void det_bswap(u_int32 *source, u_int32 *dest, int num)
{
asm("eieio\n"
    "\t andi. 6,6,0\n"
    "DetSwabL1%=:	lwzx 0,6,%0\n"
    "\t stwbrx 0,6,%1\n"
	"\t	addi 6,6,4\n"
	"\t	addi %2,%2,-1\n"
	"\t	cmplwi cr0,%2,0\n"
 	"\t	bne DetSwabL1%=\n"
    "\t	\n" : : "r" (source),"r" (dest),"r" (num): "6","0","cc");
}
#else
/* C veriosn for non OS9 */
void det_bswap(u_int32 *dst, u_int32 *src, int n)
{
	char *cs;
	char *cd;
	char t;

	cs =(char *) src;
	cd = (char *) dst;

	while (n-- > 0)
    {
		t     = cs[0];
		cd[0] = cs[3];
		cd[3] = t;
		t     = cs[1];
		cd[1] = cs[2];
		cd[2] = t;
		cs += 4;
		cd += 4;
    }
}
#endif
#endif

void det_bswap2(u_int16 *dst, u_int16 *src, int n)
{
	char *cs;
	char *cd;
	char t;

	cs =(char *) src;
	cd = (char *) dst;

	while (n-- > 0)
    {
		t     = cs[0];
		cd[0] = cs[1];
		cd[1] = t;
		cs += 2;
		cd += 2;
    }
}

void det_bswap8(void *dst, void *src, int n)
{
	char *cs;
	char *cd;
	char t;

	cs =(char *) src;
	cd = (char *) dst;

	while (n-- > 0)
    {
		t     = cs[0];
		cd[0] = cs[7];
		cd[7] = t;
		t     = cs[1];
		cd[1] = cs[6];
		cd[6] = t;
		t     = cs[2];
		cd[2] = cs[5];
		cd[5] = t;
		t     = cs[3];
		cd[3] = cs[4];
		cd[4] = t;
		cs += 8;
		cd += 8;
    }
}

static void
itof (float *dest, int32 *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}

static void
itod (double *dest, int32 *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}

static void
ftoi (int32 *dest, float *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}

static void
dtoi (int32 *dest, double *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}

static void
dtof (float *dest, double *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}

static void
ftod (double *dest, float *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}

static void
stof (float *dest, int16 *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}

static void
ustof (float *dest, u_int16 *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}

static void
stod (double *dest, int16 *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}

static void
ustod (double *dest, u_int16 *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}

static void
stoi (int32 *dest, int16 *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}
static void
ustoi (int *dest, u_int16 *src, int nwords)
{
	int i;
	dest += nwords-1;
	src += nwords-1;
	for (i=0; i<nwords; ++i)
		*dest-- = *src--;
}

static void
itos (int16 *dest, int32 *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}
static void
itous (u_int16 *dest, int32 *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}


static void
ftos (int16 *dest, float *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}
static void
ftous (u_int16 *dest, float *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}

static void
dtos (int16 *dest, double *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}
static void
dtous (u_int16 *dest, double *src, int nwords)
{
	int i;

	for (i=0; i<nwords; ++i)
		*dest++ = *src++;
}

static int do_conversion(int src_type, int dest_type, void *dest, void *src, int nwords)
{
	switch (src_type & 15)
	{
	case DET_DATA_INTEL_IEEEFLOAT:
		switch (dest_type & 15)
		{
		case DET_DATA_INTEL_IEEEFLOAT:
			break;
		case DET_DATA_INTEL_INT32:
			ftoi((int32 *)dest, (float *)src, nwords);
			break;
		case DET_DATA_INTEL_IEEEDOUBLE:
			ftod((double *)dest, (float *)src, nwords);
			break;
		case DET_DATA_INTEL_INT16:
			ftos((int16 *)dest, (float *)src, nwords);
			break;
		case DET_DATA_INTEL_UINT16:
			ftous((u_int16 *)dest, (float *)src, nwords);
			break;
		}
		break;

	case DET_DATA_INTEL_INT32:
		switch (dest_type & 15)
		{
		case DET_DATA_INTEL_IEEEFLOAT:
			itof((float *)dest, (int32 *)src, nwords);
			break;
		case DET_DATA_INTEL_INT32:
			break;
		case DET_DATA_INTEL_IEEEDOUBLE:
			itod((double *)dest, (int32 *)src, nwords);
			break;
		case DET_DATA_INTEL_INT16:
			itos((int16 *)dest, (int32 *)src, nwords);
			break;
		case DET_DATA_INTEL_UINT16:
			itous((u_int16 *)dest, (int32 *)src, nwords);
			break;
		}
		break;

	case DET_DATA_INTEL_IEEEDOUBLE:
		switch (dest_type & 15)
		{
		case DET_DATA_INTEL_IEEEFLOAT:
			dtof((float *)dest, (double *)src, nwords);
			break;
		case DET_DATA_INTEL_INT32:
			dtoi((int32 *)dest, (double *)src, nwords);
			break;
		case DET_DATA_INTEL_IEEEDOUBLE:
			break;
		case DET_DATA_INTEL_INT16:
			dtos((int16 *)dest, (double *)src, nwords);
			break;
		case DET_DATA_INTEL_UINT16:
			dtous((u_int16 *)dest, (double *)src, nwords);
			break;
		}
		break;

	case DET_DATA_INTEL_INT16:
		switch (dest_type & 15)
		{
		case DET_DATA_INTEL_IEEEFLOAT:
			stof((float *)dest, (int16 *)src, nwords);
			break;
		case DET_DATA_INTEL_INT32:
			stoi((int32 *)dest, (int16 *)src, nwords);
			break;
		case DET_DATA_INTEL_IEEEDOUBLE:
			stod((double *)dest, (int16 *)src, nwords);
			break;
		case DET_DATA_INTEL_INT16:
			break;
		case DET_DATA_INTEL_UINT16:
			break;
		}
		break;
	case DET_DATA_INTEL_UINT16:
		switch (dest_type & 15)
		{
		case DET_DATA_INTEL_IEEEFLOAT:
			ustof((float *)dest, (u_int16 *)src, nwords);
			break;
		case DET_DATA_INTEL_INT32:
			ustoi((int32 *)dest, (u_int16 *)src, nwords);
			break;
		case DET_DATA_INTEL_IEEEDOUBLE:
			ustod((double *)dest, (u_int16 *)src, nwords);
			break;
		case DET_DATA_INTEL_INT16:
			break;
		case DET_DATA_INTEL_UINT16:
			break;
		}
		break;
	}
	return 0;
}


#endif
