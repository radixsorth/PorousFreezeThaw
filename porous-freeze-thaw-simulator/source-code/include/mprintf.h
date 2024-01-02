/************************************************************\
* Output on memory streams and rank0-only ..printf functions *
* (C) 2011 Pavel Strachota                                   *
* file: mprintf.h                                            *
\************************************************************/

#if !defined __mprintf
#define __mprintf

#include <stdarg.h>
#include <stdio.h>

typedef struct {
	char * buffer;		/* current address of the buffer. The buffer is reallocated upon each output */
	size_t size;		/* total number of bytes written to the stream */
	size_t max_write_size;	/* maximum number of bytes writable at once */
} MEMSTREAM;

void Mprintf(const char * format,...);
/* Like printf(), but if MPI is enabled, prints its output in the master rank only. */

void Mfprintf(FILE * stream, const char * format,...);
/* Like Mprintf(), but prints to both stdout and the log file specified by 'stream' (if stream!=NULL). */

MEMSTREAM * mopen(size_t max_write);
/*
open a new memory stream. 'max_write' is the maximum size of output written to the stream
by one single call to mprintf() (including the trailing NULL character).
The user must make sure that this limit is not exceeded as the unsafe (but portable)
vsprintf() function is used to produce the output.
*/

void mclose(MEMSTREAM * stream);
/* close and destroy a memory stream */

int vmprintf(MEMSTREAM * stream, const char * format, va_list fields);
/*
formatted output to memory stream (va_list argument list form).
The memory buffer is expanded as necessary. Returns the number of characters
written or zero if the (re)allocation of the output buffer fails.
*/

int mprintf(MEMSTREAM * stream, const char * format,...);
/*
formatted output to memory stream. The memory buffer is expanded as necessary.
Returns the number of characters written or zero if the (re)allocation
of the output buffer fails.
*/

void Mmprintf(MEMSTREAM * stream, const char * format,...);
/* Like Mprintf(), but prints to both stdout and the memory stream buffer specified by 'stream' (if stream!=NULL). */

#endif		/* __mprintf */
