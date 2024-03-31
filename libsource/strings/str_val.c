/*
STR_VAL.C (STRINGS.H)
val DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"

long val(_conststring_ string)
/* CONVERTS string TO A NUMERIC VALUE (int or unsigned TYPE - */
/* - long TYPE CAN SUPPLY BOTH) */
/* STRING CAN CONTAIN NEGATION SIGN (-) AT THE FIRST PLACE AND */
/* DECIMAL DIGITS (0-9). OTHERWISE, THE FUNCTION SKIPS ALL OTHER CHARACTERS */
/* AND USES VALID DIGITS ONLY. IF NO VALID DIGITS ARE FOUND, 0 IS RETURNED */
{
 unsigned length=len(string),q;
 long out=0;
 char negflag=0;
 if(string[0]=='-') negflag=1;
 for(q=negflag;q<length;q++) {
  if(string[q]<'0' OR string[q]>'9') continue;
  out*=10;
  out+=string[q]-'0';
 }
 if(negflag) out=-out;
 return(out);
}
