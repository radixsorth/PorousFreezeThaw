/***********************************************\
* 4th order Runge - Kutta solver                *
* optimized for memory consumption              *
* (C) 2005-2006 Pavel Strachota                 *
* file: RK_solver.c                             *
\***********************************************/

#include "common.h"
#include "RK_solver.h"

#include <stdlib.h>
#include <string.h>

/*

 - All computations using Runge - Kutta solver are performed in FLOAT precision
   (see common.h)

 - The equation system is defined on arrays
   Dx=f(t,x) where x and f are arrays with n elements

*/

static FLOAT * K_a=NULL, * K_b=NULL;		/* for K1,K2,K3,K4 auxiliary arrays */
static FLOAT * x__=NULL;			/* x auxiliary array - the old solution at time t*/
static int max_n=0;				/* maximum dimension of the equation system */

int RK_init(int max_system_dimension)
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
{
	size_t size=max_system_dimension*sizeof(FLOAT);
	if(max_n) return(-3);
	if(max_system_dimension<=0) return(-2);

	K_a=(FLOAT *)malloc(size);
	K_b=(FLOAT *)malloc(size);
	/*
	all blocks have the same size. If the allocation of any of the blocks fails,
	then also the allocation of the last block must fail, so we can test the last one only
	*/
	if((x__=(FLOAT *)malloc(size)) == NULL) { free(K_a); free(K_b); return(-1); }

	max_n=max_system_dimension;
	return(0);
}

int RK_cleanup(void)
/* Frees memory allocated by RK_init()

return codes:
0	success
-3	not initialized yet
*/
{
	if(max_n==0 || K_a==NULL) return(-3);

	free(K_a); K_a=NULL;
	free(K_b); K_b=NULL;
	free(x__); x__=NULL;

	max_n=0;
	return(0);
}

int RK_solve(int steps, RK_SOLUTION * system)
/*
Performs 'steps' iterations using the fourth order "standard" Runge - Kutta method with
optimizations for memory consumption. However, this is a bit slower than the straightforward
implementation.

The system actually solved is defined in the 'system' structure.
The evolving solution is always stored in the array pointed to by 'system->x'. The current time level
is stored int 'system->t' and it is updated at the end of the calculation. The time step is defined
by 'system->h'.

return codes:
0	success
-2	invalid input data
-3	not initialized yet
-5	system dimension is greater than the maximum dimension passed to RK_A_init()
*/
{
	int i,k;

	/*
	The names Ki (i=1,2,3,4) have just informative purpose:
	The real memory location for them is either K_a or K_b.
	*/
	FLOAT * K1,* K2,* K3,* K4;

	/* this simplifies the notation in the formulas and also saves some time */
	if(system==NULL) return(-2);
	int n=system->n;
	FLOAT t=system->t;
	FLOAT h=system->h;
	FLOAT *x=system->x;
	if(max_n==0 || K_a==NULL) return(-3);
	if(n>max_n) return(-5);

	if(x==NULL || system->meta_f==NULL || h==0 || steps<=0) return(-2);
	RK_RightHandSide f=system->meta_f();

	/* auxiliary arrays used to compute array expressions (x+K1/2), (x+K2/2) and x+K3 and for x update*/
	FLOAT * q,* v,* w;

	/* auxiliary time variable to hold the expression (t+h/2) (not really necessary) */
	FLOAT th2=t+h/2;

	/* auxiliary time step variables to hold the expressions (h/2), (h/6) and (h/3) */
	FLOAT h2=h/2, h3=h/3, h6=h/6;

	size_t size=n*sizeof(FLOAT);

	for(k=0;k<steps;k++) {

	/*
	NOTE:
	The construction of the Ki coefficients will be direct from the right hand side f.
	(f will store its results directly to the Ki arrays). Multiplication by the time
	step h will be performed as soon as it is necessary (where the coefficients are
	further used
	*/

		/*
		The next iteration will be constructed gradually.
		(of course this is a bit slower than immediate final calculation,
		 but it takes less memory)
		*/

	/* ------------------------------------------ */

		/* save old solution */
		memcpy(x__,x,size);

	/* K1 --------------------------------------- */

		/* calculate K1 */
		K1=K_a;
		f(t,x__,K1);

		/* ---------------------------------- */

		/* update x:=x__+h/6*(K1) */
		q=x; w=K1;
		for(i=0;i<n;i++)
			(*(q++)) += h6 * (*(w++));

	/* K2 --------------------------------------- */

		/* calculate x__+K1*h/2 needed as parameter to f when calculating K2 */
		q=K_a;	w=x__;	/* also K1 had the value of K_a - restore pointer value as necessary */
		for(i=0;i<n;i++) {
			*q=(*q)*h2 + (*(w++));
			q++;
		}

		/* calculate K2 */
		K2=K_b; q=K_a;
		f(th2,q,K2);

		/* ---------------------------------- */

		/* update x:=x__+h/6*(K1+2*K2) */
		q=x; w=K2;
		for(i=0;i<n;i++)
			(*(q++)) += h3 * (*(w++));

	/* K3 --------------------------------------- */

		/* calculate x__+K2*h/2 needed as parameter to f when calculating K3 */
		q=K2;	w=x__;
		for(i=0;i<n;i++) {
			*q=(*q)*h2 + (*(w++));
			q++;
		}

		/* calculate K3 */
		K3=K_a; q=K_b;
		f(th2,q,K3);

		/* ---------------------------------- */

		/* the update by K3 will go together with K4 */

	/* K4 --------------------------------------- */

		/* calculate x+K3*h needed as parameter to f when calculating K4 */

		/* now we don't need the original values of the array x__ anymore */
		q=x__;	w=K3;
		for(i=0;i<n;i++)
			(*(q++)) += (*(w++)) * h;

		/* now we don't need the original value of t any more */
		t+=h;

		/* calculate K4 */
		K4=K_b; q=x__;
		f(t,q,K4);	/* here t is already (original) t+h */

		/* ---------------------------------- */

		/* finally update x:=x__+h/6*(K1+2*K2+2*K3+K4) */
		q=x; v=K3; w=K4;
		for(i=0;i<n;i++)
			(*(q++)) += h6*(2 * (*(v++)) + (*(w++)) );

	/* ------------------------------------------ */
		/* possibly alter the right hand side for the next time step */
		f=system->meta_f();
	}

	system->t=t;		/* update time level variable accessible to the caller */
	return(0);
}
