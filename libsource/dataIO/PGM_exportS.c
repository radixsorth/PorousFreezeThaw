/***********************************************\
* PGM format export module                      *
* selector function version                     *
* (C) 2005-2006 Pavel Strachota			*
* file: PGM_exportS.c                           *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include "_endian.h"
#include <stdlib.h>
#include <stdio.h>
#include "mathspec.h"

static const int BUFFER_SIZE=65536;	/* must be even in order to export the PGMs with maxcolor>255 correctly */

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

int PGM_exportS(SCALAR_DATA (* data)(int),SCALAR_TYPE type,int width,int height,unsigned short maxcolor,_conststring_ comment,_conststring_ path)
/*
exports the data to the Portable GrayMap format (PGM)
( for more information, see PGM_export() )

this version uses the selector function 'data()' which should return a SCALAR_DATA union where the i-th value to be exported
is stored. Here 'i' denotes the integer argument passed to 'data()' which specifies the ordinal number of the processed
data element (value) (i goes from 0 to x_dim*y_dim-1). The appropriate member of the union read by PGM_exportS() (and thus
the appropriate type of the actually exported data) is determined by the 'type' parameter.

return codes:
0	success
-1	file access error
-2	invalid input data
-4	not enough memory for buffer allocation
-5	disk full (write error)
*/
{
	FILE * outfile;

	/* total point data */
	int point_data = width*height;

	unsigned char * buf;

	int i=0,j=0;

	/* auxiliary variables used for export calculation */
	double v;
	WORD w;
	unsigned char * LByte=(unsigned char *)&w;
	unsigned char * HByte=LByte+1;
	if(_BYTEORDER != __LITTLE_ENDIAN) {	/* Big Endian byte order */
		LByte++; HByte--;
	}

	if(point_data==0 || data==NULL || maxcolor<1) return(-2);

	buf = (unsigned char *)malloc(BUFFER_SIZE);
	if(! buf) return(-4);

	outfile=fopen(path,"w");
	if(!outfile) { free(buf); return(-1); }

	/* write complete header to the beginning of the file */
	fprintf(outfile,"P5\n# %s\n%d	%d\n%hu\n",comment,width,height,maxcolor);

	/*
	The following algorithm is primarily designed to be simple, not fast.
	(There are many conditions that have to be checked upon each iteration.)
	The export time is not crucial for our purposes anyway.
	*/
	while(point_data--) {
		switch(type) {
			case SCALAR_int:	v=(double) (data(j).int_data)/maxcolor; break;
			case SCALAR_FLOAT:	v=data(j).FLOAT_data; break;
			case SCALAR_double:	v=data(j).double_data; break;
		}
		j++;
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
