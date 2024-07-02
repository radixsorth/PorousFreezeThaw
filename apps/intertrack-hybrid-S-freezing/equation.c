/**************************************************************\
*                                                              *
*                   I N T E R T R A C K - S                    *
*                                                              *
*      High Precision Phase Interface Evolution Simulator      *
*                      ("H i P P I E S")                       *
*                                                              *
* THE RIGHT HAND SIDE (generic thickness of the boundary)      *
*                                                              *
*  "SUPERMULTI" COMBINED VERSION:                              *
*  - MULTI SCHEME (2nd order / MPFA)                           *
*  - MULTI B.C. (HOMOGENEOUS NEUMANN / HOMOGENEOUS DIRICHLET)  *
*  - MULTI MODEL (4-FOLD / 6-FOLD / 8-FOLD ANISOTROPY)         *
*                                                              *
*  - hybrid OpenMP/MPI version                                 *
*    - OMP FOR parallelization of the 2nd level loop           *
*      when traversing the 3D mesh                             *
*    - OMP parallel boundary condition setup (at 2nd level)    *
*                                                              *
* -------------------------------------------------------------*
* (C) 2023 Pavel Strachota                                     *
* - Hybrid OpenMP/MPI parallelization (C) 2015 Pavel Strachota *
* - generic system of variables & parameters (C) 2021-22 P.S.  * 
* file: equation.c                                             *
\**************************************************************/

/*
THE EQUATION SYSTEM:

TODO

*/

#define MAX_BALLS_COUNT	1000
_string_ ball_positions_file = "data/spheres_positions.txt";

/* the thickness of the boundary condition layer expressed in grid nodes */
static int bcond_thickness = 2;

/* MPI nonblocking communication request objects */
static MPI_Request MPIreq_all[4*VAR_COUNT];		/* MPI request structures for all synchronization operations */
static MPI_Status MPIstat_all[4*VAR_COUNT];		/* array of MPI statuses returned by MPI_Waitall */


/*
This is the structure of precalculated data (such as vector norms in case of the vector field) . We have
an array of these structures with the same size as the vector field size in all ranks. All ranks
precalculate the elements in this structure by calling the PrecalculateData() function before the
calculation begins.

The AllocPrecalcData(), FreePrecalcData() and PrecalculateData() functions are defined in this file. The
responsibility of the first function is to allocate an array of PRECALC_DATA called 'precalc'. It must
return 0 on success and nonzero on error. The next function deallocates the array. The last function
should fill all its elements with the appropriate data. Both functions receive one integer argument - the
total size of the grid. Since the AllocPrecalcData() and PrecalculateData() functions are called
before the calculation, they may perform additional initializations, such as prepare some switches
for right hand side alternation.

If you don't need any precalculated data, you may assign a null pointer to 'precalc'. You should NOT use
an empty PRECALC_DATA structure, since it is not allowed on all compilers (gcc and Intel C compilers get
by with zero-sized structures, but other compilers (like c89 on HP-UX) don't. In C++, empty structures
are allowed by the language standard. They are replaced by structures containing one element of type
'char').

The appropriate elements of precalc array are passed to the right hand side macro as the PRE_... formal
parameters, so it can use the structure entries (directly - by the dot operator). All precalculated data
is packed in one structure in order to simplify argument passing to the macros, as well as to make the
code more flexible to modifications.
*/
typedef struct
{
	FLOAT u_noise;
} PRECALC_DATA;

PRECALC_DATA * precalc;

/* auxiliary global variables precalculated in PrecalculateData() */
static FLOAT xi_2_inv_a;		/* a/(xi^2) */
static FLOAT xi_inv_b_sqrt_a2;		/* b * sqrtF(0.5*a) / xi */

static FLOAT eps2_3;			/* 3/((p_eps1-p_eps0)^2) */
static FLOAT eps3_2;			/* 2/((p_eps1-p_eps0)^3) */

/* ==================================================================================================== */

/*
SUMMARY OF DIRECTIONS:

X: +- 1
Y: +- N1
Z: +- rowsize
*/

/* ------- The boundary conditions ------- */

FLOAT temperature_Dirichlet_B_C(FLOAT t, int i, int j, int k)
/*
returns the value of a hardcoded function at the grid node (i,j,k) and at time t.
Used for the Dirichlet boundary condition setup. The index triple is given
WITH RESPECT TO THE WHOLE GRID WITHOUT THE AUXILIARY NODES.
*/
{
	/* the geometrical coordinates of the node (i,j,k) */
	/*
	FLOAT x = L1 * (0.5+i) / n1;
	FLOAT y = L2 * (0.5+j) / n2;
	FLOAT z = L3 * (0.5+k) / total_n3;
	*/

	return(t<param[phase_switch_time] ? param[top_temp1] : param[top_temp2]);
}

