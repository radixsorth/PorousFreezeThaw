/***********************************************\
* PPM format from SCR_Color export module	*
* (C) 2005-2006 Pavel Strachota			*
* file: SCR_PPMexport.cc			*
\***********************************************/

#include "screen.h"

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

int SCR_PPMexport(SCR_Color * data, int width, int height, unsigned short maxcolor, _conststring_ comment, _conststring_ path)
/*
exports data in the SCR_Color array to the Portable PixMap format (PPM)
(does not perform standard gamma correction (due to CIE Rec. 709) since the exported image is intended
to look exactly like what appears onscreen if the image stored in the SCR_Color array is displayed)

NOTE:	PPM is a special type of PNM (Portable AnyMap format). The extension of the output
	file can be one of { .pnm, .ppm } and the file will be displayed well. The .pnm
	extension is recommended.

NOTE:	'data' should be a linear array containing the image pixel colors ordered by rows, starting at the
	top left pixel.

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

	// SCR_Color class member pointers to the R(), G(), B() methods
	typedef float (SCR_Color::*RGBComponentGetter)();
	RGBComponentGetter Component[3]={&SCR_Color::R, &SCR_Color::G, &SCR_Color::B};

	/* total point data */
	int point_data = width*height;

	unsigned char * buf;

	int i=0;
	int c;		/* component selector */

	/* auxiliary variables used for export calculation */
	WORD w;
	unsigned char * LByte=(unsigned char *)&w;
	unsigned char * HByte=LByte+1;
	if(_BYTEORDER != __LITTLE_ENDIAN) {	/* Big Endian byte order */
		LByte++; HByte--;
	}

	buf = (unsigned char *)malloc(BUFFER_SIZE);
	if(! buf) return(-4);

	if(point_data==0 || maxcolor<1 || data==NULL) return(-2);

	outfile=fopen(path,"w");
	if(!outfile) return(-1);

	/* write complete header to the beginning of the file */
	fprintf(outfile,"P6\n# %s\n%d	%d\n%hu\n",comment,width,height,maxcolor);

	/*
	The following algorithm is primarily designed to be simple, not fast.
	(There are many conditions that have to be checked upon each iteration.)
	The export time is not crucial for our purposes anyway.
	*/
	while(point_data--) {
		for(c=0;c<3;c++) {	/* alternately save R,G,B components */

			w=RoundW ( (data->*(Component[c]))() * maxcolor );

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
		data++;
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
