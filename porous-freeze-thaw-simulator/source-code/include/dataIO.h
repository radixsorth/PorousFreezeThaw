/***********************************************\
* Data import/export functions                  *
* (C) 2005-2007 Pavel Strachota                 *
* file: dataIO.h                                *
\***********************************************/

#if !defined __dataIO
#define __dataIO

#include "common.h"
#include "strings.h"

typedef enum {
	SCALAR_int,			/* int C type */
	SCALAR_FLOAT,			/* FLOAT type (long double by default, defined in common.h */
	SCALAR_double			/* double C type */
} SCALAR_TYPE;

typedef enum {				/* return values of the PNM_GetDim() function */
	PNM_PGM,
	PNM_PPM,
	PNM_Unknown=-2,
	PNM_FileOpenFailed=-1
} PNM_IMAGE_TYPE;

typedef union {				/* this is a universal data interface to selector functions */
	int int_data;
	double double_data;
	FLOAT FLOAT_data;
} SCALAR_DATA;

/* ------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

extern int export_fp_precision;

void set_export_fp_precision(int precision);
/*
sets the ASCII output precision of floating point data (for both SCALAR_FLOAT and SCALAR_double types).
This setting affects the following functions:
- VTK_export, VTK_exportS
- plain_export, plain_exportS
- gnuplot_export, gnuplot_exportS
The default value of floating point precision (the number of significant digits) is 6.
*/

int VTK_export(void *data,SCALAR_TYPE type,int x_dim,int y_dim,int z_dim,int values_per_line,_conststring_ comment,_conststring_ path);
/*
exports the data of given type to the VTK compatible STRUCTURED_POINTS format

return codes:
0	success
-1	file access error
-2	invalid input data
-5	disk full (write error)
*/

int VTK_exportS(SCALAR_DATA (* data)(int),SCALAR_TYPE type,int x_dim,int y_dim,int z_dim,int values_per_line,_conststring_ comment,_conststring_ path);
/*
exports the data of given type to the VTK compatible STRUCTURED_POINTS format

this version uses the selector function 'data()' which should return a SCALAR_DATA union where the i-th value to be exported
is stored. Here 'i' denotes the integer argument passed to 'data()' which specifies the ordinal number of the processed
data element (value) (i goes from 0 to x_dim*y_dim-1). The appropriate member of the union read by VTK_exportS() (and thus
the appropriate type of the actually exported data) is determined by the 'type' parameter.

return codes:
0	success
-1	file access error
-2	invalid input data
-5	disk full (write error)
*/

int VTK_GetGridDim(int *x_dim,int *y_dim,int *z_dim,_conststring_ path);
/*
gets the dimensions of the VTK STRUCTURED_POINTS v2.0 datafile

return codes:
0	success
-1	file access error
-2	invalid format
*/

int VTK_import(void *data,SCALAR_TYPE type,int max_read,_conststring_ path);
/*
imports the data of given type to the linear array of any internal structure.
(needs the "strings" library)

return codes:
values read	success
-1		file access error
-2		invalid format
-3		'max_read' <= 0
*/

int VTK_importS(void (*data)(int,SCALAR_DATA),SCALAR_TYPE type,int max_read,_conststring_ path);
/*
imports the data of given type to the linear array of any internal structure.

this version uses the selector function 'data()' which takes an integer argument specifying the ordinal
number (starting from 0) of the imported data element (value) and a SCALAR_DATA union that contains this value.
The 'data()' function should then process the retrieved value or store it somewhere (not in a linear array - if
you want to do this, you'd better use the VTK_import() function, which is faster). The appropriate member
of the union set by VTK_importS() (and thus the appropriate type of the actually imported data) is determined
by the 'type' parameter.

return codes:
values read	success
-1		file access error
-2		invalid format
-3		'max_read' <= 0
*/

