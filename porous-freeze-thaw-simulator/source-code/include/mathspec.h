/***************************************************************\
* Choice of the math include file and math compatibility issues	*
* (C) 2005-2013 Pavel Strachota					*
* file: mathspec.h						*
\***************************************************************/

#if !defined __mathspec
#define __mathspec

#include "common.h"

/*
For the Intel C++ compiler, you can specify <mathimf.h> instead of <math.h> here.
That will make all the programs use the Intel optimized math library (libimf).

NOTE:	Experiments proved that libimf is always used with the Intel C++ compiler,
	regardless on which header is actually included.
*/
 #include <math.h>

/*
On some compilers, the C math functions are not defined for more argument types like e.g.
pow,powf,powl are with gcc. gcc accepts these functions even in C++ when the
standard math header 'cmath' is included: In this header, all math functions are overloaded
for the three types float, double and long_double. However, the C-style math functions
(distinguihed by the 'f' or 'l' suffix) are still declared.

In the next section, we define our versions of the 'suffix-distinguished' math functions
so that they can always be used.
NOTE:	1) only the most common functions are defined here
	2) these definitions should NOT be used if the compiler supports the
	   suffix-distinguished function versions
*/

#ifdef __USE_MATH_FL_MACROS

	/* 'float' versions */
	#define acosf(x)	( (float) acos ((float)(x)) )
	#define asinf(x)	( (float) asin ((float)(x)) )
	#define atanf(x)	( (float) atan ((float)(x)) )
	#define cbrtf(x)	( (float) cbrt ((float)(x)) )
	#define ceilf(x)	( (float) ceil ((float)(x)) )
	#define cosf(x)		( (float) cos ((float)(x)) )
	#define coshf(x)	( (float) cosh ((float)(x)) )
	#define expf(x)		( (float) exp ((float)(x)) )
	#define fabsf(x)	( (float) fabs ((float)(x)) )
	#define floorf(x)	( (float) floor ((float)(x)) )
	#define logf(x)		( (float) log ((float)(x)) )
	#define log10f(x)	( (float) log10 ((float)(x)) )
	#define sinf(x)		( (float) sin ((float)(x)) )
	#define sinhf(x)	( (float) sinh ((float)(x)) )
	#define sqrtf(x)	( (float) sqrt ((float)(x)) )
	#define tanf(x)		( (float) tan ((float)(x)) )
	#define tanhf(x)	( (float) tanh ((float)(x)) )

	#define atan2f(x,y)	( (float) atan2 ((float)(x),(float)(y)) )
	#define powf(x,y)	( (float) pow ((float)(x),(float)(y)) )
	#define fmodf(x,y)	( (float) fmod ((float)(x),(float)(y)) )
	#define fminf(x,y)	( (float) fmin ((float)(x),(float)(y)) )
	#define fmaxf(x,y)	( (float) fmax ((float)(x),(float)(y)) )

	#define copysignf(x,y)	( (float) copysign ((float)(x),(float)(y)) )

	/* 'long double' versions */
	#define acosl(x)	( (long double) acos ((long double)(x)) )
	#define asinl(x)	( (long double) asin ((long double)(x)) )
	#define atanl(x)	( (long double) atan ((long double)(x)) )
	#define cbrtl(x)	( (long double) cbrt ((long double)(x)) )
	#define ceill(x)	( (long double) ceil ((long double)(x)) )
	#define cosl(x)		( (long double) cos ((long double)(x)) )
	#define coshl(x)	( (long double) cosh ((long double)(x)) )
	#define expl(x)		( (long double) exp ((long double)(x)) )
	#define fabsl(x)	( (long double) fabs ((long double)(x)) )
	#define floorl(x)	( (long double) floor ((long double)(x)) )
	#define logl(x)		( (long double) log ((long double)(x)) )
	#define log10l(x)	( (long double) log10 ((long double)(x)) )
	#define sinl(x)		( (long double) sin ((long double)(x)) )
	#define sinhl(x)	( (long double) sinh ((long double)(x)) )
	#define sqrtl(x)	( (long double) sqrt ((long double)(x)) )
	#define tanl(x)		( (long double) tan ((long double)(x)) )
	#define tanhl(x)	( (long double) tanh ((long double)(x)) )

	#define atan2l(x,y)	( (long double) atan2 ((long double)(x),(long double)(y)) )
	#define powl(x,y)	( (long double) pow ((long double)(x),(long double)(y)) )
	#define fmodl(x,y)	( (long double) fmod ((long double)(x),(long double)(y)) )
	#define fminl(x,y)	( (long double) fmin ((long double)(x),(long double)(y)) )
	#define fmaxl(x,y)	( (long double) fmax ((long double)(x),(long double)(y)) )

	#define copysignl(x,y)	( (long double) copysign ((long double)(x),(long double)(y)) )

#endif

/*
In the follwing section, the same math functions are defined with the 'F' suffix,
which indicates suitability for the FLOAT type (defined in common.h). These definition
are always set so that the precision of the arguments and the result match the  actual
definition of the FLOAT type. When dealing with the FLOAT type, it is therefore recommended
to always use these names instead of the standard functions.
*/

#if _DEFAULT_FP_PRECISION == FP_FLOAT
	#define acosF		acosf
	#define asinF		asinf
	#define atanF		atanf
	#define cbrtF		cbrtf
	#define ceilF		ceilf
	#define cosF		cosf
	#define coshF		coshf
	#define expF		expf
	#define fabsF		fabsf
	#define floorF		floorf
	#define logF		logf
	#define log10F		log10f
	#define sinF		sinf
	#define sinhF		sinhf
	#define sqrtF		sqrtf
	#define tanF		tanf
	#define tanhF		tanhf

	#define atan2F		atan2f
	#define powF		powf
	#define fmodF		fmodf
	#define fminF		fminf
	#define fmaxF		fmaxf

	#define copysignF	copysignf
#elif _DEFAULT_FP_PRECISION == FP_DOUBLE
	#define acosF		acos
	#define asinF		asin
	#define atanF		atan
	#define cbrtF		cbrt
	#define ceilF		ceil
	#define cosF		cos
	#define coshF		cosh
	#define expF		exp
	#define fabsF		fabs
	#define floorF		floor
	#define logF		log
	#define log10F		log10
	#define sinF		sin
	#define sinhF		sinh
	#define sqrtF		sqrt
	#define tanF		tan
	#define tanhF		tanh

	#define atan2F		atan2
	#define powF		pow
	#define fmodF		fmod
	#define fminF		fmin
	#define fmaxF		fmax

	#define copysignF	copysign
#elif _DEFAULT_FP_PRECISION == FP_LONG_DOUBLE
	#define acosF		acosl
	#define asinF		asinl
	#define atanF		atanl
	#define cbrtF		cbrtl
	#define ceilF		ceill
	#define cosF		cosl
	#define coshF		coshl
	#define expF		expl
	#define fabsF		fabsl
	#define floorF		floorl
	#define logF		logl
	#define log10F		log10l
	#define sinF		sinl
	#define sinhF		sinhl
	#define sqrtF		sqrtl
	#define tanF		tanl
	#define tanhF		tanhl

	#define atan2F		atan2l
	#define powF		powl
	#define fmodF		fmodl
	#define fminF		fminl
	#define fmaxF		fmaxl

	#define copysignF	copysignl
#endif

#endif
