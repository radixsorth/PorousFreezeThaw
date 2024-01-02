/*
STR_LRM.C (STRINGS.H)
left/right/mid DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"

void left(_string_ target,_conststring_ source,unsigned number)
/* COPIES FIRST number CHARACTERS OF source INTO target */
{
 unsigned length;
 length=len(source);
 if (length<=number) {
  set(target,source);
 }
 else {
  while(number--) {
   *target=*source;
   target++;
   source++;
  }				/* end of while */
 *target=0;
 }				/* end of if */
}

void right(_string_ target,_conststring_ source,unsigned number)
/* COPIES LAST number CHARACTERS OF source INTO target */
{
 unsigned length,op1;
 length=len(source);
 if (length<=number) {
  set(target,source);
 }
 else {
  for(op1=0;(op1<number);op1++)
   *(target+op1)=*(source+length-number+op1);
  *(target+number)=0;
 }
}

void mid(_string_ target,_conststring_ source,unsigned start,unsigned end)
/* COPIES start-TH TO end-TH CHARACTER OF source INTO target */
/* USE start == 1 TO ACCESS THE FIRST CHARACTER IN A STRING */
{
 if (start>len(source) OR start==0 OR start>end)
  *target=0;
 else
  left(target,source+start-1,end-start+1);
}
