/*
	Error reporting module.
	By Richard W.M. Jones.

	Header file.
*/

#ifndef _ERRORS_H_
#define _ERRORS_H_

extern int exit_on_error;			/* exit if there is an error			*/
extern int reporting;				/* reporting level, 0..3				*/
extern FILE *logfile;				/* log for errors (default is NULL)		*/

#ifdef __cplusplus
extern "C" {
#endif
extern void mprintf (int level, const char *fs, ...);
extern void msg_discard(void);
#ifdef __cplusplus
}
#endif


#define MP_ERR		-1				/* error message: never exit			*/
#define MP_ERREX	-2				/* print this, exit if `exit_on_error'	*/
#define MP_FILE		-3				/* always send this message to log file	*/
#define MP_PRINT	-4				/* always send this message to screen	*/
#define MP_INFO		MP_PRINT		/* synonym								*/

#endif /* _ERRORS_H_ */
