/***********************************************\
* Generic parser of parameter files             *
* (C) 2005 Pavel Strachota                      *
* file: pparser.h                               *
\***********************************************/

#if !defined __pparser
#define __pparser

/*
IMPORTANT:
You must link your program with 'exprsion' and 'strings' (in this order) libraries
in order to use this module. If you compile using a C compiler, you will also have
to explicitly include the C++ library (classes, init code, operator new etc...)
in the link: stdc++.
*/

#include "strings.h"
/*
the user will always want to include ee_wrapper.h, because the variables obtained by
pparse would not be accessible otherwise. Thus, even though we don't need it here,
let's include ee_wrapper.h now
*/
#include "ee_wrapper.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PP_ERROR=-1,
	PP_DEFAULT=0,
	PP_BREAK=1,
	PP_SPECIAL=2
} PP_STAT;

int pparse(_conststring_ path,PP_STAT (* phandler)(_conststring_ line,int line_number),FILE * verbose);
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

#ifdef __cplusplus
}
#endif

#endif		/* __pparser */
