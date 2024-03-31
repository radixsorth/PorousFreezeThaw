/***********************************************\
* PPM format import module                      *
* selector function version                     *
* (C) 2005 Pavel Strachota                      *
* file: PPM_importS.c                           *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include "_endian.h"
#include <stdlib.h>
#include <stdio.h>
#include "mathspec.h"

static const int BUFFER_SIZE=65536;

static _conststring_ header="P6";

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

int PPM_importS(void (*R)(int,SCALAR_DATA), void (*G)(int,SCALAR_DATA), void (*B)(int,SCALAR_DATA), SCALAR_TYPE type, int max_read, _conststring_ path)
/*
imports the data from the Portable PixMap format (PPM).
( for more information, see PPM_import() )

this version uses the selector functions 'R(), G(), B()' which take an integer argument specifying the ordinal
number (starting from 0) of the imported data element (value) and a SCALAR_DATA union that contains this value.
In particular, this means that the R (, G, B) color component of the i-th pixel of the image is passed to the
selector function R()  (, G(), B()) at the time when it is called with the first argument equal to i.

These functions should then process the retrieved value or store it somewhere (not in linear arrays - if
you want to do this, you'd better use the PPM_import() function, which is faster). The appropriate member
of the union set by PPM_importS() (and thus the appropriate type of the actually imported data) is determined
by the 'type' parameter.

NOTE:	In contrast to PPM_import(), passing NULL as R, G or B is not allowed here. If you want to
	have one color element ignored in the whole image, you must define a selector function that
	does nothing.

return codes:
RGB triplets read	success
-1			file access error
-2			invalid format
-3			'max_read' <= 0 or one or more of 'R', 'G', 'B' are NULL pointers
-4			not enough memory for buffer allocation

*/
{
	FILE * infile;

	typedef void (* SelectorFunc)(int,SCALAR_DATA);
	SelectorFunc data[3] = {R,G,B};

	char * buf;

	size_t bytes_read;
	int i=0,j=0;
	int c;		/* component selector */
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

	if(max_read<=0 || R==NULL || G==NULL || B==NULL) return(-3);

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
			for(c=0;c<3;c++) {	/* alternately load R,G,B components */

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
				(data[c])(j,d);

				if(i >= bytes_read) {		/* buffer ready for read */
					i=0;
					/* this also exits the while loop: */
					if((bytes_read=fread(buf,1,BUFFER_SIZE,infile))<=0) { max_read=0; break; }
				}
			}
			j++;
		}

	fclose(infile);
	free(buf);
	return(j);
}
