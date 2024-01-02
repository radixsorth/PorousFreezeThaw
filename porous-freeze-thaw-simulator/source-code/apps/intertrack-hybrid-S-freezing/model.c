/**************************************************************\
*                                                              *
*                   I N T E R T R A C K  -  S                  *
*                                                              *
*      High Precision Phase Interface Evolution Simulator      *
*                      ("H i P P I E S")                       *
*                                                              *
*--------------------------------------------------------------* 
*       Simulation of water freezing in the vacant space       *
*              between glass balls in a vessel                 *
*--------------------------------------------------------------* 
*                                                              *
* Features a generic thickness of the boundary layer           *
* and numerical grid layout suitable for FVM.                  *
*                                                              *
*  - hybrid OpenMP/MPI version, using RK_MPI_SAsolver_hybrid   *
*                                                              *
* -------------------------------------------------------------*
* (C) 2005-2022 Pavel Strachota                                *
* - backported extensions (C) 2013-2015 Pavel Strachota        *
* - Hybrid OpenMP/MPI parallelization (C) 2015 Pavel Strachota *
* - new features (C) 2019 Pavel Strachota                      *
* - generic system of variables & parameters (C) 2021-22 P.S.  * 
*                                                              *
* file: model.c                                                *
\**************************************************************/

/*
named array subscripts to be used to address the respective variable. There has
to be a last item called VAR_COUNT which is used as the number of the variables.
*/
enum {
	/* system variables */
	temperature_field,
	phase_field,
	glass_field,

	/* this last line is mandatory */
	VAR_COUNT
};


/* named array subscripts to be used to address the respective model parameter */
enum {	
	
	u_star, L, xi, a, b, alpha, mu,
	xi_gl, zeta,
	p_eps0, p_eps1,
	/* u_D, */ gamma,
	water_cp, ice_cp, glass_cp,
	water_lambda, ice_lambda, glass_lambda,
	water_rho, ice_rho, glass_rho,
	top_temp1, top_temp2, phase_switch_time,
	u_noise_amp,
	ball_radius,

	PARAM_COUNT
};

typedef struct {
	_conststring_ name;
	_conststring_ description;
} VAR_METADATA;

typedef struct {
	int index;
	_conststring_ name;
	_conststring_ description;
} PARAM_METADATA;

/*
names of the individual variables used in the NetCDF dataset
Variable's descriptions are available here as well. They must be given
for all variables AND for the (optional) additional quantities as well.

C99 Designated initializers are used to initialize the correct elements.
*/
VAR_METADATA variable [VAR_COUNT] = {
	[temperature_field]	=	{ "u",		"temperature field" },
	[phase_field]		=	{ "p",		"phase field" },
	[glass_field]		=	{ "gl",		"glass balls phase field" },
};

PARAM_METADATA param_info [] = {

	{ -1,			NULL,			"Physical parameters" },

	{ u_star,		"u_star",		"u*"},
	{ L,			"L",			"Specific latent heat of fusion of water [J/kg]" },
	{ water_cp,		"water_cp",		"Heat capacity of liquid water at constant pressure [J/(kg.K)]" },
	{ ice_cp,		"ice_cp",		"Heat capacity of ice at constant pressure [J/(kg.K)]" },
	{ glass_cp,		"glass_cp",		"Heat capacity of glass at constant pressure [J/(kg.K)]" },
	{ water_lambda,		"water_lambda",		"Thermal conductivity of liquid water [W/(m.K)]" },
	{ ice_lambda,		"ice_lambda",		"Thermal conductivity of ice [W/(m.K)]" },
	{ glass_lambda,		"glass_lambda",		"Thermal conductivity of glass [W/(m.K)]" },
	{ water_rho,		"water_rho",		"Density of liquid water [kg/m^3]" },
	{ ice_rho,		"ice_rho",		"Density of ice [kg/m^3]" },
	{ glass_rho,		"glass_rho",		"Density of glass [kg/m^3]" },

	{ -1,			NULL,			"Glass phase field representation parameters" },

	{ xi_gl,		"xi_gl",		"Glass phase interface thickness parameter" },
	{ zeta,			"zeta",			"Glass phase field multiplier in water indicator" },

	{ -1,			NULL,			"Phase field model parameters" },

	{ xi,			"xi",			"Phase interface thickness parameter xi" },
	{ a,			"a",			"Phase field model parameter a" },
	{ b,			"b",      		"Phase field model parameter b" },
	{ alpha,		"alpha",		"Coefficient of attachment kinetics [s/m^2]" },
	{ mu,			"mu",			"Interfacial mobility [m/(s.K)]" },

	{ -1,			NULL,			"SigmaP1-P model parameters" },

	{ p_eps0,		"p_eps0",		"p S-shape limiter 0-threshold" },
	{ p_eps1,		"p_eps1",		"p S-shape limiter 1-threshold" },

	{ -1,			NULL,			"Temperature-based freezing model parameters" },

/*	{ u_D,			"u_D",			"Freezing point depression [K]"},	*/
	{ gamma,		"gamma",		"Freezing progression factor [1]"},

	{ -1,			NULL,			"Simulation settings" },

	{ top_temp1,		"top_temp1",		"Temperature at the top of the vessel during Phase 1 [K]" },
	{ top_temp2,		"top_temp2",		"Temperature at the top of the vessel during Phase 2 [K]" },
	{ phase_switch_time,	"phase_switch_time",	"Time of switchnig from Phase 1 to Phase 2 [s]"},

	{ u_noise_amp,	"u_noise_amp",	"Temperature noise amplitude" },
	{ ball_radius,	"ball_radius",	"Radius of all glass balls [m]" },
};

size_t PARAM_INFO_SIZE = sizeof(param_info) / sizeof(PARAM_METADATA);

void InitMetadata(void)
/* initializes all necessary variables' and parameters' metadata (called from intertrack.c) */
{
}
