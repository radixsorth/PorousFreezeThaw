/***********************************************\
* PPM format export module                      *
* selector function version                     *
* (C) 2005-2006 Pavel Strachota			*
* file: PPM_exportS.c                           *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include "_endian.h"
#include <stdlib.h>
#include <stdio.h>
#include "mathspec.h"

static const int BUFFER_SIZE=65536;	/* must be even in order to export the PPMs with maxcolor>255 correctly */

static WORD RoundW(double x)
/*
rounds 'x' to the nearest WORD value.
not designed to round values exceeding the range of WORD
*/
{
	double r=floor(x);
	if(x-r>=0.5) r+=1;
	return((WORD)r);
}

int PPM_exportS(SCALAR_DATA (* R)(int), SCALAR_DATA (* G)(int), SCALAR_DATA (* B)(int), SCALAR_TYPE type, int width, int height, unsigned short maxcolor, _conststring_ comment, _conststring_ path)
/*
exports the data to the Portable PixMap format (PPM)
( for more information, see PPM_export() )

this version uses the selector functions 'R(), G(), B()' which should return a SCALAR_DATA union
where the i-th value to be exported is stored. Here 'i' denotes the integer argument passed to 'R(),
G(), B()' which specifies the ordinal number of the processed data element (value) (i goes from 0 to
width*height-1). This means that the i-th pixel of the image has the color components
(R(i),G(i),B(i)). The appropriate member of the union read by PPM_exportS() (and thus the
appropriate type of the actually exported data) is determined by the 'type' parameter.

NOTE:	In contrast to PPM_export(), passing NULL as R, G or B is not allowed here. If you want to
	have one color element zero in the whole image, you must define a selector function that
	always returns 0.

return codes:
0	success
-1	file access error
-2	invalid input data
-4	not enough memory for buffer allocation
-5	disk full (write error)
*/
{
	FILE * outfile;

	typedef SCALAR_DATA (* SelectorFunc)(int);
	SelectorFunc data[3] = {R,G,B};

	/* total point data */
	int point_data = width*height;

	unsigned char * buf;

	int i=0,j=0;
	int c;		/* component selector */

	/* auxiliary variables used for export calculation */
	double v;
	WORD w;
	unsigned char * LByte=(unsigned char *)&w;
	unsigned char * HByte=LByte+1;
	if(_BYTEORDER != __LITTLE_ENDIAN) {	/* Big Endian byte order */
		LByte++; HByte--;
	}

	if(point_data==0 || R==NULL || G==NULL || B==NULL || maxcolor<1) return(-2);

	buf = (unsigned char *)malloc(BUFFER_SIZE);
	if(! buf) return(-4);

	outfile=fopen(path,"w");
	if(!outfile) { free(buf); return(-1); }

	/* write complete header to the beginning of the file */
	fprintf(outfile,"P6\n# %s\n%d	%d\n%hu\n",comment,width,height,maxcolor);

	/*
	The following algorithm is primarily designed to be simple, not fast.
	(There are many conditions that have to be checked upon each iteration.)
	The export time is not crucial for our purposes anyway.
	*/
	while(point_data--) {
		for(c=0;c<3;c++) {	/* alternately save R,G,B components */
			switch(type) {
				case SCALAR_int:	v=(double) ((data[c])(j).int_data)/maxcolor; break;
				case SCALAR_FLOAT:	v=(data[c])(j).FLOAT_data; break;
				case SCALAR_double:	v=(data[c])(j).double_data; break;
			}
			if(v>1) v=1;
			if(v<0) v=0;

			/* CIE Rec. 709 gamma correction */

			w=RoundW ( ((v<=0.018)?(4.5*v):(1.099*pow(v,0.45)-0.099)) * maxcolor );

			/* without gamma correction: */
			/* w=(WORD) (v*maxcolor); */

			/*
			NOTE: if maxcolor<256, we will save the low byte only.
			We don't need a special conversion such as byte_data=(unsigned char)(....); for that case
			*/

			if(maxcolor>255) { buf[i]=*HByte; i++; }
			buf[i]=*LByte; i++;

			if(i == BUFFER_SIZE) {		/* buffer ready for write */
				i=0;
				if(fwrite(buf,1,BUFFER_SIZE,outfile)<BUFFER_SIZE) { fclose(outfile); free(buf); return(-5); }
			}
		}
		j++;
	}

	/*
	write the rest of the buffer
	(after that, i will be nonzero if fwrite() returns something else than i, which happens in case of error)
	*/
	if(i) i-=fwrite(buf,1,i,outfile);

	/*
	generally, i may not be nonzero even in case of a write error. This is because of so called
	FULL buffering of file streams. We detect the possible write error on the next line.
	*/
	if(i==0) i=fflush(outfile);
	fclose(outfile);
	free(buf);
	return((i!=0)?-5:0);
}
