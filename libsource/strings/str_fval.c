/*
STR_FVAL.C (STRINGS.H)
float_val DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "mathspec.h"

#include "strings.h"

double float_val(_conststring_ string)
/* CONVERTS string TO A NUMERIC VALUE (double TYPE) */
/* STRING CAN CONTAIN NEGATION SIGN (-) AT THE FIRST PLACE, */
/* DECIMAL DIGITS (0-9),ONE DECIMAL POINT (.), */
/* OR ONE EXPONENT EXPRESSION (Eñ##). */
/* (.) MUST BE BEFORE (Eñ##).OTHERWISE, ALL OTHER CHARACTERS */
/* ARE SKIPPED (!). IF NO DIGITS ARE FOUND, 0 IS RETURNED. */
{
unsigned length=len(string),x=0;
int base=10,expnum=0,decnum=0;
unsigned char chr,no,pointflag=0,expflag=0,negflag=0,expnegflag=0;
unsigned char expsignflag=0;
double out=0,decimal=0;
/*-------------------- */
if(*string=='-') negflag++,x++;
if(*string=='+') x++;
/*------ */
 for(;x<length;x++) {
  chr=*(string+x);
  if(chr=='.') {
   {if(pointflag OR expflag)
    continue;}
   pointflag=1;
   continue;
   }
  if((chr=='E')OR(chr=='e')) {
   {if(expflag)
    continue;}
   expflag=1;
   continue;
   }
/*------ */
  if((chr=='-')AND (expflag==1)) {
   expsignflag=expnegflag=1;
   expflag++;
   continue;
  }
  if((chr=='+')AND (expflag==1)) {
   expsignflag=1;
   expflag++;
   continue;
  }
  if((chr>='0')AND(chr<='9')) {
   no=chr-'0';
   if(NOT expflag)
    if(NOT pointflag) {
     out*=base;
     out+=no;
    }
    else {
     decimal*=base;
     decimal+=no;
     decnum++;
    }
   else  {
    expnum*=base;
    expnum+=no;
   if(expflag++==(4+expsignflag))
    continue;
   }
   continue;
  }
 continue;
 }
 out+=decimal/pw10(decnum);
 if(expnum) {
  while(expnum--) {
   if(expnegflag)
    out/=base;
   else
    out*=base;
  }
 }
 if(negflag)
  out=-out;
 return(out);
}
