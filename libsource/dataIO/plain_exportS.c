/***********************************************\
* plain ASCII format export module              *
* selector function version                     *
* (C) 2005-2007 Pavel Strachota			*
* file: plain_exportS.c                         *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include <stdio.h>

int plain_exportS(SCALAR_DATA (* data)(int),SCALAR_TYPE type,int x_dim,int y_dim,_conststring_ comment,_conststring_ path)
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
{
	FILE * outfile;

	int wrt_stat;

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
					fprintf(outfile,"%6d%s",data(i-1).int_data,(i%x_dim)?"	":"\n");
				wrt_stat=fprintf(outfile,"%6d\n",data(i-1).int_data);
				break;
		case SCALAR_FLOAT:
				for(i=1;i<point_data;i++)
					fprintf(outfile,"%*.*" FTC_g "%s",export_fp_precision+3,export_fp_precision,data(i-1).FLOAT_data,(i%x_dim)?"	":"\n");
				wrt_stat=fprintf(outfile,"%*.*" FTC_g "\n",export_fp_precision+3,export_fp_precision,data(i-1).FLOAT_data);
				break;
		case SCALAR_double:
				for(i=1;i<point_data;i++)
					fprintf(outfile,"%*.*g%s",export_fp_precision+3,export_fp_precision,data(i-1).double_data,(i%x_dim)?"	":"\n");
				wrt_stat=fprintf(outfile,"%*.*g\n",export_fp_precision+3,export_fp_precision,data(i-1).double_data);
	}

	/*
	generally, wrt_stat may not be negative even in case of a write error. This is because of so called
	FULL buffering of file streams. We detect the possible write error on the next line.
	*/
	if(wrt_stat>=0) wrt_stat=(fflush(outfile)!=0)?-1:1;
	fclose(outfile);
	return((wrt_stat<0)?-5:0);
}
