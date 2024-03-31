/***********************************************\
* 4th order Runge - Kutta solver                *
* classic straightforward implementation        *
* (C) 2005-2006 Pavel Strachota                 *
* file: RK_csolver.c                            *
\***********************************************/

#include "common.h"
#include "RK_csolver.h"

#include <stdlib.h>

/*

 - All computations using Runge - Kutta solver are performed in FLOAT precision
   (see common.h)

 - The equation system is defined on arrays
   Dx=f(t,x) where x and f are arrays with n elements

*/

static FLOAT * K1=NULL, *K2, *K3, *K4;		/* for K1,K2,K3,K4 auxiliary arrays */
static FLOAT * aux;				/* auxiliary array for f arguments */
static int max_n=0;				/* maximum dimension of the equation system */

int RK_c_init(int max_system_dimension)
/*
allocates memory for auxiliary arrays that allow solution of equation systems with dimension smaller than
or equal to 'max_system_dimension'.

NOTE: If you want to start another computation with greater system dimension, you must call RK_c_cleanup()
before you can pass a greater value to RK_c_init().

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
	K2=(FLOAT *)malloc(size);
	K3=(FLOAT *)malloc(size);
	K4=(FLOAT *)malloc(size);
	/*
	all blocks have the same size. If the allocation of any of the blocks fails,
	then also the allocation of the last block must fail, so we can test the last one only
	*/
	if((aux=(FLOAT *)malloc(size)) == NULL ) {
		free(K1); free(K2); free(K3); free(K4);
		return(-1);
	}

	max_n=max_system_dimension;
	return(0);
}

int RK_c_cleanup(void)
/* Frees memory allocated by RK_c_init()

return codes:
0	success
-3	not initialized yet
*/
{
	if(max_n==0 || K1==NULL) return(-3);

	free(K1); free(K2); free(K3); free(K4); free(aux);

	max_n=0;
	return(0);
}

int RK_c_solve(int steps, RK_SOLUTION * system)
/*
Performs 'steps' iterations using the fourth order "standard" Runge - Kutta method

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

	/* this simplifies the notation in the formulas and also saves some time */
	if(system==NULL) return(-2);
	int n=system->n;
	FLOAT t=system->t;
	FLOAT h=system->h;
	FLOAT *x=system->x;
	if(max_n==0 || K1==NULL) return(-3);
	if(n>max_n) return(-5);

	if(x==NULL || system->meta_f==NULL || h==0 || steps<=0) return(-2);
	RK_RightHandSide f=system->meta_f();

	/* auxiliary pointers used to compute array expressions (x+K1/2), (x+K2/2), (x+K3) and for x update*/
	FLOAT *q, *s, *u, *v, *w;

	/* auxiliary time variable to hold the expression (t+h/2) (not really necessary) */
	FLOAT th2=t+h/2;

	/* auxiliary time step variables to hold the expression (h/2), (h/6) */
	FLOAT h2=h/2, h6=h/6;

	for(k=0;k<steps;k++) {

	/*
	NOTE:
	The construction of the Ki coefficients will be direct from the right hand side f.
	(f will store its results directly to the arrays Ki). Multiplication by the time
	step h will be performed as soon as it is necessary (where the coefficients are
	further used).
	*/

	/* K1 --------------------------------------- */

		/* calculate K1 */
		f(t,x,K1);

	/* K2 --------------------------------------- */

		/* calculate x+K1*h/2 needed as parameter to f when calculating K2 */
		q=aux;	v=K1; w=x;
		for(i=0;i<n;i++)
			(*(q++)) = (*(v++))*h2 + (*(w++));

		/* calculate K2 */
		f(th2,aux,K2);


	/* K3 --------------------------------------- */

		/* calculate x+K2*h/2 needed as parameter to f when calculating K3 */
		q=aux;	v=K2; w=x;
		for(i=0;i<n;i++)
			(*(q++)) = (*(v++))*h2 + (*(w++));

		/* calculate K3 */
		f(th2,aux,K3);

	/* K4 --------------------------------------- */

		/* calculate x+K3*h needed as parameter to f when calculating K4 */
		q=aux;	v=K3; w=x;
		for(i=0;i<n;i++)
			(*(q++)) = (*(v++))*h + (*(w++));

		/* now we don't need the original value of t any more */
		t+=h;

		/* calculate K4 */
		f(t,aux,K4);

		/* ---------------------------------- */

		/* finally update x:=x+h/6*(K1+2*K2+2*K3+K4) */
		q=x; s=K1; u=K2; v=K3; w=K4;
		for(i=0;i<n;i++)
			(*(q++)) += h6*( (*(s++)) + 2 * (*(u++)) + 2 * (*(v++)) + (*(w++)) );

	/* ------------------------------------------ */
		/* possibly alter the right hand side for the next time step */
		f=system->meta_f();
	}

	system->t=t;		/* update time level variable accessible to the caller */
	return(0);
}
