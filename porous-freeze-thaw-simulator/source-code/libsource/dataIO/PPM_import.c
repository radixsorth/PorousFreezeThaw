/***********************************************\
* PPM format import module                      *
* (C) 2005 Pavel Strachota                      *
* file: PPM_import.c                            *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include "_endian.h"
#include <stdlib.h>
#include <stdio.h>
#include "mathspec.h"

static const int BUFFER_SIZE=65536;	/* must be even in order to import the PPMs with maxcolor>255 correctly */

static _conststring_ header="P6";

static int RoundI(double x)
/*
rounds 'x' to the nearest int value.
not designed to round values exceeding the range of int
*/
{
	double r=floor(x);
	if(x-r>=0.5) r+=1;
	return((int)r);
}

int PPM_import(void *R, void *G, void *B, SCALAR_TYPE type, int max_read, _conststring_ path)
/*
imports the data from the Portable PixMap format (PPM) into three separate arrays that
represent the R, G, B color components of each pixel. If you pass NULL to some of the
R, G, B pointers, the reading of the appropriate color component is skipped.
(removes standard gamma correction due to CIE Rec. 709)

the imported data will be in the range 0 to 1 for SCALAR_FLOAT and SCALAR_double types
and in the range 0 to 'maxcolor' for SCALAR_int type.

the dimensions of the image are ignored, you have to use PNM_GetDim if you need them.
Simply all the data or the colors of the first max_read pixels are read (that is, the total
count of scalar values read is max_read*3).
Control of the array dimensions is up to user's responsibility.

return codes:
RGB triplets read	success
-1			file access error
-2			invalid format
-3			'max_read' <= 0
-4			not enough memory for buffer allocation
*/
{
	FILE * infile;

	void *data[3] = {R,G,B};	/* for NULL checking only */
	int * int_data[3]={(int *)R,(int *)G,(int *)B};
	FLOAT * FLOAT_data[3]={(FLOAT *)R,(FLOAT *)G,(FLOAT *)B};
	double * double_data[3]={(double *)R,(double *)G,(double *)B};

	char * buf;

	size_t bytes_read;
	int i=0,j=0;
	int c;		/* component selector */
	unsigned short maxcolor;

	/* auxiliary variables used for import calculation */
	double v;
	WORD w=0;	/* initialization is necessary for maxcolor<256: HByte must be zero! */
	unsigned char * LByte=(unsigned char *)&w;
	unsigned char * HByte=LByte+1;
	if(_BYTEORDER != __LITTLE_ENDIAN) {	/* Big Endian byte order */
		LByte++; HByte--;
	}

	if(max_read<=0) return(-3);

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
		while(max_read--)
			for((c=0,j++);c<3;c++) {	/* alternately load R,G,B components */

				if(maxcolor>255) { *HByte=buf[i]; i++; }
				*LByte=buf[i]; i++;
				if(data[c] != NULL) {	/* skip the processed component if the destination array is NULL */

					/* CIE Rec. 709 inverse gamma transformation */

					v= (double)w/maxcolor;
					v= (v<0.081)?(v/4.5):pow(((v+0.099)/1.099),1/0.45);

					switch(type) {
						case SCALAR_int:	(*(int_data[c]++))=RoundI(v*maxcolor); break;
						case SCALAR_FLOAT:	*(FLOAT_data[c]++)=v; break;
						case SCALAR_double:	*(double_data[c]++)=v; break;
					}
				}

				if(i >= bytes_read) {		/* buffer ready for read */
					i=0;
					/* this also exits the while loop: */
					if((bytes_read=fread(buf,1,BUFFER_SIZE,infile))<=0) { max_read=0; break; }
				}
			}

	fclose(infile);
	free(buf);
	return(j);
}
