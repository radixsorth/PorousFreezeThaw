/*
STR_VDEF.C (STRINGS.H)
VARIABLE DEFINITION MODULE
COMMON C++ COMPILER EXTENSION
gcc C Language version
(C) 1998-2009 Digithell, Inc.
*/

#include "strings.h"

struct _TOSTRINGBUF tsb;
unsigned _maxstrlen=4096;	/* default maximum allowed string length */
char __stg_buf[2];		/* auxiliary buffer for the stg macro */
char _NOSTRING[]="";		/* the null string is one byte long, so we */
				/* can reduce the program size by referencing */
				/* the same null string at all places */
