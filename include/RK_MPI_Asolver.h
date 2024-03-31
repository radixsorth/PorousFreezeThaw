/***************************************************\
* 4th order Runge - Kutta solver                    *
* Merson's modification with adaptive time stepping *
* M P I  version with block resizing support        *
*                                                   *
* this file is part of                              *
* DDLBF (Digithell Dynamic Load Balancing Facility) *
*                                                   *
* (C) 2005-2013 Pavel Strachota                     *
* file: RK_MPI_Asolver.c                            *
\***************************************************/

#if !defined __RK_MPI_Asolver
#define __RK_MPI_Asolver

#include "common.h"
#include <mpi.h>

/* MPI data type for the FLOAT data type (note the two underscores in MPI__FLOAT) */
#if _DEFAULT_FP_PRECISION == FP_FLOAT
	#define MPI__FLOAT	MPI_FLOAT
#elif _DEFAULT_FP_PRECISION == FP_DOUBLE
	#define MPI__FLOAT	MPI_DOUBLE
#elif _DEFAULT_FP_PRECISION == FP_LONG_DOUBLE
	#define MPI__FLOAT	MPI_LONG_DOUBLE
#endif

/* commands from the master rank (can be ORed) */
#define RKA_CMD_h_TOO_SMALL	1
#define RKA_CMD_NAN		2
#define RKA_CMD_UPDATE		4
#define RKA_CMD_FINISHED	8
#define RKA_CMD_NEXTFINISH	16
#define RKA_CMD_BREAK		32

