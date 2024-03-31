/***********************************************\
* Scalar data to SCR_Color array converter	*
* (C) 2005 Pavel Strachota			*
* file: SCR_scalarcvt.cc			*
\***********************************************/

#include "screen.h"
#include "mathspec.h"

int SCR_ScalarCvtGrey(void * source, SCALAR_TYPE type, SCR_Color * target, int count,bool gamma)
/*
converts 'count' elements from the 'source' array representing the pixels' intensity to
the SCR_Color format and stores them in the 'target' array.

the source data should be in the range 0 to 1 for SCALAR_FLOAT and SCALAR_double types
and in the range 0 to 255 for SCALAR_int type (which corresponds to the SDL Uint8 type range)
Floating point values out of range are automatically adjusted to the nearest valid value,
which is a property of the SCR_Color class.

NOTE:	By default, gamma correction due to CIE Rec. 709 is performed on each value in order
	to make the resulting image look like it was loaded directly from a PGM image, which
	the scalar array was previously exported to. However, you can explicitly turn the gamma
	correction off by passing false to the 'gamma' parameter.

return codes:
0	success
-1	invalid input data
*/
{
	int i;

	int * int_data=(int *)source;
	FLOAT * FLOAT_data=(FLOAT *)source;
	double * double_data=(double *)source;

	float v;

	if(count<=0 || source==NULL || target==NULL) return(-1);

	if(gamma)
		for(i=0;i<count;i++) {
			switch(type) {
				case SCALAR_int:	v=(float) (*(int_data++))/255; break;
				case SCALAR_FLOAT:	v=*(FLOAT_data++); break;
				case SCALAR_double:	v=*(double_data++);
			}
			if(v<0) v=0;

			/* CIE Rec. 709 gamma correction */

			(target++)->SetGrey((v<=0.018)?(4.5*v):(1.099*powf(v,0.45)-0.099));
		}
	else
		switch(type) {
			case SCALAR_int:	for(i=0;i<count;i++) (target++)->SDL_SetGrey(*(int_data++)); break;
			case SCALAR_FLOAT:	for(i=0;i<count;i++) (target++)->SetGrey(*(FLOAT_data++)); break;
			case SCALAR_double:	for(i=0;i<count;i++) (target++)->SetGrey(*(double_data++));
		}

	return(0);
}

int SCR_ScalarConvert(void * R, void * G, void * B, SCALAR_TYPE type, SCR_Color * target, int count, bool gamma)
/*
converts 'count' elements from the 'R', 'G', 'B', arrays representing the pixels' color components to
the SCR_Color format and stores them in the 'target' array.

the source data should be in the range 0 to 1 for SCALAR_FLOAT and SCALAR_double types
and in the range 0 to 255 for SCALAR_int type (which corresponds to the SDL Uint8 type range)
Floating point values out of range are automatically adjusted to the nearest valid value,
which is a property of the SCR_Color class.

NOTE:	By default, gamma correction due to CIE Rec. 709 is performed on each value in order
	to make the resulting image look like it was loaded directly from a PPM image, which
	the scalar arrays were previously exported to. However, you can explicitly turn the
	gamma correction off by passing false to the 'gamma' parameter.

return codes:
0	success
-1	invalid input data
*/
{
	int i;

	int * int_R=(int *)R;
	FLOAT * FLOAT_R=(FLOAT *)R;
	double * double_R=(double *)R;

	int * int_G=(int *)G;
	FLOAT * FLOAT_G=(FLOAT *)G;
	double * double_G=(double *)G;

	int * int_B=(int *)B;
	FLOAT * FLOAT_B=(FLOAT *)B;
	double * double_B=(double *)B;

	float r,g,b;

	if(count<=0 || R==NULL || G==NULL || B==NULL || target==NULL) return(-1);

	if(gamma)
		for(i=0;i<count;i++) {
			switch(type) {
				case SCALAR_int:	r=(float) (*(int_R++))/255; g=(float) (*(int_G++))/255; b=(float) (*(int_B++))/255; break;
				case SCALAR_FLOAT:	r=*(FLOAT_R++); g=*(FLOAT_G++); b=*(FLOAT_B++); break;
				case SCALAR_double:	r=*(double_R++); g=*(double_G++); b=*(double_B++);
			}
			if(r<0) r=0; if(g<0) g=0; if(b<0) b=0;

			/* CIE Rec. 709 gamma correction */

			(target++)->SetRGB(	(r<=0.018)?(4.5*r):(1.099*pow(r,0.45)-0.099),
						(g<=0.018)?(4.5*g):(1.099*pow(g,0.45)-0.099),
						(b<=0.018)?(4.5*b):(1.099*pow(b,0.45)-0.099));
		}
	else
		switch(type) {
			case SCALAR_int:	for(i=0;i<count;i++) (target++)->SDL_SetRGB(*(int_R++),*(int_G++),*(int_B++)); break;
			case SCALAR_FLOAT:	for(i=0;i<count;i++) (target++)->SetRGB(*(FLOAT_R++),*(FLOAT_G++),*(FLOAT_B++)); break;
			case SCALAR_double:	for(i=0;i<count;i++) (target++)->SetRGB(*(double_R++),*(double_G++),*(double_B++));
		}
	return(0);
}
