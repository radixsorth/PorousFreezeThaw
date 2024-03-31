/*
STR_FTST.C (STRINGS.H)
float_tostring DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

/* this is needed when compiling with C99 rules, since the function fcvt() used here is deprecated */
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include "mathspec.h"

#include "strings.h"

_conststring_ float_tostring(double value,int ndigits,int expfrom)
/* CONVERTS THE double TYPE VALUE TO THE STRING AND RETURNS POINTER */
/* TO THIS STRING. THE STRING IS STATIC AND IS OVERWRITTEN UPON EACH */
/* CALL TO tostring OR float_tostring. ndigits DETERMINES THE MAXIMUM NUMBER */
/* OF DIGITS TO BE OUTPUT AND IS IN THE RANGE 1-17. NUMBERS >=10^ndigits OR */
/* <=10^-expfrom ARE OUTPUT IN THE SCIENTIFIC NOTATION. */
{
 int s,d,e;
 char exponent[6]="";
 char buffer[20],sign=0;
 char *str;

 if(value==0) { set(tsb.buf,"0"); return(tsb.buf); }	/* zero */

 if(ndigits>17) ndigits=17;
 if(ndigits<1) ndigits=1;
 if(value<0) {
  sign=1;
  value=-value;
 }

 e=(int)floor(log10(value));			/* handle scientiic notation */
 if(e>=ndigits OR e<=-expfrom) {
   value/=pw10(e);
   add(exponent,"E");
   if(e>0) add(exponent,"+");
   add(exponent,tostring(e));
  d=1;
 } else d=(e>=0)?(e+1):1;		/* obtain number of digits before '.' */
					/* (needed for fcvt) */

 /* assemble number: */
 if(sign) set(tsb.buf,"-");		/* handle sign */
 else set(tsb.buf,_NOSTRING);

 str=fcvt(value,ndigits-d,&d,&s);	/* parameter ndig passed to fcvt */
					/* is the number of digits after the */
					/* decimal point, not the total number */
					/* of digits, which we want */

 if(d>0)				/* d is now the position of '.' */
  left(buffer,str,d);			/* extract numbers before . */
 else set(buffer,"0");
 add(tsb.buf,buffer);

 /* decimal part: */
 if(ndigits>d) {
  e=d; s=0;
  set(buffer,_NOSTRING);
  while(e++<0) { s++; add(buffer,"0"); }	/* add leading zeros */
  if(d<0) d=0;
  mid(buffer+s,str,d+1,1000);			/* extract decimal part */
  d=len(buffer);
  while(d AND buffer[d-1]=='0') d--;
  buffer[d]=0;			/* remove redundant zeros from decimal part */
  if(d) {
   add(tsb.buf,".");
   add(tsb.buf,buffer);
  }
 }
 add(tsb.buf,exponent);
 return(tsb.buf);
}
