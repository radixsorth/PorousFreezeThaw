/*
STR_HVAL.C (STRINGS.H)
hexval DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"

unsigned long hexval(_conststring_ string)
/* CONVERTS string TO A NUMERIC VALUE (unsigned (!) long TYPE) */
/* THE STRING IS ASSUMED TO BE IN A HEXADECIMAL FORMAT, i.e. IT MAY */
/* CONTAIN HEX DIGITS (0-9,A-F) BOTH UPPERCASE AND LOWERCASE. */
/* OTHERWISE, THE FUNCTION SKIPS ALL OTHER CHARACTERS AND USES */
/* VALID DIGITS ONLY. IF NO VALID DIGITS ARE FOUND, 0 IS RETURNED */

/* HINT: THE CHARACTER IGNORATION FEATURE LETS YOU PASS STRINGS LIKE */
/* "0xFFFF" AND "034h" TO hexval. */
{
 unsigned long out=0;
 unsigned length=len(string),q;
 char c;
 for(q=0;q<length;q++) {
  c=string[q];
  if(c>='0' AND c<='9') c-='0';
  else if(c>='A' AND c<='F') c-='A'-0xA;
  else if(c>='a' AND c<='f') c-='a'-0xA;
  else continue;
  out<<=4;		/* multiply by 16 */
  out+=c;
 }
 return(out);
}
