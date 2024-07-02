/**************************************************************\
*                                                              *
*                   I N T E R T R A C K  -  S                  *
*                                                              *
*      High Precision Phase Interface Evolution Simulator      *
*                      ("H i P P I E S")                       *
*                                                              *
* Features a generic thickness of the boundary layer           *
* and numerical grid layout suitable for FVM.                  *
*                                                              *
*  - hybrid OpenMP/MPI version, using RK_MPI_SAsolver_hybrid   *
*                                                              *
* -------------------------------------------------------------*
* (C) 2005-2021 Pavel Strachota                                *
* - backported extensions (C) 2013-2015 Pavel Strachota        *
* - Hybrid OpenMP/MPI parallelization (C) 2015 Pavel Strachota *
* - new features (C) 2019 Pavel Strachota                      *
* - generic system of variables & parameters (C) 2021-22 P.S.  * 
*                                                              *
* file: intertrack.c                                           *
\**************************************************************/

/*
backported extensions and modifications:

delta .......... 'RK_error' has been renamed to 'delta'
tau_min .......... minimum RK time step support
DELTA_LOCAL .... choice between local and global (default) RK-Merson truncation error control
set logfile .... memory stream logging and log file support

+ Increased maximum length of used formulas to 4095 characters.
/*

/*
Terminology in source code comments
-----------------------------------

rank .......................... the unique ID of the process in the MPI universe. We also use the term 'rank n'
				with the meaning 'the process with rank n'

master rank (master process) .. the process that controls the calculation. It loads the data, parses the
				parameter file and saves the snapshots. The master rank should therefore
				naturally be run on the machine the user works at, so that he has access
				to the input data and the calculation result. The logic of many parts of
				the program (e.g. data distribution at the beginning and gathering for
				as snapshot etc.) requires the master rank to be rank 0.

				Under MPI, however, the machine where the mpirun command is issued does not
				necessarily have to run the rank 0.
				(e.g. under LAM/MPI with one process per node, the process at node "ni" will
				have a rank equal to "i" by default, and the node where the application is
				loaded from does not have to be the "n0" node.)

				The simplest way to allow intertrack to use any rank as the master rank is the RANK
				TRANSLATION through a rank permutation array. Instead of passing 'i' as a rank
				argument to MPI functions, we simply pass 'MPIrankmap[i]' ('i' is then referred
				to as a 'virtual rank' and 'MPIrankmap[i]' as a 'real rank'). Then, the program
				can make its own ordering of the (virtual) ranks, irrespective of the actual
				rank ordering provided by MPI. The permutation must be known to all ranks before
				any MPI communication occurs. The rank of the master process is therefore
				specified on the command line. Currently, the permutation just swaps rank 0 and
				the master rank specified on the command line.
				The RK solver does not have to be aware of the permutation, as the RK solver
				itself does not rely on rank ordering in any manner. However, it allows one to
				choose the master process rank, which is necessary if the calculation
				parameters (final time etc.) are available in the master rank only.
				Special care must be taken of collective communication functions like MPI_Gather(),
				which store the incoming messages from all ranks in the order of real ranks.

				MPI implementations should provide a mechanism of reordering the ranks on
				nodes from the command line of mpirun in case the user's machine cannot be made
				the n0 node. Use of the rank reordering provided by intertrack should be therefore
				unnecessary.

				(see also 'MPIrankmap' pointer definition below)

block ......................... the portion of the grid. Each process performs the equation solution on one
				block. We also say 'the process solves the block'. The block solved by the
				n-th rank is called the n-th block. Remember that ranks range from 0 to (MPIprocs-1)!

current block ................. the block solved by the process which is supposed to perform those commands
				the comment is related to.
*/

/*
intertrack uses some POSIX C library features, such as the S_Ixxxx flags. It is convenient to
explicitly announce it, even though the compilation may often work without it because:
- There are differences in what is considered to be POSIX among different library implementations.
- Compilers may include POSIX extensions automatically when some compilation switches are defined
  (such as +Oopenmp on HP-UX c89).

This macro affects the GNU C library and the HP-UX C library, and possibly others as well.
*/
#define _POSIX_SOURCE

/* controls whether the extended version of fork() is used for concurrent script execution */
#ifdef __USE_VFORK
	#define _XOPEN_SOURCE
	#define _XOPEN_SOURCE_EXTENDED
#endif

#include "AVS.h"
#include "common.h"
#include "pparser.h"
#include "cparser.h"
#include "evsubst.h"
#include "mprintf.h"

#include "RK_MPI_SAsolver.h"	/* this also includes mpi.h */

#include <netcdf.h>

#ifdef _OPENMP
	#include <omp.h>
#endif

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mathspec.h"

/*
Cell-centered finite volume meshes contain all nodes (*) in the cell interiors
and the boundary of Omega is only made up by a subset of cell edges:

+---+---+---+
|   |   |   |
| * | * | * |
|   |   |   |
+---+---+---+
|   |   |   |
| * | * | * |
|   |   |   |
+---+---+---+

This version of intertrack uses FVM meshes and always exports the whole grid in Omega.
The bcond_thickness attribute is not defined in the resulting datasets.

However, the actual computational grid is larger and contains extra auxiliary nodes to
implement the boundary conditions. These nodes are geometrically located outside
of Omega. For debugging purposes, intertrack can export them to the file as well.

However, the resulting dataset is intended for uncolorized slicing only
for the purpose of troubleshooting as no information in the dataset
indicates that the extra nodes were exported.
*/

/* =========================================================================== */

#define MAX_FORMULA_LENGTH	4095

/* Minimum interval between successive commits to the log file (in seconds) */
#define MIN_LOG_COMMIT_INTERVAL	3.0

/* =========================================================================== */
/* Data structures for output logging */

MEMSTREAM * logfile = NULL;			/* memory buffer for logging information */

static char logfile_name[1024] = "";		/* log file name */

void commit_logfile(int force_commit)
/*
commits the log buffer to the log file (if possible). If called multiple times, only the additional content
printed to the memory stream since the last commit is written.

If force_commit is nonzero, the commit is performed regardless of the time period
since the last successful call to commit_logfile(). Otherwise, the commit happens only
if the time since the last commit is at least MIN_LOG_COMMIT_INTERVAL.
*/
{
	/* number of bytes already committed to the buffer during the previous calls */
	static size_t bytes_committed = 0;
	static time_t prev_commit_time;
	time_t current_time;

	FILE * logfile_on_disk;
	/* effectively prevents this function to run on ranks other than 0 where the only log stream exists */
	if(!logfile) return;

	time(&current_time);
	/* check minimum time interval between successive commits */
	if(bytes_committed && !force_commit)
		if(difftime(current_time, prev_commit_time) < MIN_LOG_COMMIT_INTERVAL) return;

	if(*logfile_name)
		if( (logfile_on_disk=fopen(logfile_name, bytes_committed? "a" : "w")) != NULL) {
			fwrite(logfile->buffer+bytes_committed, 1, logfile->size-bytes_committed, logfile_on_disk);
			fclose(logfile_on_disk);
			bytes_committed=logfile->size;
			prev_commit_time=current_time;
		} else printf("Warning: Cannot write to the log file: %s\n", logfile_name);
}

/* =========================================================================== */
/* Data structures for MPI communication */

/* message tags (types) */
#define MPIMSG_SOLUTION	100	/* solution gathering or initial condition deployment (add q for the q-th variable) */
#define MPIMSG_BOUNDARY	200	/* boundary grid nodes exchange (add q for the q-th variable) */

#define MPIMSG_PROCNAME		400	/* processor name gathering */
#define MPIMSG_CUSTOM		500	/* please index custom transfer tags in equation.c by MPIMSG_CUSTOM+i where i>=0 */

/*
Commands for the master rank to control the others. The commands are passed
to other ranks by the MPI_Bcast() function
*/
typedef enum
{
	MPICMD_NO_COMMAND,
	MPICMD_HALT,
	MPICMD_NEXT,
	MPICMD_SOLVE,
	MPICMD_SNAPSHOT
} MPI_Command;

/* all commands are passed only through this variable */
MPI_Command MPIcmd=MPICMD_NO_COMMAND;


static char MPIprocname[256];	/* I couldn't find anywhere what the maximum length of the processor name can be... */
static int MPIprocnamelength;

static int * MPIrankmap;	/* the rank permutation array. All MPI communication in intertrack is performed using
				   RANK TRANSLATION, so that it references the real rank MPIrankmap[i] whenever it
				   wants to talk to (virtual) rank i. intertrack assumes that the (virtual) ranks are
				   ordered so that the master rank is rank 0 and the grid blocks from top to bottom
				   are assigned to ranks in an ascending order.
				   Rank translation makes intertrack independent of actual assignment of ranks to processes
				   performed by MPI. As a result, we can easily choose which rank we want to
				   use as a master rank and intertrack will establish a rank permutation that will map
				   the virtual master rank 0 to the desired rank.
				*/

static int MPIrank;		/* the VIRTUAL rank of this process */
static int MPIprocs;		/* the total number of ranks in the MPI universe */
static int MPImaster=0;		/* the REAL rank of the master process. The virtual rank of the master process
				   is always 0. */

static char MPIwfOrder=0;	/* specifies the order of the actions taken in MPI_FinalizeAndWait() function. */

static MPI_Status MPIstat;

/* the MPI data type for the tensor field ( initialized in main() )*/
static MPI_Datatype MPI_SymTENSOR3D_double;

/* =========================================================================== */
/* Time measurement */

static double MPIstart_time, MPInew_start, MPIelapsed_time;	/* for wall time measurement purposes: */

static double AUX_time, AUX_time2;				/* for the measurement of auxiliary tasks duration */

/* calendar time variables */
static time_t calendar_time, cal_start_time, cal_start_batch_time;
static struct tm * br_time;					/* broken-down time */


/* =========================================================================== */
/* Model parameters */

static FLOAT L1, L2, L3;	/* domain dimensions */

/*
The definitions of names, descriptions and subscript mnemonics
for all state variables and physical quantities of the equation system
are provided in a separate file:
*/

#include "model.c"

/* the structure of calculation parameters broadcast to all processes */
/*
WARNING: Passing the calculation parameters as a structure of plain binary data requires
	 two restricting prerequisites to be met:
	 1) The compilers must obey the same structure member alignment rules on all nodes.
	    For example, there are no additional bytes in the structure on a 32bit Intel architecture
	    when compiled with gcc or icc with no special memory alignment options, regardless of
	    the actual type of FLOAT.
	 2) All nodes must have an identical architecture. This above all means that the data types must
	    have the same length and representation and that the byte ordering must be the same on
	    all machines (Little Endian vs. Big Endian).

	 The above mentioned constraints could be eliminated if we passed the parameters as single values,
	 specifying their correct MPI data type. MPI should perform the necessary conversions automatically.
	 For more information, refer to the MPI documentation.
*/
typedef struct
{
	FLOAT L1, L2, L3;
	int n1, n2, total_n3;

	int calc_mode;
	char icond_mode;
	char grid_IO_mode;

	FLOAT model_parameters[PARAM_COUNT];
	char icond_formula[VAR_COUNT][4096];
} MPI_Calculation;

static FLOAT final_time;
static FLOAT starting_time;		/* this can be overriden by the continue_series option */

/* =========================================================================== */
/* Implementation data structures */

static char out_file[4096]="";		/* output files path template*/
static char out_file_suffix[256]="";	/* suffix added to each snapshot pathname (possibly empty) */

/* initial conditions specifications (either math formulae or a path to the dataset containing the initial condition) */
static char icond_formula[VAR_COUNT][4096];	/* initial conditions formulas for all variables */
static char icond_file[4096]="";		/* initial conditions dataset file name (contains both u and p) */

/* debug log / snapshot trigger */
static char debug_logging=0;		/* if nonzero, one line will be written to the debug log after each successful
					   time step of the RK solver. Information about the solution progress
					   and estimated time until next regular snapshot is provided in the records. */
static char debug_logfile[4096]="";	/* debug log filename */
static FILE * debug_logfile_ID=NULL;	/* the log file is accessed from within several functions
					   => its stream ID must be global */
static char snapshot_trigger=0;		/* if nonzero, immediate on-demand snapshot generation (after the computation
					   of the current time step is finished) will be triggered by the presence
					   of the file 'snapshot_trigger_file'. After the snapshot is saved, the trigger
					   file is deleted. To create another snapshot, recreate the file (e.g. by using
					   the 'touch' command. */
static char snapshot_trigger_file[4096];

static char pproc_script[4096]="";	/* the path to the post-processing script */

static char pproc_nofail=0;	/* if set to nonzero, intertrack will terminate in case the post-processing
				   script ends with a nonzero exit status */
static char pproc_nowait=0;	/* if set to nonzero, intertrack will execute the the post-processing script as a
				   concurrent process with a lower priority and it will proceed to the next
				   iteration immediately. No script error checking will be done and the
				   script output will be redirected to /dev/null.
				   - The script may continue execution after intertrack has finished all the computations.
				     In this case, intertrack halts all ranks except for the master rank and then waits
				     for all the scripts to finish. This is important on load-management systems like
				     LSF since they kill the child processes when the master terminates.
				   - More scripts might be running at the same time if their execution time
				   is longer than the iteration calculation time. */
static char pproc_submitted=0;	/* nonzero if at least one post processing script has been executed in concurrent
				   mode. Used by the MPI_FinalizeAndWait() function to determine if there is a need
				   to wait for the script(s) to finish. */

