/************************************************************\
* Output on memory streams and rank0-only ..printf functions *
* (C) 2011 Pavel Strachota                                   *
* file: mprintf.c                                            *
\************************************************************/

#include "common.h"
#include "mprintf.h"

#include <string.h>
#include <stdlib.h>

#ifdef PARA
	#include <mpi.h>

	static int MPIrank=-1;
#endif


void Mprintf(const char * format,...)
/* Like printf(), but if MPI is enabled, prints its output in the master rank only. */
{
	#ifdef PARA
		if(MPIrank < 0) MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
		if(MPIrank != 0) return;
	#endif

	va_list fields;
	va_start(fields, format);

	vprintf(format,fields);
}

void Mfprintf(FILE * stream, const char * format,...)
/* Like Mprintf(), but prints to both stdout and the log file specified by 'stream' (if stream!=NULL). */
{
	va_list fields, fields2;
	/*
	pointer to variable arguments - depending on compiler,
	it may be damaged when passed to vprintf()
	*/
	va_start(fields, format);
	/* we need another pointer for the call to vmprintf() */
	va_start(fields2, format);

	#ifdef PARA
		if(MPIrank < 0) MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
		if(MPIrank != 0) return;
	#endif

	vprintf(format,fields);
	if(stream) {
		vfprintf(stream,format,fields2);
		fflush(stream);
	}
}

MEMSTREAM * mopen(size_t max_write)
/*
open a new memory stream. 'max_write' is the maximum size of output written to the stream
by one single call to mprintf() (including the trailing NULL character).
The user must make sure that this limit is not exceeded as the unsafe (but portable)
vsprintf() function is used to produce the output.
*/
{
	MEMSTREAM * s;
	s = (MEMSTREAM*)malloc(sizeof(MEMSTREAM));
	if(s==NULL) return(NULL);

	s->buffer = NULL;
	s->size = 0;
	s->max_write_size = max_write;

	return(s);
}

void mclose(MEMSTREAM * stream)
/* close and destroy a memory stream */
{
	if(stream->buffer) free(stream->buffer);
	free(stream);
}

int vmprintf(MEMSTREAM * stream, const char * format, va_list fields)
/*
formatted output to memory stream (va_list argument list form).
The memory buffer is expanded as necessary. Returns the number of characters
written or zero if the (re)allocation of the output buffer fails.
*/
{
	char * tmp_buffer;
	char * new_buffer;
	size_t tmp_size, new_size;

	tmp_buffer = (char *)malloc(stream->max_write_size);
	if(tmp_buffer == NULL) return(0);

	tmp_size = vsprintf(tmp_buffer,format,fields);

	new_size = stream->size + tmp_size;
	if(stream->size == 0)
		new_buffer = (char *)malloc(new_size+1);	/* always +1 for the trailing zero */
	else
		new_buffer = (char *)realloc(stream->buffer, new_size+1);

	if(new_buffer == NULL) return(0);

	strcpy(new_buffer + stream->size, tmp_buffer);
	stream->size = new_size;
	stream->buffer = new_buffer;
	free(tmp_buffer);

	return(tmp_size);
}

int mprintf(MEMSTREAM * stream, const char * format,...)
/*
formatted output to memory stream. The memory buffer is expanded as necessary.
Returns the number of characters written or zero if the (re)allocation
of the output buffer fails.
*/
{
	va_list fields;
	va_start(fields, format);

	return(vmprintf(stream,format,fields));
}

void Mmprintf(MEMSTREAM * stream, const char * format,...)
/* Like Mprintf(), but prints to both stdout and the memory stream buffer specified by 'stream' (if stream!=NULL). */
{
	va_list fields, fields2;
	/*
	pointer to variable arguments - depending on compiler,
	it may be damaged when passed to vprintf()
	*/
	va_start(fields, format);
	/* we need another pointer for the call to vmprintf() */
	va_start(fields2, format);

	#ifdef PARA
		if(MPIrank < 0) MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
		if(MPIrank != 0) return;
	#endif

	vprintf(format,fields);
	if(stream) vmprintf(stream,format,fields2);
}
