/***********************************************\
* SCR_Color array display function		*
* (C) 2005 Pavel Strachota			*
* file: SCR_disparray.cc			*
\***********************************************/

#include "screen.h"

int SCR_DispArray(int x, int y, SCR_Color * data, int width, int height)
/*
displays the data in the 'data' array as an image of the given 'width' and 'height'.
Handles the screen borders so that a memory error does not occur.

The screen must be locked before a call to SCR_DispArray()

return codes:
0	success
-1	invalid input data
*/
{
	int i,j,k;
	int top=y;
	int lft=x;
	int btm=(y+height>SCR_GetScreen()->h)?SCR_GetScreen()->h:y+height;
	int rgt=(x+width>SCR_GetScreen()->w)?SCR_GetScreen()->w:x+width;

	// we need to shift the image pointer if the top left point does not fit in the screen
	if(top<0) { data-=(top*width); top=0; }
	if(lft<0) { data-=lft; lft=0; }

	if(data==NULL || width<=0 || height <=0) return(-1);

	for(j=top;j<btm;j++) {
		k=0;
		for(i=lft;i<rgt;i++)
			SCR_putpixel(i,j,data[k++]);
		data+=width;
	}
	return(0);
}
