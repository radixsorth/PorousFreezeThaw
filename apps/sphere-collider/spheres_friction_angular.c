/*
SPHERES
simulation of collisions and settling of falling spheres into a vessel
C version with OpenMP parallel processing
(C) 2022-2024 Pavel Strachota
*/

#include "RK_MPI_SAsolver.h"	/* this also includes mpi.h */

#include <netcdf.h>

#ifdef _OPENMP
	#include <omp.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mathspec.h"

/* -------------------------------------------------------------- */

// number of spheres (not constant, as it can be overriden (only decreased) by the initial condition)
int n = 200;
// sphere radius
const FLOAT r = 0.1;
// initial height of the lowest sphere
const FLOAT h0 = 1.0+r;
// vessel base dimensions
const FLOAT R = 1.0;
// final time
const FLOAT T = 8.0;

// initial conditions (definitions are below)
void icond_2spheres(FLOAT **y_ptr, FLOAT **color_ptr);
void icond_sparse(FLOAT **y_ptr, FLOAT **color_ptr);
void icond_dense(FLOAT **y_ptr, FLOAT **color_ptr);
// choose the initial condition here:
void (*icond)(FLOAT **, FLOAT **) = icond_dense;

// coefficient of restitution
const FLOAT COR = 0.4;
// focusing of the transition from full force when the collision is in its
// first (approaching) phase and the reduced force when the collision is in
// its second (separation) phase. The higher the value, the thinner is
// the transition (see the rebound() function)
const FLOAT dissipation_focusing = 10;

// friction coefficient
const FLOAT friction = 0.1;

// friction factor reducing the forces when mutual surface velocity is less than p_eps1
const FLOAT p_eps1 = 0.01;

// parameters of the collision force
const FLOAT collision_force_multiplier = 10;
const FLOAT collision_force_exponent = 15;

// maximum surface distance of interaction
const FLOAT max_surf_dist = r;

// gravity acceleration (not constant, as it can be overriden by the initial condition)
FLOAT g[3] = {0, 0, 0 -9.81};

// RK setup
const FLOAT ht = 0.1;			/* initial time step */
const FLOAT ht_min = 1e-9;		/* minimum value of ht for the RK iteration to be considered successful (see RK_MPI_SAsolver.h) */
const FLOAT delta = 0.1;

// snapshots
const int snapshots = 400;
const char * filename_base = "snap";
const char * filename_format = "OUTPUT/%s_%03d.csv";

// regularization
const FLOAT ZERO = 1e-8;

/* -------------------------------------------------------------- */

// walls definition

typedef struct {
	FLOAT P[3];		/* reference point*/
	FLOAT n[3];		/* normal vector */
} PLANE;

PLANE wall[] = {
	 { {0,0,0}, {0,0,-1} }	/* bottom */
	,{ {0,0,0}, {-1,0,0} }	/* left */
	,{ {1,0,0}, {1,0,0} }	/* right */
//	,{ {1,0,0}, {1,0,-1} }	/* right - inclined*/
	,{ {0,0,0}, {0,-1,0} }	/* front */
	,{ {0,1,0}, {0,1,0} }	/* rear */
};

const int num_walls = sizeof(wall) / sizeof(PLANE);
/* or the floor (bottom) only */
//const int num_walls = 1;

/* -------------------------------------------------------------- */

/* constants */

const FLOAT zero_vector[3] = {0,0,0};

/* moment of inertia of a unit-mass solid ball */
const FLOAT I = 2.0 / 5.0 * r * r;

/* -------------------------------------------------------------- */


static char MPIprocname[256];	/* I couldn't find anywhere what the maximum length of the processor name can be... */
static int MPIprocnamelength;
static int MPIrank;		/* the VIRTUAL rank of this process */
static int MPIprocs;		/* the total number of ranks in the MPI universe */

/*
Commands for the master rank to control the others. The commands are passed
to other ranks by the MPI_Bcast() function
*/
typedef enum
{
	MPICMD_NO_COMMAND,
	MPICMD_HALT,
	MPICMD_SOLVE,
	MPICMD_SNAPSHOT
} MPI_Command;

/* all commands are passed only through this variable */
MPI_Command MPIcmd=MPICMD_NO_COMMAND;


/* the definitions of the following functions are at the end of the file */
void HaltAllRanks(int code);
void CheckErrorAcrossRanks(int error, int code, char ** err_messg);
const char * format_time(double seconds);

