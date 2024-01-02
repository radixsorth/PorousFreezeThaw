/*
STR_TST.C (STRINGS.H)
tostring DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"
#include <stdio.h>


_conststring_ tostring(long value)
/* CONVERTS THE long TYPE VALUE TO THE STRING AND RETURNS POINTER */
/* TO THIS STRING. THE STRING IS STATIC AND IS OVERWRITTEN UPON EACH */
/* CALL TO tostring OR float_tostring. */
{
 sprintf(tsb.buf,"%ld",value);
 return(tsb.buf);
}
