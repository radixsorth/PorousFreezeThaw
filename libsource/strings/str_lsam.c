/*
STR_LSAM.C (STRINGS.H)
len/set/add/makestring DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"

unsigned set(_string_ target,_conststring_ source)
/* COPIES STRING source TO PREVIOULSY ALLOCATED ARRAY target */
/* RETURNS LENGTH OF source (IN BYTES). */
/* WARNING: THERE MUST BE ENOUGH SPACE ALLOCATED FOR target! */
/* set COPIES A MAXIMUM OF _maxstrlen BYTES OF source TO target! */
{
 unsigned length=0;
 while(*source) {
  *target=*source;
  target++;
  source++;
  if((++length)==_maxstrlen) break;
 }
 *target=0;
return (length);
}

unsigned len(_conststring_ string)
/* RETURNS LENGTH OF string IN BYTES */
/*
AUTOMATICALLY TRUNCATES STRINGS TO _maxstrlen CHARACTERS
EVEN THOUGH THE ARGUMENT IS A POITRER TO CONST !!!
THIS IS JUST A PROTECTION AGAINST UNTERMINATED STRINGS:
IF ALL STRINGS ARE SHORT ENOUGH, NOTHING INTERESTING HAPPENS.
*/
{
 unsigned length=0;
 while(*string) {
  string++;
  if((++length)==_maxstrlen) { *((_string_)string)=0; break; }	/* override pointer to const */
 }
 return(length);
}

void add(_string_ string1,_conststring_ string2)
/* ADDS string2 TO string1 */
/* DOESN'T ADD ANYTHING IF THE LENGTH AFTER THE OPERATION WOULD EXCEED */
/* THE RANGE GIVEN BY _maxstrlen */
{
 unsigned l1=len(string1);
 unsigned l2=len(string2);
 if((l1+l2)>_maxstrlen) return;
 set(string1+len(string1),string2);
}

void makestring(_string_ destination,char character,unsigned n)
/* CREATES THE string OF n characters */
{
 if(n>_maxstrlen) n=_maxstrlen;
 while(n) {
  *destination=character;
  *destination++; n--; }
 *destination=0;
}
