/***************************************************\
* 4th order Runge - Kutta solver                    *
* Merson's modification with adaptive time stepping *
* (C) 2005-2013 Pavel Strachota                     *
* file: RK_Asolver.c                                *
\***************************************************/

#include "common.h"
#include "RK_Asolver.h"

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

int RK_A_init(int max_system_dimension)
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
{
	size_t size=max_system_dimension*sizeof(FLOAT);
	if(max_n) return(-3);
	if(max_system_dimension<=0) return(-2);

	K1=(FLOAT *)malloc(size);
	K3=(FLOAT *)malloc(size);
	K4=(FLOAT *)malloc(size);
	K5=(FLOAT *)malloc(size);
	/*
	all blocks have the same size. If the allocation of any of the blocks fails,
	then also the allocation of the last block must fail, so we can test the last one only
	*/
	if((aux=(FLOAT *)malloc(size)) == NULL ) {
		free(K1); free(K3); free(K4); free(K5);
		return(-1);
	}

	max_n=max_system_dimension;
	last_NAN=0;
	return(0);
}

int RK_A_cleanup(void)
/* Frees memory allocated by RK_A_init()

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

void RK_A_handle_NAN(int hN)
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
After the first iteration, h is already corrected and thus no problems with NANs
will occur in most cases. Since NAN handling takes time, it is recommended not to turn
it ON unless you really need it e.g. for experiments in the testing phase.

NOTE: You can only use NAN handling when the SIGFPE signal raising is disabled.
(and by default it is) See the C library documentation for details.

NOTE: NAN handling requires the C library to be ISO C99 compliant.
*/
{
 handle_NAN=(hN==0)?0:1;
}

int RK_A_check_NAN()
/*
returns nonzero if there a NAN or +-INF occured in the last calculation (upon the last call to
RK_A_Solve(). This may imply some problem with your equation or too loose setting of delta_.
*/
{
 return(last_NAN);
}

int RK_A_solve(FLOAT final_time, RK_SOLUTION * system)
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
{
	int i,k;
	char last=0;

	/* this simplifies the notation in the formulas and also saves some time */
	if(system==NULL) return(-2);
	int n = system->n;
	FLOAT t = system->t;
	FLOAT h = system->h;
	FLOAT h_min = system->h_min;
	FLOAT delta = system->delta;
	FLOAT *x = system->x;

	if(max_n==0 || K1==NULL) return(-3);
	if(n>max_n) return(-5);

	if(x==NULL || system->meta_f==NULL) return(-2);
	RK_RightHandSide f=system->meta_f();

	FLOAT * K2=K3;		/* we don't have to remember K2 */

	/* auxiliary pointers used to compute array expressions and for x update*/
	FLOAT *q, *s, *u, *v, *w;

	/* auxiliary time step variables to hold the expression (h/2), (h/3), (h/6), (h/8) */
	FLOAT h2,h3,h6,h8;

	/* the error */
	FLOAT e,eps;

	last_NAN=0;

	/* automatically reverse */
	if((final_time>t && h<0) || (final_time<t && h>0)) h*=-1;

	if(h==0) h=final_time-t;
	while(1) {
	/*
	NOTE:
	The construction of the Ki coefficients will be direct from the right hand side f.
	(f will store its results directly to the Ki arrays). Multiplication by the time
	step h will be performed as soon as it is necessary (where the coefficients are
	further used).
	*/
		if(fabsF(final_time-t)<=fabsF(h)) { system->h=h; h=final_time-t; last=1; }
		else last=0;

		h2=h/2; h3=h/3; h6=h/6; h8=h/8;


	/* K1 --------------------------------------- */

		/* calculate K1 */
		f(t,x,K1);

	/* K2 --------------------------------------- */

		/* calculate x+K1*h/3 needed as parameter to f when calculating K2 */
		q=aux;	v=K1; w=x;
		for(i=0;i<n;i++)
			(*(q++)) = (*(v++))*h3 + (*(w++));

		/* calculate K2 */
		f(t+h3,aux,K2);

	/* K3 --------------------------------------- */

		/* calculate x+(K1+K2)*h/6 needed as parameter to f when calculating K3 */
		q=aux;	u=K1; v=K2; w=x;
		for(i=0;i<n;i++)
			(*(q++)) = ( (*(u++)) + (*(v++)) )*h6 + (*(w++));

		/* calculate K3 */
		f(t+h3,aux,K3);

	/* K4 --------------------------------------- */

		/* calculate x+(K1+3*K3)*h/8 needed as parameter to f when calculating K4 */
		q=aux;	u=K1; v=K3; w=x;
		for(i=0;i<n;i++)
			(*(q++)) = ( (*(u++)) + 3 * (*(v++)) )*h8 + (*(w++));

		/* calculate K4 */
		f(t+h2,aux,K4);

	/* K5 --------------------------------------- */

		/* calculate x+(0.5*K1-1.5*K3+2*K4)*h needed as parameter to f when calculating K5 */
		q=aux;	s=K1; u=K3; v=K4; w=x;
		for(i=0;i<n;i++)
			(*(q++)) = ( 0.5 * (*(s++)) - 1.5 * (*(u++)) + 2 * (*(v++)) )*h + (*(w++));

		/* calculate K5 */
		f(t+h,aux,K5);

	/* ========================================== */
		/* calculate the error */
		s=K1; u=K3; v=K4; w=K5;
		eps=0;

#ifndef __DISABLE_NAN_HANDLING
		if(handle_NAN) {
			for(i=0;i<n;i++) {
				e = fabsF( 0.2 * (*(s++)) - 0.9 * (*(u++)) + 0.8 * (*(v++)) - 0.1 * (*(w++)) );
				/* NAN and +-INF handling */
				if(! isfinite(e)) { eps=-1; break; }
				if(e>eps) eps=e;
			}
			if(eps<0) {		/* a NAN or +-INF occurred */
				last_NAN=1;
				h/=10;
				/* if h is very low - perhaps we can't reach a suitable time step at all => stop */
				if(h/(final_time-t)<1e-12) { system->t=t; return(-4); }
				/* try again with a smaller time step */
				continue;
			}
		} else
#endif
			for(i=0;i<n;i++) {
				e = fabsF( 0.2 * (*(s++)) - 0.9 * (*(u++)) + 0.8 * (*(v++)) - 0.1 * (*(w++)) );
				if(e>eps) eps=e;
			}

	/* ========================================== */
		/* relate the error with delta - the error desired */

		if(eps<delta || fabsF(h)<h_min) {
			/* okay - the error is acceptable (either eps is in tolerance or the time step is too short - see RK_Asolver.h) */
			/* update the solution x:=x+h/3*( (K1+K5)/2 + 2*K4 ) */
			t+=h;
			q=x; u=K1; v=K4; w=K5;
			for(i=0;i<n;i++)
				(*(q++)) += h3*( 0.5 * ( (*(u++)) + (*(w++)) ) + 2 * (*(v++)) );

			system->steps++;

			if(last) break;	/* already at the end of the interval => end */
			/* possibly alter the right hand side for the next time step */
			f=system->meta_f();
			/* this only prevents division by zero: continue with twice as large step */
			if(eps==0) { h*=2; continue; }
		}
		h=powF((delta/eps),0.2) * 0.8 * h;
	/* ------------------------------------------ */
	}

	system->t=t;		/* update time level variable accessible to the caller */
	return(0);
}