/* Time measurement */

static double MPIstart_time, MPInew_start, MPIelapsed_time;

/* -------------------------------------------------------------- */

FLOAT randF()
/* returns a pseudo-random number between 0 a 1 */
{
	return(((FLOAT)rand()) / ((FLOAT)RAND_MAX));
}

#define VEC(arg,i) (arg+3*(i))

/* vector arithmetic functions */

static inline void vmov(FLOAT *a, const FLOAT *b)
{
	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
}

static inline void vadd(FLOAT * a, const FLOAT * b)
{
	a[0] += b[0];
	a[1] += b[1];
	a[2] += b[2];
}

static inline void vsub(FLOAT * a, const FLOAT * b)
{
	a[0] -= b[0];
	a[1] -= b[1];
	a[2] -= b[2];
}

static inline void vmult(FLOAT * a, const FLOAT b)
{
	a[0] *= b;
	a[1] *= b;
	a[2] *= b;
}

static inline void vmadd(FLOAT * a, const FLOAT b, const FLOAT *c)
{
	a[0] += b*c[0];
	a[1] += b*c[1];
	a[2] += b*c[2];
}

static inline FLOAT dot(const FLOAT * a, const FLOAT * b)
{
	return( a[0]*b[0] + a[1]*b[1] + a[2]*b[2] );
}

static inline FLOAT norm(const FLOAT * a)
{
	return( sqrtF(dot(a,a)) );
}

static inline void cross(FLOAT * dest, const FLOAT * a, const FLOAT * b)
{
	dest[0] = a[1]*b[2] - a[2]*b[1];
	dest[1] = a[2]*b[0] - a[0]*b[2];
	dest[2] = a[0]*b[1] - a[1]*b[0];
}

/* -------------------------------------------------------------- */

FLOAT kin_energy_fraction;		/* =COR^2 ... initialized in main() */

static inline FLOAT rebound(FLOAT v)
/*
a smooth version of a function that basically returns
1 for v>0
kin_energy_fraction for v<0
*/
{
	return( kin_energy_fraction + 0.5*(1.0-kin_energy_fraction)*(1.0+tanh(v*dissipation_focusing)) );
}

static inline FLOAT collision_factor(FLOAT surface_distance)
/*
a collision force factor depending on the distance of the surfaces of the colliding objects
*/
{
	return( collision_force_multiplier * expF(-(collision_force_exponent*(surface_distance)/r)) );
}

static inline FLOAT friction_factor(FLOAT x)
/* Sshape (sigma-limiter) based force multiplication factor ensuring that friction vanishes as mutual velocity goes to zero */
{
	static const FLOAT p_eps1 = 0.01;

	static const FLOAT eps2_3 = 3.0 / (p_eps1*p_eps1);
	static const FLOAT eps3_2 = 2.0 / (p_eps1*p_eps1*p_eps1);

	if(x >= p_eps1) return ( 1.0 );
	return ( x*x*(eps2_3 - eps3_2*x) );
}

