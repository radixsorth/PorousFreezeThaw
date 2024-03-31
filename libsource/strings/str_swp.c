/*
STR_SWP.C (STRINGS.H)
swap DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"
#include "stdlib.h"

int s_swap(_string_ string1,_string_ string2)
/* SWAPS TWO STRINGS UP TO _maxstrlen CHARACTERS DIRECTLY IN MEMORY */
/* IF ONE OF THE STRINGS' LENGTHS EXCEEDS _maxstrlen, ONLY THE FIRST */
/* _maxstrlen CHARACTERS ARE SWAPPED. */
/* RETURNS 0 ON SUCCESS AND -1 ON MEMORY ALLOCATION ERROR */
/* WARNING: THERE MUST BE ENOUGH ALLOCATED SPACE IN ALL TWO STRINGS, */
/* OR THE SHORTER ONE WILL BE REPLACED INCORRECTLY (CAUSING MEMORY PROBLEMS) */
{
 /* proper function is esnured by the len function features */
 _string_ swptool;
  swptool = (char *) malloc(_maxstrlen+1);

 if(!swptool) return(-1);

 set(swptool,string1);
 set(string1,string2);
 set(string2,swptool);
 free(swptool);
 return(0);
}
