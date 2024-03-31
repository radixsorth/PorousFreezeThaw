/***********************************************\
* plain ASCII format export module              *
* (C) 2005-2007 Pavel Strachota			*
* file: plain_export.c                          *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include <stdio.h>

int plain_export(void * data,SCALAR_TYPE type,int x_dim,int y_dim,_conststring_ comment,_conststring_ path)
/*
exports the data of given type to the plain ASCII file
the file contains the data in a table with x_dim columns and y_dim lines,
which is suitable for display with gnuplot:

a vector (1 x y_dim) with the command:		" plot 'file.dta' with lines "

a table (x_dim x y_dim) with the command:	" splot 'file.dta' matrix with steps "
						" splot 'file.dta' matrix with steps palette"

return codes:
0	success
-1	file access error
-2	invalid input data
-5	disk full (write error)
*/
{
	FILE * outfile;

	int wrt_stat;

	int * int_data=(int *)data;
	FLOAT * FLOAT_data=(FLOAT *)data;
	double * double_data=(double *)data;

	/* total point data */
	int point_data = x_dim*y_dim;
	int i;

	if(point_data==0 || data==NULL) return(-2);

	outfile=fopen(path,"w");
	if(!outfile) return(-1);

	fprintf(outfile,"# %s\n",comment);			/* write comment to the beginning of the file */

	/* only point_data-1 values will be output in the loop,
	the last one will be output alone, followed by '\n' */

	switch(type) {
		case SCALAR_int:
				for(i=1;i<point_data;i++)
					fprintf(outfile,"%6d%s",*(int_data++),(i%x_dim)?"	":"\n");
				wrt_stat=fprintf(outfile,"%6d\n",*int_data);
				break;
		case SCALAR_FLOAT:
				for(i=1;i<point_data;i++)
					fprintf(outfile,"%*.*" FTC_g "%s",export_fp_precision+3,export_fp_precision,*(FLOAT_data++),(i%x_dim)?"	":"\n");
				wrt_stat=fprintf(outfile,"%*.*" FTC_g "\n",export_fp_precision+3,export_fp_precision,*FLOAT_data);
				break;
		case SCALAR_double:
				for(i=1;i<point_data;i++)
					fprintf(outfile,"%*.*g%s",export_fp_precision+3,export_fp_precision,*(double_data++),(i%x_dim)?"	":"\n");
				wrt_stat=fprintf(outfile,"%*.*g\n",export_fp_precision+3,export_fp_precision,*double_data);
	}

	/*
	generally, wrt_stat may not be negative even in case of a write error. This is because of so called
	FULL buffering of file streams. We detect the possible write error on the next line.
	*/
	if(wrt_stat>=0) wrt_stat=(fflush(outfile)!=0)?-1:1;
	fclose(outfile);
	return((wrt_stat<0)?-5:0);
}