void rhs(FLOAT t,const FLOAT * y, FLOAT * dy_dt)
/*
the right hand side of the equation system
*/
{
	int i,j;
	FLOAT mp[3], mv[3], mv_tangent[3], sv[3], torque[3];
	FLOAT distance, heading, CF, mv_tangent_magnitude, FF;
	// human-understandable aliases for the portions of the arrays y and dy_dt
	const FLOAT * pos = y;
	const FLOAT * vel = y + 3*n;
	const FLOAT * angvel = y + 6*n;
	FLOAT * acc = dy_dt + 3*n;
	FLOAT * angacc = dy_dt + 6*n;

	// calculate acceleration of all particles
	#pragma omp for
	for(i=0;i<n;i++) {

		// derivative of position is velocity
		vmov(VEC(dy_dt,i), VEC(vel,i));
		
		// initialize acceleration to gravity acceleration
		vmov(VEC(acc,i), g);

		// initialize angular acceleration to zero
		vmov(VEC(angacc,i), zero_vector);

		// repulsive & frictional forces between all particle pairs
		for(j=0;j<n;j++) {
			if(i==j) continue;
			// mutual position (i-th w.r.t. j-th particle)
			vmov(mp, VEC(pos,i));
			vsub(mp, VEC(pos,j));
			distance = norm(mp) + ZERO;
			// normalize mutual position for further use
			vmult(mp, 1.0/distance);
			// calculate the distance between surfaces
			distance -= 2*r;
			// ignore spheres that are too far away
			if(distance > max_surf_dist) continue;
			CF = collision_factor(distance);
			// mutual velocity (of i-th particle w.r.t. j-th particle)
			vmov(mv, VEC(vel,i));
			vsub(mv, VEC(vel,j));
			// derivative of mutual distance w.r.t. time (or projection of mv into the direction mp)
			// (shows if the particles are moving toward or away from each other)
			heading = dot(mv,mp);
			// tangential mutual velocity (of the center of the i-th particle w.r.t. j-th particle)
			vmov(mv_tangent, mv);
			vmadd(mv_tangent, -heading, mp);
			
			/*
			Note: as long as r is the same for all spheres, one could simplify this to first sum
			both angular velocities and then perform the vector product.
			*/
			// account for surface velocity of rotation of i-th sphere v_tangent = \vec{omega} \times \vec{r}
			// note that mp points in the OPPOSITE direction than \vec{r}, which is the vector from particle center to the point of contact at the surface
			cross(sv, VEC(angvel,i), mp);
			vmadd(mv_tangent, -r, sv);
			// account for surface velocity of rotation of j-th sphere
			cross(sv, VEC(angvel,j), mp);
			vmadd(mv_tangent, -r, sv);

			// normalize tangential velocity
			mv_tangent_magnitude = norm(mv_tangent) + ZERO;
			vmult(mv_tangent, 1.0/mv_tangent_magnitude);
			// add the acceleration of the i-th particle induced by the j-th particle:
			// 1) repulsive force
			vmadd(VEC(acc,i), CF * rebound(-heading), mp);
			// 2) frictional force: calculate the magnitude
			FF = CF * friction * friction_factor(mv_tangent_magnitude);
			// ... apply linear impulse (in the direction opposite to mv_tangent!)
			vmadd(VEC(acc,i), -FF , mv_tangent);
			// ... apply angular impulse
			// the formula for torque is \tau = \vec{r} \times \vec{F}, but we need to postpone the multiplications by scalars to the next line
			// Note that mv_tangent points in the opposite direction than the tangential force, but so does mp with respect to \vec{r},
			// so the below cross product calculates the torque with the correct orientation.
			cross(torque, mp, mv_tangent);
			vmadd(VEC(angacc,i), r*FF/I, torque);
		}
		
		// repulsive & frictional forces at the walls
		for(j=0;j<num_walls;j++) {
			// position w.r.t. the wall reference point
			vmov(mp,VEC(pos,i));
			vsub(mp,wall[j].P);
			// calculate the distance between surfaces
			distance = fabsF(dot(mp,wall[j].n)) - r;
			// ignore the walls that are too far away
			if(distance > max_surf_dist) continue;
			CF = collision_factor(distance);
			// velocity toward (!) the wall
			heading = dot(VEC(vel,i),wall[j].n);
			// tangential mutual velocity of the particle w.r.t. the wall surface
			vmov(mv_tangent, VEC(vel,i));
			vmadd(mv_tangent, -heading, wall[j].n);
			// account for surface velocity of rotation of i-th sphere
			// note that here wall[j].n points in the SAME direction than \vec{r}
			cross(sv, VEC(angvel,i), wall[j].n);
			vmadd(mv_tangent, r, sv);
			// normalize tangential velocity
			mv_tangent_magnitude = norm(mv_tangent) + ZERO;
			vmult(mv_tangent, 1.0/mv_tangent_magnitude);
			// apply repulsive force
			vmadd(VEC(acc,i), - CF * rebound(heading), wall[j].n);
			// calculate magnitude of the frictional force
			FF = CF * friction * friction_factor(mv_tangent_magnitude);
			// apply linear impulse of the frictional force (in the direction opposite to mv_tangent!)
			vmadd(VEC(acc,i), -FF, mv_tangent);
			// apply angular impulse of the frictional force
			cross(torque, wall[j].n, mv_tangent);
			vmadd(VEC(angacc,i), -r*FF/I, torque);	// here, the minus sign must compensate the orientation of mv_tangent
		}
	}
}


