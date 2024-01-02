/*
STR_SRF.C (STRINGS.H)
sr_filter DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"
int sr_filter(stringarray strings,_conststring_ pattern,int startstring,int endstring)
/* SEARCHS FOR A PATTERN IN THE ARRAY OF STRINGS AND LOCATES THE STRINGS, */
/* THAT INCLUDE THE PATTERN, TO THE BEGINNING OF THE ARRAY */
/* RETURNS THE NUMBER OF PATTERNS FOUND */
/* RETURNS -1 ON ERROR */
{
 int arrpos;
 int patt_no=0;
 _string_ swt;
 if(startstring>=endstring) return(-1);

 for(arrpos=startstring;arrpos<=endstring;arrpos++) {
  if(instr(strings[arrpos],pattern,1,NOT SENSITIVE)) {
   if(arrpos!=patt_no) {
    swt=strings[arrpos]; strings[arrpos]=strings[patt_no]; strings[patt_no]=swt;
   }
   patt_no++;
  }
 }
 return(patt_no);
}
