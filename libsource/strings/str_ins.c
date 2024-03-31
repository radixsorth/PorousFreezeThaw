/*
STR_INS.C (STRINGS.H)
instr DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include <ctype.h>

#include "strings.h"

unsigned instr(_conststring_ source,_conststring_ pattern,unsigned position,char mode)
/* SEARCHS pattern STRING IN source STRING FROM position (MIN 1) */
/* RETURNS POSITION OF THE BEGINNING OF THE pattern STRING */
/* IF source DOES NOT CONTAIN pattern, RETURNS 0 */
/* IF pattern IS NULL, RETURNS position */
/* IF source IS NULL, RETURNS 0 */
/* mode DETERMINES WHETHER THE SEARCH IS CASE SENSITIVE (SENSITIVE) OR NOT */
/* (NOT SENSITIVE) , ! SENSITIVE RESPECTIVELY. */
{
 unsigned pattl,ppos=0;
 if ((position<1) OR (position>len(source)))
  return(0);
 pattl=len(pattern);
 if(pattl==0) return(position);
 if(len(source)==0) return(0);
 source+=position-1;
 if(mode)

  while(*(source+ppos)) {
   if ((*(source+ppos))==(*(pattern+ppos))) {
    ppos++;
    if (ppos==pattl) return(position);
   } else {
    ppos=0;
    source++; position++;
   }
  }
 else
  while(*(source+ppos)) {
   if (tolower(*(source+ppos))==tolower(*(pattern+ppos))) {
    ppos++;
    if (ppos==pattl) return(position);
   } else {
    ppos=0;
    source++; position++;
   }
  }

 return(0);
}