void bcond_setup_Combined(FLOAT t, FLOAT * w, FLOAT (*Dirichlet_cond)(FLOAT,int,int,int))
/*
set up the combined Neumann / Dirichlet boundary condition for the solution w

At the top of the domain, the Dirichlet boundary condition is set, taking the actual thickness
of the boundary condition layer into account. The values of the B.C. are given by the
Dirichlet_cond() function, which must return the correct result based on time t and the
node index triple. The node indices  passed to Dirichlet_cond() are given with respect
to the WHOLE GRID WITHOUT THE AUXILIARY NODES. Negative node indices therefore occur.

Elsewhere, the Neumann boundary condition is set.
*/
{
	int i,j,k;

	FLOAT * w_ptr, * w_ptr1, * w_ptr2, * w_ptr1m, * w_ptr2m;	/* "m" stands for "mirror" */

	/*
	Neumann boundary condition setup (in the X,Y-plane, except the front and the rear of the block,
	which will be either added later or received from the neighbor rank).

	Begin at the first row after the boundary condition (in the Z direction).
	*/
	#pragma omp for schedule(runtime)
	for(k=0;k<n3;k++) {

		/* w_ptr indicates the beginning of the processed X-Y plane */
		w_ptr = w + (bcond_thickness+k)*rowsize;

		/* first we set up the side edges along the X,Y plane */
		for(j=0;j<n2;j++) {
			w_ptr1m = w_ptr1 = w_ptr + (bcond_thickness+j)*N1 + bcond_thickness;
			w_ptr2m = w_ptr2 = w_ptr1 + n1 - 1;
			for(i=0;i<bcond_thickness;i++) {
				*(--w_ptr1m) = *(w_ptr1++);	/* left edge */
				*(++w_ptr2m) = *(w_ptr2--);	/* right edge */
			}
		}

		/* the top and the bottom boundary layer of the X,Y plane */
		for(j=0;j<bcond_thickness;j++) {
			w_ptr1 = w_ptr + (bcond_thickness+j)*N1;	w_ptr1m = w_ptr + (bcond_thickness-j-1)*N1;
			w_ptr2 = w_ptr + (bcond_thickness+n2-j-1)*N1;	w_ptr2m = w_ptr + (bcond_thickness+n2+j)*N1;
			for(i=0;i<N1;i++) {
				*(w_ptr1m++) = *(w_ptr1++);	/* bottom layer */
				*(w_ptr2m++) = *(w_ptr2++);	/* top layer */
			}
		}
	}

	/* Finally, set up the top and bottom layers in the Z direction (this part depends on the current rank) */
	if(MPIrank==0) {
		/* the first rank has to set up the Neumann boundary condition at the front (the beginning of the array in the sense of the Z axis) */
		for(k=0;k<bcond_thickness;k++)
			#pragma omp for schedule(runtime)
			for(j=0;j<N2;j++) {
				w_ptr1m	=  w_ptr1 = w + bcond_size + j*N1;
				w_ptr1m	-= (k+1)*rowsize;
				w_ptr1	+=  k*rowsize;
				for(i=0;i<N1;i++) *(w_ptr1m++)=*(w_ptr1++);
			}
	}
	if(MPIrank==MPIprocs-1) {
		/* the last rank has to set up the Dirichlet boundary condition at the rear (the end of the array in the sense of the Z axis) */
		for(k=0;k<bcond_thickness;k++)
			#pragma omp for schedule(runtime)
			for(j=0;j<N2;j++) {
				w_ptr = w + subgridSize + bcond_size + k*rowsize + j*N1;
				for(i=0;i<N1;i++) *(w_ptr++) = Dirichlet_cond(t, i-bcond_thickness, j-bcond_thickness, k+total_n3);
			}
	}
	#pragma omp barrier
}

