/***********************************************\
* Common definitions header                     *
* (C) 2005 Pavel Strachota                      *
* file: common.h                                *
\***********************************************/

#if !defined __common
#define __common


#define FP_FLOAT	1
#define FP_DOUBLE	2
#define FP_LONG_DOUBLE	3

/*
In this file, there is a FLOAT type defined, which can be one of the
 - float
 - double
 - long double
types. However, to allow all programs that depend on this file to correctly
handle this type, there has to be some other stuff defined, in particular
the format strings for the ..printf and ..scanf functions. Hence, you can
not change the typedef directly, but you may modify the following definition:
*/

#define _DEFAULT_FP_PRECISION	FP_DOUBLE

/*
IMPORTANT:
  When you use the C formatted I/O functions with the FLOAT type, you can use the FTC_x
(for output) and iFTC_x (for input) macros that represent the TYPE CHARACTER of the FLOAT
format specifier in the format string. This will ensure that there will be no need to modify
your source code when the actual type of FLOAT changes. The example shows exactly what it means
(notice the use of several string literals that are concatenated by the compiler):
	printf("x = %" FTC_g ", y = %+10.6" FTC_G ".\n",x,y);
instead of
	printf("x = %Lg, y = %+10.6LG.\n",x,y);
if we assumed that FLOAT is defined as long double.
  Another problem is the choice of math function versions (e.g. sqrtf or sqrt or sqrtl) depending
on the actual FLOAT type. This is left up to you, but you can always use the _DEFAULT_FP_PRECISION
macro to help you decide.

Everything below this point should not be modified without deep understanding:
------------------------------------------------------------------------------
*/

#ifdef _DEFAULT_FP_PRECISION
	#if _DEFAULT_FP_PRECISION == FP_FLOAT
		typedef float FLOAT;
		/*
		float type characters for output are the same as for double due
		to automatic conversions of parameters passed through an
		ellipsis (...)
		*/
		#define FTC_e "e"
		#define FTC_E "E"
		#define FTC_f "f"
		#define FTC_g "g"
		#define FTC_G "G"
		#define iFTC_e "e"
		#define iFTC_f "f"
		#define iFTC_g "g"
	#elif _DEFAULT_FP_PRECISION == FP_DOUBLE
		typedef double FLOAT;
		#define FTC_e "e"
		#define FTC_E "E"
		#define FTC_f "f"
		#define FTC_g "g"
		#define FTC_G "G"
		#define iFTC_e "le"
		#define iFTC_f "lf"
		#define iFTC_g "lg"
	#elif _DEFAULT_FP_PRECISION == FP_LONG_DOUBLE
		typedef long double FLOAT;
		#define FTC_e "Le"
		#define FTC_E "LE"
		#define FTC_f "Lf"
		#define FTC_g "Lg"
		#define FTC_G "LG"
		#define iFTC_e "Le"
		#define iFTC_f "Lf"
		#define iFTC_g "Lg"
	#else
		#error FATAL ERROR in common.h: FLOAT has an unknown floating point type.
	#endif
#else
		#error FATAL ERROR in common.h: FLOAT does not have a type.
#endif

/* ---------------------- */

typedef unsigned short WORD;

typedef struct {
	FLOAT x;
	FLOAT y;
} VECTOR2D;

#endif
