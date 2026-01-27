/*
	Extended DET-file library
	By Richard W.M. Jones
*/

#ifndef _DETFILE_H_
#define _DETFILE_H_

/* set LOCAL BYTE ORDERING correctly for the local machine */
#define INTEL_ORDER			1234
#define MOTOROLA_ORDER		4321
/* Changed for PC */
#if !defined(__CYGWIN__) && !defined(__MINGW32__) && (defined(__LINUX__) || defined(__linux__) || defined(USE_GTK))
#include <endian.h>
#if __BYTE_ORDER == __BIG_ENDIAN 
#define LOCAL_BYTE_ORDERING	MOTOROLA_ORDER
#else
#define LOCAL_BYTE_ORDERING	INTEL_ORDER
#endif
#else
#if defined(__386__) || defined (__i386__) || defined (__i486)
#define LOCAL_BYTE_ORDERING	INTEL_ORDER
#else
#define LOCAL_BYTE_ORDERING	MOTOROLA_ORDER
#endif
#endif
#define DET_HEADER_SIZE		2048/* fixed size of the header					*/
#define DET_MAX_COMMENT		32	/* max. lines of comment allowed			*/
#define DET_MAX_UNKNOWN		32	/* max. unknown fields allowed				*/
#define DET_MAX_XSP_CARDS   10	/* maximum number of XSPRESS3/4 cards       */

#define DET_MAGIC \
	"Daresbury Laboratory Detector Group Image File Format"

/* Don't change the following type numbering system, unless you change `what_
 * conversion()' to match.
 This was 1,2 10, 11 - which was all to cock I think WIH 25/6/96
		(not 0x11, 0x12)
 */
#define DET_DATA_INTEL_IEEEFLOAT		1	/* these types are supported in files*/
#define DET_DATA_INTEL_INT32			2
#define DET_DATA_INTEL_IEEEDOUBLE		3
#define DET_DATA_INTEL_UINT16			4
#define DET_DATA_INTEL_INT16			5
#define DET_DATA_MOTOROLA_IEEEFLOAT		0x11
#define DET_DATA_MOTOROLA_INT32			0x12
#define DET_DATA_MOTOROLA_IEEEDOUBLE 	0x13
#define DET_DATA_MOTOROLA_UINT16		0x14
#define DET_DATA_MOTOROLA_INT16			0x15

#if LOCAL_BYTE_ORDERING == MOTOROLA_ORDER
#define DET_LOCAL_FLOAT		0x11	/* these types can be passed to read/write	*/
#define DET_LOCAL_INT32		0x12
#define DET_LOCAL_DOUBLE	0x13	/* these types can be passed to read/write	*/
#else
#if LOCAL_BYTE_ORDERING == INTEL_ORDER
#define DET_LOCAL_FLOAT		1	/* these types can be passed to read/write	*/
#define DET_LOCAL_INT32		2
#define DET_LOCAL_DOUBLE	3
#else
#error
#endif
#endif

#ifdef __cplusplus 
extern "C" {
#endif


typedef struct {
	FILE *fp;					/* points to file							*/
	char mode;					/* access mode (r, w, u)					*/
	long filepos;				/* position within file						*/
	enum {empty, hs_read, built, written} header_state;
								/* State of header block in det struct 		*/
	int width;					/* width of image							*/
	int height;					/* height of image							*/
	double aspect;				/* Pixel Aspect ratio if known				*/
	int data_type;				/* data type (see above)					*/
	char *x_lab, *y_lab, *z_lab;/* X, Y and Z labels						*/
	char *title;				/* title string								*/
	char *date, *time;			/* date and time strings					*/
	char *comment[DET_MAX_COMMENT];	/* list of lines of comments			*/
	struct {					/* unknown fields in the file header		*/
		char *label, *value;
	} unknown[DET_MAX_UNKNOWN];
/* Note: char * fields are malloced, so if they are changed, then the old
 * memory must generally be freed. Comment and unknown fields are set to
 * NULL if they have are empty.
 */
 	char header[DET_HEADER_SIZE];	/* Space to build the header			*/
	void *priv;						/* Used for write conversion 			*/
	int num_t;						/* Number of time frames in 3-d det file */
	enum {DetScopeNone, DetScopeXspress3, DetScopeXspress3Mini, DetScopeNgpdADQ14, DetScopeNgpdZynq} scope_data_type;
	struct { int chan_sel[2], src_sel[2], alt[2]; } scope_data[DET_MAX_XSP_CARDS];
} DETFILE;
DETFILE *	det_open (const char *filename, const char *mode);
DETFILE *	det_fpopen (FILE *fp, const char *mode);
void		det_close (DETFILE *);
void		det_fpclose (DETFILE *); /* frees DETFILE struct, doesn't close fp*/
int			det_write_header (DETFILE *);
int			det_write (DETFILE *, int x, int y, int width, int height, void *data, int local_type);
int			det_read (DETFILE *, int x, int y, int width, int height, void *data, int local_type);
int			det_set_date_time (DETFILE *);
int			det_size_of (int data_type);
void		det_dump_header (DETFILE *, FILE *);
int			det_read3d (DETFILE *det, int x, int y, int t, int width, int height, int num_t, void *data, int local_type);
int			det_write3d (DETFILE *det, int x, int y, int t, int width, int height, int num_t, void *data, int local_type);

#ifdef __cplusplus 
}
#endif

/* char *		strsave (char *);	 use this when setting string fields */

#endif /* _DETFILE_H_ */
