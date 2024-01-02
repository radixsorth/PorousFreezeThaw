/***********************************************\
* PPM format to SCR_Color import module		*
* (C) 2005 Pavel Strachota			*
* file: SCR_PPMimport.cc			*
\***********************************************/

#include "screen.h"

#include "_endian.h"
#include <stdlib.h>
#include <stdio.h>

static const int BUFFER_SIZE=65536;	/* must be even in order to import the PPMs with maxcolor>255 correctly */

static _conststring_ header="P6";

int SCR_PPMimport(SCR_Color *data,_conststring_ path)
/*
imports the data from the Portable PixMap format (PPM) into an SCR_Color array
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

	float RGBdata[3];

	char * buf;

	size_t bytes_read;
	int i=0,j=0;
	int c;		/* component selector */
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
			for((c=0,j++);c<3;c++) {	/* alternately load R,G,B components */

				if(maxcolor>255) { *HByte=buf[i]; i++; }
				*LByte=buf[i]; i++;

				RGBdata[c] = (float)w/maxcolor;

				if(i >= bytes_read) {		/* buffer ready for read */
					i=0;
					/* this also exits the while loop: */
					if((bytes_read=fread(buf,1,BUFFER_SIZE,infile))<=0) break;
				}
			}
			(data++)->SetRGB(RGBdata[0],RGBdata[1],RGBdata[2]);
		}

	fclose(infile);
	free(buf);
	return(j);
}