int save_snapshot(int snap, FLOAT *y, FLOAT * color)
/* saves the particle-related quantities & scalar color "color" to snapshot with number "snap" */
{
	FILE * f;
	int i;
	char filename[1024];

	FLOAT * pos = y;
	FLOAT * vel = y + 3*n;
	FLOAT * angvel = y + 6*n;

	sprintf(filename, filename_format, filename_base, snap);
	f = fopen(filename,"w");
	if(f == NULL) return(-1);
	// output header
	fprintf(f,"x,y,z,vx,vy,vz,avx,avy,avz,color\n");
	// output particle positions & (scalar) particle color
	for(i=0;i<n;i++)
		fprintf(f,"%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n", VEC(pos,i)[0], VEC(pos,i)[1], VEC(pos,i)[2], VEC(vel,i)[0], VEC(vel,i)[1], VEC(y,i)[2], VEC(angvel,i)[0], VEC(angvel,i)[1], VEC(angvel,i)[2], color[i]);
	fclose(f);
}

RK_RightHandSide m_rhs()
/* right hand side meta pointer */
{
	return(rhs);
}

/* -------------------------------------------------------------- */

/* versions of the initial conditions */

void alloc_data(FLOAT **y_ptr, FLOAT **color_ptr)
{
	*y_ptr = (FLOAT *)malloc(9*n*sizeof(FLOAT));
	*color_ptr = (FLOAT *)malloc(n*sizeof(FLOAT));
}

void icond_2spheres(FLOAT **y_ptr, FLOAT **color_ptr)
{
	n = 2;	/* force only 2 particles */
	vmov(g,zero_vector);	/* no gravity acceleration */

	alloc_data(y_ptr, color_ptr);

	FLOAT * pos = *y_ptr;
	FLOAT * vel = pos + 3*n;
	FLOAT * angvel = vel + 3*n;
	FLOAT * color = *color_ptr;

	FLOAT a[3] = {0,100,0};
	FLOAT v0[3] = {0,0,-1};

	int i;
	/* initial positions and velocities */
	for(i=0;i<n;i++) {
		VEC(pos,i)[0] = 0.45+1.2*r*i; // 0.5;
		VEC(pos,i)[1] = 0.5;
		VEC(pos,i)[2] = h0+5.0*r*i;

		/* z coordinate used as color */
		color[i] = VEC(pos,i)[2];

		vmov(VEC(vel,i), zero_vector);
		vmov(VEC(angvel,i), zero_vector);
	}
	vmov(VEC(vel,1),v0);
	//vmov(VEC(angvel,1), a);
}

void icond_sparse(FLOAT **y_ptr, FLOAT **color_ptr)
{
	alloc_data(y_ptr, color_ptr);

	FLOAT * pos = *y_ptr;
	FLOAT * vel = pos + 3*n;
	FLOAT * angvel = vel + 3*n;
	FLOAT * color = *color_ptr;

	int i;
	/* initial positions and velocities */
	for(i=0;i<n;i++) {
		VEC(pos,i)[0] = r+(R-2*r)*randF();
		VEC(pos,i)[1] = r+(R-2*r)*randF();
		VEC(pos,i)[2] = h0+2.0*r*i;

		/* z coordinate used as color */
		color[i] = VEC(pos,i)[2];

		vmov(VEC(vel,i), zero_vector);
		vmov(VEC(angvel,i), zero_vector);
	}
}

void icond_dense(FLOAT **y_ptr, FLOAT **color_ptr)
{
	alloc_data(y_ptr, color_ptr);

	FLOAT * pos = *y_ptr;
	FLOAT * vel = pos + 3*n;
	FLOAT * angvel = vel + 3*n;
	FLOAT * color = *color_ptr;

	int i;

	int balls_per_row = floor(R/(2.5*r));
	FLOAT distance = R/balls_per_row;
	int zi=1, yi=1, xi=1;

	for(i=0;i<n;i++) {
		/* initialize initial positions of the spheres in a jittered grid */
		VEC(pos,i)[0] = (xi-0.5)*distance + 0.25*r*randF();
		VEC(pos,i)[1] = (yi-0.5)*distance + 0.25*r*randF();
		VEC(pos,i)[2] = h0 + (zi-0.5)*distance + 0.25*r*randF();

		xi++;
		if(xi>balls_per_row) {
			xi = 1; yi++;
			if(yi>balls_per_row) {
				yi = 1; zi ++;
			}
		}	

		/* z coordinate used as color */
		color[i] = VEC(pos,i)[2];

		vmov(VEC(vel,i), zero_vector);
		vmov(VEC(angvel,i), zero_vector);
	}
}

