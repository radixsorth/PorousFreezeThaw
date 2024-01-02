/*
STR_CMP.C (STRINGS.H)
scompare DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include <ctype.h>

#include "strings.h"

int scompare(_conststring_ string1,_conststring_ string2,char mode)
/* COMPARES string1 AND string2. */
/* RETURNS POSITIVE NUMBER IF string1 > string2 ( ALPHABETICALLY SORTED ) */
/* OR NEGATIVE IF string1 < string2 OR 0 IF THEY ARE THE SAME */
/* scompare DOESN'T CARE OF _maxstrlen TO ACHIEVE HIGHER SPEED !!! */
/* mode DETERMINES WHETHER THE COMPARISON IS CASE SENSITIVE (SENSITIVE) OR NOT */
/* (NOT SENSITIVE) , ! SENSITIVE RESPECTIVELY. */
{
 int diff;
 do {
  if(*string1==0 AND *string2==0) return(0);
						/* there was no difference */
						/* until end */
 if(mode) diff=*string1-*string2;
  else  diff=tolower(*string1)-tolower(*string2);
  string1++,string2++;
 } while(NOT diff);
 return(diff);
}
