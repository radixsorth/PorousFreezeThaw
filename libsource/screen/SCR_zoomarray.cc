/*******************************************************\
* SCR_Color array display function			*
* (Zoom-enabled version with bilinear interpolation)	*
* (C) 2005 Pavel Strachota				*
* file: SCR_zoomarray.cc				*
\*******************************************************/

#include "screen.h"
#include "mathspec.h"

#define D(x,y) (data[(y)*data_width+(x)])

int SCR_ZoomArray(int x, int y, int width, int height, SCR_Color * data, int data_width, int data_height)
/*
displays the data in the 'data' array as an image of the given 'data_width' and 'data_height'.
so that it fits into the rectangle of the given 'width' and 'height' with its top left corner
at the coordinates 'x' and 'y'. Bilinear interpolation is used to smoothen the resutling image
displayed.

Handles the screen borders so that a memory error does not occur.

The screen must be locked before a call to SCR_ZoomArray()

return codes:
0	success
-1	invalid input data: NOTE: height and width must be >1
*/
{
	int i,j,k;
	int top=y;
	int lft=x;
	int btm=(y+height>SCR_GetScreen()->h)?SCR_GetScreen()->h:y+height;
	int rgt=(x+width>SCR_GetScreen()->w)?SCR_GetScreen()->w:x+width;

	float rx,ry;	// coordinates in the image data resulting from backward resampling
	int rxi,ryi;	// the greatest interger coordinate in the image data that is less than or
			// equal to rx, ry.
	float dx,dy;	// dx=rx-rxi, dy=ry-ryi

	float eps=0.001;// eps should avoid reading of an array element out of range (at the end of each line
			// and at the bottom line).
			// For example, at the bottom line, ryi should not accuretely be equal to data_height-1,
			// since in that case, also the array elements at the nonexistent line D(...,ryi+1)
			// (where ryi+1==data_height) would be involved in the calculation. However, the result
			// would seem correct, since those elements would be multiplied by a zero coefficient.

	float dw=data_width-1-eps;
	float dh=data_height-1-eps;

	// we need to shift the image pointer if the top left point does not fit in the screen

	if(data==NULL || width<=1 || height <=1) return(-1);

	// note that the following calculation with SCR_color variables is slow, since there are SDL functions
	// called within every operation to synchronize the Uint32 structure member and the floating point r,g,b
	// structure members. For more information, refer to the 'screen.cc' source file.

	width--; height--;
	for(j=top;j<btm;j++) {
		for(i=lft;i<rgt;i++) {
			rx=((float)(i-x)*dw)/width;
			ry=((float)(j-y)*dh)/height;
			rxi=(int)floorf(rx);
			ryi=(int)floorf(ry);
			dx=rx-rxi;
			dy=ry-ryi;
			SCR_putpixel(i,j, (1-dy) * ((1-dx)*D(rxi,ryi)+dx*D(rxi+1,ryi)) + dy * ((1-dx)*D(rxi,ryi+1)+dx*D(rxi+1,ryi+1)) );
		}
	}
	return(0);
}