/* -------------------------------------------------------------- */

	
int main(int argc, char *argv[])
{
	int q;

	#ifdef _OPENMP
	 int OMP_support = 1;		/* nonzero if OpenMP support has been enabled at compile time */
	 int OMP_threads = omp_get_max_threads();
	#else
	 int OMP_support = 0;		/* nonzero if OpenMP support has been enabled at compile time */
	 int OMP_threads = 1;
	#endif

	#ifdef _OPENMP
	 int thread_level_required = MPI_THREAD_FUNNELED, thread_level_provided;
	 int init_error_code = MPI_Init_thread(&argc, &argv, thread_level_required, &thread_level_provided);
	 if(init_error_code != MPI_SUCCESS || thread_level_provided < thread_level_required)
	#else
	 if(MPI_Init(&argc, &argv) != MPI_SUCCESS)
	#endif
	{
		printf("FATAL ERROR: Could not initialize MPI.\n");
		/* call this in case the cause for the error is the insufficient level of threading support */
		MPI_Finalize();
		return(2);
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
	MPI_Comm_size(MPI_COMM_WORLD, &MPIprocs);

	MPI_Get_processor_name(MPIprocname, &MPIprocnamelength);

	/* ---------- preparation ---------- */

	/*
	The value returned by time() changes every second. In order not to initialize the PRNG with the same
	value on all ranks, which would be VERY UNDESIRABLE, we add a bit of stuff that should distinguish
	between ranks. Of course, time is a bit different if we run on different computers, and
	multiplication by this big prime number should avoid accidental match of the calculated values.

	*/
	srand(time(NULL)+101009*MPIrank);
	
	
	FLOAT * y;		/* the solution - allocated from within icond() */
	FLOAT * color;	/* a constant scalar value to be mapped to the color of each of the spheres */
	
	if(MPIrank==0) printf("Initializing...\n");
	icond(&y, &color);

	/* normalize the normal vectors of all planes */
	{
		FLOAT nrm;
		for(q=0;q<num_walls;q++) {
			nrm = norm(wall[q].n);
			vmult(wall[q].n, 1.0/nrm);
		}
	}

	int chunk_start[1] = { 0 };
	int chunk_size[1] = { 9*n };
	FLOAT chunk_eps_mult[1] = { 1.0 };
	RK_MEM_DIST mem_dist = { 1, chunk_start, chunk_size, chunk_eps_mult };

	/* definition of the system solution structure */
	RK_MPI_S_SOLUTION eqSystem = {
		&mem_dist,
		0,
		y,
		m_rhs,
		ht,
		ht_min,
		delta,
		DELTA_GLOBAL,
		NULL,
		NULL,	/* service callback not used */
		0L,	/* steps */
		0L	/* steps_total */
	};

	q=RK_MPI_SA_init(9*n, MPI_COMM_WORLD, 0);

	/* RK solver initialization check - this also represents a barrier in the program flow */
	{
		char * RK_Init_errors[]= { "RK_MPI_SA_init: Not enough memory.", "Invalid block dimension." };
		char * RK_mem_dist_errors[]= { "", "",
						"RK_MPI_SA_check_mem: unitialized.",
						"",
						"RK_MPI_SA_check_mem: chunks out of memory",
						"RK_MPI_SA_check_mem: invalid chunk specification",
						"RK_MPI_SA_check_mem: number of chunks is negative or zero"
					};
		CheckErrorAcrossRanks(-q, 1, RK_Init_errors);

		/* also thoroughly check the chunk organization (in fact, this is for debugging only) */
		CheckErrorAcrossRanks( -RK_MPI_SA_check_mem(&mem_dist), 1, RK_mem_dist_errors);
	}

	kin_energy_fraction = COR * COR;


	/* carry out the simulation and save the snapshots */
	int snap;
	FLOAT t;
	MPIstart_time=MPI_Wtime();
	MPIelapsed_time=0;

	for(snap=0; snap<snapshots; snap++)
	{
			if(MPIrank==0) {
				t = (T/(snapshots-1))*snap;
				printf("Solving until t=%f ....",t); fflush(stdout);
				MPInew_start=MPI_Wtime();
				q=RK_MPI_SA_solve(t, &eqSystem);
				MPIelapsed_time+=(MPI_Wtime()-MPInew_start);
				printf("Done. Elapsed wall time: %s, %ld R-K steps (%ld total)\n",
				format_time(MPIelapsed_time), eqSystem.steps, eqSystem.steps_total);

				/* for compatibility with MATLAB code, the numbering starts from 1*/
				printf("Saving snapshot %d of %d.\n", snap+1, snapshots);
				save_snapshot(snap+1, y, color);
			} else {
				;	// currently, MPI parallelization is not supported
			}
	}

	if(MPIrank==0)
	{
		printf("\nSimulation completed in: %s.\n",format_time(MPI_Wtime()-MPIstart_time));
	}

	MPI_Finalize();
	return(0);
}

/* ============================================================================= */

void HaltAllRanks(int code)
/*
This is called by MPI rank 0 if the simulation cannot be started because of
some initialization error or when the simulation is complete. All ranks then receive
the HALT command and they should quit with the return code 'code'.
*/
{
	if(MPIprocs>1) {
		printf("\nBroadcasting the HALT command to other ranks...\n");
		MPIcmd=MPICMD_HALT;
		MPI_Bcast(&MPIcmd, 1, MPI_INT, 0, MPI_COMM_WORLD);
		/* also broadcast the error code so that all ranks return the same value */
		MPI_Bcast(&code, 1, MPI_INT, 0, MPI_COMM_WORLD);

		/* wait until all ranks process the HALT command */
		MPI_Barrier(MPI_COMM_WORLD);
		printf("All ranks halted.\n");
	}

	MPI_Finalize();

	exit(code);
}

void CheckErrorAcrossRanks(int error, int code, char ** err_messg)
/*
This is called by all ranks. It gathers the error codes to the master
rank and this rank reports the list of all failing ranks together
with the appropriate error messages. If some of the ranks reports an error,
all ranks exit with the error code 'code'.

The user must ensure that 'error' assumes nonnegative values, which can be
treated as subscripts to the array of error messages 'err_messg'. The
messages should not contain the word "Error", since it is added
automatically by this function, together with the appropriate processs ranks.

'error==1' points to the first error message.
'error==0' means success.
The err_messg array is used only by rank 0.
*/
{
	int i;
	int * errors=NULL;
	int error_reported=0;

	if(MPIprocs==1)
		if(error) {
			printf("Error: %s\n", err_messg[error-1]);
			MPI_Finalize();
			exit(code);
		} else return;


	/* this is so small that we don't believe the allocation can fail */
	if(MPIrank==0)
		errors=(int *)malloc(MPIprocs*sizeof(int));

	/* this gathers the error codes from all ranks, in the order of REAL ranks */
	MPI_Gather(&error, 1, MPI_INT, errors, 1, MPI_INT, 0, MPI_COMM_WORLD);

	if(MPIrank==0) {
		for(i=0;i<MPIprocs;i++)
			if(errors[i]) {
				printf("Error in virtual rank %d: %s\n", i, err_messg[errors[i]-1]);
				error_reported=1;
			}
		if(error_reported) HaltAllRanks(code);
		MPIcmd=MPICMD_NO_COMMAND;
	}

	/*
	the following is executed by all ranks
	When the rank 0 reaches this line, it means that no error has occurred (see HaltAllRanks() )
	*/

	MPI_Bcast(&MPIcmd, 1, MPI_INT, 0, MPI_COMM_WORLD);

	if(MPIcmd==MPICMD_HALT) {
		/* in case of termination, the master rank also broadcasts the error code */
		MPI_Bcast(&code, 1, MPI_INT, 0, MPI_COMM_WORLD);

		/* for the purpose of this, again, see HaltAllRanks() */
		MPI_Barrier(MPI_COMM_WORLD);

		MPI_Finalize();
		exit(code);
	}

	if(MPIrank==0)
		free(errors);
}

const char * format_time(double seconds)
/* Break the given time into parts. Return the given amount of time specified in seconds as a string in the format H:MM:SS.ss */
{
	static char time_str[64];
	int hours, minutes;

	if(seconds<0) seconds=0;	/* this is just to prevent outputs in the form 0:-1:60 */

	/* prevent nonsense integer overflows or even string buffer overflow, especially in time estimates in RKService() */
	if(seconds > 31536000.0) return ( "[> 1 year]" );

	minutes=(int)(floor(seconds/60));
	hours=minutes/60;
	seconds-=60*minutes;
	minutes-=60*hours;
	sprintf(time_str,"%d:%02d:%05.2f", hours, minutes, seconds);
	return(time_str);
}