void bcond_setup_Neumann(FLOAT * w)
/*f
set up the boundary conditions on the solution w

This version of bcond_setup() sets the zero Neumann boundary condition by mirroring
the node values with respect to the boundary. This effectively approximates the b.c.
with a central difference of even order.

For the FVM method, the boundary lies between the nodes and thence the first mirrored
phantom node has the same value as its immediate neighbor

The actual thickness of the phantom node layer layer is taken into account and the b.c.
is approximated with the order 2*bcond_thickness.
*/
{
	int i,j,k;

	FLOAT * w_ptr, * w_ptr1, * w_ptr2, * w_ptr1m, * w_ptr2m;	/* "m" stands for "mirror" */

	/*
	Neumann boundary condition setup (in the X,Y-plane, except the front and the rear of the block,
	which will be either added later or received from the neighbor rank).

	Begin at the first row after the boundary condition (in the Z direction).
	*/
	#pragma omp for schedule(runtime)
	for(k=0;k<n3;k++) {

		/* w_ptr indicates the beginning of the processed X-Y plane */
		w_ptr = w + (bcond_thickness+k)*rowsize;

		/* first we set up the side edges along the X,Y plane */
		for(j=0;j<n2;j++) {
			w_ptr1m = w_ptr1 = w_ptr + (bcond_thickness+j)*N1 + bcond_thickness;
			w_ptr2m = w_ptr2 = w_ptr1 + n1 - 1;
			for(i=0;i<bcond_thickness;i++) {
				*(--w_ptr1m) = *(w_ptr1++);	/* left edge */
				*(++w_ptr2m) = *(w_ptr2--);	/* right edge */
			}
		}

		/* the top and the bottom boundary layer of the X,Y plane */
		for(j=0;j<bcond_thickness;j++) {
			w_ptr1 = w_ptr + (bcond_thickness+j)*N1;	w_ptr1m = w_ptr + (bcond_thickness-j-1)*N1;
			w_ptr2 = w_ptr + (bcond_thickness+n2-j-1)*N1;	w_ptr2m = w_ptr + (bcond_thickness+n2+j)*N1;
			for(i=0;i<N1;i++) {
				*(w_ptr1m++) = *(w_ptr1++);	/* bottom layer */
				*(w_ptr2m++) = *(w_ptr2++);	/* top layer */
			}
		}
	}

	/* Finally, set up the top and bottom layers in the Z direction (this part depends on the current rank) */
	if(MPIrank==0) {
		/* the first rank has to set up the Neumann boundary condition at the front (the beginning of the array in the sense of the Z axis) */
		for(k=0;k<bcond_thickness;k++)
			#pragma omp for schedule(runtime)
			for(j=0;j<N2;j++) {
				w_ptr1m	=  w_ptr1 = w + bcond_size + j*N1;
				w_ptr1m	-= (k+1)*rowsize;
				w_ptr1	+=  k*rowsize;
				for(i=0;i<N1;i++) *(w_ptr1m++)=*(w_ptr1++);
			}
	}
	if(MPIrank==MPIprocs-1) {
		/* the last rank has to set up the Neumann boundary condition at the rear (the end of the array in the sense of the Z axis) */
		for(k=0;k<bcond_thickness;k++)
			#pragma omp for schedule(runtime)
			for(j=0;j<N2;j++) {
				w_ptr1m	=  w_ptr1 = w + subgridSize + bcond_size + j*N1;
				w_ptr1	-= (k+1)*rowsize;
				w_ptr1m	+=  k*rowsize;
				for(i=0;i<N1;i++) *(w_ptr1m++)=*(w_ptr1++);
			}
	}
	#pragma omp barrier
}


void bcond_setup(FLOAT t, FLOAT * w)
/*
This function is responsible for setting the boundary conditions for both u and p.
It is called from within the right hand side and from within the main code
before a snapshot is taken (only if the whole grid including the auxiliary nodes is saved).
It must be able to be called by any rank and operate accordingly.
*/
{
	FLOAT * u = VAR(w,temperature_field);
	FLOAT * p = VAR(w,phase_field);
	FLOAT * gl = VAR(w,glass_field);

	/* temperature boundary conditions are of Dirichlet type at the top and of Neumann type elsewhere */
	bcond_setup_Combined(t, u, temperature_Dirichlet_B_C);
	/* ice phase field always has zero Neumann b.c. */
	bcond_setup_Neumann(p);
	/* glass phase field always has zero Neumann b.c. */
	bcond_setup_Neumann(gl);
}

/* ==================================================================================================== */

/* Data synchronization */

void sync_solution(FLOAT * w)
/* Performs the MPI communication on the solution array w */
{
	#pragma omp single
	{
		int q;
		MPI_Request * req = MPIreq_all;
		int req_count = 0;

		if(MPIprocs > 1) {
			/* Interchange the border rows with the adjacent blocks using nonblocking communication */
			/* NOTE: pointer type cast discards the const modifer */

			/* synchronize the top b.c. with the block above */
			if(MPIrank > 0) {
				req_count += 2*VAR_COUNT;
				for(q=0;q<VAR_COUNT;q++) {
					MPI_Isend((void *)(VAR(w,q)+bcond_size),bcond_size,MPI__FLOAT,MPIrankmap[MPIrank-1],MPIMSG_BOUNDARY+q,MPI_COMM_WORLD,req++);
					MPI_Irecv((void *)VAR(w,q),bcond_size,MPI__FLOAT,MPIrankmap[MPIrank-1],MPIMSG_BOUNDARY+q,MPI_COMM_WORLD,req++);
				}
			}

			/* synchronize the bottom b.c. with the block below */
			if(MPIrank < MPIprocs-1) {
				req_count += 2*VAR_COUNT;
				for(q=0;q<VAR_COUNT;q++) {
					MPI_Isend((void *)(VAR(w,q)+subgridSize),bcond_size,MPI__FLOAT,MPIrankmap[MPIrank+1],MPIMSG_BOUNDARY+q,MPI_COMM_WORLD,req++);
					MPI_Irecv((void *)(VAR(w,q)+bcond_size+subgridSize),bcond_size,MPI__FLOAT,MPIrankmap[MPIrank+1],MPIMSG_BOUNDARY+q,MPI_COMM_WORLD,req++);
				}
			}

			/* Wait for transfer completion before the calculation begins (this brings no time penalty for LAM MPI with TCP RPI used) */
			MPI_Waitall(req_count, MPIreq_all, MPIstat_all);				/* wait for all sync. operations */
		}
	}

}

/* ==================================================================================================== */

#define EPS_REGULARIZATION	1E-10

