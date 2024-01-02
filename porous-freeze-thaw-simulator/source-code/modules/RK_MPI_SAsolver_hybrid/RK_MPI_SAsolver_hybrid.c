/***************************************************************\
* 4th order Runge - Kutta solver                                *
* Merson's modification with adaptive time stepping             *
* - hybrid OpenMP / M P I  version with block resizing support  *
* - supports SPARSE system distribution in memory               *
*                                                               *
* this file is part of                                          *
* DDLBF (Digithell Dynamic Load Balancing Facility)             *
*                                                               *
* (C) 2005-2015 Pavel Strachota                                 *
* file: RK_MPI_SAsolver_hybrid.c                                *
\***************************************************************/

/*
OpenMP note:

OpenMP parallelization of this RK/Merson solver consists in enclosing the whole time
iteration loop into an OpenMP parallel region. The right hand side is called within
the loop BY ALL THREADS! The user is responsible for using OpenMP orphaned directives
inside the right hand side to implement the appropriate work sharing!

IMPORTANT ADVICE:

This version parallelizes the INNER LOOPS OVER THE INDIVIDUAL CHUNKS when calculating
the coefficients Ki. Use this version if your system has a small amount
of large chunks.
*/

#include "common.h"

/* the header file is shared with the MPI-only version (RK_MPI_SAsolver.c) */
#include "RK_MPI_SAsolver.h"

/* force inclusion of ISO C99 declarations and definitions */
#define _ISOC99_SOURCE

/*
Define this if your library is not C99 compliant (e.g. the isfinite() function is not defined).
If defined, the NAN handling is not compiled in at all. This means that you can use the functions
that control NAN handling, but you will never encounter that a NAN has occurred.
This flag can also be defined through a compiler option (see the settings.mk file).
*/
/* #define __DISABLE_NAN_HANDLING */


#include <stdlib.h>
#include "mathspec.h"

/*

 - All computations using Runge - Kutta solver are performed in FLOAT precision
   (see common.h)

 - The equation system is defined on arrays
   Dx=f(t,x) where x and f are arrays with n elements

*/

static FLOAT * K1=NULL, *K3, *K4, *K5;		/* for K1,(K2,)K3,K4,K5 auxiliary arrays */
static FLOAT * aux;				/* auxiliary array for f arguments */
static int max_n=0;				/* maximum dimension of the equation system */

static int handle_NAN=0;			/* nonzero if NAN and +-INF handling is enabled */
static int last_NAN=0;				/* nonzero if NAN or +-INF occurred during last calculation */

static MPI_Comm RKcomm;				/* the communicator incorporating all processes in the calculation */
static int MPIrank;				/* the rank of the current process */
static int MPIprocs;				/* total number of processes in the MPI universe */
static int MPImaster;				/* the rank of the master process ( set by RK_MPI_SA_init() )*/


int RK_MPI_SA_init(int max_block_size, MPI_Comm comm, int master_rank)
/*
allocates memory for auxiliary arrays. All chunks solved within the current node must fit into the
continuous block of memory of 'max_block_size'. Initializes the RK solver so that it can perform a parallel
calculation in the group of processes specified by the communicator 'comm'.
Typically, MPI_COMM_WORLD will be used.

'master_rank' determines the rank that will be responsible for the calculation progress on all
processes. The master rank has a privileged position among the ranks, e.g. some parameters of
the calculation are ignored in all other ranks. More information can be found in the comments
throughout the files RK_MPI_SAsolver.h and RK_MPI_SAsolver.c.

NOTE: If you want to start another computation with a greater block size, you must call
RK_MPI_SA_cleanup() before you can pass a greater value to RK_MPI_SA_init().

return codes:
0	success
-1	not enough memory
-2	invalid system dimension
-3	already initialized
-4	MPI not initialized
*/
{
	if(MPI_Comm_rank(comm,&MPIrank)!=MPI_SUCCESS || MPI_Comm_size(comm,&MPIprocs)!=MPI_SUCCESS) return(-4);

	size_t size=max_block_size*sizeof(FLOAT);
	if(max_n) return(-3);
	if(max_block_size<=0) return(-2);

	K1=(FLOAT *)malloc(size);
	K3=(FLOAT *)malloc(size);
	K4=(FLOAT *)malloc(size);
	K5=(FLOAT *)malloc(size);
	/*
	all blocks have the same size. If the allocation of any of the blocks fails, we suppose that
	then also the allocation of the last block must fail, so we can test the last one only
	*/
	if((aux=(FLOAT *)malloc(size)) == NULL ) {
		free(K1); free(K3); free(K4); free(K5);
		return(-1);
	}

	max_n=max_block_size;
	last_NAN=0;

	RKcomm=comm;
	MPImaster=master_rank;
	return(0);
}

