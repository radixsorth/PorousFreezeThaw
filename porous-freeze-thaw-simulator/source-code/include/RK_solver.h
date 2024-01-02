/***********************************************\
* 4th order Runge - Kutta solver                *
* optimized for memory consumption              *
* (C) 2005-2006 Pavel Strachota                 *
* file: RK_solver.h                             *
\***********************************************/

#if !defined __RK_solver
#define __RK_solver

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
The Runge - Kutta solver integrates an equation system in the form Dx=f(t,x) where x and f are vectors
with n elements. (n is the system dimension)
------------------------------------------------------------------------------------------------------
The following structure defines all the necessary data for a separate equation system solution. Thanks
to that, calculations of more than one equation system can be in progress at the same time (of course,
only one of them running at the moment), provided that all the systems have smaller dimension than the
one passed to RK_init(). Before RK_solve() can be called to carry out the calculation, both of the
following must be ready:
1) The sover must be initialized by a call to RK_init()
2) the RK_SOLUTION structure must be set up for the particular equation system

THE CONCEPT OF THE RIGHT HAND SIDE META POINTER:
The Runge Kutta solver employs a callback right hand side function, which evaluates the right hand side
of the whole equation system (see below). Now consider a sophisticated solution of a parabolic PDE by
the method of lines, where the used spatial discretization scheme alters from time step to another
(e.g. instead of using an average of backward and forward difference, we alter the use of backward and
forward differences in the successive time steps, whis is faster). This results in an alternation of
the right hand side in the RK solver. To allow this, the RK_SOLUTION structure contains a
"meta-pointer" to the right hand side named 'meta_f'. This is a pointer to a function, of which the
return value is a pointer to the actual right hand side function. The function pointed to by 'meta_f'
is invoked before each time step. Its return value is used as the right hand side function for the next
time step. Its typical work is to toggle some switch and return a value dependent on that switch.

ABOUT THE RIGHT HAND SIDE FUNCTION (referred to as 'f' here):
'f' has 3 arguments: 'f(t,x,dest)' and must fill the array 'dest' that has the same dimension as 'x'
with the values of the right hand side. The internal structure and array handling inside 'f' is left
to user's responsibility. 'x' is a pointer to const and thus the elements of array cannot be modified
from inside 'f'. The system dimension is not passed to 'f', the user should handle it correctly by
his own means. For example, one can think of an array as of a matrix. 'f' must not suppose that
the parameter 'x' has any relationship with the location of the solution 'x' in this structure !!!
*/

/* pointer to the right hand side */
typedef void (*RK_RightHandSide)(FLOAT,const FLOAT *,FLOAT *);

typedef struct {
	int n;						/* system dimension */
	FLOAT t;					/* current time level */
	FLOAT * x;					/* pointer to the solution */
	RK_RightHandSide (*meta_f)();			/* the right hand side META pointer */

	FLOAT h;					/* the desired time step */
	FLOAT delta;					/* not used here: for compatibility with RK_A_solve() only */
} RK_SOLUTION;

int RK_init(int max_system_dimension);
/*
allocates memory for auxiliary arrays that allow solution of equation systems with dimension smaller than
or equal to 'max_system_dimension'.

NOTE: If you want to start another computation with greater system dimension, you must call RK_cleanup()
before you can pass a greater value to RK_init().

return codes:
0	success
-1	not enough memory
-2	invalid system dimension
-3	already initialized
*/

int RK_cleanup(void);
/* Frees memory allocated by RK_init()

return codes:
0	success
-3	not initialized yet
*/

int RK_solve(int steps, RK_SOLUTION * system);
/*
Performs 'steps' iterations using the fourth order "standard" Runge - Kutta method with
optimizations for memory consumption. However, this is a bit slower than the straightforward
implementation.

The system actually solved is defined in the 'system' structure.
The evolving solution is always stored in the array pointed to by 'system->x'. The current time level
is stored in 'system->t' and it is updated at the end of the calculation. The time step is defined
by 'system->h'.

return codes:
0	success
-2	invalid input data
-3	not initialized yet
-5	system dimension is greater than the maximum dimension passed to RK_A_init()
*/

#ifdef __cplusplus
}
#endif

#endif		/* __RK_solver */