static inline FLOAT euclidean_norm(FLOAT v1, FLOAT v2, FLOAT v3)
/* computes an Euclidean norm of a vector v with components v1, v2, v3 */
{
	return( sqrtF(v1*v1 + v2*v2 + v3*v3) + EPS_REGULARIZATION );
}

/* ------------------ */


static inline FLOAT rho(FLOAT p, FLOAT gl)
/* calculate density of the material with the given composition */
{
	return (gl*param[glass_rho] + (1.0-gl)*(p*param[ice_rho]+(1.0-p)*param[water_rho]));
}

static inline FLOAT cp(FLOAT p, FLOAT gl)
/* calculate density of the material with the given composition */
{
	return (gl*param[glass_cp] + (1.0-gl)*(p*param[ice_cp]+(1.0-p)*param[water_cp]));
}

static inline FLOAT lambda(FLOAT p, FLOAT gl)
/* calculate density of the material with the given composition */
{
	return (gl*param[glass_lambda] + (1.0-gl)*(p*param[ice_lambda]+(1.0-p)*param[water_lambda]));
}

static inline FLOAT water_indicator(FLOAT gl)
/* indicator function of the space filled with water (either phase) */
{
	return( fmaxF(0.0, 1.0 - param[zeta]*gl) );
}

/* Reaction term - MODEL 0 (phase field / GradP) */

static inline FLOAT f_GradP(FLOAT u, FLOAT p, FLOAT gradp_norm)
/* returns the reaction term for the GradP model divided by xi^2 */
{
	return( xi_2_inv_a*p*(1.0-p)*(p-0.5) - param[b]*param[alpha]*param[mu]*gradp_norm*(u-param[u_star]) );
}

/* Reaction term - MODEL 1 (phase field / SigmaP1-P) */

static inline FLOAT Sshape(FLOAT x)
{

    if(x <= param[p_eps0]) return ( 0.0 );
    if(x >= param[p_eps1]) return ( 1.0 );
    x -= param[p_eps0];
    return ( x*x*(eps2_3 - eps3_2*x) );
}

static inline FLOAT f_SigmaP1_P(FLOAT u, FLOAT p)
/* returns the reaction term for the SigmaP1-P model, divided by xi^2 */
{
	return( xi_2_inv_a*p*(1.0-p)*(p-0.5) - xi_inv_b_sqrt_a2*param[alpha]*param[mu]*Sshape(p)*Sshape(1.0-p)*fmaxF(p*(1.0-p),0.0)*(u-param[u_star]) );
}

/* Explicit dependence of phase field on water temperature - MODEL 2 */

static inline FLOAT phf(FLOAT u)
/*
Note that this function actually neednot be used directly if the phase field is evolved together with the temperature field
according to its time derivative at every instant, i.e. dp_dt = dphf_du(u[___]) * du_dt;
For this to be done, du_dt needs to be calculated first (see f_generic_model2()).
*/
{
	/* Sasha's version with "freezing point depression" u_D */
	/*
	f(u >= (param[u_star] - param[u_D])) return 0;
	else return (1.0 - powF(param[u_D]/(param[u_star]-u), gamma));
	*/

	/* My own smooth version (uses the parameter gamma differently, but with a similar effect: the larger gamma, the quicker the phase transition) */

	return ( 0.5* (1.0 - tanhF( param[gamma]*(u-param[u_star]))) );
}

static inline FLOAT dphf_du(FLOAT u)
/* derivative of the above function */
{
	/* Sasha's version with "freezing point depression" u_D */
	/*
	if(u >= (param[u_star] - param[u_D])) return 0;
	else return (- param[gamma]*powF(param[u_D]/(param[u_star]-u), gamma+1)/param[u_D]);
	*/
	/* My own version - see phf() */
	FLOAT aux = coshF( param[gamma]*(u-param[u_star]) );
	return( -0.5*param[gamma]/(aux*aux) );
}


/* ==================================================================================================== */


int AllocPrecalcData(void)
{
	precalc = (PRECALC_DATA *)malloc(sizeof(PRECALC_DATA)*n1*n2*n3);

	return ( precalc == NULL );
}

void FreePrecalcData(void)
{
	free(precalc);
}

