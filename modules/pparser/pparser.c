/***********************************************\
* Generic parser of parameter files             *
* (C) 2005, 2015 Pavel Strachota                *
* file: pparser.c                               *
\***********************************************/

#include "common.h"
#include "pparser.h"
#include "ee_wrapper.h"

#include <stdio.h>
#include <string.h>

#define CPARSER_STRING_LIMIT 4095

static _conststring_ ev_errors[]={	"OK",
					"Domain",
					"Division by zero",
					"Overflow",
					"Underflow",
					"Total loss of precision",
					"Syntax",
					"Stack overflow",
					"Prefetch failed"
				};

int pparse(_conststring_ path,PP_STAT (* phandler)(_conststring_ line,int line_number),FILE * verbose)
/*
parses a file with parameters with the default format:

var1	expression1
var2	expression2
etc...

for each line read (including LF), it runs the phandler function which handles special cases
such as non-numeric commands. phandler must return the following values:
<0	(PP_ERROR)	invalid line format =>  pparse will stop and return an error code -2.
0	(PP_DEFAULT)	not a special case => the line will be expected to be in the default format
			and the expression will be parsed by the Digithell Expression Evaluator
1	(PP_BREAK)	break: the file parser will stop at the current line and successfully return
>1	(PP_SPECIAL)	a correctly handled secial case => pparse will skip this line with no other action

if phandler is NULL, nothing will be called and the file will be parsed the default way.

the parsed variables are accessible through the functions defined in ee_wrapper.h

'verbose' determines whether pparse should inform about its work. If 'verbose != NULL',
pparse writes its progress information to the stream 'verbose'

return codes:
0	file successfully parsed
-1	could not open the input file
-2	special case parse error
-3	evaluator error: expression
-4	evaluator error: could not set a variable
*/
{
	FILE * params;
	char linebuf[CPARSER_STRING_LIMIT+1];
	char varbuf[32];
	char exprbuf[CPARSER_STRING_LIMIT+1];

	int k,l=0;

	double tmp;

	if(verbose) fprintf(verbose,"PParse Parameter-File Parser v1.0\nProcessing the file: %s\n",path);
	params = fopen(path,"r");
	if(!params) {
		if(verbose) fprintf(verbose,"PParse fatal error: Can't open %s.\n",path);
		return(-1);
	}
	
	do {
		if(! fgets(linebuf,CPARSER_STRING_LIMIT,params)) break;
		l++;

		/* call user handler */
		if(phandler) {
			k=phandler(linebuf,l);
			if(k==1) {
				if(verbose) fprintf(verbose,"PParse: User handler reported break at line %d.\n",l);
				break;
			}
			if(k>1) continue;
			if(k<0) {
				if(verbose) fprintf(verbose,"PParse error: User handler reported an error at line %d. Stop.\n",l);
				return(-2);
			}
		}

		if(sscanf(linebuf,"%s %[^\n]",varbuf,exprbuf)<2) {
			if(verbose) fprintf(verbose,"PParse error: Invalid line format at line %d. Stop.\n",l);
			return(-3);
		}

		if(verbose) fprintf(verbose,"%-*s = %-*s  | ",20,varbuf,40,exprbuf);

		tmp=eval(exprbuf);
		if(ev_error()) {
			if(verbose) fprintf(verbose,"\nPParse evaluation error: (%s) at line %d, byte %d. Stop.\n",ev_errors[ev_error()],l,ev_pos());
			return(-3);
		}
		if(ev_def_var(varbuf,tmp)) {
			if(verbose) fprintf(verbose,"\nPParse error: Can't define variable %s. Stop.\n",varbuf);
			return(-4);
		}
		if(verbose) fprintf(verbose,"= %g\n",tmp);
	} while (1);

	fclose(params);
	if(verbose) fprintf(verbose,"PParse: Parameters file processing complete.\n");
	return(0);
}