int RK_MPI_SA_cleanup(void)
/* Frees memory allocated by RK_MPI_SA_init()

return codes:
0	success
-3	not initialized yet
*/
{
	if(max_n==0 || K1==NULL) return(-3);

	free(K1); free(K3); free(K4); free(K5); free(aux);

	max_n=0;
	return(0);
}

void RK_MPI_SA_handle_NAN(int hN)
/*
Turns the NAN and +-INF handling ON/OFF.
Pass a nonzero value to RK_MPI_SA_handle_NAN() to turn it ON, pass 0 to turn it OFF.

By default, the NAN handling is disabled: Under special circumstances
(very singular right side and extremely long initial time step) NANs or +-INF may
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
{
 handle_NAN=(hN==0)?0:1;
}

int RK_MPI_SA_check_NAN()
/*
returns nonzero if there a NAN or +-INF occurred in the last calculation (upon the last call to
RK_MPI_SA_Solve(). This may imply some problem with your equation or too loose setting of delta.

NOTE: This function works on all processes involved in the calculation
*/
{
 return(last_NAN);
}

int RK_MPI_SA_check_mem(RK_MEM_DIST * n)
/*
Checks whether the system memory distribution is defined correctly. For the chunk placement rules,
see the definition of RK_MEM_DIST.

return codes:
0	success
-3	not initialized yet
-5	last chunk exceeds the maximum allowed space (specified by the call to RK_MPI_SA_init()
-6	invalid chunk specification (bad chunk order, overlapping chunks or nonpositive chunk length)
-7	number of chunks is negative or zero
*/
{
	int n_chunks=n->n_chunks;

	int offset=0, prev_offset, i;

	if(max_n==0 || K1==NULL) return(-3);

	if(n_chunks<=0) return(-7);

	/* at the beginning of each iteration, offset points to the first available offset where the chunk may start */
	for(i=0;i<n_chunks;i++) {
		prev_offset=offset;
		offset = n->chunk_start[i];
		if(offset<prev_offset) return(-6);
		prev_offset=offset;
		offset += n->chunk_size[i];
		if(offset<=prev_offset) return(-6);
	}
	if(offset>max_n) return(-5);

	return(0);
}


