/***********************************************\
* PGM format to SCR_Color import module		*
* (C) 2005 Pavel Strachota			*
* file: SCR_PGMimport.cc			*
\***********************************************/

#include "screen.h"

#include "_endian.h"
#include <stdlib.h>
#include <stdio.h>

static const int BUFFER_SIZE=65536;	/* must be even in order to import the PGMs with maxcolor>255 correctly */

static _conststring_ header="P5";

int SCR_PGMimport(SCR_Color *data,_conststring_ path)
/*
imports the data from the grayscale Portable GrayMap format (PGM) into an SCR_Color array
(does not remove standard gamma correction since the image is intended to be displayed
without any modifications)

there must be enough space in the 'data' array to store the whole image. You may use the
PNM_GetDim() function from the dataIO library if you need to know the image dimensions
in advance.

return codes:
pixels read	success
-1		file access error
-2		invalid format
-3		'data' is a null pointer
-4		not enough memory for buffer allocation
*/
{
	FILE * infile;

	char * buf;

	size_t bytes_read;
	int i=0,j=0;
	unsigned short maxcolor;

	/* auxiliary variables used for import calculation */
	WORD w=0;	/* initialization is necessary for maxcolor<256: HByte must be zero! */
	unsigned char * LByte=(unsigned char *)&w;
	unsigned char * HByte=LByte+1;
	if(_BYTEORDER != __LITTLE_ENDIAN) {	/* Big Endian byte order */
		LByte++; HByte--;
	}

	if(data==NULL) return(-3);

	buf = (char *)malloc(BUFFER_SIZE);
	if(! buf) return(-4);

	/* -------- begin header processing */

	infile=fopen(path,"r");
	if(!infile) { free(buf); return(-1); }

	fgets(buf,len(header)+1,infile);
	if(! match(buf, header)) { fclose(infile); return(-2); }

	fscanf(infile,"%s",buf);		/* width ... ignored*/
	while((i=instr(buf,"#",1,SENSITIVE))>0) {
		fscanf(infile,"%*[^\n]");
		if(i==1) fscanf(infile,"%s",buf);
		else { buf[i]=0; break; }
	}

	fscanf(infile,"%s",buf);		/* height ... ignored*/
	while((i=instr(buf,"#",1,SENSITIVE))>0) {
		fscanf(infile,"%*[^\n]");
		if(i==1) fscanf(infile,"%s",buf);
		else { buf[i]=0; break; }
	}

	fscanf(infile,"%s",buf);		/* maxcolor */
	while((i=instr(buf,"#",1,SENSITIVE))>0) {
		fscanf(infile,"%*[^\n]");
		if(i==1) fscanf(infile,"%s",buf);
		else { buf[i]=0; break; }
	}
	maxcolor=val(buf);

	/*
	The following algorithm is primarily designed to be simple, not fast.
	(There are many conditions that have to be checked upon each iteration.)
	The import time is not crucial for our purposes anyway.
	*/

	fread(buf,1,1,infile);		/* skip the whitespace after maxcolor value */
	if((bytes_read=fread(buf,1,BUFFER_SIZE,infile))>0)
		while(bytes_read>0) {

			if(maxcolor>255) { *HByte=buf[i]; i++; }
			*LByte=buf[i]; i++;

			(data++)->SetGrey((float)w/maxcolor);
			j++;

			if(i >= bytes_read) {		/* buffer ready for read */
				i=0;
				bytes_read=fread(buf,1,BUFFER_SIZE,infile);
			}
		}

	fclose(infile);
	free(buf);
	return(j);
}