int plain_export(void * data,SCALAR_TYPE type,int columns,int rows,_conststring_ comment,_conststring_ path);
/*
exports the data of given type to the plain ASCII file.
the file contains the data in a table with 'columns' columns of data and 'rows' lines,
which is suitable for display with gnuplot:

a vector (1 x y_dim) with the command:	 	" plot 'file.dta' with lines "

a table (x_dim x y_dim) with the command:	" splot 'file.dta' matrix with steps "
						" splot 'file.dta' matrix with steps palette"

return codes:
0	success
-1	file access error
-2	invalid input data
-5	disk full (write error)
*/

int plain_exportS(SCALAR_DATA (* data)(int),SCALAR_TYPE type,int x_dim,int y_dim,_conststring_ comment,_conststring_ path);
/*
exports the data of given type to the plain ASCII file
the file contains the data in a table with x_dim columns and y_dim lines.

this version uses the selector function 'data()' which should return a SCALAR_DATA union where the i-th value to be exported
is stored. Here 'i' denotes the integer argument passed to 'data()' which specifies the ordinal number of the processed
data element (value) (i goes from 0 to x_dim*y_dim-1). The appropriate member of the union read by plain_exportS() (and thus
the appropriate type of the actually exported data) is determined by the 'type' parameter.

return codes:
0	success
-1	file access error
-2	invalid input data
-5	disk full (write error)
*/

int gnuplot_export(void * data,SCALAR_TYPE type,int x_dim,int y_dim,_conststring_ comment,_conststring_ path);
/*
exports the data of given type to the plain ASCII file. The file contains y_dim blocks of x_dim values, one
value on one line. The blocks are separated by an empty line. This is the traditional format used by the
gnuplot's splot command to draw a surface:

" splot 'file.dta' with steps "

NOTE:	In order to produce a vector (single-variable function graph) and to make gnuplot draw it, you need
	to set y_dim=1. This is opposite to the plain_export() function, which would place all values onto
	one line in this case. However, the actual order of the data is the same as in the file produced by
	plain_export(). You can therefore use plain_import() to load data produced by both plain_export() and
	gnuplot_export() functions.

return codes:
0	success
-1	file access error
-2	invalid input data
-5	disk full (write error)
*/

int gnuplot_exportS(SCALAR_DATA (* data)(int),SCALAR_TYPE type,int x_dim,int y_dim,_conststring_ comment,_conststring_ path);
/*
exports the data of given type to the plain ASCII file. The file contains y_dim blocks of x_dim values, one
value on one line. The blocks are separated by an empty line. This is the traditional format used by the
gnuplot's splot command to draw a surface.

this version uses the selector function 'data()' which should return a SCALAR_DATA union where the i-th value to be exported
is stored. Here 'i' denotes the integer argument passed to 'data()' which specifies the ordinal number of the processed
data element (value) (i goes from 0 to x_dim*y_dim-1). The appropriate member of the union read by plain_exportS() (and thus
the appropriate type of the actually exported data) is determined by the 'type' parameter.

return codes:
0	success
-1	file access error
-2	invalid input data
-5	disk full (write error)
*/

int plain_import(void * data,SCALAR_TYPE type,int max_read,_conststring_ path);
/*
imports the data of the given type from the plain ASCII format generated by plain_export/gnuplot_export.

it skips the first line and then reads a maximum of 'max_read' values into the array 'data'.
control of the array dimensions is up to user's responsibility.

return codes:
values read	success
-1		file access error
-3		'max_read' <= 0
*/

int plain_importS(void (*data)(int,SCALAR_DATA),SCALAR_TYPE type,int max_read,_conststring_ path);
/*
imports the data of the given type from the plain ASCII format generated by plain_export/gnuplot_export.

it skips the first line and then reads a maximum of 'max_read' values.

this version uses the selector function 'data()' which takes an integer argument specifying the ordinal
number (starting from 0) of the imported data element (value) and a SCALAR_DATA union that contains this value.
The 'data()' function should then process the retrieved value or store it somewhere (not in a linear array - if
you want to do this, you'd better use the plain_import() function, which is faster). The appropriate member
of the union set by plain_importS() (and thus the appropriate type of the actually imported data) is determined
by the 'type' parameter.

return codes:
values read	success
-1		file access error
-3		'max_read' <= 0 or 'data' is a null pointer
*/