int RK_MPI_SA_solve(FLOAT final_time, RK_MPI_S_SOLUTION * system)
/*
Performs the ODE system integration up to the time level 'final_time', using the
Merson's modification of the fourth order Runge-Kutta scheme with adaptive
time stepping. The number of iterations necessary to reach the 'final_time'
depends on the behavior of the equation system itself.

The system actually solved is defined in the 'system' structure.
The block of the evolving solution is always stored in the array pointed to by 'system->x'.
The current time level is stored in 'system->t' and it is updated at the end of the calculation,
as well as the current time step 'system->h'.

NOTE: The 'final_time' parameter is ignored on all other ranks than the master rank.

return codes:
0	success
-2	invalid 'system' specification
-3	not initialized yet
-4	break: cannot adjust time step - permanent occurrence of NANs. (only if NAN handling is ON)
-5	last chunk exceeds available memory (see also RK_MPI_SA_check_mem() )
-6	error in one of the other processes
*/
{
	int i,k;
	int n_chunks, chunk_start, chunk_size;
	FLOAT chunk_eps_mult;

	int command=0;

	int error_code=0;		/* for error code negotiation between the master ranks and the rest*/
	int errcode_from_others;
	int return_value=0;		/* for return value specification before breaking from the OpenMP parallel region */

	/* this simplifies the notation in the formulas and also saves some time */
	if(system==NULL) return(-2);
	RK_MEM_DIST * n = system->n;

	/* this initialization is relevant in the master rank only */
	FLOAT t = system->t;
	FLOAT h = system->h;
	FLOAT new_h;
	FLOAT h_min = system->h_min;
	FLOAT delta = system->delta;
	int delta_mode = (int)system->delta_mode;

	FLOAT *x = system->x;

	/*
	obtain the right hand side - here we have different order of commands than in the serial version,
	since we can't return immediately in the case of error. The error codes are collected to the master
	rank several lines below.
	*/
	RK_RightHandSide f;
	if(system->meta_f != NULL) f=system->meta_f();

	/*
	the error checking would normally go in reverse order (see RK_Asolver.c).
	Instead of "error_code=x;", the "return(x);" statement would be normally used.
	This would result in the same "priority" of error codes. In fact, the order of
	the tests can be arbitrary, none of the tests requires success of the previous one.
	*/
	if(n==NULL) error_code=(-5);
	else {
		n_chunks=n->n_chunks;
		if((n->chunk_start[n_chunks-1]+n->chunk_size[n_chunks-1]) > max_n) error_code=(-5);
	}

	if(max_n==0 || K1==NULL) error_code=(-3);

	if(x==NULL || system->meta_f==NULL) error_code=(-2);

	if(MPIrank==MPImaster && delta<=0) error_code=(-2);

	/*
	NOTE:	MPI_Allreduce is used here instead of the following two step interaction:
		1) MPI_Reduce to the master rank
		2) MPI_Bcast from the master that would specify a command to the other ranks
	*/

	MPI_Allreduce(&error_code,&errcode_from_others,1,MPI_INT,MPI_MIN,RKcomm);
	if(error_code) return(error_code);	/* return the error of the current rank */

	/* errcode_from_others is <0 if any of the processes encountered an error */
	/* and since we have passed here, we know that the error didn't occur in this process */
	if(errcode_from_others<0) return(-6);

	FLOAT * K2=K3;		/* we don't have to remember K2 */

	/* auxiliary pointers used to compute array expressions and for x update*/
	FLOAT *q, *s, *u, *v, *w;

	/* auxiliary time step variables to hold the expression (h/2), (h/3), (h/6), (h/8) */
	FLOAT h2,h3,h6,h8;

	/* the error */
	FLOAT e,eps,max_eps;

	/* NAN handling communication variables */
	int NAN_occurred_local;
	int NAN_occurred_global;

	last_NAN=0;

	/* automatically reverse and also perform initial adjustment */
	if(MPIrank==MPImaster) {
		if((final_time>t && h<0) || (final_time<t && h>0)) h*=-1;
		if(h==0 || fabsF(final_time-t)<=fabsF(h)) {
			h=final_time-t;
			command |= RKA_CMD_FINISHED;
		}
	}

	/* set NAN handling to the same state on all ranks */
	MPI_Bcast(&handle_NAN,1,MPI_INT,MPImaster,RKcomm);

	/* broadcast these values from the master rank to all other ranks */
	/* (Multiple calls are inefficient. However, this all occurs only once, so we can afford that) */
	MPI_Bcast(&final_time,1,MPI__FLOAT,MPImaster,RKcomm);
	MPI_Bcast(&t,1,MPI__FLOAT,MPImaster,RKcomm);
	MPI_Bcast(&h,1,MPI__FLOAT,MPImaster,RKcomm);
	MPI_Bcast(&delta,1,MPI__FLOAT,MPImaster,RKcomm);
	MPI_Bcast(&delta_mode,1,MPI_INT,MPImaster,RKcomm);

	/*
	I M P O R T A N T
	-----------------
	The calculation of the new time step as well as other simple calculations are
	performed indvidually by each process in order to minimize interactions. However, this
	may cause slight differences in results caused by different platforms, different
	optimizations etc.
	In order to maintain program flow consistency, all PROGRAM FLOW CONTROLLING CONDITIONS
	based on a comparison of floating point values are performed by the master rank only.
	The master rank then sends commands to other ranks. This is the safest way to control
	the whole cluster.
	*/

	#pragma omp parallel default(shared) private(i, k, e, chunk_start, chunk_size, q, u, v, w, s)
	while(1) {
		#pragma omp single
		{
			h2=h/2.0; h3=h/3.0; h6=h/6.0; h8=h/8.0;

#ifndef __DISABLE_NAN_HANDLING
			NAN_occurred_local=0;
#endif
		}	/* OMP single */

		/*
		NOTE:
		The construction of the Ki coefficients will be direct from the right hand side f.
		(f will store its results directly to the Ki arrays). Multiplication by the time
		step h will be performed as soon as it is necessary (where the coefficients are
		further used).
		*/

	/* K1 --------------------------------------- */

		/* calculate K1 */
		f(t,x,K1);

	/* K2 --------------------------------------- */

		/* calculate x+K1*h/3 needed as parameter to f when calculating K2 */
		for(k=0;k<n_chunks;k++) {
			chunk_start=n->chunk_start[k];
			chunk_size=n->chunk_size[k];

			q=aux+chunk_start;
			v=K1+chunk_start;
			w=x+chunk_start;

			#pragma omp for
			for(i=0;i<chunk_size;i++)
				q[i] = v[i]*h3 + w[i];
		}

		/* calculate K2 */
		f(t+h3,aux,K2);

	/* K3 --------------------------------------- */

		/* calculate x+(K1+K2)*h/6 needed as parameter to f when calculating K3 */
		for(k=0;k<n_chunks;k++) {
			chunk_start=n->chunk_start[k];
			chunk_size=n->chunk_size[k];

			q=aux+chunk_start;
			u=K1+chunk_start;
			v=K2+chunk_start;
			w=x+chunk_start;

			#pragma omp for
			for(i=0;i<chunk_size;i++)
				q[i] = ( u[i] + v[i] )*h6 + w[i];
		}

		/* calculate K3 */
		f(t+h3,aux,K3);

	/* K4 --------------------------------------- */

		/* calculate x+(K1+3*K3)*h/8 needed as parameter to f when calculating K4 */
		for(k=0;k<n_chunks;k++) {
			chunk_start=n->chunk_start[k];
			chunk_size=n->chunk_size[k];

			q=aux+chunk_start;
			u=K1+chunk_start;
			v=K3+chunk_start;
			w=x+chunk_start;

			#pragma omp for
			for(i=0;i<chunk_size;i++)
				q[i] = ( u[i] + 3.0 * v[i] )*h8 + w[i];
		}

		/* calculate K4 */
		f(t+h2,aux,K4);

	/* K5 --------------------------------------- */

		/* calculate x+(0.5*K1-1.5*K3+2*K4)*h needed as parameter to f when calculating K5 */
		for(k=0;k<n_chunks;k++) {
			chunk_start=n->chunk_start[k];
			chunk_size=n->chunk_size[k];

			q=aux+chunk_start;
			s=K1+chunk_start;
			u=K3+chunk_start;
			v=K4+chunk_start;
			w=x+chunk_start;

			#pragma omp for
			for(i=0;i<chunk_size;i++)
				q[i] = ( 0.5 * s[i] - 1.5 * u[i] + 2.0 * v[i] )*h + w[i];
		}

		/* calculate K5 */
		f(t+h,aux,K5);

	/* ========================================== */

		/* calculate the error estimate */
		#pragma omp single
		{
			system->steps_total++;
			eps=0.0;
		}

#ifndef __DISABLE_NAN_HANDLING
		if(handle_NAN)

		/*
		NOTE: this part cannot be OpenMP-parallelized properly
		as there is a conditional break from one of the for loops
		*/
		#pragma omp single
		{
			for(k=0;k<n_chunks;k++) {
				chunk_start=n->chunk_start[k];
				chunk_size=n->chunk_size[k];
				chunk_eps_mult=n->chunk_eps_mult[k];

				q=aux+chunk_start;
				s=K1+chunk_start;
				u=K3+chunk_start;
				v=K4+chunk_start;
				w=K5+chunk_start;

				for(i=0;i<chunk_size;i++) {
					e = chunk_eps_mult * fabsF( 0.2 * (*(s++)) - 0.9 * (*(u++)) + 0.8 * (*(v++)) - 0.1 * (*(w++)) );
					/* NAN and +-INF handling */
					if(! isfinite(e)) { NAN_occurred_local=1; goto nan_occurred; }
					if(e>eps) eps=e;
				}
			}
			nan_occurred:
			/* report the NAN occurrence to the master */
			MPI_Reduce(&NAN_occurred_local,&NAN_occurred_global,1,MPI_INT,MPI_BOR,MPImaster,RKcomm);

			/* now NAN_occurred_global is 1 if a NAN has occurred in any of the ranks, and 0 otherwise */
			if(MPIrank==MPImaster)
				if(NAN_occurred_global) {
					command |= RKA_CMD_NAN;
					/* only the master decides whether the time step is already too small for
					   further division */
					if(h/(final_time-t)<1e-11)
						command |= RKA_CMD_h_TOO_SMALL;
				}
		} else		/* also ends the OMP single block */
#endif
			for(k=0;k<n_chunks;k++) {
				chunk_start=n->chunk_start[k];
				chunk_size=n->chunk_size[k];
				chunk_eps_mult=n->chunk_eps_mult[k];

				q=aux+chunk_start;
				s=K1+chunk_start;
				u=K3+chunk_start;
				v=K4+chunk_start;
				w=K5+chunk_start;

/* this macro must be defined manually when the compiler is OpenMP 3.1 compliant */
#ifdef __OPENMP31
				#pragma omp for reduction(max:eps)
				for(i=0;i<chunk_size;i++) {
					e = chunk_eps_mult * fabsF( 0.2 * s[i] - 0.9 * u[i] + 0.8 * v[i] - 0.1 * w[i] );
					if(e>eps) eps=e;
				}
#else
				{
					FLOAT eps_thread_max=0.0;	/* automatically thread-private as it is declared inside a parallel region */
					#pragma omp for
					for(i=0;i<chunk_size;i++) {
						e = chunk_eps_mult * fabsF( 0.2 * s[i] - 0.9 * u[i] + 0.8 * v[i] - 0.1 * w[i] );
						if(e>eps_thread_max) eps_thread_max=e;
					}
					#pragma omp critical		/* perform the reduction operation manually */
					{ if(eps_thread_max>eps) eps=eps_thread_max; }
				}
#endif
			}

#ifndef __OPENMP31
	/*
	This only applies for the code version that implements the "max" reduction manually: (OpenMP 3.0 and below):

	There is no barrier for the threads that pass through a critical section! We have to put an explicit barrier here
	to ensure that eps contains the complete maximum before one of the threads uses it in the following OMP SINGLE block.
	Without a barrier, the remaining threads may not have finished the work yet and the maximum in eps is wrong here.
	*/
	#pragma omp barrier
#endif


	/* now as we have the new epsilon, the decision making and MPI communication is only done by one thread */
		#pragma omp single
		{

		/* ========================================== */
			/*
			transfer the maximum to all ranks, since they need it to calculate new h
			(even though only the master decides what to do)
			*/
			MPI_Allreduce(&eps,&max_eps,1,MPI__FLOAT,MPI_MAX,RKcomm);

		/* ========================================== */
			/* compare the error with delta (the error desired) and prepare a new time step */
			/* (this happens even though there was a NAN - in that case it has no sense) */

			if(delta_mode == DELTA_LOCAL) max_eps *= fabsF(h3);

			new_h = ((max_eps>0.0) ? powF((delta/max_eps),0.2)*0.8 : 2.0) * h;	/* double the time step if max_eps==0 */

			/*
			ONLY the master rank must decide whether the next step will be (a candidate for ) the last one
			(that is, whether the new h is greater than the distance from the final time) before the
			command is broadcast to the cluster. That's why the new time step has been calculated above.

			Note that other ranks don't perform the following comparison! They look for the
			RKA_CMD_NEXTFINISH command instead!

			However, this issue must be taken into account only if the current time step will
			be successful (the relative error is acceptable and no NANs occured). Otherwise, the next
			time step will be a retry and it will surely be smaller than the current one. It will
			therefore not pass after final_time PROVIDED THAT the current time step does not pass
			after final_time. (This is an inductive condition. The first step of the induction
			is performed before the loop begins, where the time step is truncated if necessary.)
			*/

			if(MPIrank==MPImaster)
				if(max_eps<delta || fabsF(h)<h_min) {
					/*
					this means the error is acceptable (either eps is in tolerance or the time step is
					too short - see RK_MPI_SAsolver.h) => the step should be successful (This says nothing
					about NANs - however, NANs are treated completely separately in the command
					interpretation section below, so we don't have to care about them here)
					*/
					command |= RKA_CMD_UPDATE;

					/* notice the addition of the current h here: we're talking about the NEXT step */
					if( fabsF(final_time-(t+h)) <= fabsF(new_h) )
						command |= RKA_CMD_NEXTFINISH;
				}

			/* ========================================== */

			/* broadcast the command to all ranks */
			MPI_Bcast(&command,1,MPI_INT,MPImaster,RKcomm);

			/* ========================================== */

		}	/* OMP single */

		/* handle commands (performed by all ranks and all threads) */

#ifndef __DISABLE_NAN_HANDLING
		/* testing handle_NAN here would be useless - the following will never be true with handle_NAN==0 */
		if(command & RKA_CMD_NAN) {
			#pragma omp single
			{ last_NAN=1; }

			/* h is very small - perhaps we can't reach a suitable time step at all => stop */
			if(command & RKA_CMD_h_TOO_SMALL) {
				#pragma omp single
				{
					system->t=t;
					return_value = -4;
				}
				break;		/* leaving a parallel region by "return" is not allowed. But all threads can break from the while loop... */
			}

			#pragma omp single
			{
				/* try again with a smaller time step */
				h/=10;
				command=0;	/* of course, this is useful for the master rank only */
			}	/* OMP single */
		} else
#endif
		{

		/* no NANs */
			if(command & RKA_CMD_UPDATE) {
				/* okay - the error is acceptable */
				/* update the solution x:=x+h/3*( (K1+K5)/2 + 2*K4 ) */
				#pragma omp single
				{ t+=h; }
				for(k=0;k<n_chunks;k++) {
					chunk_start=n->chunk_start[k];
					chunk_size=n->chunk_size[k];

					q=x+chunk_start;
					u=K1+chunk_start;
					v=K4+chunk_start;
					w=K5+chunk_start;

					#pragma omp for
					for(i=0;i<chunk_size;i++)
						q[i] += h3*( 0.5 * ( u[i] + w[i] ) + 2.0 * v[i] );
				}

				#pragma omp single
				{
					system->steps++;

					/* Invoke the service callback if one is defined. */

					if(system->Service_Callback != NULL) {
						/* make the current values of t and h available to the service callback */
						system->t = t;
						system->h = h;
						/*
						the following test is done in all ranks. However, the result in the master rank
						is the one that matters as consequently the command broadcast is performed again.
						*/
						if(system->Service_Callback(final_time, system)) command |= RKA_CMD_BREAK;
					}

					/* ========================================== */

					/* broadcast the command to all ranks again in case it has been changed by RK_Service_Callback() */
					MPI_Bcast(&command,1,MPI_INT,MPImaster,RKcomm);

					/* ========================================== */
				}	/* OMP single */

				if(command & RKA_CMD_FINISHED) break;	/* already at the end of the interval => end */

				if(command & RKA_CMD_BREAK) {	/* interrupted by the service callback (in rank 0) */
					#pragma omp single
					{
						system->t=t;		/* update time level variable accessible to the caller */
						system->h=new_h;	/* begin with the updated h upon next call */
						return_value = 1;
					}
					break;
				}

				/*
				Now the new time level has been successfully calculated, so we may rearrange the blocks...
				NOTE:	It is clear that all ranks must cooperate on the data rearrangement. This means,
					no data is exchanged between ranks i and j before they have finished writing
					down the new time level and called DDLBF_Rearrange(). No call to MPI_Barrier()
					is therefore necessary in your callback function in order to ensure that the
					correct data is already available. It may however be necessary to make a barrier
					in DDLBF_Rearrange() because of internal logic of data rearrangement.

				NOTE:	In fact, there are no fundamental problems that would prevent us from calling
					the callback at the beginning of each time level caculation instead of here.
					In that case, we could call it even if the previous time step was unsuccessful.
					However, this would invoke DDLBF_Rearrange() also at the very beginning
					of the calculation, which we usually don't want.

				NOTE:	The call to DDLBF_Rearrange() is always single-threaded.
				*/
				#pragma omp single
				{
					if(system->DDLBF_Rearrange != NULL) {
						n = system->n = system->DDLBF_Rearrange(n);	/* return of NULL is not checked */
						n_chunks = n->n_chunks;
					}

					/* possibly alter the right hand side for the next time step */
					f=system->meta_f();
				}
			}

			/*
			if there is a RKA_CMD_NEXTFINISH command issued, the RKA_CMD_UPDATE has been issued too
			(see above). This lets us place the following condition outside the RKA_CMD_UPDATE
			condition above even though it logically belongs there. Here it is easier to set the command
			for the next iteration.
			*/

			#pragma omp single
			{
				if(command & RKA_CMD_NEXTFINISH) {
					/*
					this ensures that a reasonable time step will be stored to system->h to be
					prepared for a continued calculation. If we did this at return from the function,
					we would store the already truncated time step, which is undesirable.
					NOTE:	It does not matter whether we use h or new_h here, since they are
						probably very similar. new_h is used for compatibility with RK_Asolve(),
						in order to get exactly the same results
					*/
					system->h=new_h;
					h=final_time-t;
					command=RKA_CMD_FINISHED; /* next step will be the last (if it's successful) */
				} else {
					command=0;
					h=new_h;
				}
			}
		}

	/* ========================================== */

	}	/* while(1) & OMP parallel */

	if(!return_value)	system->t=t;		/* update time level variable accessible to the caller (this happens in all ranks) */
	return(return_value);
}