int PrecalculateData(FLOAT * var_eps_mult)
{
	/* precalculate auxiliary global variables */
	xi_2_inv_a		= param[a] / (param[xi]*param[xi]);
	xi_inv_b_sqrt_a2	= param[b] * sqrtF(0.5*param[a]) / param[xi];


	eps2_3 = 3.0 / ((param[p_eps1]-param[p_eps0])*(param[p_eps1]-param[p_eps0]));
	eps3_2 = 2.0 / ((param[p_eps1]-param[p_eps0])*(param[p_eps1]-param[p_eps0])*(param[p_eps1]-param[p_eps0]));

	/* initialize temperature noise field */
	{
		int i;
		int precalc_subgridsize = n1*n2*n3;
		PRECALC_DATA * pr = precalc;
		for(i=0; i<precalc_subgridsize; i++)
			(pr++)->u_noise = param[u_noise_amp] * (((FLOAT)rand() / (FLOAT)RAND_MAX) - 0.5);
	}

	/* initialize the glass phase field: 0=water, 1=glass */
	{
		int i,j,k,q;
		FLOAT x,y,z;
		FLOAT glass_phf;
		FLOAT * ptr;
		FILE * ball_positions;
		FLOAT bx[MAX_BALLS_COUNT+1], by[MAX_BALLS_COUNT+1], bz[MAX_BALLS_COUNT+1];
		int ball_count;

		char * Glass_balls_errors[]=	{
							"Reading glass balls positions failed.",
						};

		int error_code = 0;

		/* read the ball centers coordinates from file */
		if(MPIrank==0) {
			ball_positions = fopen(ball_positions_file,"r");
			if(ball_positions != NULL) {
				for(ball_count=0;ball_count<MAX_BALLS_COUNT;ball_count++) {
					if(fscanf(ball_positions, "%" iFTC_g " %" iFTC_g " %" iFTC_g, bx+ball_count, by+ball_count, bz+ball_count) < 3) break;
					bx[ball_count] = bx[ball_count] * param[beads_scaling] + param[beads_offset_x];
					by[ball_count] = by[ball_count] * param[beads_scaling] + param[beads_offset_y];
					bz[ball_count] = bz[ball_count] * param[beads_scaling] + param[beads_offset_z];
				}
				fclose(ball_positions);

				/* debug - report ball positions */
				// for(q=0;q<ball_count;q++) {
				// 	Mmprintf(logfile, "Ball %d coordinates: [%g, %g, %g]\n", q, bx[q], by[q], bz[q]);
				// }
				Mmprintf(logfile, "Successfully read coordinates of %d glass balls.\n\n", ball_count);
			} else {
				Mmprintf(logfile, "ERROR: Could not read glass balls coordinates from: %s\n", ball_positions_file);
				error_code = 1;

			}
		}

		/* error check */
		CheckErrorAcrossRanks(error_code, 1, Glass_balls_errors);

		/* broadcast the balls centers data to all ranks */
		MPI_Bcast(&ball_count, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);
		MPI_Bcast(bx, ball_count, MPI__FLOAT, MPIrankmap[0], MPI_COMM_WORLD);
		MPI_Bcast(by, ball_count, MPI__FLOAT, MPIrankmap[0], MPI_COMM_WORLD);
		MPI_Bcast(bz, ball_count, MPI__FLOAT, MPIrankmap[0], MPI_COMM_WORLD);

		/* initialize the glass phase field */
		ptr = VAR(solution,glass_field) + bcond_size;
		for(k=0;k<n3;k++) {
			z = L3* (0.5+k+first_row) / total_n3;
			ptr += bcond_thickness*N1;
			for(j=0;j<n2;j++) {
				y = L2 * (0.5+j) / n2;
				ptr += bcond_thickness;
				for(i=0;i<n1;i++) {
					x = L1 * (0.5+i) / n1;
					for(q=0;q<ball_count;q++) {
						/* sharp identification (1 or 0) */
//						if(euclidean_norm(x-bx[q],y-by[q],z-bz[q]) <= param[ball_radius]) *ptr = 1.0;
						/* phase field profile similar to that of the solution (requires initial condition set to zero in the parameters file) */
						glass_phf = 0.5*(1.0 - tanh(0.5/param[xi_gl]*(euclidean_norm(x-bx[q],y-by[q],z-bz[q]) - param[ball_radius])));
						if(*ptr < glass_phf)  *ptr = glass_phf;
					}
					ptr++;
				}
				ptr += bcond_thickness;
			}
			ptr += bcond_thickness*N1;
		}
	}

	/* override the default value 1.0 of the RKM eps multiplier for scalar variables */
	//for(q=0;q<VAR_COUNT;q++) var_eps_mult[q] = param[....TODO....+q];

	/* report the model choice based on the value of calc_mode*/
	if(MPIrank==0) {
		_string_ models [] = { "Phase field / GradP", "Phase field / SigmaP1-P", "Heat equation with latent heat release focusing" };
		_string_ model;
		switch(calc_mode) {
				case 0:
				case 1:
				case 2:
					model = models[calc_mode];
					break;
				case 10:
				case 11:
					model = models[calc_mode-10];
					break;
				default:
					Mmprintf(logfile,	"\nError : invalid calc_mode value %d\n\n", calc_mode);
					return(1);
		}
		Mmprintf(logfile,	"\nSolidification model: %s\n\n", model);

	}

	return(0);
}


/* ==================================================================================================== */
/* The right hand side itself */

/* ---------------------------- PHASE FIELD-BASED MODELS (MODEL 0,1) ---------------------------- */

