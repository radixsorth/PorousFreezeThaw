/*******************************************************\
* C language wrapper for Digithell Expression Evaluator *
* (C) 2005-2009 Pavel Strachota                         *
* file: ee_wrapper.cc                                   *
\*******************************************************/

#include "exprsion.h"
#include <stddef.h>

/*
This module provides several functions to allow simple use
of the Digithell Expression Evaluator in C.
Restrictions:
- no user operator and function definition
- no definition of constants (no Setup mode)
- only 100 extra variables

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

#define EXTRA_VARS	100


static unsigned qflag=0;
static unsigned qptr=0;		// ternary operator ? : occurrence counter
				// (see the expression evaluator extensions below)

static EXPRESSION e(11+EXTRA_VARS,4000);// EXTRA_VARS extra variables reserved.
					// (the "11" relates to operator extensions, see below)

extern "C" void * New_Evaluator()
/*
allocates a new instance of class EXPRESSION and returns a pointer to it. This pointer can be then
used as the first argument of the leading-underscore variants of the evaluator wrapper functions.

Returns the pointer to the new instance on success and NULL on error.
*/
{
	EXPRESSION * new_e;
	try {
		new_e = new EXPRESSION(EXTRA_VARS,200);
	} catch(...) {
		return(0);
	}

	new_e->Enter_Setup();
	new_e->Remove("rand");	// remove 'rand' as it is not thread safe
	new_e->Exit_Setup();
	return(new_e);
}

extern "C" void Delete_Evaluator(void * evaluator)
/* deletes the instance of class EXPRESSION previously allocated by New_Evaluator() */
{
	delete ((EXPRESSION *)evaluator);
}

extern "C" double eval(_conststring_ expr)
/* evaluates the given expression */
{
	qptr=0;
	return(e(expr));
}

extern "C" double _eval(void * evaluator, _conststring_ expr)
{
	return((*((EXPRESSION *)evaluator))(expr));
}

extern "C" EVAL_ERROR ev_error(void)
/* returns the error code of the last evaluation */
{
	return(e.Error);
}

extern "C" EVAL_ERROR _ev_error(void * evaluator)
{
	return(((EXPRESSION *)evaluator)->Error);
}

extern "C" int ev_pos(void)
/* if there was an error, this is the position in the string where it occurred */
{
	return(e.Location);
}

extern "C" int _ev_pos(void * evaluator)
{
	return(((EXPRESSION *)evaluator)->Location);
}

extern "C" int ev_def_var(_conststring_ name, double value)
/*
defines a variable

return codes:
0	success
-1	invalid identifier
-2	too many defined variables
-3	identifier exists and has incompatible type
-4	trying to define a keyword
*/
{
	return(e.Set(name,value));
}

extern "C" int _ev_def_var(void * evaluator, _conststring_ name, double value)
{
	return(((EXPRESSION *)evaluator)->Set(name,value));
}

extern "C" int ev_undef_var(_conststring_ name)
/*
undefines a variable

return codes:
0	success
-1	invalid identifier
-2	identifier not defined
-4	trying to undefine a keyword
*/
{
	return(e.Remove(name));
}

extern "C" int _ev_undef_var(void * evaluator, _conststring_ name)
{
	return(((EXPRESSION *)evaluator)->Remove(name));
}

/* ----- advanced functions ----- */

extern "C" int ev_get_index(_conststring_ identifier)
/*
gets an index of a variable in the identifier table. This index is
then used as a parameter of ev_set_value() function.

return codes:
index	SUCCESS
-1	INVALID IDENTIFIER
-3	IDENTIFIER DOES NOT EXIST OR IS NOT OF TYPE 'VAR'
*/
{
	return(e.GetIndex(identifier));
}

extern "C" int _ev_get_index(void * evaluator, _conststring_ identifier)
{
	return(((EXPRESSION *)evaluator)->GetIndex(identifier));
}

extern "C" void ev_set_var_value(int index, double value)
/*
sets the value of the variable pointed to by index. The index is obtained
by the call to ev_get_index()
*/
{
	e.SetValue(index,value);
}

extern "C" void _ev_set_var_value(void * evaluator, int index, double value)
{
	((EXPRESSION *)evaluator)->SetValue(index,value);
}

extern "C" int ev_parse(_conststring_ expr)
/*
parses the given expression and prepares it for multiple evaluation.

return codes:
0	success
-1	error

NOTE:	In case of error, use ev_error() and ev_pos() to find out more.
*/
{
	return(e.Parse(expr));
}

