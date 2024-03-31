/*******************************************************\
* C language wrapper for Digithell Expression Evaluator *
* (C) 2005-2009 Pavel Strachota                         *
* file: ee_wrapper.h                                    *
\*******************************************************/

#if !defined __ee_wrapper
#define __ee_wrapper

#include "strings.h"

/*
This module provides several functions to allow simple use
of the Digithell Expression Evaluator in C.
Restrictions:
- no user operator and function definition
- no definition of constants (no Setup mode)
- only 100 extra variables

IMPORTANT:
You must link your program with 'exprsion' and 'strings' (in this order) libraries
in order to use this module. If you compile using a C compiler, you will also have
to explicitly include the C++ library (classes, init code, operator new etc...)
in the link: stdc++.

-------------

There are two variants of each function that accesses the evaluator. The one without
the leading underscore uses one predefined shared instance of the evaluator class. The
leading underscore variants use a custom instance of class EXPRESSION which is
created using the New_Evaluator() function and destroyed using Delete_Evaluator().

Creating multiple instances of the evaluator may be necessary e.g. in multithreaded
applications or when different sets of variables are used interchangeably in the
evaluator environment.

The ternary operator ?: from the evaluator extensions and the rand operator are available
in the shared evaluator only as they are not thread safe.
*/

typedef enum
{
 NO_ERROR,		/* successful evaluation */
 E_DOMAIN,		/* argument not in domain of operator or function */
 E_DIVZERO,		/* division by zero */
 E_OVERFLOW,		/* result is >MAXDOUBLE */
 E_UNDERFLOW,		/* reslut is <MINDOUBLE */
 E_TLOSS,		/* total loss of significant digits */
 /* constants used by the expression evaluator itself: */
 E_SYNTAX,		/* invalid identifier/syntax error */
 E_STACK,		/* operator or value stack overflow */
 E_PRELOAD		/* ev_evaluate() called when preload buffer is empty */
			/* or ev_parse() resp. eval() called and the */
			/* expression was too long (preload table full) */
} EVAL_ERROR;


#ifdef __cplusplus
extern "C" {
#endif

void * New_Evaluator();
/*
allocates a new instance of class EXPRESSION and returns a pointer to it. This pointer can be then
used as the first argument of the leading-underscore variants of the evaluator wrapper functions.

Returns the pointer to the new instance on success and NULL on error.
*/

void Delete_Evaluator(void * evaluator);
/* deletes the instance of class EXPRESSION previously allocated by New_Evaluator() */

/* ----------------------------------------- */

double eval(_conststring_ expr);
/* evaluates the given expression */

double _eval(void * evaluator, _conststring_ expr);

EVAL_ERROR ev_error(void);
/* returns the error code of the last evaluation */

EVAL_ERROR _ev_error(void * evaluator);

int ev_pos(void);
/* if there was an error, this is the position in the string where it occurred */

int _ev_pos(void * evaluator);

int ev_def_var(_conststring_ name, double value);
/*
defines a variable

return codes:
0	success
-1	invalid identifier
-2	too many defined variables
-3	identifier exists and has incompatible type
-4	trying to define a keyword
*/

int _ev_def_var(void * evaluator, _conststring_ name, double value);

int ev_undef_var(_conststring_ name);
/*
undefines a variable

return codes:
0	success
-1	invalid identifier
-2	identifier not defined
-4	trying to undefine a keyword
*/

int _ev_undef_var(void * evaluator, _conststring_ name);

/* ----- advanced functions ----- */

int ev_get_index(_conststring_ identifier);
/*
gets an index of a variable in the identifier table. This index is
then used as a parameter of ev_set_value() function.

return codes:
index	SUCCESS
-1	INVALID IDENTIFIER
-3	IDENTIFIER DOES NOT EXIST OR IS NOT OF TYPE 'VAR'
*/

int _ev_get_index(void * evaluator, _conststring_ identifier);

void ev_set_var_value(int index, double value);
/*
sets the value of the variable pointed to by index. The index is obtained
by the call to ev_get_index()
*/

void _ev_set_var_value(void * evaluator, int index, double value);

int ev_parse(_conststring_ expr);
/*
parses the given expression and prepares it for multiple evaluation.

return codes:
0	success
-1	error

NOTE:	In case of error, use ev_error() and ev_pos() to find out more.
*/

int _ev_parse(void * evaluator, _conststring_ expr);

double ev_evaluate(void);
/*
evaluates the pre-parsed expression. Use ev_error() to find out whether
the call to ev_evaluate() has been successfull.
*/

double _ev_evaluate(void * evaluator);

void ev_reset(void);
/* undefines all variables */

void _ev_reset(void * evaluator);

double ev_random(unsigned long long seed);
/*
returns a random real number in the interval (0,1). Pass a nonzero value
to reseed the underlying PRNG. The PRNG is independent of the global C library
random number generator.
*/

double _ev_random(void * evaluator, unsigned long long seed);

void install_evaluator_extensions(void);
/*
makes additional functions and operators available. This function should be called
as early as possible (otherwise, we could already have more than EXTRA_VARS variables
defined and the operator registration might not succeed)
This function should be called only once in the program.

The extensions are:
FUNCTIONS: max , min , sgn
OPERATORS: < , > , = , and, or, not,
and the ?: ternary operator.

Caution: The ternary operator E1?E2:E3 always evaluates both E2 and E3, even though only one
of them is returned as a result. So use with care!
*/

void _install_evaluator_extensions(void * evaluator);

#ifdef __cplusplus
}
#endif

#endif		/* __ee_wrapper */
