/***********************************************\
* PGM format import module                      *
* selector function version                     *
* (C) 2005 Pavel Strachota                      *
* file: PGM_importS.c                           *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include "_endian.h"
#include <stdlib.h>
#include <stdio.h>
#include "mathspec.h"

static const int BUFFER_SIZE=65536;	/* must be even in order to import the PGMs with maxcolor>255 correctly */

static _conststring_ header="P5";

static int RoundI(double x)
/*
rounds 'x' to the nearest WORD value.
not designed to round values exceeding the range of WORD
*/
{
	double r=floor(x);
	if(x-r>=0.5) r+=1;
	return((int)r);
}

int PGM_importS(void (*data)(int,SCALAR_DATA),SCALAR_TYPE type,int max_read,_conststring_ path)
/*
imports the data from the grayscale Portable GrayMap format (PGM)
( for more information, see PGM_import() )

this version uses the selector function 'data()' which takes an integer argument specifying the ordinal
number (starting from 0) of the imported data element (value) and a SCALAR_DATA union that contains this value.
The 'data()' function should then process the retrieved value or store it somewhere (not in a linear array - if
you want to do this, you'd better use the PGM_import() function, which is faster). The appropriate member
of the union set by PGM_importS() (and thus the appropriate type of the actually imported data) is determined
by the 'type' parameter.

return codes:
values read	success
-1		file access error
-2		invalid format
-3		'max_read' <= 0 or 'data' is a null pointer
-4		not enough memory for buffer allocation
*/
{
	FILE * infile;

	char * buf;

	size_t bytes_read;
	int i=0,j=0;
	unsigned short maxcolor;

	SCALAR_DATA d;

	/* auxiliary variables used for import calculation */
	double v;
	WORD w=0;	/* initialization is necessary for maxcolor<256: HByte must be zero! */
	unsigned char * LByte=(unsigned char *)&w;
	unsigned char * HByte=LByte+1;
	if(_BYTEORDER != __LITTLE_ENDIAN) {	/* Big Endian byte order */
		LByte++; HByte--;
	}

	if(max_read<=0 || data==NULL) return(-3);

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
		while(max_read--) {

			if(maxcolor>255) { *HByte=buf[i]; i++; }
			*LByte=buf[i]; i++;

			/* CIE Rec. 709 inverse gamma transformation */

			v= (double)w/maxcolor;
			v= (v<0.081)?(v/4.5):pow(((v+0.099)/1.099),1/0.45);

			switch(type) {
				case SCALAR_int:	d.int_data=RoundI(v*maxcolor); break;
				case SCALAR_FLOAT:	d.FLOAT_data=v; break;
				case SCALAR_double:	d.double_data=v; break;
			}
			data(j,d);
			j++;

			if(i >= bytes_read) {		/* buffer ready for read */
				i=0;
				if((bytes_read=fread(buf,1,BUFFER_SIZE,infile))<=0) break;
			}
		}

	fclose(infile);
	free(buf);
	return(j);
}
