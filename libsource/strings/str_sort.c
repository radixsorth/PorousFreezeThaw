/*
STR_SORT.C (STRINGS.H)
sort DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2005 Digithell, Inc.
*/

#include "strings.h"

int sort(stringarray strings,int startstring,int endstring)
/* PERFORMS SIPMPLE ASCII ALPHABETICAL BUBBLESORT IN AN ARRAY OF STRINGS, */
/* SORTING THE SMALLEST ASCII CODE TO THE BEGINNING. */
/* SORTING STARTS ON STRING startstring AND ENDS ON endstring */
/* RETURNS 0 ON SUCCESS AND -1 ON ERROR */
/* TO SORT LARGE AMOUNTS OF DATA, USE THE QuickSort FUNCTION (qsort.h) */
{
 int swapflag;
 int arrpos;
 _string_ swt;

 if(startstring>=endstring) return(-1);

 do {
  swapflag=0;

  for(arrpos=startstring;arrpos<endstring;arrpos++) {		/* swap ? */
   if(scompare(strings[arrpos],strings[arrpos+1],NOT SENSITIVE) > 0) {
    swapflag=1;
    swt=strings[arrpos]; strings[arrpos]=strings[arrpos+1]; strings[arrpos+1]=swt;
   }
  }

 } while (startstring<(--endstring) AND swapflag);
 return(0);
}