extern "C" int _ev_parse(void * evaluator, _conststring_ expr)
{
	return(((EXPRESSION *)evaluator)->Parse(expr));
}

extern "C" double ev_evaluate(void)
/*
evaluates the pre-parsed expression. Use ev_error() to find out whether
the call to ev_evaluate() has been successfull.
*/
{
	return(e.Eval());
}

extern "C" double _ev_evaluate(void * evaluator)
{
	return(((EXPRESSION *)evaluator)->Eval());
}

extern "C" void ev_reset(void)
/* undefines all variables */
{
	e.Reset();
}

extern "C" void _ev_reset(void * evaluator)
{
	((EXPRESSION *)evaluator)->Reset();
}

extern "C" double ev_random(unsigned long long seed)
/*
returns a random real number in the interval (0,1). Pass a nonzero value
to reseed the underlying PRNG. The PRNG is independent of the global C library
random number generator.
*/
{
	return(e.Random(seed));
}

extern "C" double _ev_random(void * evaluator, unsigned long long seed)
{
	return(((EXPRESSION *)evaluator)->Random(seed));
}

// ------------------------------------------

// Expression evaluator extensions - originally taken from the GraphShow System6000 application

static double __sgn__(double x,EVAL_ERROR *)
{
 if(x>0) return(1); else if(x<0) return (-1); else return(0);
}

static double __max__(double x,double y,EVAL_ERROR *)
{
 if(x>y) return(x); else return(y);
}


static double __min__(double x,double y,EVAL_ERROR *)
{
 if(x<y) return(x); else return(y);
}

// logical operators
static double __isless__(double x,double y,EVAL_ERROR *)
{
	if(x<y) return(1); else return(0);
}

static double __isgreater__(double x,double y,EVAL_ERROR *)
{
	if(x>y) return(1); else return(0);
}

static double __isequal__(double x,double y,EVAL_ERROR *)
{
	if(x==y) return(1); else return(0);
}

static double __and__(double x,double y,EVAL_ERROR *)
{
	if(x!=0 AND y!=0) return(1); else return(0);
}

static double __or__(double x,double y,EVAL_ERROR *)
{
	if(x!=0 OR y!=0) return(1); else return(0);
}

static double __not__(double x,EVAL_ERROR *)
{
	if(x!=0) return(0); else return(1);
}

static double __qmark__(double x,double y,EVAL_ERROR *error)
{
	if(qptr>15) { *error=E_STACK; return(0); }
	if(x!=0) qflag |= 1<<qptr; else qflag &= ~(1<<qptr);
	qptr++;
	return(y);
}

static double __colon__(double x,double y,EVAL_ERROR *error)
{
	if(NOT qptr) { *error=E_SYNTAX; return(0); }
	qptr--;
	if(qflag&(1<<qptr)) return(x);
	else return(y);
}

extern "C" void install_evaluator_extensions()
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
{
	e.Enter_Setup();
	e.Set("sgn",__sgn__,16);
	e.Set("max",__max__,16);
	e.Set("min",__min__,16);

	e.Set("<",__isless__,24);
	e.Set(">",__isgreater__,24);
	e.Set("=",__isequal__,24);

	e.Set("and",__and__,26);
	e.Set("or",__or__,26);
	e.Set("not",__not__,25);

	e.Set("?",__qmark__,27);
	e.Set(":",__colon__,28);
	e.Exit_Setup();
}

extern "C" void _install_evaluator_extensions(void * evaluator)
{
	((EXPRESSION *)evaluator)->Enter_Setup();
	((EXPRESSION *)evaluator)->Set("sgn",__sgn__,16);
	((EXPRESSION *)evaluator)->Set("max",__max__,16);
	((EXPRESSION *)evaluator)->Set("min",__min__,16);

	((EXPRESSION *)evaluator)->Set("<",__isless__,24);
	((EXPRESSION *)evaluator)->Set(">",__isgreater__,24);
	((EXPRESSION *)evaluator)->Set("=",__isequal__,24);

	((EXPRESSION *)evaluator)->Set("and",__and__,26);
	((EXPRESSION *)evaluator)->Set("or",__or__,26);
	((EXPRESSION *)evaluator)->Set("not",__not__,25);

	((EXPRESSION *)evaluator)->Exit_Setup();
}
