/***********************************************\
* PPM format export module                      *
* (C) 2005-2006 Pavel Strachota			*
* file: PPM_export.c                            *
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

int PPM_export(void *R, void *G, void *B, SCALAR_TYPE type, int width, int height, unsigned short maxcolor, _conststring_ comment, _conststring_ path)
/*
exports data in the R, G, B arrays to the Portable PixMap format (PPM)
(includes standard gamma correction due to CIE Rec. 709)

NOTE:	PPM is a special type of PNM (Portable AnyMap format). The extension of the output
	file can be one of { .pnm, .ppm } and the file will be displayed well. The .pnm
	extension is recommended.

The R, G, B color components of each pixel are read from three separate arrays of the same 'type'.
Formally, if we neglect the data type, the i-th pixel of the image has a color with components
(R[i],G[i],B[i]).

You can specify the same array for more color components (if you make it for all three, the result
will be a grayscale image). If you specify NULL instead of a pointer to an array, zero will be
substituted for the appropriate color component.

The data should be in the range 0 to 1 for SCALAR_FLOAT and SCALAR_double types
and in the range 0 to 'maxcolor' for SCALAR_int type. The resulting grayscale range
is 0 to 'maxcolor'. Values out of range are automatically adjusted.

NOTE:	For maxcolor >255, each color component for each pixel is coded in two bytes. Some viewers
	do not dislpay this format correctly, but the command line tools like pnmtopng work well with it.

return codes:
0	success
-1	file access error
-2	invalid input data
-4	not enough memory for buffer allocation
-5	disk full (write error)
*/
{
	FILE * outfile;

	void *data[3] = {R,G,B};	/* for NULL checking only */
	int * int_data[3]={(int *)R,(int *)G,(int *)B};
	FLOAT * FLOAT_data[3]={(FLOAT *)R,(FLOAT *)G,(FLOAT *)B};
	double * double_data[3]={(double *)R,(double *)G,(double *)B};

	/* total point data */
	int point_data = width*height;

	unsigned char * buf;

	int i=0;
	int c;		/* component selector */

	/* auxiliary variables used for export calculation */
	double v;
	WORD w;
	unsigned char * LByte=(unsigned char *)&w;
	unsigned char * HByte=LByte+1;
	if(_BYTEORDER != __LITTLE_ENDIAN) {	/* Big Endian byte order */
		LByte++; HByte--;
	}

	buf = (unsigned char *)malloc(BUFFER_SIZE);
	if(! buf) return(-4);

	if(point_data==0 || maxcolor<1) return(-2);

	outfile=fopen(path,"w");
	if(!outfile) return(-1);

	/* write complete header to the beginning of the file */
	fprintf(outfile,"P6\n# %s\n%d	%d\n%hu\n",comment,width,height,maxcolor);

	/*
	The following algorithm is primarily designed to be simple, not fast.
	(There are many conditions that have to be checked upon each iteration.)
	The export time is not crucial for our purposes anyway.
	*/
	while(point_data--)
		for(c=0;c<3;c++) {	/* alternately save R,G,B components */

			if(data[c] == NULL)	/* assume zero value if the pointer to the processed component is NULL */
				w=0;
			else {
				switch(type) {
					case SCALAR_int:	v=(double) (*(int_data[c]++))/maxcolor; break;
					case SCALAR_FLOAT:	v=*(FLOAT_data[c]++); break;
					case SCALAR_double:	v=*(double_data[c]++); break;
				}
				if(v>1) v=1;
				if(v<0) v=0;

				/* CIE Rec. 709 gamma correction */

				w=RoundW ( ((v<=0.018)?(4.5*v):(1.099*pow(v,0.45)-0.099)) * maxcolor );

				/* without gamma correction: */
				/* w=(WORD) (v*maxcolor); */
			}

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
