/***********************************************\
* An interface to SDL + PNM image handling	*
* (C) 2005-2006 Pavel Strachota			*
* file: screen.h				*
\***********************************************/

#ifndef __cplusplus
	#error FATAL ERROR: The 'screen.h' header is intended for use with C++ only.
#endif

/*
This file contains functions that allow the simplest access
to an SDL surface: drawing pixels.
*/

#if ! defined __SDL_screen
#define __SDL_screen

#include "SDL/SDL.h"
#include "dataIO.h"

// The following class implements a robust color-handling
// mechanism suitable both for color arithmetic and for use with SDL

class SCR_Color
{
	Uint32 Color;
	float r,g,b;	// parallel representation in 3 channels
			// within the range from 0 to 1

	void RefreshColor();	// recalculate Color from r,g,b (internal)
	void Refreshrgb();	// recalculate r,g,b from Color (internal)

	static float Restrict(float x)	  // restrict x to the range from 0 to 1 (internal)
	 { return((x<0)?0:((x>1)?1:x)); }
	// NOTE: all methods that change r,g,b call Restrict() to make sure
	// that the resulting color is valid.

	public:
	// methods
	float R() { return(r); }
	float G() { return(g); }
	float B() { return(b); }

	Uint8 SDL_R();	// returns the red channel calculated by SDL_GetRGB
	Uint8 SDL_G();	// returns the green channel
	Uint8 SDL_B();	// returns the blue channel

	void SetRGB(float rr, float gg, float bb);
	void SetR(float rr);
	void SetG(float gg);
	void SetB(float bb);
	void SetGrey(float intensity) { SetRGB(intensity,intensity,intensity); }

	// uses SDL_MapRGB() to construct the Color and then it recalculates the values
	// of r,g,b.
	void SDL_SetRGB(Uint8 RR, Uint8 GG, Uint8 BB);
	void SDL_SetGrey(Uint8 intensity) { SDL_SetRGB(intensity,intensity,intensity); }

	// uses SDL_GetRGB() to construct the Uint8 values of R,G,B channels from Color,
	// changes the desired channel to the value specified, reconstructs Color
	// from the new values and finally recalculates r,g,b. This ensures that
	// the color that is seen on the screen will only change the channel really
	// desired to be changed. That would not be so obvious if we calculated
	// just the r,g or b attribute and then recalculated Color from r,g,b.
	void SDL_SetR(Uint8 RR);
	void SDL_SetG(Uint8 GG);
	void SDL_SetB(Uint8 BB);

	float Intensity();	// returns the color intensity due to the known empiric formula.
				// The r,g,b values are used for the calculation.
				// The return value is between 0 and 1.

	// the arithmetic operators work with the r,g,b color
	// representation and the resulting 'color' attribute is
	// computed from the resulting values of r,g,b.
	SCR_Color operator + (SCR_Color c);
	SCR_Color operator - (SCR_Color c);

	SCR_Color operator * (SCR_Color c);	// multiplication by components
	SCR_Color operator * (float alpha);	// scalar multiplication
	SCR_Color operator / (float alpha);	// scalar division
	friend SCR_Color operator * (float alpha, SCR_Color c);

	SCR_Color & operator += (SCR_Color c);
	SCR_Color & operator -= (SCR_Color c);

	SCR_Color & operator *= (SCR_Color c);
	SCR_Color & operator *= (float alpha);

	SCR_Color & operator /= (float alpha);

	SCR_Color & operator = (Uint32 c);

	// the color equality operator (compares the truly VISIBLE colors)
	bool operator == (SCR_Color c) { if(Color==c.Color) return(true); return(false); }

	// the type cast operator that allows SCR_Color to stand at the place of Uint32
	operator Uint32 () { return(Color); }

	// constructors
	SCR_Color(Uint32 c);
	SCR_Color(Uint8 RR, Uint8 GG, Uint8 BB,int);	// the last argument only distinguishes
	SCR_Color(float rr, float gg, float bb);	// between constructors
	SCR_Color() : Color(0),r(0),g(0),b(0) { };
};

// --------------------------------------------------

SDL_Surface * SCR_GetScreen();
/*
returns the screen SDL surface pointer
(use this for advanced operations on the screen by means
of SDL functions)
*/

bool SCR_IsInit();
// returns true if the SDL screen is initialized

int SCR_InitScreen(int width, int heigth, int colordepth, bool fullscreen=false);
/*
initializes an SDL screen

return codes:
0	success
-1	could not initialize SDL video
-2	could not initialize video mode
*/

int SCR_ResizeScreen(int new_width, int new_height);
/*
tries to resize the screen to the new width and height

return codes:
0	success
-1	could not resize the SDL screen
*/


void SCR_Lock();
// locks the screen. The screen can only be written to when it is locked.

void SCR_Unlock();
// unlocks the screen

int SCR_Flip();
/*
performs an SDL_Flip() call on the screen. See SDL documentation
for details. In short, SCR_Flip() ensures that the changes made
to the SDL surface appear onscreen.

return codes:
0	success
-1	error
*/

// WARNING: touching pixels outside the screen area will most likely cause
// a memory protection error !

void SCR_putpixel(int x, int y, SCR_Color c);

SCR_Color SCR_getpixel(int x, int y);

// --------------------------------------------------
// Here follow the PNM image handling functions

int SCR_PGMimport(SCR_Color *data,_conststring_ path);
/*
imports the data from the grayscale Portable GrayMap format (PGM) into an SCR_Color array
(does not remove standard gamma correction since the image is intended to be displayed
without any modifications)

there must be enough space in the 'data' array to store the whole image. You may use the
PNM_GetDim() function from the dataIO library if you need to know the image dimensions
in advance.

return codes:
pixels read	success
-1		file access error
-2		invalid format
-3		'data' is a null pointer
-4		not enough memory for buffer allocation
*/

int SCR_PPMimport(SCR_Color *data,_conststring_ path);
/*
imports the data from the Portable PixMap format (PPM) into an SCR_Color array
(does not remove standard gamma correction since the image is intended to be displayed
without any modifications)

there must be enough space in the 'data' array to store the whole image. You may use the
PNM_GetDim() function from the dataIO library if you need to know the image dimensions
in advance.

return codes:
pixels read	success
-1		file access error
-2		invalid format
-3		'data' is a null pointer
-4		not enough memory for buffer allocation
*/

int SCR_PPMexport(SCR_Color * data, int width, int height, unsigned short maxcolor, _conststring_ comment, _conststring_ path);
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

int SCR_ScalarCvtGrey(void * source, SCALAR_TYPE type, SCR_Color * target, int count, bool gamma = true);
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

int SCR_ScalarConvert(void * R, void * G, void * B, SCALAR_TYPE type, SCR_Color * target, int count, bool gamma = true);
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

int SCR_DispArray(int x, int y, SCR_Color * data, int width, int height);
/*
displays the data in the 'data' array as an image of the given 'width' and 'height'.
Handles the screen borders so that a memory error does not occur.

The screen must be locked before a call to SCR_DispArray()

return codes:
0	success
-1	invalid input data
*/

int SCR_ZoomArray(int x, int y, int width, int height, SCR_Color * data, int data_width, int data_height);
/*
displays the data in the 'data' array as an image of the given 'data_width' and 'data_height'.
so that it fits into the rectangle of the given 'width' and 'height' with its top left corner
at the coordinates 'x' and 'y'. Bilinear interpolation is used to smoothen the resutling image
displayed.

Handles the screen borders so that a memory error does not occur.

The screen must be locked before a call to SCR_ZoomArray()

return codes:
0	success
-1	invalid input data
*/

#endif		// __SDL_screen
