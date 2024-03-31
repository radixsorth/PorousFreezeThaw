/***********************************************\
* PNM image dimension query function            *
* (C) 2005 Pavel Strachota                      *
* file: PNM_getdim.c                            *
\***********************************************/
#include "dataIO.h"

#include <stdio.h>

static _conststring_ headers[]={"P5","P6"};
static int types_count=2;

PNM_IMAGE_TYPE PNM_GetDim(int *width, int *height, _conststring_ path)
/*
gets the PNM image dimensions

return codes:
PNM_PGM			the file is a PGM image
PNM_PPM			the file is a PPM image
PNM_Unknown		the file has an unknown format (in that case, width and height are undefined)
PNM_FileOpenFailed	could not open the file
*/
{
	char buf[256];
	int entries_read=0;
	int i,j;

	FILE * infile;

	infile=fopen(path,"r");
	if(!infile) return(PNM_FileOpenFailed);

	fgets(buf,2+1,infile);		/* 2 is the header length fixed for all headers */
	for(j=0;j<types_count;j++)
		if(match(buf, headers[j])) {
			fscanf(infile,"%s",buf);
			while((i=instr(buf,"#",1,SENSITIVE))>0) {		/* handle one or more comments */
				fscanf(infile,"%*[^\n]");			/* skip to the end of line */
				if(i==1) fscanf(infile,"%s",buf);
				else { buf[i]=0; break; }
			}
			*width=val(buf);

			fscanf(infile,"%s",buf);
			while((i=instr(buf,"#",1,SENSITIVE))>0) {
				fscanf(infile,"%*[^\n]");
				if(i==1) fscanf(infile,"%s",buf);
				else { buf[i]=0; break; }
			}
			*height=val(buf);

			fclose(infile);
			return((PNM_IMAGE_TYPE)j);
		}

	fclose(infile);
	return(PNM_Unknown);
}