void f_generic_model01(FLOAT t,const FLOAT * const_w,FLOAT * dw_dt)
/* GENERIC VERSION */
{
	/*
	subscript offset variables to simplify notation and speed up the memory dereferencing: letters indicate p(lus) 1,
	m(inus) 1 in the respective directions, in the order XYZ. The underscore indicates that there is no offset
	in the respective direction.
	*/

	const int ___ = 0;

	/* basic one-coordinate offsets */
	const int p__ = 1,		m__ = -1;
	const int _p_ = N1,		_m_ = -N1;
	const int __p = rowsize,	__m = -rowsize;

	/* compound offsets */
	const int pp_ = p__ + _p_,	mm_ = m__ + _m_;
	const int p_p = p__ + __p,	m_m = m__ + __m;
	const int _pp = _p_ + __p,	_mm = _m_ + __m;

	const int pm_ = p__ + _m_,	mp_ = m__ + _p_;
	const int p_m = p__ + __m,	m_p = m__ + __p;
	const int _pm = _p_ + __m,	_mp = _m_ + __p;


	PRECALC_DATA * pre=precalc;

	int i,j,k;

	int skip_line = bcond_thickness * N1;
	int offset;

	FLOAT * u, *p, *gl, *du_dt, *dp_dt, *dgl_dt;
	PRECALC_DATA * pr;

	FLOAT this_rho, this_cp, this_lambda;

	/* $1 \over h_{1}$ , $1 \over h_{2}$ and $1 \over h_{3}$ */
	FLOAT h1 = ((FLOAT)n1) / L1;
	FLOAT h2 = ((FLOAT)n2) / L2;
	FLOAT h3 = ((FLOAT)total_n3) / L3;

	/* squares and multiples of h1,h2,h3 used in the difference quotients */
	FLOAT h1_2 = h1*h1, h1_div_2 = 0.5*h1;
	FLOAT h2_2 = h2*h2, h2_div_2 = 0.5*h2;
	FLOAT h3_2 = h3*h3, h3_div_2 = 0.5*h3;

	/*
	The input array uu is modifed in some places of this function, which requires explicit removal
	of the const modifier. However, no parts of u that really represent the input data of the ODE
	equation system (i.e., the 'chunks' in terms of the RK_MPI_SAsolver module) are damaged. This
	is clear, since the boundary condition nodes are exactly those not included in the chunks.

	We discard the const modifier and use 'u' instead of 'const_u' from now on.
	*/
	FLOAT * w = (FLOAT *)const_w;

	bcond_setup(t, w);

	sync_solution(w);

	for(k=0;k<n3;k++) {
		/*
		the parallel loop iteration scheduling policy is set to 'runtime'.
		It is therefore controlled by the value of the OMP_SCHEDULE environment variable
		*/
		#pragma omp for schedule(runtime) nowait
		for(j=0;j<n2;j++) {
			offset = bcond_size + k*rowsize + skip_line + j*N1 + bcond_thickness;
			u = VAR(w,temperature_field) + offset;
			p = VAR(w,phase_field) + offset;
			gl = VAR(w,glass_field) + offset;
			pr = precalc + (k*n2 + j)*n1;

			du_dt = VAR(dw_dt,temperature_field) + offset;
			dp_dt = VAR(dw_dt,phase_field) + offset;
			dgl_dt = VAR(dw_dt,glass_field) + offset;

			for(i=0;i<n1;i++) {

				/* THE RIGHT HAND SIDE FORMULA using finite volume method to discretize div(grad(p)) and div(D(grad(p))) */
				/* ----------------------------------------------------------------------------------------------------- */

				this_rho = rho(p[___],gl[___]);
				this_cp = cp(p[___],gl[___]);
				this_lambda = lambda(p[___],gl[___]);

				/*
				A. gradually compute the right hand side of the Allen-Cahn equation (divided by $\alpha \xi^{2}$):
				--------------------------------------------------------------------------------------------------
				*/
				/* div(lambda*grad(u)) */
				*dp_dt =  (
					/* YZ planes */	  h1_2 * (	- ( - p[m__] + p[___] )
									+ ( - p[___] + p[p__] )
							            ) +
					/* XZ planes */	  h2_2 * (	- ( - p[_m_] + p[___] )
									+ ( - p[___] + p[_p_] )
							            ) +
					/* XY planes */	  h3_2 * (	- ( - p[__m] + p[___] )
									+ ( - p[___] + p[__p] )
							            )
					);

				/* source term */
				switch(calc_mode) {
					case 0:
					case 10:
						/* GradP model */
						*dp_dt += f_GradP(u[___]+pr->u_noise, p[___],
							euclidean_norm(
										h1_div_2 * ( - p[m__] + p[p__] ),
										h2_div_2 * ( - p[_m_] + p[_p_] ),
										h3_div_2 * ( - p[__m] + p[__p] )
									)
								);
						break;
					case 1:
					case 11:
						/* SigmaP1-P model */
						*dp_dt += f_SigmaP1_P(u[___]+pr->u_noise, p[___]);
				}

				/* finally, divide the result by the factor standing on the LHS (except for xi^2) */
				*dp_dt /= param[alpha];

				/* evolve the phase field only outside the glass balls */
				*dp_dt *= water_indicator(gl[___]);
				
				/*
				B. compute the right hand side of the heat equation in one formula:
				-------------------------------------------------------------------
				The first 'hi' in the square of 'hi' (hi_2) is in fact 1 / the volume of the cell
				(h1*h2*h3 = $1\over{h_{1}h_{2}h_{3}}$) multiplied by the area of the face
				(e.g. h_{2}h_{3} on the first line).
				The second 'hi' belongs to the respective difference quotient.
				*/
				switch(calc_mode) {
					case 10:
					case 11:
						/* solve the Allen-Cahn equation only, using constant-in-time temperature (equivalent to setting all lambdas and L to zero) */
						*du_dt = 0.0;
						break;
					default:
						*du_dt =	(	(
							/* YZ planes */	  h1_2 * (	- lambda(0.5*(p[m__]+p[___]),0.5*(gl[m__]+gl[___])) * ( - u[m__] + u[___] )
											+ lambda(0.5*(p[___]+p[p__]),0.5*(gl[___]+gl[p__])) * ( - u[___] + u[p__] )
										) +
							/* XZ planes */	  h2_2 * (	- lambda(0.5*(p[_m_]+p[___]),0.5*(gl[_m_]+gl[___])) * ( - u[_m_] + u[___] )
											+ lambda(0.5*(p[___]+p[_p_]),0.5*(gl[___]+gl[_p_])) * ( - u[___] + u[_p_] )
										) +
							/* XY planes */	  h3_2 * (	- lambda(0.5*(p[__m]+p[___]),0.5*(gl[__m]+gl[___])) * ( - u[__m] + u[___] )
											+ lambda(0.5*(p[___]+p[__p]),0.5*(gl[___]+gl[__p])) * ( - u[___] + u[__p] )
										)
										) / this_rho	
										+ param[L] * (*dp_dt)
									) / this_cp;
				}

				/*
				C. compute the right hand side of the glass phase field equation:
				-------------------------------------------------------------------
				Currently, the glass balls are static
				*/
				*dgl_dt = 0.0;

				/* ----------------------------------------------------------------------------------------------------- */

				u++; p++; gl++; du_dt++; dp_dt++; dgl_dt++; pr++;
			}
		}
	}

	#pragma omp barrier
}