PNM_IMAGE_TYPE PNM_GetDim(int *width, int *height, _conststring_ path);
/*
gets the PNM image dimensions

return codes:
PNM_PGM			the file is a PGM image
PNM_PPM			the file is a PPM image
PNM_Unknown		the file has an unknown format (in that case, width and height are undefined)
PNM_FileOpenFailed	could not open the file
*/

int PGM_export(void *data,SCALAR_TYPE type,int width,int height,unsigned short maxcolor,_conststring_ comment,_conststring_ path);
/*
exports the data in the array to the Portable GrayMap format (PGM)
(includes standard gamma correction due to CIE Rec. 709)

NOTE:	PGM is a special type of PNM (Portable AnyMap format). The extension of the output
	file can be one of { .pgm, .pnm, .ppm } and the file will be displayed well. The .pnm
	extension is recommended.

the data should be in the range 0 to 1 for SCALAR_FLOAT and SCALAR_double types
and in the range 0 to 'maxcolor' for SCALAR_int type. The resulting grayscale range
is 0 to 'maxcolor'. Values out of range are automatically adjusted.

NOTE:	For maxcolor >255, each pixel is coded in two bytes. Some viewers do not dislpay
	this format correctly, but the command line tools like pnmtopng work well with it.

return codes:
0	success
-1	file access error
-2	invalid input data
-4	not enough memory for buffer allocation
-5	disk full (write error)
*/

int PGM_exportS(SCALAR_DATA (* data)(int),SCALAR_TYPE type,int width,int height,unsigned short maxcolor,_conststring_ comment,_conststring_ path);
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

int PGM_import(void *data,SCALAR_TYPE type,int max_read,_conststring_ path);
/*
imports the data from the grayscale Portable GrayMap format (PGM) into a single array
(removes standard gamma correction due to CIE Rec. 709)

the imported data will be in the range 0 to 1 for SCALAR_FLOAT and SCALAR_double types
and in the range 0 to 'maxcolor' for SCALAR_int type.

the dimensions of the image are ignored, you have to use PNM_GetDim if you need them.
simply all the values or the first max_read values (pixel colors) are read from the file.
control of the array dimensions is up to user's responsibility.

return codes:
values read	success
-1		file access error
-2		invalid format
-3		'max_read' <= 0 or 'data' is a null pointer
-4		not enough memory for buffer allocation
*/

int PGM_importS(void (*data)(int,SCALAR_DATA),SCALAR_TYPE type,int max_read,_conststring_ path);
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

int PPM_export(void *R, void *G, void *B, SCALAR_TYPE type, int width, int height, unsigned short maxcolor, _conststring_ comment, _conststring_ path);
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

int PPM_exportS(SCALAR_DATA (* R)(int), SCALAR_DATA (* G)(int), SCALAR_DATA (* B)(int), SCALAR_TYPE type, int width, int height, unsigned short maxcolor, _conststring_ comment, _conststring_ path);
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

int PPM_import(void *R, void *G, void *B, SCALAR_TYPE type, int max_read, _conststring_ path);
/*
imports the data from the Portable PixMap format (PPM) into three separate arrays that
represent the R, G, B color components of each pixel. If you pass NULL to some of the
R, G, B pointers, the reading of the appropriate color component is skipped.
(removes standard gamma correction due to CIE Rec. 709)

the imported data will be in the range 0 to 1 for SCALAR_FLOAT and SCALAR_double types
and in the range 0 to 'maxcolor' for SCALAR_int type.

the dimensions of the image are ignored, you have to use PNM_GetDim if you need them.
Simply all the data or the colors of the first max_read pixels are read (that is, the total
count of scalar values read is max_read*3).
Control of the array dimensions is up to user's responsibility.

return codes:
RGB triplets read	success
-1			file access error
-2			invalid format
-3			'max_read' <= 0
-4			not enough memory for buffer allocation
*/

int PPM_importS(void (*R)(int,SCALAR_DATA), void (*G)(int,SCALAR_DATA), void (*B)(int,SCALAR_DATA), SCALAR_TYPE type, int max_read, _conststring_ path);
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

#ifdef __cplusplus
}
#endif

#endif		/* __dataIO */