#ifdef __cplusplus
extern "C" {
#endif

/*
The Runge - Kutta solver integrates an equation system in the form Dx=f(t,x) where x and f are vectors
with n elements. (n is the system dimension)
------------------------------------------------------------------------------------------------------
The following structure defines all the necessary data for a separate equation system solution. Thanks
to that, calculations of more than one equation system can be in progress at the same time (of course,
only one of them running at the moment), provided that all the blocks have smaller dimension than the
size passed to RK_MPI_A_init(). Before RK_MPI_A_solve() can be called to carry out the calculation,
all of the following must be ready:
1) The MPI run time environment must be initialized by a call to MPI_Init();
2) The sover must be initialized by a call to RK_MPI_A_init()
3) the RK_MPI_SOLUTION structure must be set up for the particular equation system

NOTE:	'h' is the initial time step. If 'h==0', then the time step will be calculated
	automatically from the value of 'final_time-t' (final_time is passed to RK_MPI_A_solve).

NOTE:	The NAN handling features (see below) are common for all calculations. If you want to check
	whether a NAN occurred during the calculation, you must to that immediately after the
	calculation finishes. Another successive calculation even for a different equation system
	overwrites the NAN status again.

IMPORTANT:
In 'h', RK_MPI_A_solve() always stores the value of the last estimated time step BEFORE the final trimmed
time step that finishes the calculation right at final_time. This allows multiple calls to
RK_MPI_A_solve() with the time step being maintained from the previous reasonable estimates.

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

NOTE:	The 'meta_f' function is called independently on all processes of the parallel calculation.
	The decision on right hand side choice should therefore depend only on the count of 'meta_f'
	invocations, or on whatever else ensuring that the right hand side functions on all processes
	always correspond to each other. Of course, the number of 'meta_f' invocations is the same on
	all processes.

ABOUT THE RIGHT HAND SIDE FUNCTION (referred to as 'f' here):
'f' has 3 arguments: 'f(t,x,dest)' and must fill the array 'dest' that has the same dimension as 'x'
with the values of the right hand side. The internal structure and array handling inside 'f' is left
to user's responsibility. 'x' is a pointer to const and thus the elements of array cannot be modified
from inside 'f'. The system dimension is not passed to 'f', the user should handle it correctly by
his own means. For example, one can think of an array as of a matrix. 'f' must not suppose that
the parameter 'x' has any relationship with the location of the solution 'x' in this structure !!!

PARALLELIZATION AND LOAD BALANCING:
With MPI, the solution is performed by more than one process, possibly on different machines (at least,
on different processors). Each process computes only a part of the whole equation system, which is
referred to as a block. One process (the master process) controls the calculation. Its rank is passed
to RK_MPI_A_init(). Before the calculation begins, the ODE system initial condition must be distributed
among the processes.

Before each coefficient (K1,K2,...) is calculated (that is, each time the right hand side is evaluated),
it will likely be necessary to transfer certain values of the array passed to the formal parameter x of
the right hand side between some processes that take part in the calculation. That's because the
particular equations in the whole system are not independent and some of them use the value of x from a
different block. For the following reasons, the data interchange must be carried out directly in the
right hand side function:

 - which processes are involved in the transfer and what data is to be sent depends on the problem
   being solved, and so there can't be any general form of the transfer built into the solver

 - the actual contents of the (second) FORMAL PARAMETER 'x' of the right hand side depend on the
   coefficient being calculated. Written in a symbolic notation, the values used are as follows:
   K1 = f( t,     x );
   K2 = f( t+h/3, x+K1*h/3 );
   K3 = f( t+h/3, x+(K1+K2)*h/6 );
   K4 = f( t+h/2, x+(K1+3*K3)*h/8 );
   K5 = f( t+h,	  x+(0.5*K1-1.5*K3+2*K4)*h );
   In each step, the array passed to f DEPENDS ON THE PREVIOUSLY CALCULATED COEFFICIENT. Therefore, the
   values of x that are needed from some other block depend on the result of the PREVIOUS coefficient
   calculation in that block and thus they have to be obtained BEFORE the calculation of the next
   coefficient. Since the right hand side in each block is always passed its own part of the correct
   array, the best implementation of the communication is to incorporate it directly to the beginning of
   the function that evaluates the right hand side.

We will demonstrate the data interchange needs on a parabolic PDE problem:
Consider the solution of a semidiscrete scheme resulting from the method of lines. It is defined
on a (say 2-dimensional) grid and we have one ODE for each node of the grid. In order to calculate the
value of the solution at a certain node and at the time (t+h), we need the current value (at time t)
at that node as well as at some neighbor nodes. If we divide the (square) grid into square blocks for
parallel calculation, we will need to interchange the values of the BLOCK BOUNDARY nodes between those
processes that solve the problem on the ADJACENT blocks.
In other words: Apart from the block itself, each process will need to know the values at the boundary
nodes of the adjacent blocks. However, these values must be stored in SEPARATE array(s), since the solver
is universal: It does not know about the nature of the original problem and the array passed to the "x"
formal parameter of the right hand side f will contain exactly the data corresponding to the block only.

DDLBF is an approach that allows the blocks to resize during the calculation, depending on the
different speeds the processes calculate at. This is not always possible, e.g. if the problem is
defined on a square grid which is divided into n times n square blocks. The solver provides a callback
that should control block resizing. If you don't use DDLBF, you can set the pointer to DDLBF callback
to NULL.

Note that the callback function is called identically regardless on the process rank. The user is responsible
for the rank management.

CALCULATION TIME MEASUREMENT PRECAUTIONS:
The calculation wall time measurement must only consider those parts of calculation where no data
transfers occur. If the right hand side synchronizes data between blocks, we cannot measure calculation time
for a larger amount of code than one right hand side evaluation. Otherwise, the ranks would be synchronized
before the end of time measurement and we would get very similar times for all ranks, even though they would
be running on machines with very different speed. The solver therefore provides no means for time measurement,
and it should be performed from within the right hand side function. A different attitude to time measurement
is to perform a reference calculation in the DDLBF_Rearrange() callback and measure its time. This will however
make the rearrangement check process longer.
*/

/* pointer to the right hand side */
typedef void (*RK_RightHandSide)(FLOAT,const FLOAT *,FLOAT *);

typedef struct __struct_RK_MPI_SOLUTION {
	int n;						/* the (current) dimension of the processed block.
							   The solver does not have to know the size of the whole
							   system at all. */

	FLOAT t;					/* current time level (only needed by the master rank) */
	FLOAT * x;					/* pointer to the solution */
	RK_RightHandSide (*meta_f)();			/* the right hand side META pointer */

	FLOAT h;					/* the current time step (only needed by the master rank) */
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
	FLOAT delta;					/* the maximum desired relative error (only needed by the master rank) */

	/* DDLBF callback function (please read their description carefully) */

	int (* DDLBF_Rearrange)(int);			/* this is called after each calculation of the next time level
							   when the arrays "x" contain a value of x(t+h) in all processes.
							   It should perform the actual block rearrangement based on the
							   (accumulated) elapsed time of the calculation.

							   This function is passed the current block size and it must return
							   the new size of the block, greater than zero and less than or equal
							   to max_n. If the block size is not to be changed, DDLBF_Rearrange()
							   simpy returns the same value that has been passed to it. This
							   function is also responsible for the following:
							   - send the elapsed time to the master process OR (if master)
							     gather the elapsed times from other processes,
							     use them to calculate the new block sizes and
							     broadcast the the new block sizes to other processes

							     NOTE:	The rank of the 'master' process mentioned here that
							     		would control the block redistribution can be
							     		different from the master process of the solver
									(the rank passed to RK_MPI_A_init()).

							   - send/receive data in order to rearrange the blocks among
							     all processes. This is not done automatically by the solver,
							     since the operation may need much more processing than just
							     sending (and/or moving in memory) the parts of the solution "x".
							     THE NEW BLOCK DATA MUST BE STORED IN THE ARRAY 'x' AGAIN.

							   NOTE:	It is up to user whether he wants to rearrange the blocks
							   		after each time level (this would be unwise, and thus
									in most iterations, the callback functions should just
									return do nothing with the blocks).*/

	/* Service callback function */

	int (* Service_Callback)(FLOAT, const struct __struct_RK_MPI_SOLUTION * const);
							/* a service callback that may optionally provide logging capabilities
							   or other services during the computation. It is called after each
							   successful time step, similarly as the DDLBF_Rearrange function. The
							   first argument passed to this function (in any rank) is the final
							   computation time as given in the master rank to RK_MPI_A_solve().
							   Secondly, a pointer to the system structure is passed to this
							   function (analogue of the 'this' keyword in C++ member functions) in
							   order to let it access the computation status data. If the callback
							   returns nonzero in rank 0, the computation is immediately terminated.
							   Return values in other ranks are ignored. The callback does not have
							   to be defined in all ranks (can be NULL).
							*/

	long steps;					/* the number of time steps performed by the solver. This variable is
							   increased in all ranks each time a successful time step is
							   calculated. This is useful for some EOC (Experimental Order of
							   Convergence) computations. The user must initialize/reset this
							   variable manually. Note that the time step length varies. */
} RK_MPI_SOLUTION;

int RK_MPI_A_init(int max_block_size, MPI_Comm comm, int master_rank);
/*
allocates memory for auxiliary arrays that allow solution of equation systems with block size smaller
than or equal to 'max_block_size'. Initializes the RK solver so that it can perform a parallel calculation
in the group of processes specified by the communicator 'comm'. Typically, MPI_COMM_WORLD will be used.

'master_rank' determines the rank that will be responsible for the calculation progress on all
processes. The master rank has a privileged position among the ranks, e.g. some parameters of
the calculation are ignored in all other ranks. More information can be found in the comments
throughout the files RK_MPI_Asolver.h and RK_MPI_Asolver.c.

NOTE: If you want to start another computation with greater block size, you must call
RK_MPI_A_cleanup() before you can pass a greater value to RK_MPI_A_init().

return codes:
0	success
-1	not enough memory
-2	invalid system dimension
-3	already initialized
-4	MPI not initialized
*/

int RK_MPI_A_cleanup(void);
/* Frees memory allocated by RK_MPI_A_init()

return codes:
0	success
-3	not initialized yet
*/

void RK_MPI_A_handle_NAN(int hN);
/*
Turns the NAN and +-INF handling ON/OFF.
Pass a nonzero value to RK_MPI_A_handle_NAN() to turn it ON, pass 0 to turn it OFF.

By default, the NAN handling is disabled: Under special circumstances
(very singular right hand side and extremely long initial time step) NANs or +-INF may
appear during the solution and the relative error eps (compared to delta_) will
then be undefined. Hence, there will be no means of obtaining the new value of the
time step and most likely the solver will get stuck in an infinite loop. NAN handling
finds such cases and automatically retries with a 10 times smaller time step, until
the NANs stop to occur. If the obtained time step is extremely small so that the calculation
would take hours, the solver breaks the calculation and reports an error.

NOTE: The very FIRST step is seldom SO MUCH rapid so that NANs develop IMMEDIATELY.
After the first iteration, h is already corrected and thus no problems with NANs
will occur in most cases. Since NAN handling takes time, it is recommended not to turn
it ON unless you really need it e.g. for experiments in the testing phase.

NOTE: You can only use NAN handling when the SIGFPE signal raising is disabled.
(and by default it is) See the C library documentation for details.

NOTE:	NAN handling requires the C library to be ISO C99 compliant.

WARNING: This function works on the master process only !!!
*/

int RK_MPI_A_check_NAN();
/*
returns nonzero if there a NAN or +-INF occurred in the last calculation (upon the last call to
RK_MPI_A_Solve(). This may imply some problem with your equation or too loose setting of delta.

NOTE: This function works on all processes involved in the calculation
*/

int RK_MPI_A_solve(FLOAT final_time, RK_MPI_SOLUTION * system);
/*
Performs the ODE system integration up to the time level 'final_time', using the
Merson's modification of the fourth order Runge-Kutta scheme with adaptive
time stepping. It will not be obvious how many iterations are necessary to reach
the 'final_time' !!!

The system actually solved is defined in the 'system' structure.
The block of the evolving solution is always stored in the array pointed to by 'system->x'.
The current time level is stored in 'system->t' and it is updated at the end of the calculation,
as well as the current time step 'system->h'.

NOTE: The 'final_time' parameter is ignored on all other ranks than the master rank.

return codes:
0	success
1	interrupted by Service_Callback() called in the master rank
-2	invalid 'system' specification
-3	not initialized yet
-4	break: cannot adjust time step - permanent occurrence of NANs. (only if NAN handling is ON)
-5	block dimension is greater than the maximum dimension passed to RK_MPI_A_init()
-6	error in one of the other processes
*/

#ifdef __cplusplus
}
#endif

#endif		/* __RK_MPI_Asolver */