/* ---------------------------- TEMPERATURE-BASED MODELS (MODEL 2) ---------------------------- */

void f_generic_model2(FLOAT t,const FLOAT * const_w,FLOAT * dw_dt)
/* GENERIC VERSION */
{
	/*
	subscript offset variables to simplify notation and speed up the memory dereferencing: letters indicate p(lus) 1,
	m(inus) 1 in the respective directions, in the order XYZ. The underscore indicates that there is no offset
	in the respective direction.
	*/

	const int ___ = 0;

	/* basic one-coordinate offsets */
	const int p__ = 1,		m__ = -1;
	const int _p_ = N1,		_m_ = -N1;
	const int __p = rowsize,	__m = -rowsize;

	/* compound offsets */
	const int pp_ = p__ + _p_,	mm_ = m__ + _m_;
	const int p_p = p__ + __p,	m_m = m__ + __m;
	const int _pp = _p_ + __p,	_mm = _m_ + __m;

	const int pm_ = p__ + _m_,	mp_ = m__ + _p_;
	const int p_m = p__ + __m,	m_p = m__ + __p;
	const int _pm = _p_ + __m,	_mp = _m_ + __p;


	PRECALC_DATA * pre=precalc;

	int i,j,k;

	int skip_line = bcond_thickness * N1;
	int offset;

	FLOAT * u, *p, *gl, *du_dt, *dp_dt, *dgl_dt;
	PRECALC_DATA * pr;

	FLOAT this_rho, this_cp, this_lambda;
	FLOAT dp_du;

	/* $1 \over h_{1}$ , $1 \over h_{2}$ and $1 \over h_{3}$ */
	FLOAT h1 = ((FLOAT)n1) / L1;
	FLOAT h2 = ((FLOAT)n2) / L2;
	FLOAT h3 = ((FLOAT)total_n3) / L3;

	/* squares and multiples of h1,h2,h3 used in the difference quotients */
	FLOAT h1_2 = h1*h1, h1_div_2 = 0.5*h1;
	FLOAT h2_2 = h2*h2, h2_div_2 = 0.5*h2;
	FLOAT h3_2 = h3*h3, h3_div_2 = 0.5*h3;

	/*
	The input array uu is modifed in some places of this function, which requires explicit removal
	of the const modifier. However, no parts of u that really represent the input data of the ODE
	equation system (i.e., the 'chunks' in terms of the RK_MPI_SAsolver module) are damaged. This
	is clear, since the boundary condition nodes are exactly those not included in the chunks.

	We discard the const modifier and use 'u' instead of 'const_u' from now on.
	*/
	FLOAT * w = (FLOAT *)const_w;

	bcond_setup(t, w);

	sync_solution(w);

	for(k=0;k<n3;k++) {
		/*
		the parallel loop iteration scheduling policy is set to 'runtime'.
		It is therefore controlled by the value of the OMP_SCHEDULE environment variable
		*/
		#pragma omp for schedule(runtime) nowait
		for(j=0;j<n2;j++) {
			offset = bcond_size + k*rowsize + skip_line + j*N1 + bcond_thickness;
			u = VAR(w,temperature_field) + offset;
			p = VAR(w,phase_field) + offset;
			gl = VAR(w,glass_field) + offset;
			pr = precalc + (k*n2 + j)*n1;

			du_dt = VAR(dw_dt,temperature_field) + offset;
			dp_dt = VAR(dw_dt,phase_field) + offset;
			dgl_dt = VAR(dw_dt,glass_field) + offset;

			for(i=0;i<n1;i++) {

				/* THE RIGHT HAND SIDE FORMULA using finite volume method to discretize div(grad(p)) and div(D(grad(p))) */
				/* ----------------------------------------------------------------------------------------------------- */

				/*
				Even for the temperature-based model, the evolution of the phase field is calculated pointwise
				from the known value of its derivative, so it can be used here
				*/

				this_rho = rho(p[___],gl[___]);
				this_cp = cp(p[___],gl[___]);
				this_lambda = lambda(p[___],gl[___]);

				/* evolve the phase field only outside the glass balls */
				dp_du = dphf_du(u[___]) * water_indicator(gl[___]);
				
				/*
				B. first compute the right hand side of the heat equation in one formula:
				-------------------------------------------------------------------------
				The first 'hi' in the square of 'hi' (hi_2) is in fact 1 / the volume of the cell
				(h1*h2*h3 = $1\over{h_{1}h_{2}h_{3}}$) multiplied by the area of the face
				(e.g. h_{2}h_{3} on the first line).
				The second 'hi' belongs to the respective difference quotient.
				*/
				*du_dt =	(
					/* YZ planes */	  h1_2 * (	- lambda(0.5*(p[m__]+p[___]),0.5*(gl[m__]+gl[___])) * ( - u[m__] + u[___] )
									+ lambda(0.5*(p[___]+p[p__]),0.5*(gl[___]+gl[p__])) * ( - u[___] + u[p__] )
							            ) +
					/* XZ planes */	  h2_2 * (	- lambda(0.5*(p[_m_]+p[___]),0.5*(gl[_m_]+gl[___])) * ( - u[_m_] + u[___] )
									+ lambda(0.5*(p[___]+p[_p_]),0.5*(gl[___]+gl[_p_])) * ( - u[___] + u[_p_] )
							            ) +
					/* XY planes */	  h3_2 * (	- lambda(0.5*(p[__m]+p[___]),0.5*(gl[__m]+gl[___])) * ( - u[__m] + u[___] )
									+ lambda(0.5*(p[___]+p[__p]),0.5*(gl[___]+gl[__p])) * ( - u[___] + u[__p] )
							            )
						) / ( this_rho * (this_cp - param[L]*dp_du) );
				
				/*
				A. compute the derivative of the phase field locally so that it evolves according to phf(u[___]),
				but only in the free space between the glass beads
				-------------------------------------------------------------------------------------------------
				 */
				*dp_dt = dp_du * (*du_dt);

				/*
				C. compute the right hand side of the glass phase field equation:
				-------------------------------------------------------------------
				Currently, the glass balls are static
				*/
				*dgl_dt = 0.0;

				/* ----------------------------------------------------------------------------------------------------- */

				u++; p++; gl++; du_dt++; dp_dt++; dgl_dt++; pr++;
			}
		}
	}

	#pragma omp barrier
}


