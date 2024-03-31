/***************************************************\
* 4th order Runge - Kutta solver                    *
* Merson's modification with adaptive time stepping *
* (C) 2005-2013 Pavel Strachota                     *
* file: RK_Asolver.h                                *
\***************************************************/

#if !defined __RK_Asolver
#define __RK_Asolver

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
one passed to RK_A_init(). Before RK_A_solve() can be called to carry out the calculation, both of the
following must be ready:
1) The sover must be initialized by a call to RK_A_init()
2) the RK_SOLUTION structure must be set up for the particular equation system

NOTE:	'h' is the initial time step. If 'h==0', then the time step will be calculated
	automatically from the value of 'final_time-t' (final_time is passed to RK_A_solve).

NOTE:	The NAN handling features (see below) are common for all calculations. If you want to check
	whether a NAN occurred during the calculation, you must to that immediately after the
	calculation finishes. Another successive calculation even for a different equation system
	overwrites the NAN status again.

IMPORTANT:
In 'h', RK_A_solve() always stores the value of the last estimated time step BEFORE the final trimmed
time step that finishes the calculation right at final_time. This allows multiple calls to
RK_A_solve() with the time step being maintained from the previous reasonable estimates.

'n' is the number of scalar equations within the system, i.e. the required size of all arrays involved
in the computation.

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
time step (at the very beginning and after a successful time step calculation, i.e. when the relative
error is acceptable). Its typical work is to toggle some switch and return a value dependent on that
switch.

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

	FLOAT h;					/* the current time step */
	FLOAT h_min;					/* the minimum allowed time step for rejection. If time step drops below
							   min_h, it is always considered successful. This allows to overcome
							   discontinuities with respect to time in the right hand side (e.g. due
							   to jumps in boundary conditions specification in the case of solving
							   semidiscrete schemes for PDE). In such a case, it is inevitable that
							   the time when the discontinuity occurs falls in between the time levels
							   of evaluation of the Kn coefficients. As a result, the error eps
							   is large regardless of the time step, a series of unsuccessful iterations
							   occurs and h drops to almost zero. This is prevented here by extending the
							   condition for succesful time step from "eps<delta" to "eps<delta || h<min_h".
							   (only needed by the master rank) */
	FLOAT delta;					/* the maximum desired relative error */

	long steps;					/* the number of time steps performed by the solver.
							   This variable is increased each time a successful time
							   step is calculated. This is useful for some EOC
							   (Experimental Order of Convergence) computations.
							   The user must initialize/reset this variable manually.
							   Note that the time step length varies. */
} RK_SOLUTION;

int RK_A_init(int max_system_dimension);
/*
allocates memory for auxiliary arrays that allow solution of equation systems with dimension smaller than
or equal to 'max_system_dimension'.

NOTE: If you want to start another computation with greater system dimension, you must call RK_A_cleanup()
before you can pass a greater value to RK_A_init().

return codes:
0	success
-1	not enough memory
-2	invalid system dimension
-3	already initialized
*/

int RK_A_cleanup(void);
/* Frees memory allocated by RK_A_init()

return codes:
0	success
-3	not initialized yet
*/

void RK_A_handle_NAN(int hN);
/*
Turns the NAN and +-INF handling ON/OFF.
Pass a nonzero value to RK_A_handle_NAN() to turn it ON, pass 0 to turn it OFF.

By default, the NAN handling is disabled: Under special circumstances
(very singular right hand side and extremely long initial time step) NANs or +-INF may
appear during the solution and the relative error eps (compared to delta_) will
then be undefined. Hence, there will be no means of obtaining the new value of the
time step and most likely the solver will get stuck in an infinite loop. NAN handling
finds such cases and automatically retries with a 10 times smaller time step, until
the NANs stop to occur. If the obtained time step is extremely small so that the calculation
would take hours, the solver breaks the calculation and reports an error.

NOTE: The very FIRST step is seldom SO MUCH rapid so that NANs develop IMMEDIATELY.
After the first iteration, h__ is already corrected and thus no problems with NANs
will occur in most cases. Since NAN handling takes time, it is recommended not to turn
it ON unless you really need it e.g. for experiments in the testing phase.

NOTE: You can only use NAN handling when the SIGFPE signal raising is disabled.
(and by default it is) See the C library documentation for details.

NOTE: NAN handling requires the C library to be ISO C99 compliant.
*/

int RK_A_check_NAN();
/*
returns nonzero if there a NAN or +-INF occured in the last calculation (upon the last call to
RK_A_Solve(). This may imply some problem with your equation or too loose setting of delta_.
*/

int RK_A_solve(FLOAT final_time, RK_SOLUTION * system);
/*
Performs the ODE system integration up to the time level 'final_time', using the
Merson's modification of the fourth order Runge-Kutta scheme with adaptive
time stepping. It will not be obvious how many iterations are necessary to reach
the 'final_time' !!!

The system actually solved is defined in the 'system' structure.
The evolving solution is always stored in the array pointed to by 'system->x'. The current time level
is stored in 'system->t' and it is updated at the end of the calculation, as well as the current
time step 'system->h'.

return codes:
0	success
-2	inavalid 'system' specification
-3	not initialized yet
-4	break: cannot adjust time step - permanent occurrence of NANs. (only if NAN handling is ON)
-5	system dimension is greater than the maximum dimension passed to RK_A_init()
*/

#ifdef __cplusplus
}
#endif

#endif		/* __RK_Asolver */