static char skip_icond=0;	/* nonzero if the initial conditions snapshot (snapshot 0) should be skipped */
static int calc_mode;		/* - a parameter that can be used in equation.c to determine which
				   discretization to use (if more than one is included). For example, in
				   the AllocPrecalcData() function, one may set up some pointers so that
				   the right hand side meta-pointers return the appropriate values.
				   Alternatively, the meta-pointers may decide on their return value by
				   checking calc_mode directly. See equation.c file for more details
				   about AllocPrecalcData() and the meta-pointers.
				   - 'calc_mode' is obtained from the parameter file as the value of the
				   variable with the same name. This is an optional parameter, it is not
				   directly used by any function in intertrack.c.
				   - 'calc_mode' has a form of a numeric parameter so that it may assume as
				   many different values as needed by the particular right hand side.
				   Moreover, it is designed to change in batch processing.
				   - 'calc_mode' is available in all ranks */
static int N1, N2, N3;		/* computation grid dimensions, including the boundary conditions
				   (N3 is the block depth, including the front and rear layers for data
				   interchange. This is logical since each block can be looked at as a
				   whole standalone grid with special boundary conditions imposed by the
				   synchronization procedure.). */
static int n1,n2,n3;		/* same as Ni, without the bounding rows designed for data interchange
				   between ranks or for the boundary condition. In other words,
				   ni = Ni - 2*bcond_thickness */
static int total_N3;		/* the depth of the whole grid */
static int total_n3;		/* total number of rows actually calculated, without the boundary condition rows.
				   In other words, total_n3 = total_N3 - 2*bcond_thickness */
static int first_row;		/* generally, the absolute row index in the grid of the first row actually
				   calculated by the current rank, with respect to the inner grid. However, it may as well
				   be treated as the absolute row index of the first row of the current block
				   (i.e., including the boundary rows), with respect to the whole grid (this is because
				   the whole grid is obtained from the inner grid by the same extension as the whole block
				   from the 'inner block', i.e. the block of nodes actually treated by the current rank. */
static int rowsize;		/* size of one row (layer) of the block: rowsize=N1*N2; */
static int bcond_size;		/* bcond_size = bcond_thickness * rowsize; This variable is used e.g. in MPI calls
				   controlling synchronization at blocks' boundaries and in the bcond_setup() boundary
				   condition setup routine. */
static int subgridSIZE;		/* The total number of nodes in the current block. subgridSIZE = N1*N2*N3 */
static int subgridSize;		/* the number of nodes including the boundary in the plane x,y. In other words,
				   subgridSize = N1*N2*n3. In fact, this variable has scarce usage. */
static int subgridsize;		/* the number of nodes of the current block saved to the snapshot. If grid_IO_mode==1,
				   then this is also the number of nodes actually calculated in the current block.
				   In other words, subgridsize = n1*n2*n3. Otherwise, the boundary condition must
				   be taken into account and the value of subgridsize depends on the position of
				   the block in the grid. */
static char continue_series=0;	/* if nonzero and the initial conditions are loaded from file, Intertrack will adopt the
				   starting time, time step and snapshot number from the initial conditions dataset.
				   This is useful for seamless continuation of a previously interrupted computation.
				   However, other parameters (final time, total number of snapshots, input tensor
				   field etc. are still read from the parameters file. Intertrack can only recognize
				   incompatibility in the initial conditions if the starting time is beyond
				   the final time or if the starting snapshot number is greater or equal to the total
				   number of snapshots. Other changes of parameters (with respect to the calculation the
				   initial conditions came from) will be accepted. The remaining time period is
				   always uniformly divided between the remaining number of snapshots to be taken. */
static int __snapshot;		/* global variable holding the number of the currently computed snapshot. It is used
				   by the RK slover debug logging from within the RKService() callback (see further on) */

static char grid_IO_mode=1;	/* grid input/output mode: 0 = full grid with auxiliary nodes (for troubleshooting only!!),
							   1 = inner grid (covering Omega) only */
static char icond_mode=0;	/* initial conditions mode: 0 = formulae, 1 = from file */

static char comment[100]="";	/* a comment saved as a string attribute to the resulting NetCDF datasets */

/* =========================================================================== */
/*
The definitions of names, descriptions and subscript mnemonics
for all state variables and physical quantities of the equation system
are provided in a separate file:
*/

static FLOAT * solution;		/* aggregate memory area accomodating all solution variables */

static FLOAT model_parameters[PARAM_COUNT];	/* the array of all other model parameters */
static FLOAT * param = model_parameters;	/* abbreviation to model_parameters (not used in intertrack.c but rather in equation.c) */

/* the following macro locates the correct beginning of the var_no-th state variable within the var_vector */
#define VAR(var_vector,var_no) ((var_vector) + (var_no)*subgridSIZE)

/* =========================================================================== */

static char should_break;	/* a flag set to 1 by the 'break' command handler.
				   The PParse user handler returns PP_BREAK when should_break
				   is nonzero and the parsing of the parameter file
				   is then terminated */

/* batch processing variables */

/*
INTERTRACK can make batch processing possible by repeating the whole calculation (including parameter
file parsing) as if it were in the following loop:
for(i1=1;i1<=loopI[1];i1++)
 for(i2=1;i2<=loopI[2];i2++)
   ...
      for(iN=1;iN<=loopI[N];iN++) { process the parameters, perform the calculation and save the result }
The i1,...,iN loop control variables are available for use in the parameter file.
*/

#define MAX_NESTED_LOOPS	20

static int loopUbound[MAX_NESTED_LOOPS];	/* array of loop upper bounds. The maximum of MAX_NESTED_LOOPS
						   nested loops is allowed */
static int loopI[MAX_NESTED_LOOPS];		/* array of loop control variables i1,i2,...,iN */
static int loopN=0;				/* the number of nested loops (0 = no batch processing) */
static int loopTotal=1;				/* total number of iterations (of the innermost loop, i.e. of the calculation) */
static int loopIter=0;				/* the current iteration number (1 to loopTotal) */
static char loopContinue;			/* the flag indicating that the current iteration should be skipped */
static int loopUb_digits;			/* the number of digits of the largest loop upper bound */
static char loopVarString[256];			/* the string of the current values of the loop control variables */

/* string-for-number substitution variables */

static _string_ loopVarMnemonic[MAX_NESTED_LOOPS];	/* contains mnemonic names for loop control variable values -
							   implemented as a space-separated list of symbols for each
							   variable. Initialized by the 'mnemonic' command. The command
							   is executed only upon the first iteration in the batch mode.
							   You should therefore place it before a 'continue_if' command
							   that skips the calculation (and parameter file parsing)
							   in the first iteration. */
/*
note that all variables that are initialized here don't need to be reinitialized in each iteration,
Note that this proposition holds only if the parameters file does not change during the calculation!
(changing that file may lead to unpredictable results)
*/

/* =========================================================================== */
/* The MPI management functions */

void BuildRankMap(int master_rank)
/*
Fills the rank permutation array and maps the virtual rank 0 to the specified
master rank
*/
{
	int i;

	for(i=0;i<MPIprocs;i++)
		MPIrankmap[i]=i;

	MPIrankmap[master_rank]=0;
	MPIrankmap[0]=master_rank;
}

void MPI_FinalizeAndWait(void)
/*
This function is called by the master rank before termination. It performs the following
actions in the order specified by the MPIwfOrder flag (by the 'pproc_waitfirst' option).
By default, the actions take place in the same order as listed below.
- calls MPI_Finalize()
- waits until all post-processing scripts executed in concurrent mode terminate.

NOTE:	The aim is to halt all ranks except rank 0, which should wait for the post-processing
	scripts to finish. Depending on the implementation of MPI_Finalize(), we might need
	to place it before the waiting procedure, otherwise the other ranks would not terminate.
	(There may be a barrier in MPI_Finalize()). On the other hand, if there is no barrier
	in MPI_Finalize(), it may be better to place it after the waiting procedure, as it is
	in LAM MPI. The reason is that there is some confusion about what happens after the
	(possible) return from MPI_Finalize().

	In the MPI 1.1 standard, it is recommended but not required that the program returns
	from MPI_Finalize(). The only process for which it is required is the REAL rank 0.
	Using rank translation with the virtual rank 0 mapped to any other rank than 0 may cause
	the code after MPI_Finalize() not to execute, depending on MPI implementation.
*/
{
	if(!MPIwfOrder) MPI_Finalize();

	if(pproc_nowait && pproc_submitted) {
		pid_t pid;

		Mmprintf(logfile, "\nMaster rank waiting for post-processing scripts to finish...\n");
		while( (pid=wait(NULL)) >=0 )
			Mmprintf(logfile, "Script PID %d finished.\n", pid);
		Mmprintf(logfile, "All scripts finished. Master rank halted.\n");
	}

	if(MPIwfOrder) MPI_Finalize();
}


