/***********************************************\
* VTK format import module                      *
* (C) 2005 Pavel Strachota                      *
* file: VTK_import.c                            *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include <stdio.h>

static _conststring_ header =		"# vtk DataFile Version 2.0\n";

static _conststring_ VTK_filemode =		"ASCII\n";
static _conststring_ VTK_datamode =		"DATASET STRUCTURED_POINTS\n";
static _conststring_ VTK_dimmarker=		"DIMENSIONS";
static _conststring_ VTK_lastsetting =		"LOOKUP_TABLE";	/* last line before the data itself */

int VTK_GetGridDim(int *x_dim,int *y_dim,int *z_dim,_conststring_ path)
/*
gets the dimensions of the VTK STRUCTURED_POINTS v2.0 datafile

return codes:
0	success
-1	file access error
-2	invalid format
*/
{
	char buf[256];

	FILE * infile;

	infile=fopen(path,"r");
	if(!infile) return(-1);

	fgets(buf,255,infile);
	if(! match(buf, header)) { fclose(infile); return(-2); }

	fgets(buf,255,infile);	/* comment */
	fgets(buf,255,infile);
	if(! match(buf, VTK_filemode)) { fclose(infile); return(-2); }

	fgets(buf,255,infile);
	if(! match(buf, VTK_datamode)) { fclose(infile); return(-2); }

	fscanf(infile,"%s",buf);
	if(! instr(buf,VTK_dimmarker,1,SENSITIVE)) { fclose(infile); return(-2); }

	fscanf(infile,"%d %d %d",x_dim,y_dim,z_dim);

	fclose(infile);
	return(0);
}

int VTK_import(void *data,SCALAR_TYPE type,int max_read,_conststring_ path)
/*
imports the data of given type to the linear array of any internal structure.

return codes:
values read	success
-1		file access error
-2		invalid format
-3		'max_read' <= 0
*/
{
 	FILE * infile;
	int r,x,y,z;
	int tr;
	char buf[256];

        int * int_data=(int *)data;
        FLOAT * FLOAT_data=(FLOAT *)data;
        double * double_data=(double *)data;

	if((r=VTK_GetGridDim(&x,&y,&z,path))!=0) return(r);
	if(max_read<=0) return(-3);

	infile=fopen(path,"r");

	do fgets(buf,255,infile);
	while(! instr(buf,VTK_lastsetting,1,SENSITIVE));	/* skips to the data */

	tr=r=1;
	switch(type) {						/* EOF is -1 */
		case SCALAR_int:
				while((max_read--) && r!=EOF)
					tr+=(r=fscanf(infile,"%d",int_data++));
				break;
		case SCALAR_FLOAT:
				while((max_read--) && r!=EOF)
					tr+=(r=fscanf(infile,"%" iFTC_g,FLOAT_data++));
				break;
		case SCALAR_double:
				while((max_read--) && r!=EOF)
					tr+=(r=fscanf(infile,"%lg",double_data++));
	}

	/* in case the loop ended because of 'max_read': 'tr' was not decreased */
	if(r!=EOF) tr--;

	fclose(infile);
	return(tr);
}