/* ---------------------------- SINGLE RANK VERSION ---------------------------- */

void f_single(FLOAT t,const FLOAT * const_w,FLOAT * dw_dt)
/* SINGLE RANK VERSION */
{
}

/* ---------------------------- TOP BLOCK VERSION ---------------------------- */

void f_top(FLOAT t,const FLOAT * const_w,FLOAT * dw_dt)
/* TOP BLOCK VERSION */
{
}

/* ---------------------------- MIDDLE BLOCK VERSION ---------------------------- */

void f_middle(FLOAT t,const FLOAT * const_w,FLOAT * dw_dt)
/* MIDDLE BLOCK VERSION */
{
}

/* ---------------------------- BOTTOM BLOCK VERSION ---------------------------- */

void f_bottom(FLOAT t,const FLOAT * const_w,FLOAT * dw_dt)
/* BOTTOM BLOCK VERSION */
{
}

/* -------------------------------------------------------- */

/* right hand side meta-pointers (for RK solver) */

/* USE THESE META-POINTERS IF YOU WANT TO HAVE A DIFFERENT INTERPRETATION OF RHS FOR EACH RANK TYPE


RK_RightHandSide mf_single()
{
	return(f_single);
}

RK_RightHandSide mf_top()
{
	return(f_top);
}

RK_RightHandSide mf_middle()
{
	return(f_middle);
}

RK_RightHandSide mf_bottom()
{
	return(f_bottom);
}
*/

/* GENERIC RHS (ONE FUNCTION FOR ALL RANK TYPES) */

static inline RK_RightHandSide mf_generic()
{
	switch(calc_mode) {
		case 2:
			return(f_generic_model2);
		default:
			return(f_generic_model01);
	}	
}

RK_RightHandSide mf_single()
{
	return(mf_generic());
}

RK_RightHandSide mf_top()
{
	return(mf_generic());
}

RK_RightHandSide mf_middle()
{
	return(mf_generic());
}

RK_RightHandSide mf_bottom()
{
	return(mf_generic());
}