void HaltAllRanks(int code)
/*
This is called by MPI virtual rank 0 if intertrack could not start the calculation because of
some initialization error or when the calculation is complete. All ranks then receive
the HALT command and they should quit with the return code 'code'.
*/
{
	if(MPIprocs>1) {
		Mmprintf(logfile, "\nBroadcasting the HALT command to other ranks...\n");
		MPIcmd=MPICMD_HALT;
		MPI_Bcast(&MPIcmd, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);
		/* also broadcast the error code so that all ranks return the same value */
		MPI_Bcast(&code, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

		/* wait until all ranks process the HALT command */
		MPI_Barrier(MPI_COMM_WORLD);
		Mmprintf(logfile, "All ranks halted.\n");
	}

	commit_logfile(1);	/* force the commit */

	/* close the log buffer in memory */
	mclose(logfile);

	MPI_FinalizeAndWait();

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
			Mmprintf(logfile, "Error: %s\n", err_messg[error-1]);
			MPI_FinalizeAndWait();
			exit(code);
		} else return;


	/* this is so small that we don't believe the allocation can fail */
	if(MPIrank==0)
		errors=(int *)malloc(MPIprocs*sizeof(int));

	/* this gathers the error codes from all ranks, in the order of REAL ranks */
	MPI_Gather(&error, 1, MPI_INT, errors, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

	if(MPIrank==0) {
		for(i=0;i<MPIprocs;i++)
			if(errors[i]) {
				Mmprintf(logfile, "Error in virtual rank %d: %s\n", i, err_messg[errors[MPIrankmap[i]]-1]);
				error_reported=1;
			}
		if(error_reported) HaltAllRanks(code);
		MPIcmd=MPICMD_NO_COMMAND;
	}

	/*
	the following is executed by all ranks
	When the rank 0 reaches this line, it means that no error has occurred (see HaltAllRanks() )
	*/

	MPI_Bcast(&MPIcmd, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

	if(MPIcmd==MPICMD_HALT) {
		/* in case of termination, the master rank also broadcasts the error code */
		MPI_Bcast(&code, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

		/* for the purpose of this, again, see HaltAllRanks() */
		MPI_Barrier(MPI_COMM_WORLD);

		MPI_Finalize();
		exit(code);
	}

	if(MPIrank==0)
		free(errors);
}

/* =========================================================================== */

#include "equation.c"		/* the right hand side of the equation */

/* NOTE: In equation.c, the variable 'bcond_thickness' must be defined and initialized */

/* =========================================================================== */

/* data precalculation with an error check */

void PrecalcData_with_check(FLOAT * var_eps_mult)
{
	char * Precalc_errors[]= { "Error in generic data precalculation function." };

	CheckErrorAcrossRanks	(
					PrecalculateData(var_eps_mult) ? 1 : 0,
					1, Precalc_errors
				);
}

/* =========================================================================== */
/* auxiliary expression evaluation functions */

double evchk(_conststring_ expr)
/* internally used to retrieve values of single variables only */
{
	double x=eval(expr);
	if(! ev_error()) return(x);
	Mmprintf(logfile, "Error: Undefined variable '%s'.\nStop.\n", expr);
	HaltAllRanks(1);
	return(1);
}

double evchkD(_conststring_ expr, double default_value)
/* like evchk, but returns variable default values in case of error */
{
	double x=eval(expr);
	if(! ev_error()) return(x);
	Mmprintf(logfile, "Warning: Undefined variable '%s', using default value: %g\n", expr, default_value);
	return(default_value);
}

int ToInt(double x)
/*
rounds 'x' to the nearest integer value.
not designed to round values exceeding the range of int.
*/
{
	double r=floor(x);
	if(x-r>=0.5) r+=1;
	return((int)r);
}

/* =========================================================================== */
/* the structures for cparser */

CP_STAT skip_command(int cmd);
CP_STAT skip_set_option(int cmd, int opt, _conststring_ value);

CP_STAT set_comment(int cmd, int opt, _conststring_ value)
{
	set(comment, value);
	Mmprintf(logfile, "Comment set: %s\n", comment);
	return(CP_SUCCESS);
}

/* --- I/O file path setting handlers --- */

CP_STAT generic_set_path(_string_ target, _conststring_ value, _conststring_ success_message_format)
/*
this function provides a template for path setting option handlers
*/
{
	int e;
	e=ev_subst(target, value);
	switch(e) {
		case -1:	Mmprintf(logfile, "Error: Environment variable undefined.\n"); return(CP_ERROR);
		case -2:	Mmprintf(logfile, "Error: Illegal environment variable name.\n"); return(CP_ERROR);
		default:	Mmprintf(logfile, success_message_format, target);
	}
	return(CP_SUCCESS);
}

CP_STAT set_out_file_suffix(int cmd, int opt, _conststring_ value)
{
	set(out_file_suffix, value);
	Mmprintf(logfile, "Output file suffix set: %s\n", value);
	return(CP_SUCCESS);
}

CP_STAT set_out_file(int cmd, int opt, _conststring_ value)
{
	return(generic_set_path(out_file, value, "Output file template set: %s\n"));
}

CP_STAT set_icond_formula(int cmd, int opt, _conststring_ value)
{
	icond_mode=0;
	set(icond_formula[opt], value);
	Mmprintf(logfile, "Initial condition formula for %s (%s) set: %s\n", variable[opt].name, variable[opt].description, value);
	return(CP_SUCCESS);
}

CP_STAT set_icond_file(int cmd, int opt, _conststring_ value)
{
	icond_mode=1;
	return(generic_set_path(icond_file, value, "Initial conditions input dataset set: %s\n"));
}


/* setting of initial conditions handling modes */

CP_STAT set_skip_icond(int cmd, int opt, _conststring_ value)
{
	skip_icond=1;
	Mmprintf(logfile, "Output of the initial conditions (snapshot 0) will be skipped.\n");
	return(CP_SUCCESS);
}

CP_STAT set_continue_series(int cmd, int opt, _conststring_ value)
{
	continue_series=1;
	Mmprintf(logfile, "Series continuation mode ON.\n");
	return(CP_SUCCESS);
}

/* --- init/progess log --- */

CP_STAT set_logfile(int cmd, int opt, const char * value)
{
	return(generic_set_path(logfile_name, value, "Log file init/progress information set: %s\n"));
}

/* debug log and on-demand snapshot generation settings */

CP_STAT set_debug_logfile(int cmd, int opt, _conststring_ value)
{
	debug_logging=1;
	return(generic_set_path(debug_logfile, value, "Debug RK solver logging has been turned ON, output goes to file: %s\n"));
}

CP_STAT set_snapshot_trigger(int cmd, int opt, _conststring_ value)
{
	snapshot_trigger=1;
	return(generic_set_path(snapshot_trigger_file, value, "On-demand snapshot generation ON. Snapshot will be triggered by file: %s\n"));
}

/* grid output mode setting */

CP_STAT grid_output(int cmd, int opt, _conststring_ value)
{
	static _string_ grid_out_modes[] = { "FULL (Warning: For troubleshooting only!)", "STANDARD" };

	grid_IO_mode=opt;

	Mmprintf(logfile, "Grid I/O mode set to: %s\n", grid_out_modes[opt]);
	return(CP_SUCCESS);
}


/* --- batch mode option handlers --- */

void batch_mode_warning(_conststring_ what)
{
		Mmprintf(logfile, "Warning: '%s' supported in batch processing mode only.\n", what);
}

CP_STAT set_pproc_script(int cmd, int opt, _conststring_ value)
{
	if(!loopN) {
		batch_mode_warning("set pproc_script");
		return(CP_SUCCESS);
	}
	return(generic_set_path(pproc_script, value, "Post-processing script set: %s\n"));
}

CP_STAT set_pproc_nofail(int cmd, int opt, _conststring_ value)
{
	if(!loopN)
		batch_mode_warning("set pproc_nofail");
	else {
		pproc_nofail=1;
		Mmprintf(logfile, "Forcing termination in case of error in post-processing script.\n");
	}
	return(CP_SUCCESS);
}

CP_STAT set_pproc_nowait(int cmd, int opt, _conststring_ value)
{
	if(!loopN)
		batch_mode_warning("set pproc_nowait");
	else if(loopIter==1) {
		pproc_nowait=1;
		Mmprintf(logfile, "!!! ENTERING CONCURRENT POST-PROCESSING SCRIPT EXECUTION MODE !!!\n");
	}
	return(CP_SUCCESS);
}

CP_STAT set_pproc_waitfirst(int cmd, int opt, _conststring_ value)
{
	if(!loopN)
		batch_mode_warning("set pproc_waitfirst");
	else if(loopIter==1) {
		MPIwfOrder=1;
		Mmprintf(logfile, "MPI_Finalize() will be called  A F T E R  all post-processing scripts have finished.\n");
	}
	return(CP_SUCCESS);
}

_conststring_ mnemonic(CP_CURRENT_COMMAND * c, _conststring_ opts)
/*
loads the string of space-delimited mnemonic names for the given loop control variable.
To set the string for variable i4, one should write
mnemonic 4: string1 string2 string3 .... stringN
For each loop control variable, the string can be defined only once.
Each string can be delimited by any number of whitespaces.
*/
{
	_string_ opt;
	int k;

	if(!loopN) {
		batch_mode_warning("mnemonic");
		return(opts);
	}

	if(loopIter==1) {		/* load strings in the first iteration only */
		/* the option string is returned including the leading spaces and the trailing LF */
		while(*opts==' ' || *opts==0x9 || *opts=='\n') opts++;
		opt=(_string_)opts;
		/* cut the trailing LF */
		while(*opt!='\n' && *opt!=0) opt++;
		*opt=0;

		k=instr(opts, ":", 1, SENSITIVE);				/* k will lose this meaning on the next line... */
		if(k!=0) { opt=(_string_)opts+k; opt[-1]=0; k=val(opts); };	/* get the loop control variable number into 'k' */
		if(k<=0) {
			Mmprintf(logfile, "mnemonic: Invalid loop control variable specification.\n");
			return(NULL);
		}
		if(k>loopN) {
			Mmprintf(logfile, "Warning: Ignored 'mnemonic' request for an unused loop control variable.\n");
			return(opts);
		}
		if(loopVarMnemonic[k-1] != NULL) {
			Mmprintf(logfile, "Warning: Mnemonic already defined for loop control variable i%d.\n", k);
			return(opts);
		}

		/* store the mnemonic names string */
		loopVarMnemonic[k-1]=(_string_)malloc(len(opt)+1);
		set(loopVarMnemonic[k-1], opt);
		Mmprintf(logfile, "Mnemonic names set for values of i%d.\n", k);
	}
	return(opts);	/* this is just some nonzero pointer. The string contents are ignored anyway. */
}

_conststring_ continue_if(CP_CURRENT_COMMAND * c, _conststring_ opts)
{
	double result;
	_string_ opt;

	if(!loopN) {
		batch_mode_warning("continue_if");
		return(opts);
	}

	/* the option string is returned including the leading spaces and the trailing LF */
	while(*opts==' ' || *opts==0x9 || *opts=='\n') opts++;
	opt=(_string_)opts;
	/* cut the trailing LF */
	while(*opt!='\n' && *opt!=0) opt++;
	*opt=0;

	result = eval(opts);
	if(ev_error()) {
		Mmprintf(logfile, "continue_if: Error in expression.\n");
		return(NULL);
	}
	if(result!=0) {
		should_break=1;
		loopContinue=1;
	}
	return(opts);	/* this is just some nonzero pointer. The string contents are ignored anyway. */
}

CP_STAT break_now(int cmd)
{
	should_break=1;
	return(CP_SUCCESS);
}

/* --- cparser structures for all supported commands and options --- */

CP_OPTION cmd_set [] =		{
					{ "comment", CP_REQUIRED, set_comment },
					{ "out_file", CP_REQUIRED, set_out_file },
					{ "out_file_suffix", CP_REQUIRED, set_out_file_suffix },
					{ "icond_file", CP_REQUIRED, set_icond_file },
					{ "skip_icond", CP_NONE, set_skip_icond },
					{ "continue_series", CP_NONE, set_continue_series },

					{ "logfile", CP_REQUIRED, set_logfile },
					{ "debug_logfile", CP_REQUIRED, set_debug_logfile },
					{ "snapshot_trigger", CP_REQUIRED, set_snapshot_trigger },

					{ "pproc_script", CP_REQUIRED, set_pproc_script },
					{ "pproc_nofail", CP_NONE, set_pproc_nofail },
					{ "pproc_nowait", CP_NONE, set_pproc_nowait },
					{ "pproc_waitfirst", CP_NONE, set_pproc_waitfirst },

					{ "slice_outfile", CP_REQUIRED, skip_set_option },
					{ "slice_input_dataset", CP_REQUIRED, skip_set_option },
					{ "slice_stepping", CP_REQUIRED, skip_set_option },
					{ "slice_colormap", CP_REQUIRED, skip_set_option },

					{ NULL, CP_NONE, NULL }
			  	};

CP_OPTION cmd_icond [VAR_COUNT+1];

/*
the content of the cmd_icond structure is created dynamically by means of initialize_cparser_structs()
*/

void initialize_cparser_structs(void)
{
	CP_OPTION cmd_icond_template = { NULL, CP_REQUIRED, set_icond_formula };
	CP_OPTION stopper = { NULL, CP_NONE, NULL };

	int q;

	for(q=0;q<VAR_COUNT;q++) {
		cmd_icond[q] = cmd_icond_template;
		cmd_icond[q].name = variable[q].name;
	}
	cmd_icond[VAR_COUNT] = stopper;
}


CP_OPTION cmd_grid [] =		{
					{ "full", CP_NONE, grid_output },
					{ "inner", CP_NONE, grid_output },

					{ NULL, CP_NONE, NULL }
			  	};

/* ---------- */

CP_COMMAND commands [] =	{
					{ "set", cmd_set, NULL, NULL },
					{ "icond", cmd_icond, NULL, NULL },
					{ "grid", cmd_grid, NULL, NULL },

					/* the mnemonic command */
					{ "mnemonic", NULL, mnemonic, NULL },

					/* the continue_if command */
					{ "continue_if", NULL, continue_if, NULL },
					/* the break command */
					{ "break", NULL, NULL, break_now },

		/* commands in the parameters-file that are not variables but they have no meaning in this program */
					{ "slice_output", NULL, NULL, skip_command },
					{ "slice_along", NULL, NULL, skip_command },
					{ "slice_reverse_order", NULL, NULL, skip_command },
					{ NULL, NULL, NULL, NULL }
				};

/* ---------------------------- */
/*
the handlers that use some structures defined above and could not be defined
until the respective structures had been defined. Their prototypes are somewhere
above.
*/


CP_STAT skip_command(int cmd)
{
	Mmprintf(logfile, "Skipping command: %s.\n", commands[cmd].name);
	return(CP_SUCCESS);
}

CP_STAT skip_set_option(int cmd, int opt, _conststring_ value)
{
	Mmprintf(logfile, "Skipping option '%s' for the 'set' command.\n", cmd_set[opt].name);
	return(CP_SUCCESS);
}

/* ---------------------------- */

PP_STAT handle_special(_conststring_ s, int l)
/* custom parse function for pparse, using the CParse command line parsing system */
{
	int i;

	i=CP_runcommand(s, commands, stdout);
	switch(i) {
		case 0: if(should_break) return(PP_BREAK); return(PP_SPECIAL);
		case -1: break;
		case -2:
		case -3:
		case -4: return(PP_ERROR);
	}

	return(PP_DEFAULT);
}

/* ---------------------------- */

_conststring_ format_time(double seconds)
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

_conststring_ format_date(const struct tm * broken_time)
/* Convert the structure 'broken_time' to a string in the format YYYY-MM-DD hh:mm:ss */
{
	static char date_str[64];
	strftime(date_str,64,"%F %H:%M:%S",broken_time);
	return(date_str);
}

/* ---------------------------- */

/* the RK solver service callback - detailed logging facility / on-demand snapshot creation */

int RKService(FLOAT final_snapshot_time, const RK_MPI_S_SOLUTION * const self)
/* This function is only called in the master rank. */
{
	/* output debug information to log file */
	if(debug_logging) {

		/* ................................time elapsed to last snapshot save...+ time elapsed in current snapshot computation */
		double MPIelapsed_time_to_log =	MPIelapsed_time			+ (MPI_Wtime() - MPInew_start);

		double MPIestimated_time_to_next_snapshot = MPIelapsed_time_to_log * ((final_snapshot_time-starting_time) / (self->t-starting_time) - 1.);
		double MPIestimated_time_to_completion = MPIelapsed_time_to_log * ((final_time-starting_time) / (self->t-starting_time) - 1.);

		time(&calendar_time);
		br_time=localtime(&calendar_time);

		fprintf(debug_logfile_ID,
			"%s - step %08ld, t=%10.4" FTC_E ", tau=%10.4" FTC_E ", Elapsed time: %s",
			format_date(br_time),
			self->steps,
			self->t,
			self->h,
			format_time(MPIelapsed_time_to_log)
			);
		/* the fprintf() call must be split as multiple calls to format_time() share the same static buffer */
		fprintf(debug_logfile_ID,", Est. time to snapshot %d (t=%10.4" FTC_E "): %s",
			__snapshot,
			final_snapshot_time,
			format_time(MPIestimated_time_to_next_snapshot)
		);
		/* the command must be split as multiple calls to format_time() share the same static buffer */
		fprintf(debug_logfile_ID,", Est. time to final t=%10.4" FTC_E "): %s\n",
			final_time,
			format_time(MPIestimated_time_to_completion)
		);
		fflush(debug_logfile_ID);
	}

	if(snapshot_trigger) {
		struct stat s;
		/* nonzero return value => computation will be interrupted and snapshot save action will be triggered */
		if(!stat(snapshot_trigger_file,&s)) return(1);
	}

	return(0);
}

/* =========================================================================== */

int main(int argc, char *argv[])
{
	FLOAT tau;			/* initial time step */
	FLOAT tau_min;			/* minimum value of tau for the RK iteration to be considered successful (see RK_MPI_SAsolver.h) */
	FLOAT delta;
	int starting_snapshot=0;	/* this can be overriden by the continue_series option */
	int total_snapshots;
	int q;				/* all-purpose locally-used variable */

	char * com_format = "Intertrack simulation (%s). Time: %" FTC_g;
	char buf[4096];
	char filename[4096];
	_string_ base_name, path;


	/* the sparse system distribution specification arrays for RK_MPI_SAsolver */
	int * chunk_start, * chunk_size;
	FLOAT * chunk_eps_mult;
	int n_chunks;

	/*
	solution cache (used when deploying initial conditions or collecting the data for a snapshot)
	We use a cache for the following reasons:
	- The computational grid can be of any floating point type but the NetCDF datasets exclusively use 'double'.
	- The datasets do not normally contain the auxiliary grid nodes. Transcription from the dataset to
	  the computational grid therefore happens by small chunks placed in a highly non-contiguous pattern.
	  However, invoking a NetCDF read operation for each chunk is inefficient.
	- With a cache, any generalization of the transcription procedure is possible.
	*/
	double * data_cache[VAR_COUNT];

	/*
	messages for errors that may occur in any rank (or, more precisely, only some of them can.
	The others may not, but they are anyway checked collectively using the CheckErrorAcrossRanks()
	function.)
	*/
	char * Common_errors[]=	{
				"Not enough memory to allocate the RK chunk specification array.",
				"Not enough memory to allocate the variables.",
				"Not enough memory to allocate the precalculated data array.",
				"Not enough memory to allocate the cache for variables import/export."
				};

	/* ---------- variable & parameters metadata initialization ---------- */

	InitMetadata();		/* this has to be done only once during the execution of the process */

	initialize_cparser_structs();

	/* ---------- MPI initialization ---------- */


	int alloc_error_code;		/* this is used with a call to CheckErrorAcrossRanks() */

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
		printf("Intertrack FATAL ERROR: Could not initialize MPI.\n");
		/* call this in case the cause for the error is the insufficient level of threading support */
		MPI_Finalize();
		return(2);
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
	MPI_Comm_size(MPI_COMM_WORLD, &MPIprocs);

	MPI_Get_processor_name(MPIprocname, &MPIprocnamelength);

	/*
	NOTE:	The rank 0 execution blocks are not indented.
		Instead, they are indicated by special >>> MASTER <<< markers.
		Remember that variables declared withing such block are
		local to the block !
	*/

	/*
	this array is so small that we don't believe the allocation can fail.

        Since no MPI communication is allowed until the rank mapping (permutation)
        array is set up, we can't tell the other ranks about a failure in this
        allocation. We must rely on the MPI run time environment that should stop
        all ranks if one of the ranks returns a nonzero error code. The error
        message should be redirected to the user's terminal by MPI RTE. (This is
        the only message possibly printed by other rank than the master)
	*/
	if((MPIrankmap=(int *)malloc(MPIprocs*sizeof(int))) == NULL) {
		printf("intertrack FATAL ERROR: Total shortage of memory in REAL rank %d running at %s\n", MPIrank, MPIprocname);
		return(3);
	}

	if(argc>=3) MPImaster=val(argv[2]);			/* set the custom master rank */

	if(MPImaster<0 || MPImaster>=MPIprocs) MPImaster=0;	/* ignore invalid master rank specification */

	BuildRankMap(MPImaster);				/* fill the rank permutation array */

	/* make MPIrank denote the VIRTUAL rank of this process. (Until now, it was the real rank) */
	for(q=0;q<MPIprocs;q++)
		if(MPIrankmap[q]==MPIrank) { MPIrank=q; break; }

	/* from now on, we can test whether this process is master by the condition below: */


	/* ---------- node listing ---------- */


	/* all the processor names are sent to master and listed in the order of the ranks */
/* ####### B E G I N >>> MASTER <<< ####### */ if(MPIrank==0) {

	/* create the logfile buffer - this can not (normally) fail */
	logfile = mopen(65536);

	Mmprintf(logfile,
		"\nWelcome to INTERTRACK MPI/OpenMP Hybrid-Parallel Solver\n"
		"------------------------------------------------\n"
		"Running %d rank%s, MASTER (virtual) rank 0 on : %s\n"
		"OpenMP threading support is %s. Number of threads: %d\n",
		MPIprocs, (MPIprocs>1?"s":""), MPIprocname, OMP_support ? "ON" : "OFF", OMP_threads);

	for(q=1;q<MPIprocs;q++) {
		/* for MPI_Recv(), we specify the maximum, not the exact number of elements to be received */
		MPI_Recv(buf, 256, MPI_CHAR, MPIrankmap[q], MPIMSG_PROCNAME, MPI_COMM_WORLD, &MPIstat);
		Mmprintf(logfile, "Rank %d running on : %s\n", q, buf);
	}

	if(MPImaster!=0) Mmprintf(logfile, "\nRank translation is active. The REAL master rank is rank %d\n", MPImaster);
	Mmprintf(logfile, "------------------------------------------------\n\n");

/* ####### E N D >>> MASTER <<<, B E G I N >>> OTHER <<< ####### */ } else {

	/* "+1" makes the null terminator of the string be transferred as well */
	MPI_Send(MPIprocname, MPIprocnamelength+1, MPI_CHAR, MPIrankmap[0], MPIMSG_PROCNAME, MPI_COMM_WORLD);

/* ####### E N D >>> OTHER <<< ####### */ }


	/* ---------- preparation ---------- */

	/*
	The value returned by time() changes every second. In order not to initialize the PRNG with the same
	value on all ranks, which would be VERY UNDESIRABLE, we add a bit of stuff that should distinguish
	between ranks. Of course, time is a bit different if we run on different computers, and
	multiplication by this big prime number should avoid accidental match of the calculated values.

	*/
	srand(time(NULL)+101009*MPIrank);
	/* also initialize the stand-alone PRNG used in the expression evaluator 'rand' function */
	ev_random(time(NULL)+101009*MPIrank);
	/*
	NOTE:	You can re-initialize the stand-alone PRNG by using the rand(x) function in the parameter file,
		where x is nonzero. However, this only affects rank 0. If you then use the rand() function in
		the initial condition formula, you must realize that the ranks other than 0 will still use a
		time-based PRNG seed and your calculation will give different results each time it is run!
	*/
	install_evaluator_extensions();

/* ####### B E G I N >>> MASTER <<< ####### */ if(MPIrank==0) {

	Mmprintf(logfile,
		"/**************************************************************\\\n"
		"*                                                              *\n"
		"*                     I N T E R T R A C K                      *\n"
		"*                                                              *\n"
		"*      High Precision Phase Interface Evolution Simulator      *\n"
		"*                      (\"H i P P I E S\")                       *\n"
		"*                                                              *\n"
		"\\**************************************************************/\n\n"

		"InterTrack Version %s.%s, Build %d (%s)\n"
		"(C) 2005-2011, 2015, 2021-2024 Pavel Strachota\n"
		"****************************************************************\n\n"
		"syntax: intertrack param_file [master_rank] [ubound_list]\n\n",
		AVS_MajorVersion(), AVS_MinorVersion(), AVS_Build(), AVS_VersionInfo() );

	if(argc<2) {
		Mmprintf(logfile, "Not enough arguments.\n");
		HaltAllRanks(0);
	}

	/* use NetCDF4/HDF5 format by default which removes all data size constraints */
	if(nc_set_default_format(NC_FORMAT_NETCDF4, NULL) != NC_NOERR)
		Mmprintf(logfile, "WARNING: NetCDF4 format not available. Falling back to NetCDF classic format.\n");

	/* ---------- batch processing initiation ---------- */

	/* define a variable that can be used in the parameter file - the number of processes */
	ev_def_var("MPIprocs", MPIprocs);

	/* make the batch processing variable names available (set them all to 1) - even when not in batch mode */
	{
		char VarNameBuf[4];
		for(q=0;q<MAX_NESTED_LOOPS;q++) {
			set(VarNameBuf, "i");
			add(VarNameBuf, tostring(q+1));
			ev_def_var(VarNameBuf, 1);
		}
		ev_def_var("loopIter",1);
	}

	if(argc>=4) {
		/* parse the loop upper bound list supplied in the last command line argument */

		int next;
		int maxUb_estimate=10;
		_string_ Ubound_list=argv[3];

		for(loopN=0;loopN<MAX_NESTED_LOOPS;loopN++) {
			next=instr(Ubound_list, ",", 1, SENSITIVE);
			if(next) Ubound_list[next-1]=0;		/* separate the substring without copying it anywhere else */

			if((loopUbound[loopN]=val(Ubound_list)) <= 0) {
				Mmprintf(logfile, "\nError: Upper bound of loop no. %d must be a positive integer.\n", loopN+1);
				HaltAllRanks(1);
			}

			if(!next) { loopN++; break; }	/* there is no next value */
			Ubound_list+=next;
		}
		/* having passed the above loop, we know that there's at least one loop successfully defined */

		for(q=0;q<loopN;q++) {
			loopVarMnemonic[q]=NULL;	/* initialize loop-variable-value mnemonic strings */
			loopI[q]=1;			/* initialize all loop control variables by 1 */
			loopTotal*=loopUbound[q];	/* calculate the total number of iterations */
		}
		loopI[loopN-1]=0;	/* this is because we make the vars incrementation BEFORE the calculation */

		/* figure out the number of digits of the largest loop upper bound (don't use logarithm for that) */
		loopUb_digits=1;
		for(q=0;q<loopN;q++)
			while(loopUbound[q]>=maxUb_estimate) { loopUb_digits++; maxUb_estimate*=10; }

		Mmprintf(logfile, "\nENTERING BATCH PROCESSING MODE: %d loop%s defined, %d iterations in total.\n", loopN, (loopN>1)?"s":_NOSTRING, loopTotal);
		cal_start_batch_time=time(&calendar_time);
	}

/* ####### E N D >>> MASTER <<< ####### */ }

	/* ---------- the beginning of the batch processing loop ---------- */

	/*
	the body of the following batch processing loop is not indented, since the body is almost the whole
	rest of the main() function. If there is no batch processing required, this loop has only one iteration.
	*/
	while(1) {	/* THE BATCH PROCESSING LOOP */

/* ####### B E G I N >>> MASTER <<< ####### */ if(MPIrank==0) {

	if(loopN) {
		char VarNameBuf[4];

		/* set the variables for the next iteration: */
		/*
		1) find the innermost variable available for incrementation:
		Note that the innermost variable is iN and the outermost one is i1.
		*/
		for(q=loopN-1;q>=0;q--)
			if(loopI[q]<loopUbound[q]) break;
		if(q<0) {						/* the loops are completed */
			time(&calendar_time);
			Mmprintf(logfile, "\nBATCH PROCESSING COMPLETED IN:	%s\n", format_time(difftime(calendar_time, cal_start_batch_time)));

			if(debug_logging) fclose(debug_logfile_ID);

			HaltAllRanks(0);
		}
		loopI[q]++;		/* 2) increment the q-th variable = the next iteration of the q-th loop */
		while(q<loopN-1) loopI[++q]=1;		/* 3) all inner loops below q are restarted */
		loopIter++;
		loopContinue=0;

		Mmprintf(logfile,
			"\nSTARTING ITERATION %d OF %d:\n"
			"----------------------------------------------------------------------\n",
			loopIter, loopTotal);
		for(q=0;q<loopN;q++) {
			set(VarNameBuf, "i");
			add(VarNameBuf, tostring(q+1));
			ev_def_var(VarNameBuf, loopI[q]);
			Mmprintf(logfile, "%s = %d\n", VarNameBuf, loopI[q]);
		}
		/*
		make the current iteration number available in the variable loopIter. This makes it easy
		to e.g. skip the given number of iterations by the 'continue_if' command.
		*/
		ev_def_var("loopIter",loopIter);
		Mmprintf(logfile, "\n");
	}

	/* ---------- parameters file processing ---------- */

	should_break=0;
	if(pparse(argv[1], handle_special, stdout)) HaltAllRanks(1);

	if(loopN && loopContinue) { Mmprintf(logfile, "Iteration %d skipped. Continue...\n", loopIter); continue; }

	if(len(out_file)==0) { Mmprintf(logfile, "Fatal error: Output file not specified.\nStop.\n"); HaltAllRanks(1); }
	if(len(pproc_script)>0 && system(NULL)==0) {
		Mmprintf(logfile, "Fatal error: Command processor for postproc script not defined. Can not use postprocessing on this system.\nStop.\n");
		HaltAllRanks(1);
	}

	/* extract the base name of the output files (useful for batch processing only) */
	/* NOTE: File names are loaded redundantly in each iteration, which is however just a cosmetic drawback. */
	base_name=path=out_file;
	while((q=instr(base_name, "/", 1, SENSITIVE))>0) base_name+=q;

	if(loopN) {
		int i;

		/* create the variable values string */
		char VarValueBuf[128];
		_string_ mnemo, vvb;
		int nextMnemo;

		set(loopVarString, "_");
		for(q=0;q<loopN;q++) {

			i=0;
			if((mnemo=loopVarMnemonic[q])!=NULL) {		/* separate the loopI[q]-th mnemonic */
				mnemo--;
				nextMnemo=1;
				while(*(++mnemo))
					if(*mnemo==' ' || *mnemo==0x9) nextMnemo=1;
					else {
						if(nextMnemo) {
							i++; if(i==loopI[q]) break;
						}
						nextMnemo=0;
					}
			}
			if(i==loopI[q]) {	/* repeat the condition here --> this allows us to handle both cases:
						  insert the number instead of the mnemonic if 1) there are no mnemonics
						  for the q-th variable or 2) there are not enough mnemonics for the
						  q-th variable */
				vvb=VarValueBuf; *vvb='_'; vvb++;
				while(*mnemo!=' ' && *mnemo!=0x9 && *mnemo!=0)
					*(vvb++)=*(mnemo++);
				*vvb=0;
			} else
				sprintf(VarValueBuf, "_%0*d", loopUb_digits, loopI[q]);
			add(loopVarString, VarValueBuf);
		}

		/* create the directory for output */
		sprintf(filename, "%s%s", out_file, loopVarString);
		if(mkdir(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
			if(errno != EEXIST) {
				Mmprintf(logfile, "Batch mode error: Can't create output directory %s.\nStop.\n", filename);
				HaltAllRanks(1);
			}
		}

	/* ---------- parameters setting ---------- */


	Mmprintf(logfile, "\nSetting geometry parameters:\n");

	L1=evchk("L1");
	Mmprintf(logfile, "Domain base width: %" FTC_g "\n", L1);

	L2=evchk("L2");
	Mmprintf(logfile, "Domain base height: %" FTC_g "\n", L2);

	L3=evchk("L3");
	Mmprintf(logfile, "Domain depth: %" FTC_g "\n", L3);

	/* ------- */

	Mmprintf(logfile, "\nSetting model parameters:\n");

	for(q=0;q<PARAM_INFO_SIZE;q++) {
		if(param_info[q].index < 0) {
			Mmprintf(logfile, "\n--- %s ---\n\n", param_info[q].description);
			continue;
		}
		model_parameters[param_info[q].index] = evchk(param_info[q].name);
		/* modify the indentation as desired (no automatic "longest description" search is performed to indent correctly) */
		Mmprintf(logfile,
			"%-70s : %-23s = %" FTC_g "\n",
			param_info[q].description,
			param_info[q].name,
			model_parameters[param_info[q].index]
			);
	}

	/* all variables must be set up even though they may not be necessary in some models */

	Mmprintf(logfile, "\nSetting numerical solution parameters:\n");

	calc_mode=ToInt(evchkD("calc_mode", 0));
	Mmprintf(logfile, "Calculation mode: %d\n", calc_mode);

	/* Note: if icond_mode==1, the grid dimensions will be checked after examining the initial conditions dataset */

	n1=ToInt(evchkD("n1",0));
	Mmprintf(logfile, "Grid X inner nodes: %d\n", n1);
	if(!icond_mode && n1<1) {
		Mmprintf(logfile, "Error: The grid width must be at least 1.\nStop.\n");
		HaltAllRanks(2);
	}

	n2=ToInt(evchkD("n2",0));
	Mmprintf(logfile, "Grid Y inner nodes: %d\n", n2);
	if(!icond_mode && n2<1) {
		Mmprintf(logfile, "Error: The grid height must be at least 1.\nStop.\n");
		HaltAllRanks(2);
	}

	total_n3=ToInt(evchkD("n3",0));
	Mmprintf(logfile, "Grid Z inner nodes: %d\n", total_n3);
	/*
	The grid must have at least as many rows (in the y direction) so that the height of the smallest block
	is greater than or equal to the thickness of the boundary condition layer. Boundary conditions exchange
	among multiple blocks is not supported.
	In order to satisfy this requirement, you can do one of the following:
	- increase the grid height
	- decrease the number of processes. You can explicitly control the number of processes in
	  the MPI universe via the -np flag of the mpirun command. For example, type
		  mpirun -np 3 intertrack [program parameters]
	  to run 3 instances of intertrack.
	*/
	if(!icond_mode && total_n3/MPIprocs<bcond_thickness) {
		Mmprintf(logfile, "Error: The grid depth is too small for parallelization on %d ranks.\nStop.\n", MPIprocs);
		HaltAllRanks(2);
	}

	Mmprintf(logfile, "Boundary conditions auxiliary node layer thickness: %d\n", bcond_thickness);

	total_snapshots=ToInt(evchk("saved_files"));
	Mmprintf(logfile, "Number of snapshots (the zeroth snapshot is the init. cond.): %d\n", total_snapshots);

	tau=evchk("tau");
	Mmprintf(logfile, "Initial time step: %" FTC_g "\n",tau);

	final_time=evchk("final_time");
	Mmprintf(logfile, "Final time : %" FTC_g "\n", final_time);

	delta=evchk("delta");
	Mmprintf(logfile, "Runge-Kutta-Merson solver tolerance (delta) : %" FTC_g "\n", delta);

	tau_min=evchkD("tau_min",0.0);
	Mmprintf(logfile, "Time step lower bound for RKM iteration to be controlled by delta : %" FTC_g "\n", tau_min);

	Mmprintf(logfile, "Comment: %s\n", comment);

	/* ---------- Input file check and dimension adjustment ---------- */

	starting_time = 0;

	/* check initial conditions dataset if initial conditions are to be loaded from file */
	if(icond_mode) {
		int icond_dataset_ID;

		Mmprintf(logfile, "\nChecking availability of the initial conditions input dataset ...\n");

		if(nc_open (icond_file, NC_NOWRITE, &icond_dataset_ID) != NC_NOERR) {
			Mmprintf(logfile, "Error: Can not open the initial conditions dataset.\nStop.\n");
			HaltAllRanks(1);
		} else {

			char * dimensions[3] = {"n1", "n2", "n3"};
			int * dim_lengths[3] = { &n1, &n2, &total_n3 };
			int dim_ID;
			int var_ID;
			size_t icond_dim_length;

			/* check if the file content is correct... */

			for(q=0;q<VAR_COUNT;q++)
				if( nc_inq_varid(icond_dataset_ID, variable[q].name, &var_ID) != NC_NOERR ) {
					Mmprintf(logfile, "Error: The given NetCDF dataset '%s' does not contain the initial condition.\nStop.\n", icond_file);
					nc_close(icond_dataset_ID);
					HaltAllRanks(1);
				}

			/* initial conditions NetCDF dataset open successfully */

			/*
			The following code checks the compatibility of the initial conditions dataset with the current settings.
			The existence of the n1, n2, n3 dimensions in the dataset is checked and their values are compared to
			those already defined in the n1, n2, and total_n3 variables, respectively. Then:
				a) their values are updated if they were equal to 0
				b) an error is returned if they were already defined differently (but not as 0)
			*/

			Mmprintf(logfile, "Dataset opened. Checking dimensions:");
			for(q=0;q<3;q++) {
				Mmprintf(logfile, " %s=", dimensions[q]);
				if(nc_inq_dimid(icond_dataset_ID, dimensions[q], &dim_ID) != NC_NOERR) {
					Mmprintf(logfile, "? NOT FOUND!\nError: Invalid NetCDF dataset.\nStop.\n");
					nc_close(icond_dataset_ID);
					HaltAllRanks(1);
				}
				nc_inq_dimlen(icond_dataset_ID, dim_ID, &icond_dim_length);

				if(icond_dim_length!=*dim_lengths[q])
					if(*dim_lengths[q]==0) {
						Mmprintf(logfile, "%lu(STORED)", icond_dim_length);
						*dim_lengths[q]=icond_dim_length;
					} else {
						Mmprintf(logfile, "%lu MISMATCH!\nError: %s has been previously defined as %d.\nStop.\n", icond_dim_length, dimensions[q], *dim_lengths[q]);
						nc_close(icond_dataset_ID);
						HaltAllRanks(1);
					}
				else Mmprintf(logfile, "%lu(OK)", icond_dim_length);
			}
			Mmprintf(logfile, ".\n");

			if(continue_series) {	/* continue from the snapshot and time level given in the initial conditions dataset */
				double starting_time_d;
				double final_time_d;
				double tau_d;

				Mmprintf(logfile,	"\nSeries continuation mode has been requested.\n"
							"Obtaining settings from the initial condition file:\n");

				if(
					( nc_get_att_int(icond_dataset_ID, NC_GLOBAL, "snapshot", &starting_snapshot) != NC_NOERR )
				||	( nc_get_att_int(icond_dataset_ID, NC_GLOBAL, "total_snapshots", &total_snapshots) != NC_NOERR )
				||	( nc_get_att_double(icond_dataset_ID, NC_GLOBAL, "t", &starting_time_d) != NC_NOERR )
				||	( nc_get_att_double(icond_dataset_ID, NC_GLOBAL, "final_time", &final_time_d) != NC_NOERR )
				||	( nc_get_att_double(icond_dataset_ID, NC_GLOBAL, "tau", &tau_d) != NC_NOERR )
				) {
					Mmprintf(logfile, 	"Error: The initial conditions file is corrupted and does not contain the required information.\n"
								"Please remove the 'continue_series' option and restart the simulation.\nStop.");
					nc_close(icond_dataset_ID);
					HaltAllRanks(1);
				}
				starting_time = starting_time_d; final_time = final_time_d; tau = tau_d;

				Mmprintf(logfile, "Starting snapshot: %d\n", starting_snapshot);
				Mmprintf(logfile, "Starting time: %" FTC_g "\n", starting_time);
				Mmprintf(logfile, "Initial time step override: %" FTC_g "\n", tau);
				Mmprintf(logfile, "Final time override: %" FTC_g "\n", final_time);
				Mmprintf(logfile, "Total number of snapshots override: %d\n", total_snapshots);
			}

			nc_close(icond_dataset_ID);
		}


		/*
		check total_n3 with respect to the number of ranks (in the formula-based initial conditions mode, this is has already
		been done before). n1 and n2 do not need to be checked as it is sure they are positive: As the dimensions of the initial
		condition are always positive, n1 and n2 have been either
						1) left as their value already corresponed to the dataset, or
						2) changed from zero to the actual dimension of the dataset, or
						3) reported as invalid (mismatch with the dataset - this includes negative values).
		*/
		if(total_n3/MPIprocs<bcond_thickness) {
			Mmprintf(logfile, "Error: The grid depth is too small for parallelization on %d ranks.\nStop.\n", MPIprocs);
			HaltAllRanks(2);
		}

	} else if(continue_series)
		Mmprintf(logfile, "Warning: continue_series is only meaningful when the initial conditions are loaded from file.\n");

/* ####### E N D >>> MASTER <<< ####### */ }

	/* ---------- initial error check ---------- */

	/*
	Exit the other ranks if there was an error in the initial parameter setup. Of course, the master rank
	reaches there only if no error has occurred. Otherwise, it exits through a call to exit() from within
	the HaltAllRanks() function.
	*/

	MPI_Bcast(&MPIcmd, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);
	/*
	of course, MPICMD_HALT can occur here only in ranks other than master, which terminates
	with a call to exit() from within HaltAllRanks()
	*/
	if(MPIcmd==MPICMD_HALT) {
		int code;	/* in case of termination, the master rank also broadcasts the error code */
		MPI_Bcast(&code, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

		/* the barrier is needed here since the broadcast from the master rank may be buffered
		   and don't want to quit the master before we ensure that all ranks "acknowledge" the
		   command. That's why MPI_Barrier is used in HaltAllRanks(). In order to pass the
		   barrier, all other ranks also have to call MPI_Barrier(); */
		MPI_Barrier(MPI_COMM_WORLD);

		MPI_Finalize();
		return(code);
	}


	/* ---------- parameter broadcast and calculation initialization ---------- */

/* ####### B E G I N >>> MASTER <<< ####### */ if(MPIrank==0) {

	/* create and initialize the calculation parameters structure */
	MPI_Calculation MPIcalc = {
					L1, L2, L3,
					n1, n2, total_n3,

					calc_mode,
					icond_mode,
					grid_IO_mode
				};

	/* export model parameters */
	for(q=0;q<PARAM_COUNT;q++)	MPIcalc.model_parameters[q] = model_parameters[q];

	if(!icond_mode)
		for(q=0;q<VAR_COUNT;q++)
			set(MPIcalc.icond_formula[q], icond_formula[q]);

	Mmprintf(logfile, 	"\nInitializing the computation:\n"
				"-----------------------------\n");
	AUX_time2 = MPI_Wtime();

	/* broadcast the calculation parameters structures to all ranks */
	MPI_Bcast(&MPIcalc, sizeof(MPIcalc), MPI_BYTE, MPIrankmap[0], MPI_COMM_WORLD);

/* ####### E N D >>> MASTER <<<, B E G I N >>> OTHER <<< ####### */ } else {

	MPI_Calculation MPIcalc;
	MPI_Bcast(&MPIcalc, sizeof(MPIcalc), MPI_BYTE, MPIrankmap[0], MPI_COMM_WORLD);

	/* restore domain & computational domain dimensions */
	L1=MPIcalc.L1; L2=MPIcalc.L2; L3=MPIcalc.L3;
	n1=MPIcalc.n1; n2=MPIcalc.n2; total_n3=MPIcalc.total_n3;
	
	/* restore simulation control parameters */
	calc_mode = MPIcalc.calc_mode;
	icond_mode = MPIcalc.icond_mode;
	grid_IO_mode = MPIcalc.grid_IO_mode;

	/* restore model parameters */
	for(q=0; q<PARAM_COUNT; q++)	model_parameters[q] = MPIcalc.model_parameters[q];

	if(!icond_mode)
		for(q=0;q<VAR_COUNT;q++)
			set(icond_formula[q], MPIcalc.icond_formula[q]);

	tau=1;	/* the initial time step is ignored in ranks other than 0 */

/* ####### E N D >>> OTHER <<< ####### */ }

	/* Set the grid block information variables */

	N1 = n1 + 2*bcond_thickness;
	N2 = n2 + 2*bcond_thickness;
	total_N3 = total_n3 + 2*bcond_thickness;

  	n3 = total_n3/MPIprocs;
	first_row = MPIrank*n3;
	/* the remaining rows are distributed one by one among the ranks, starting from rank 0 */
	if(MPIrank < total_n3%MPIprocs) {
		n3++;
		first_row+=MPIrank;
	} else
		first_row+=total_n3%MPIprocs;

	N3 = n3 + 2*bcond_thickness;

	rowsize = N1*N2;
	bcond_size = bcond_thickness*rowsize;

	subgridSIZE = rowsize * N3;
	subgridSize = rowsize * n3;
	n_chunks = VAR_COUNT*n2*n3;		/* there are n2*n3 chunks for each variable */

	/* set subgridsize depending on the value of grid_IO_mode */
	if(grid_IO_mode) subgridsize = n1*n2*n3;
	else subgridsize = rowsize * (n3 + ((MPIrank==0) + (MPIrank==MPIprocs-1)) * bcond_thickness);


/* #### MEMORY ALLOCATION / INITIALIZATION BLOCK  S T A R T #### */ {

	if(MPIrank==0) Mmprintf(logfile, "Allocating memory.\n");

	alloc_error_code=0;

	if( (chunk_start=(int *)malloc(n_chunks*sizeof(int))) == NULL ) alloc_error_code=1;
	else if( (chunk_size=(int *)malloc(n_chunks*sizeof(int))) == NULL ) alloc_error_code=1;
	else if( (chunk_eps_mult=(FLOAT *)malloc(n_chunks*sizeof(FLOAT))) == NULL ) alloc_error_code=1;
	/* computational grid */
	else if( (solution=(FLOAT *)malloc(VAR_COUNT*subgridSIZE*sizeof(FLOAT))) == NULL ) alloc_error_code=2;
	else if(AllocPrecalcData()) alloc_error_code=3;
	/*
	for the result collection, the cache of 'subgridsize' elements is sufficient both for sending and receiving.
	This is because regardless of the value of grid_IO_mode, subgridsize in the master rank is always at
	least as large as in any other rank.
	*/
	else
		for(q=0;q<VAR_COUNT;q++)
			if( (data_cache[q] = (double *)malloc(subgridsize*sizeof(double))) == NULL) { alloc_error_code=4; break; }

	/* check for allocation errors */
	CheckErrorAcrossRanks(alloc_error_code, 1, Common_errors);

/* #### MEMORY ALLOCATION / INITIALIZATION BLOCK  E N D #### */ }


	/* initial conditions generation (each rank initializes its own portion of the grid) */
	switch(icond_mode) {

		case 0:
		/*
		generate initial conditions based on the given formulae. Syntax errors are caught here,
		(this happens in the formula parsing phase) but not the math errors. Remember that
		variables defined in the parameter file generally can NOT be used in the formula as they
		are available in rank 0 only. Variables that can be used are x,y,z and they assume the
		values from 0 to L1,L2,L3 respectively, so that the value of the initial condition
		at the exact geometrical position of each grid node can be determined. An alternative set
		of variables spanning the range (0,1) instead is _x,_y,_z.

		In addition to the variables mentioned below, the names of the physical quantities may
		be used in the formulas for other physical quantities' initial conditions. Each such name
		evaluates to the initial value of the corresponding quantity at the location x.
		MetaSim uses a multi-pass algorithm to ensure that the name of any quantity can be used in the
		i.c. formula for any other quantity.
		*/

		{
			char local_string_pool[(VAR_COUNT+1)*256];
			char * icond_eval_errors[VAR_COUNT+1];

			int icond_eval_errcode=0;
			int i, j, k, qq;
			size_t idx;
			double _x,_y,_z;
			int x_index, _x_index;
			int y_index, _y_index;
			int z_index, _z_index;
			int q_index[VAR_COUNT];

			/* multi-pass evaluation management data */
			int pass_no = 1;
			int progress_made = 0;		/* set to nonzero if some progress has been made in the current
							   pass (initial condition for at least one quantity successfully
							   defined) */
			int completed_count = 0;
			int completed[VAR_COUNT];

			FLOAT * var;

			/* assemble the error messages for the individual physical quantities */
			for(q=0;q<VAR_COUNT;q++) {
				icond_eval_errors[q] = local_string_pool + q*256;
				sprintf(icond_eval_errors[q],"Syntax error in initial condition formula for %s (%s).", variable[q].name, variable[q].description);
			}

			/*
			reset the expression evaluator in all ranks (removes all variable definitions added during
			parameters file processing in rank 0) and define the model parameters as variables to make
			them available for use in the initial conditions formulas. This unifies the set of available
			symbols in the evaluators IN ALL RANKS. The numerical solution parameters won't be accessible.
			*/
			ev_reset();

			/*
			make the batch processing loop variables accessible in the initial condition formulas
			(the loopIter variable is not defined here as it's not necessary)
			*/
			{
				char VarNameBuf[4];

				for(q=0;q<MAX_NESTED_LOOPS;q++) {
					set(VarNameBuf, "i");
					add(VarNameBuf, tostring(q+1));
					ev_def_var(VarNameBuf, (q<loopN) ? loopI[q] : 1);	/* always define all i's; all unused ones are set to 1 */
				}
			}

			/* make the model parameters accessible in the initial condition formulas */
			ev_def_var("L1", L1);
			ev_def_var("L2", L2);
			ev_def_var("L3", L3);

			for(q=0;q<PARAM_INFO_SIZE;q++) if(param_info[q].index >= 0)
				ev_def_var(param_info[q].name, model_parameters[param_info[q].index]);


			/* x, y, z will span the range (0;L1), (0;L2), (0;L3) respectively */
			ev_def_var("x", 0); x_index=ev_get_index("x");
			ev_def_var("y", 0); y_index=ev_get_index("y");
			ev_def_var("z", 0); z_index=ev_get_index("z");

			/* _x, _y, _z will span the range (0;1) (relative position in the domain along the respective direction) */
			ev_def_var("_x", 0); _x_index=ev_get_index("_x");
			ev_def_var("_y", 0); _y_index=ev_get_index("_y");
			ev_def_var("_z", 0); _z_index=ev_get_index("_z");

			/*
			calculate initial condition value at each node of the computation grid
			Initial conditions for u and p are calculated successively in order to
			avoid using multiple evaluator instances at the same time. The initial
			condition is set in the interior of the grid (excluding the auxiliary
			nodes) only.
			----------------------------------------------------------------------
			*/

			for(q=0;q<VAR_COUNT;q++) completed[q]=0;

			do {
				if(MPIrank==0) Mmprintf(logfile, "\n--- Initial condition setup: PASS %d ---\n",pass_no);
				icond_eval_errcode = 0;
				progress_made = 0;

				/* iterate over those variables that have not yet been completed */
				for(q=0;q<VAR_COUNT;q++) if(!completed[q]) {

					if(ev_parse(icond_formula[q])) {
						icond_eval_errcode=q+1;
						/*
						skip a formula with a syntax error and hope that the error is
						caused by using a name of the physical quantity that has not yet
						been initialized. In that case, the parsing will succeed in the
						next pass.
						*/
						continue;
					}

					if(MPIrank==0) Mmprintf(logfile, "Calculating initial condition for %s (%s).\n", variable[q].name, variable[q].description);

					var = VAR(solution,q);
					idx = bcond_size;
					for(k=0;k<n3;k++) {
						_z = (0.5+k+first_row) / total_n3;
						ev_set_var_value(_z_index, _z);
						ev_set_var_value(z_index, L3*_z);
						idx += bcond_thickness*N1;
						for(j=0;j<n2;j++) {
							_y = (0.5+j) / n2;
							ev_set_var_value(_y_index, _y);
							ev_set_var_value(y_index, L2*_y);
							idx += bcond_thickness;
							for(i=0;i<n1;i++) {
								_x = (0.5+i) / n1;
								ev_set_var_value(_x_index, _x);
								ev_set_var_value(x_index, L1*_x);

								/*
								From the second pass onward, also update the evaluator variables corresponding
								to the already defined quantities so that they refer to the values
								at the current location (x,y,z).
								
								Skipping this in the first pass means that this computationally costly
								part is only performed if there is at least one formula that refers to the
								other variables.
								*/
								if(pass_no>1)
									for(qq=0;qq<VAR_COUNT;qq++)
										if(completed[qq])
											ev_set_var_value(q_index[qq], VAR(solution,qq)[idx]);

								/* evaluate the initial condition */
								var[idx++] = ev_evaluate();
							}
							idx += bcond_thickness;
						}
						idx += bcond_thickness*N1;
					}

					completed[q] = 1;
					progress_made = 1;
					completed_count++;

				}

				/*
				make all quantities that have been successfully calculated available as variables
				in the expression evaluator. For those already defined in the previous pass, the
				variable index remains the same.
				*/
				for(qq=0;qq<VAR_COUNT;qq++)
					if(completed[qq]) {
						ev_def_var(variable[qq].name, 0);
						q_index[qq] = ev_get_index(variable[qq].name);
					}

				pass_no++;

			} while(progress_made && completed_count<VAR_COUNT);
			/*
			if all quantities have been initialized correctly, icond_eval_errcode should
			be zero. Otherwise, it will indicate the number of the last quantity
			whose i.c. formula contained an (unrecoverable) syntax error
			*/
			CheckErrorAcrossRanks(icond_eval_errcode, 1, icond_eval_errors);

			/* ---------------------------------------------------------------------- */
			ev_reset();	/* prepare the expression evaluator for the next iteration in batch processsing mode */
		}
		break;

		case 1:
		/* load the initial conditions from file in the master rank and distribute it to the other ranks */

/* ####### B E G I N >>> MASTER <<< ####### */ if(MPIrank==0) {

			int icond_dataset_ID;
			int var_ID[VAR_COUNT];

			/*
			variables sequentially assuming the same value as the correspoding ones in the receiving ranks
			(without the "send_" prefix)
			*/
			int send_rank;
			int send_n3;
			int send_first_row;

			/* we don't need to check for errors now since evrything has been already examined */
			nc_open(icond_file, NC_NOWRITE, &icond_dataset_ID);

			Mmprintf(logfile, "Loading the initial conditions from file:\n");
			for(q=0;q<VAR_COUNT;q++)
				nc_inq_varid(icond_dataset_ID, variable[q].name, var_ID+q);

			for(send_rank=MPIprocs-1;send_rank>=0;send_rank--) {

				/* calculate the n3, first_row variables as they are in rank q: */
  				send_n3 = total_n3/MPIprocs;
				send_first_row = send_rank*send_n3;
				if(send_rank < total_n3%MPIprocs) {
					send_n3++;
					send_first_row += send_rank;
				} else
					send_first_row += total_n3%MPIprocs;

				/* load the data into u_/p_cache (which may be larger than is necessary here if grid_IO_mode==0) */
				{
					/*
					prepare the data subgrid starting corner and dimensions, as required for the call to
					the nc_get_vara_double() function. For more information, see the NetCDF documentation.
					*/
					size_t nc_start[3] = { send_first_row, 0, 0 };
					size_t nc_count[3] = { send_n3, n2, n1 };

					Mmprintf(logfile, "Reading block %d ... ", q); fflush(stdout);
					AUX_time = MPI_Wtime();
					for(q=0;q<VAR_COUNT;q++)
						nc_get_vara_double(icond_dataset_ID, var_ID[q], nc_start, nc_count, data_cache[q]);
					Mmprintf(logfile, "Done in %s", format_time(MPI_Wtime()-AUX_time));
				}

				if(send_rank!=0) {
					Mmprintf(logfile, ". Sending data to rank %d ... ", send_rank); fflush(stdout);
					AUX_time = MPI_Wtime();
					for(q=0;q<VAR_COUNT;q++)
						MPI_Send(data_cache[q], n1*n2*send_n3, MPI_DOUBLE, MPIrankmap[send_rank], MPIMSG_SOLUTION+q, MPI_COMM_WORLD);
					Mmprintf(logfile, "Done in %s", format_time(MPI_Wtime()-AUX_time));
				}

				Mmprintf(logfile, "\n");
			}

			nc_close(icond_dataset_ID);

/* ####### E N D >>> MASTER <<<, B E G I N >>> OTHER <<< ####### */ } else {

			/* receive the portion of the initial condition dataset into data_cache[..] */
			for(q=0;q<VAR_COUNT;q++)
				MPI_Recv(data_cache[q], n1*n2*n3, MPI_DOUBLE, MPIrankmap[0], MPIMSG_SOLUTION+q, MPI_COMM_WORLD, &MPIstat);

/* ####### E N D >>> OTHER <<< ####### */ }

			/* case 1 (initial conditions from file) - final phase: transcribe data from u_/p_cache to u/p */
			{
				int i, j, k;
				double * cache_ptr;
				FLOAT * ptr;

				/*
				Transcribe the cache to the computational grid. Unlike in the snapshot saving
				procedure in the master rank, here we always transcribe the inner grid only.
				No more complex memory re-organization happens (see ../megiddo/megiddo.c for
				an example of such case).
				-----------------------------------------------------------------------------
				*/
				if(MPIrank==0) Mmprintf(logfile, "Initializing solution arrays.\n");
				for(q=0;q<VAR_COUNT;q++) {
					cache_ptr = data_cache[q];
					for(k=0;k<n3;k++)
						for(j=0;j<n2;j++) {
							ptr = VAR(solution,q) + (k+bcond_thickness) * rowsize + (j+bcond_thickness) * N1 + bcond_thickness;
							for(i=0;i<n1;i++) *(ptr++)=*(cache_ptr++);
						}
				}
			}
	} /* end switch(icond_mode) */

	/* ---------- the calculation itself and snapshots saving ---------- */

	RK_RightHandSide (*mf)();			/* which right hand side to use (which r.h.s. meta-pointer) */

	/*
	the following choice of the right hand side meta-pointer is final, it won't change during the
	calculation. However, each meta-pointer may alter the actual right hand side version in the
	course of the RK solver progress. The versions to alter all belong to the same type with respect
	to block position (single, top, middle, bottom). They only differ in the discretization scheme used.
	On the other hand, at a given time, all ranks must use the same discretization scheme (but a
	different r.h.s version with respect to the block position).
	*/
	if(MPIrank==0)
		if(MPIprocs==1) mf=mf_single;		/* single process ... a right hand side with no communication */
		else mf=mf_bottom;			/* right hand side for the bottom block */
	else if(MPIrank==MPIprocs-1) mf=mf_top;		/* right hand side for the top block */
	else mf=mf_middle;				/* right hand side for a middle block */


	/*
	initialize the RK sparse system distribution arrays, so that the chunks cover exactly the interior of the grid.
	The chunk structure covers all defined variables. Including the exterior auxiliary nodes,
	these grids follow in memory one after another. From the point of view of the RK solver, their
	ordering is irrelevant.
	*/
	{
		int j, k;
		int c=0;	/* chunk number - grows sequentially as we define chunks for all variables

		/* 1. temperature field chunks */
		for(q=0;q<VAR_COUNT;q++)
			for(k=0;k<n3;k++)
				for(j=0;j<n2;j++) {
					chunk_start[c]		= q*subgridSIZE + (k+bcond_thickness)*rowsize + (j+bcond_thickness)*N1 + bcond_thickness;
					chunk_size[c]		= n1;
					chunk_eps_mult[c]	= 1.0;
					c++;
				}
	}


	RK_MEM_DIST mem_dist = { n_chunks, chunk_start, chunk_size, chunk_eps_mult };

	/* definition of the system solution structure */
	RK_MPI_S_SOLUTION eqSystem = {
		&mem_dist,
		starting_time,
		solution,
		NULL,
		tau,
		tau_min,
		delta,
		DELTA_GLOBAL,
		NULL,
		(MPIrank==0 && (debug_logging || snapshot_trigger)) ? RKService : NULL,	/* service callback in master rank only */
		0L,	/* steps */
		0L	/* steps_total */
	};

	/*
	The following line is a workaround of a bug of a C/C++ compiler on HP-UX: Passing 'mf' as the 4th
	initializer in the above structure initialization would not assign 'mf' to the respective 'meta_f'
	structure member. Instead, some garbage value would be assigned (it seems it's an address of 'mf', which
	is a total nonsense - of course, it results in a segmentation fault when eqSystem.meta_f() is called.).
	Moreover, other compilation issues are caused by 'mf' at that place, such as compiler 'panic' ... under
	some circumstances, the program does not compile at all (e.g. when OpenMP is enabled).

	It is strange that replacing 'mf' by some of the concrete functions (like 'mf_single'), we get a correct
	result. Here we have replaced 'mf' by NULL. The 'meta_f' structure member therefore has to be initialized
	explicitly:
	*/
	eqSystem.meta_f=mf;

	q=RK_MPI_SA_init(VAR_COUNT*subgridSIZE, MPI_COMM_WORLD, MPImaster);
	/* RK_MPI_SA_handle_NAN(1); */		/* has only meaning in master rank, is ignored in the others */

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

	/*
	everything ready - now precalculate some data if necessary. The int PrecalculateData(FLOAT *) function
	is defined in equation.c and returns 0 on success and nonzero on error. Aside from custom data
	precalculation, it can also modify the default RKM error estimate multipliers for individual chunks.
	The default expression evaluator used in rank 0 for parameters file parsing is left available
	in case PrecalculateData() needs some specific variables to be set up.
	 */
	PrecalcData_with_check(chunk_eps_mult);

/* ####### B E G I N >>> MASTER <<< ####### */ if(MPIrank==0) {

	int snapshot, l;			/* other loop control variables (in addition to 'q') */
	int on_demand_snapshot = 0;		/* the number of the on-demand snapshot. This number is part
						   of the snapshot dataset file name and is reset to 0
						   after the next ordinary snapshot is created */
	char is_on_demand_snapshot = 0;		/* nonzero if the solution is interrupted before the
						   ordinary snapshot time has been reached. The current status
						   of the solution is then written to an on-demand snapshot.
						   This event is triggered by the presence of the snapshot
						   trigger file snapshot_trigger_file (if this feature is enabled). */

	/* NetCDF handles required to be accessible at several places of the dataset export procedure */
	int dataset_ID;
	int n3_var_ID, n2_var_ID, n1_var_ID;
	int u_var_ID[VAR_COUNT];

	Mmprintf(logfile, "All initialization procedures completed in %s\n", format_time(MPI_Wtime()-AUX_time2));

	/* debug log file creation (will be created only once, even if in batch mode) */
	if(debug_logging) {
		/* create the debug log file - debug_logfile_ID==NULL for the first time the program reaches this point */
		if(!debug_logfile_ID) {
			debug_logfile_ID = fopen(debug_logfile,"w");
			/* debug_logfile_ID stays equal to NULL if the logfile creation fails */
			if(!debug_logfile_ID) {
				Mmprintf(logfile, "\nError: Can't create the debugging log file.\nStop.\n");
				HaltAllRanks(1);
			}
			fprintf(debug_logfile_ID, "Intertrack RK solver debugging log:\n\n");
		}
		if(loopIter>0) fprintf(debug_logfile_ID, "\nSTARTING LOG FOR ITERATION %d OF %d.\n\n",loopIter, loopTotal);
		fflush(debug_logfile_ID);
	}

 	cal_start_time=time(&calendar_time);
 	br_time=localtime(&calendar_time);

	MPIstart_time=MPI_Wtime();

 	Mmprintf(logfile, "\nStarting the simulation on: %s\n\n", format_date(br_time));

	MPIelapsed_time=0;


	for(snapshot=starting_snapshot;snapshot<total_snapshots;snapshot++) {
 		Mmprintf(logfile, "Calculating snapshot %d ... ", snapshot); fflush(stdout);

		MPInew_start=MPI_Wtime();
 		if(snapshot>starting_snapshot) {
			/* time of the next snapshot */
 			FLOAT next_snapt = starting_time + ((final_time-starting_time)*(snapshot-starting_snapshot)) / (total_snapshots-1-starting_snapshot);

			MPIcmd=MPICMD_SOLVE;
			MPI_Bcast(&MPIcmd, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

			/* publish the snapshot number to a global variable __snapshot (used by the debug logger) */
			__snapshot = snapshot;

			/*
			solve the system. If RK_MPI_SA_solve() return value is 1, its execution has been
			interrupted by the RKService() callback and an on-demand snapshot should be written.
			*/
			is_on_demand_snapshot = (RK_MPI_SA_solve(next_snapt, &eqSystem)==1) ? 1 : 0;
		}
		MPIelapsed_time+=(MPI_Wtime()-MPInew_start);

		time(&calendar_time);
		br_time=localtime(&calendar_time);

		if(is_on_demand_snapshot) {
			/* on-demand (triggered) snapshot */
			snapshot--;	/* this is not yet the next ordinary snapshot */

			Mmprintf(logfile, "On-demand snapshot triggered on %s - elapsed wall time: %s, %ld R-K steps, t=%" FTC_g "\n",
				format_date(br_time), format_time(MPIelapsed_time), eqSystem.steps, eqSystem.t);
			if(loopN)
				sprintf(filename, "%s%s/%s.%03d.%03d%s%s",
					path, loopVarString, base_name, snapshot, on_demand_snapshot, loopVarString, out_file_suffix);
			else
				sprintf(filename, "%s.%03d.%03d%s", path, snapshot, on_demand_snapshot, out_file_suffix);
			Mmprintf(logfile, "Saving file: %s ... [", filename); fflush(stdout);

			on_demand_snapshot++;
		} else {
			/* ordinary snapshot time reached */
			Mmprintf(logfile, "Done on %s - elapsed wall time: %s, %ld R-K steps (%ld total)\n",
				format_date(br_time), format_time(MPIelapsed_time), eqSystem.steps, eqSystem.steps_total);
			if(loopN)
				sprintf(filename, "%s%s/%s.%03d%s%s",
					path, loopVarString, base_name, snapshot, loopVarString, out_file_suffix);
			else
				sprintf(filename, "%s.%03d%s", path, snapshot, out_file_suffix);
			Mmprintf(logfile, "Saving file: %s ... [", filename); fflush(stdout);

			if(snapshot==starting_snapshot && skip_icond) {
				Mmprintf(logfile, "SKIPPED]\n");
				continue;
			}

			/* reset the on-demand snapshot counter (can only be nonzero if snapshot > starting_snapshot) */
			on_demand_snapshot = 0;
		}

		AUX_time = MPI_Wtime();		/* remember the time of snapshot creation */

		/* prepare the output NetCDF dataset */
		{
			int nc_error_code;
			int dim_IDs[3];

			nc_error_code = nc_create (filename, NC_CLOBBER, &dataset_ID);
			if(nc_error_code!=NC_NOERR) {
				Mmprintf(logfile, "NetCDF error: %s.\n", nc_strerror(nc_error_code));
				HaltAllRanks(1);
			}

			/* define the solution grid dimensions */
			nc_def_dim (dataset_ID, "n3", grid_IO_mode?total_n3:total_N3, dim_IDs);
			nc_def_dim (dataset_ID, "n2", grid_IO_mode?n2:N2, dim_IDs+1);
			nc_def_dim (dataset_ID, "n1", grid_IO_mode?n1:N1, dim_IDs+2);

			/*
			now define the NetCDF variables.
			Each definition may cause an error due to insufficient disk space. We proceed with
			the definitions only if the previous definition succeeds. The strange use of the
			'do-nothing' statements only keeps the if/else conditions aligned at the same level
			of code structure.
			*/

			if( (nc_error_code=nc_def_var(dataset_ID, "n3", NC_DOUBLE, 1, dim_IDs, &n3_var_ID)) != NC_NOERR) ; /* do nothing */
			else if( (nc_error_code=nc_def_var(dataset_ID, "n2", NC_DOUBLE, 1, dim_IDs+1, &n2_var_ID)) != NC_NOERR) ;
			else if( (nc_error_code=nc_def_var(dataset_ID, "n1", NC_DOUBLE, 1, dim_IDs+2, &n1_var_ID)) != NC_NOERR) ;
			else for(q=0;q<VAR_COUNT;q++)
				if( (nc_error_code=nc_def_var(dataset_ID, variable[q].name, NC_DOUBLE, 3, dim_IDs, u_var_ID+q)) != NC_NOERR) break;


			/* a single error check block handles any of the above definitions */
			if(nc_error_code != NC_NOERR) {
				Mmprintf(logfile, "NetCDF error: %s.\n", nc_strerror(nc_error_code));
				nc_close(dataset_ID);
				HaltAllRanks(1);
			}
		}

		/*
		save computation parameters - these attributes may or may not be used by other postprocessing
		software, by Intertack itself or they can be extracted by the ncdump command line utility
		for informational purpose only.
		*/
		{
			/* construct the double precision versions of the FLOAT computation parameters */
			double	L1_d = L1, L2_d = L2, L3_d = L3;
			double model_parameter_d;

			double delta_d = delta;
			double final_time_d = final_time;
			double t_d = eqSystem.t;
			double tau_d = eqSystem.h;

			int snapshot_bnd_thickness = grid_IO_mode*bcond_thickness;	/* 0 if the whole grid should be output */

			nc_put_att_double(dataset_ID, NC_GLOBAL, "L1", NC_DOUBLE, 1, &L1_d);
			nc_put_att_double(dataset_ID, NC_GLOBAL, "L2", NC_DOUBLE, 1, &L2_d);
			nc_put_att_double(dataset_ID, NC_GLOBAL, "L3", NC_DOUBLE, 1, &L3_d);

			for(q=0;q<PARAM_INFO_SIZE;q++) if(param_info[q].index >= 0)
			{
				model_parameter_d = model_parameters[param_info[q].index];
				nc_put_att_double(dataset_ID, NC_GLOBAL, param_info[q].name, NC_DOUBLE, 1, &model_parameter_d);
			}

			/* also output calc_mode as this can be used e.g. to choose the mathematical model */
			nc_put_att_int(dataset_ID, NC_GLOBAL, "calc_mode", NC_INT, 1, &calc_mode);

			nc_put_att_double(dataset_ID, NC_GLOBAL, "delta", NC_DOUBLE, 1, &delta_d);
			nc_put_att_double(dataset_ID, NC_GLOBAL, "tau", NC_DOUBLE, 1, &tau_d);
			nc_put_att_double(dataset_ID, NC_GLOBAL, "t", NC_DOUBLE, 1, &t_d);
			nc_put_att_double(dataset_ID, NC_GLOBAL, "final_time", NC_DOUBLE, 1, &final_time_d);

			nc_put_att_int(dataset_ID, NC_GLOBAL, "snapshot", NC_INT, 1, &snapshot);
			nc_put_att_int(dataset_ID, NC_GLOBAL, "total_snapshots", NC_INT, 1, &total_snapshots);
		}

		/* furthermore, save the comment as a global attribute of the NetCDF dataset */
		sprintf(buf, com_format, comment, eqSystem.t);
		nc_put_att_text(dataset_ID, NC_GLOBAL, "title", len(buf) , buf);

		nc_enddef(dataset_ID);

		/*
		save auxiliary coordinate arrays to the NetCDF dataset. These arrays contain the respective z,y,x
		coordinates of the grid nodes. If saved as variables with the same names as the dimensions' names
		n3,n2,n1 (ordered by importance), they are taken into account by the VisIt visualization software
		in renderings of NetCDF arrays.
		*/
		{
			double * x_grid_coords, * y_grid_coords, * z_grid_coords;
			int n3_ = grid_IO_mode ? total_n3 : total_N3;
			int n2_ = grid_IO_mode ? n2 : N2;
			int n1_ = grid_IO_mode ? n1 : N1;
			int bcond_thickness_ = grid_IO_mode ? 0 : bcond_thickness;
			int i, j, k;

			/*
			allocate the auxiliary arrays. For code structure improvement, all snapshot saving technicalities
			are concentrated together. Of course, these arrays could be allocated and initialized somewhere above
			only once per each batch mode iteration, but the additional work costs nothing anyway.
			*/
			alloc_error_code = 0;	/* this is in fact redundant as alloc_error_code should still be zero */
			if( (z_grid_coords=(double *)malloc(n3_*sizeof(double))) == NULL ) alloc_error_code=1;
			else if( (y_grid_coords=(double *)malloc(n2_*sizeof(double))) == NULL ) alloc_error_code=1;
			else if( (x_grid_coords=(double *)malloc(n1_*sizeof(double))) == NULL ) alloc_error_code=1;

			if(alloc_error_code) {
				Mmprintf(logfile, "Error: Could not allocate the grid coordinate descriptor arrays.\nStop.\n");
				nc_close(dataset_ID);
				HaltAllRanks(1);
			}

			/* fill the arrays */
			for(k=0;k<n3_;k++) z_grid_coords[k] = L3 * (0.5+k-bcond_thickness_)/total_n3;
			for(j=0;j<n2_;j++) y_grid_coords[j] = L2 * (0.5+j-bcond_thickness_)/n2;
			for(i=0;i<n1_;i++) x_grid_coords[i] = L1 * (0.5+i-bcond_thickness_)/n1;

			nc_put_var_double(dataset_ID, n3_var_ID, z_grid_coords);
			nc_put_var_double(dataset_ID, n2_var_ID, y_grid_coords);
			nc_put_var_double(dataset_ID, n1_var_ID, x_grid_coords);

			/* delete the coordinate arrays */
			free(x_grid_coords);
			free(y_grid_coords);
			free(z_grid_coords);
		}
		/*
		dataset created successfully - gather the solution from all ranks
		NOTE:	Up to this point, the other ranks don't even know that the master is preparing the snapshot.
		*/

		MPIcmd=MPICMD_SNAPSHOT;
		MPI_Bcast(&MPIcmd, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

		/* collect the data and save immediately. The auxiliary (boundary condition) nodes are saved if and only if grid_IO_mode==0. */
		for(l=0;l<MPIprocs;l++) {

			int snapshot_bnd_thickness = grid_IO_mode*bcond_thickness;	/* 0 if the whole grid should be output */
			int n1_ = n1;
			int n2_ = n2;
			int n3_;
			int first_row_;

			/*
			calculate the n3, first_row variables as they are in rank l
			(including the master itself if l==0)
			*/
  			n3_ = total_n3/MPIprocs;
			first_row_ = l*n3_;
			if(l < total_n3%MPIprocs) {
				n3_++;
				first_row_ += l;
			} else
				first_row_ += total_n3%MPIprocs;

			/*
			when saving the whole grid, we have to account for the number of ranks and the position of the
			block corresponding to rank l in the grid. The communication arrays ('boundary condition' in the Z
			direction) must not be saved to the output dataset.

			Here n3_ and first_row_ are modified so that they reflect the dimension of the block in the Y direction and
			the position of its first block in the output dataset. If grid_IO_mode==1, then n3_ and first_row_
			have the same value as the respective variables without the trailing underscore in rank l.
			*/
			if(!grid_IO_mode) {
				n1_=N1; n2_=N2;
				if(l==0) n3_ += bcond_thickness; else first_row_ += bcond_thickness;
				if(l==MPIprocs-1) n3_ += bcond_thickness;
			}

			if(l) {
				/* subgridsize in rank 0 may be several rows greater than the actual amount of data, but that doesn't matter */
				for(q=0;q<VAR_COUNT;q++)
					MPI_Recv(data_cache[q], subgridsize, MPI_DOUBLE, MPIrankmap[l], MPIMSG_SOLUTION+q, MPI_COMM_WORLD, &MPIstat);
			} else {
				int i, j, k;
				double * cache_ptr;
				FLOAT * ptr;

				/*
				update the boundary conditions in the event that the auxiliary nodes are required to be saved.
				The bcond_setup() function is defined in the file equation.c
				*/
				if(!grid_IO_mode) bcond_setup(eqSystem.t, solution);

				/*
				Transcribe the solution to the cache before saving (for additional information, see the same
				procedure in the initial conditions loading part. If grid_IO_mode==1, the boundary conditions
				in the plane X-Y are skipped here (the boundary conditions in the direction of Z are skipped
				automatically due to the memory organization of the solution).
				*/

				for(q=0;q<VAR_COUNT;q++) {
					cache_ptr = data_cache[q];
					for(k=0;k<n3_;k++)
						for(j=0;j<n2_;j++) {
							ptr = VAR(solution,q) + (k+snapshot_bnd_thickness) * rowsize + (j+snapshot_bnd_thickness) * N1 + snapshot_bnd_thickness;
							for(i=0;i<n1_;i++) *(cache_ptr++)=*(ptr++);
						}
				}
			}

			/* save the block to the file */
			{
				/*
				prepare the data subgrid starting corner and dimensions, as required for the call to
				the nc_get_vara_double() function. For more information, see the NetCDF documentation.
				*/
				size_t nc_start[3] = { first_row_, 0, 0 };
				size_t nc_count[3] = { n3_, n2_, n1_ };

				for(q=0;q<VAR_COUNT;q++)
					nc_put_vara_double(dataset_ID, u_var_ID[q], nc_start, nc_count, data_cache[q]);

			}

			/* move the progress meter (each star represents data collection from one process) */
			Mmprintf(logfile, "*"); fflush(stdout);
		}

		nc_close(dataset_ID);
		Mmprintf(logfile, "] Done in %s\n", format_time(MPI_Wtime()-AUX_time));

		/* delete the snapshot trigger file */
		if(is_on_demand_snapshot) unlink(snapshot_trigger_file);

		commit_logfile(0);	/* update the log file on disk (non-forced update - see commit_logfile()) */
	}

	/* print the total wall time spent on the actual calculation */
	time(&calendar_time);
	br_time=localtime(&calendar_time);
	Mmprintf(logfile, "\nSimulation completed OK on: %s\n"
		"Total wall time:	%s\n", format_date(br_time), format_time(MPIelapsed_time));

	/* the total wall time of the calculation, including snapshot saving and data gathering */
	Mmprintf(logfile, "Overall wall time:	%s\n", format_time(MPI_Wtime()-MPIstart_time));
	Mmprintf(logfile, "Elapsed calendar time:	%s\n", format_time(difftime(calendar_time, cal_start_time)));
	Mmprintf(logfile, "Total successful R-K steps:	%ld\n", eqSystem.steps);
	Mmprintf(logfile, "Total R-K steps: 		%ld\n", eqSystem.steps_total);

	if(!loopN) HaltAllRanks(0);

	/* invoke the post-processing script if one is defined */
	if(*pproc_script) {
		/* script command line arguments: iteration_number, path_to_output_directory, output_directory_name */
		sprintf(buf, "%s %d \"%s%s\" \"%s%s\"", pproc_script, loopIter, path, loopVarString, base_name, loopVarString);
		/* invoke the command */
		Mmprintf(logfile, "Invoking the post-processing script: %s\n", buf);

		if(pproc_nowait) {
			/* concurrent script execution mode */

			pid_t pid;

			#ifdef __USE_VFORK
				pid=vfork();
			#else
				pid=fork();
			#endif
			/* pid==0 in child process and pid<0 in parent process in case of error (no child process exists in this case) */
			if(pid<0)
				if(pproc_nofail)
					{ Mmprintf(logfile, "Error: fork() failed. Could not create the shell process.\nStop.\n"); HaltAllRanks(1); }
				else
					Mmprintf(logfile, "Warning: fork() failed. No script invoked.\n");
			else if(pid>0) {
				Mmprintf(logfile, "The post-processing script running as PID %d.\n", pid);
				pproc_submitted=1;
			} else {
				/* pid==0, the child process */
				/*
				NOTE:	If vfork() is used, the address spaces of the parent and the child processes are
					shared until the execl() below. However, here we don't modify any local variables that
					are used by the parent process, so we can use vfork() safely. The parent process
					execution is suspended until a call to exec...() or _exit().
				*/
				int handle;
				_string_ SHELL="/bin/sh";
				int priority_grace=3;

				handle=open("/dev/null", O_WRONLY);
				dup2(handle, 1);	/* redirect stdout to /dev/null */
				dup2(handle, 2);	/* redirect stderr to /dev/null */
				/*
			        decrease the priority of the shell process so that it has almost the lowest priority (the
			        highest nice value). 'priority_grace' determines how much the priority should be greater than
			        the minimum priority. On some systems, the highest nice value is actually PRIO_MAX-1, so we
			        need to have priority_grace>=2 in order to set the process priority to something higher than
			        the absolute minimum. The condition ensures that we never try to increase the priority of the
			        process (this would fail anyway if the process is not run with root privileges)
				*/
				if(getpriority(PRIO_PROCESS, 0) < PRIO_MAX-priority_grace) setpriority(PRIO_PROCESS, 0, PRIO_MAX-priority_grace);
				/* execute the shell command */
				execl(SHELL, SHELL, "-c", buf, NULL);
				/* this is just in case execl() fails. (this child process does not behave as an MPI process) */
				close(handle);
				exit(1);	/* exit() calls _exit(), which also makes the parent process continue execution (vfork() only) */
			}
		} else {
			/* standard script execution mode */

			q=system(buf);
			if(q==-1)
				Mmprintf(logfile, "Could not create the shell process.\n");
			else
				Mmprintf(logfile, "The script ended with return value %d.\n", WEXITSTATUS(q));
			if(q!=0)
				if(pproc_nofail)
					{ Mmprintf(logfile, "Error invoking the post-processing script.\nStop.\n"); HaltAllRanks(1); }
				else Mmprintf(logfile, "Warning: The post-processing script did not finish properly.\n");
		}
	}

	/* in batch mode, proceed to the next iteration */
	MPIcmd=MPICMD_NEXT;
	MPI_Bcast(&MPIcmd, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

/* ####### E N D >>> MASTER <<<, B E G I N >>> OTHER <<< ####### */ } else {
	/*
	The calculation is fully controlled by the master rank 0. The other ranks
	receive commands and perform them. MPICMD_SOLVE means that the RK solver will
	be called. The master rank must also call the solver and provide it with
	the next time level to be reached. The other commands only perform the particular
	actions and DO NOT call the solver.
	*/
	int listen=1;
	while(listen) {
		MPI_Bcast(&MPIcmd, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);	/* wait for the next command */
		switch(MPIcmd) {
			case MPICMD_NEXT:		/* proceed to the next iteration (occurs in batch mode only) */
				listen=0; break;
			case MPICMD_HALT:
			{
				int code;
				MPI_Bcast(&code, 1, MPI_INT, MPIrankmap[0], MPI_COMM_WORLD);

				MPI_Barrier(MPI_COMM_WORLD);

				MPI_Finalize();
				return(code);
			}

			case MPICMD_SOLVE:
				RK_MPI_SA_solve(0, &eqSystem);
				break;

			case MPICMD_SNAPSHOT:
				/* copy the solution grid block to the 'double' buffer and send it to the master */
				{
					int i, j, k;
					double * cache_ptr;
					FLOAT * ptr;

					/*
					Boundary condition (auxiliary node) layer thickness to be omitted from the output dataset.
					This value is set to 0 if the whole grid should be output, and to bcond_thickness otherwise.
					*/
					int snapshot_bnd_thickness = grid_IO_mode*bcond_thickness;

					/*
					Here we construct the actual dimensions of the block saved (possibly including the boundary
					condition). However, the situation here is simpler than in rank 0.
					*/
					int n1_ = grid_IO_mode ? n1 : N1;
					int n2_ = grid_IO_mode ? n2 : N2;
					int n3_ = n3 + ((MPIrank==MPIprocs-1) ? (1-grid_IO_mode)*bcond_thickness : 0);

					/*
					update the boundary conditions in the event the auxiliary nodes are required to be saved.
					(the current time level is available in eqSystem.t in all ranks)
					*/
					if(!grid_IO_mode) bcond_setup(eqSystem.t, solution);

					/* perform transcription to the cache. For more information, see above (the same procedure in the master rank). */
					for(q=0;q<VAR_COUNT;q++) {
						cache_ptr = data_cache[q];
						for(k=0;k<n3_;k++)
							for(j=0;j<n2_;j++) {
								ptr = VAR(solution,q) + (k+bcond_thickness) * rowsize + (j+snapshot_bnd_thickness) * N1 + snapshot_bnd_thickness;
								for(i=0;i<n1_;i++) *(cache_ptr++)=*(ptr++);
							}
					}

					/* subgridsize has been defined so that it is always equal to n1_ * n2_ * n3_ */
					for(q=0;q<VAR_COUNT;q++)
						MPI_Send(data_cache[q], subgridsize, MPI_DOUBLE, MPIrankmap[0], MPIMSG_SOLUTION+q, MPI_COMM_WORLD);
				}

			/* NOTE: MPICMD_NO_COMMAND should not occur here. If it did, it would be ignored */
		}
	}

/* ####### E N D >>> OTHER <<< ####### */ }

	/* the following happens in all ranks. They pass here only in batch mode after a successful calculation */

	RK_MPI_SA_cleanup();

	for(q=0;q<VAR_COUNT;q++) free(data_cache[q]);

	FreePrecalcData();
	free(solution);
	free(chunk_size);
	free(chunk_start);
	free(chunk_eps_mult);

	}	/* END OF THE BATCH PROCESSING LOOP */

	/* in fact, this should be unreachable */

	MPI_Finalize();
	return(0);
}
