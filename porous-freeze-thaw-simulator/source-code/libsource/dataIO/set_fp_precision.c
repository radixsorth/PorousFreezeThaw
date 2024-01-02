/***********************************************\
* floating point precision setting for export   *
* (C) 2005-2007 Pavel Strachota			*
* file: set_fp_precision.c                      *
\***********************************************/
#include "common.h"
#include "dataIO.h"

int export_fp_precision=6;

void set_export_fp_precision(int precision)
/*
sets the ASCII output precision of floating point data (for both SCALAR_FLOAT and SCALAR_double types).
This setting affects the following functions:
- VTK_export, VTK_exportS
- plain_export, plain_exportS
- gnuplot_export, gnuplot_exportS
The default value of floating point precision (the number of significant digits) is 6.
*/
{
	export_fp_precision=precision;
}
