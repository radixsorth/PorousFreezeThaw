/***********************************************\
* VTK format import module                      *
* selector function version                     *
* (C) 2005 Pavel Strachota                      *
* file: VTK_importS.c                           *
\***********************************************/
#include "common.h"
#include "dataIO.h"

#include <stdio.h>

static _conststring_ VTK_lastsetting =	"LOOKUP_TABLE";	/* last line before the data itself */

int VTK_importS(void (*data)(int,SCALAR_DATA),SCALAR_TYPE type,int max_read,_conststring_ path)
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
{
 	FILE * infile;
	int r,x,y,z;
	int tr;
	char buf[256];

	SCALAR_DATA d;

	if((r=VTK_GetGridDim(&x,&y,&z,path))!=0) return(r);
	if(max_read<=0) return(-3);

	infile=fopen(path,"r");

	do fgets(buf,255,infile);
	while(! instr(buf,VTK_lastsetting,1,SENSITIVE));	/* skips to the data */

	tr=r=0;
	switch(type) {						/* EOF is -1 */
		case SCALAR_int:
				while((max_read--)) {
					if(fscanf(infile,"%d",&(d.int_data))==EOF) break;
					data(tr,d);
					tr++;
				}
				break;
		case SCALAR_FLOAT:
				while((max_read--)) {
					if(fscanf(infile,"%" iFTC_g,&(d.FLOAT_data))==EOF) break;
					data(tr,d);
					tr++;
				}
				break;
		case SCALAR_double:
				while((max_read--)) {
					if(fscanf(infile,"%lg",&(d.double_data))==EOF) break;
					data(tr,d);
					tr++;
				}
	}

	fclose(infile);
	return(tr);
}
