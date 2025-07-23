# PorousFreezeThaw
Numerical solvers for:

- simulation of freezing and thawing of water in porous media at pore scale
- DEM simulation of spherical particle dynamics (for creating a porous bed of particles
  at the bottom of a container)

This repository contains supplementary materials for the manuscripts

**Pavel Strachota - Three-dimensional phase-field simulations of water freezing and thawing at pore-scale**

and

**Pavel Strachota - DEM Simulations of Settling of Spherical Particles using a Soft Contact Model and Adaptive Time Stepping**

The directory structure is as follows:

```
── apps
│   ├── intertrack-hybrid-S-freezing                # C source code of the freezing/thawing simulator
│   │   ├── data
│   │   ├── OUTPUT
│   │   └── results                                 # results (case settings files "Params", log files, visualizations)
│   │       ├── 100_low-resolution
│   │       ├── 200_medium-resolution
│   │       │   └── Movies-with-temperature-field
│   │       ├── 400_high-resolution
│   │       │   ├── Movies-without-temperature-field
│   │       │   └── Movies-with-temperature-field
│   │       └── PhysRevE-2025
│   │           └── 200_medium-resolution
│   │               └── Movies-with-temperature-field
│   ├── sphere-collider                             # C source code of the full DEM simulator
│   │   └── OUTPUT
│   └── sphere-collider-MATLAB                      # MATLAB code of the basic DEM simulator (more cases)
│       ├── results                                 # results produced in MATLAB (workspace snapshots, videos, logs)
│       │   ├── 100-spheres-for-porous-bed
│       │   ├── 100-spheres-high_elevation
│       │   ├── 200-spheres-for-porous-bed-1
│       │   └── 200-spheres-for-porous-bed-2
│       └── source-code
│           └── plots
```

*(software infrastructure and modules needed to build the apps are below)*

```
├── include
├── lib
├── libsource
│   ├── dataIO
│   ├── exprsion
│   ├── screen
│   └── strings
├── modules
│   ├── cparser
│   ├── evsubst
│   ├── mprintf
│   ├── pparser
│   ├── RK_Asolver
│   ├── RK_csolver
│   ├── RK_MPI_Asolver
│   ├── RK_MPI_SAsolver
│   ├── RK_MPI_SAsolver_hybrid
│   ├── RK_MPI_SAsolver_hybrid2
│   └── RK_solver
├── _settings
└── _template
    └── AVS
```

## DEM simulator of spherical particle dynamics (MATLAB)

Run the ``spheres.m`` script using a recent MATLAB distribution (anything newer than R2020a will do). The source code is heavily commented.
The script requires that a ``plots`` directory exists in its path where the visualization of the simulation is stored in the form of a series of images.

## Building the binaries of the simulators written in C

The code of the simulators is written in C and C++. For easy compilation, the following software environment is required:

- Linux operating system
- any recent ``gcc`` compiler with OpenMP support
- an MPI library with development files, such as OpenMPI
- ``make``

To build the software, run ``make_all`` in the ``porous-freeze-thaw-simulator`` directory. 

## Freezing and thawing simulations in porous media

The binaries of the simulator will be placed in

``porous-freeze-thaw-simulator/apps/intertrack-hybrid-S-freezing``

To run the simulation of with the default parameters settings (the ``Params`` file), issue the command

```
./Run M N
```  

where ``M`` is the number of MPI ranks and ``N`` is the number of OpenMP threads. If ``N`` is omitted, it defaults to 1.
If both ``M`` and ``N`` are omitted, one single-threaded process is launched.

The positions of the sphere centers are contained in the ``data/spheres_positions.txt`` file. This path is currently hardcoded in ``equation.c``


## DEM simulations of spherical particle settling

The binaries of the simulator will be placed in

``porous-freeze-thaw-simulator/apps/sphere-collider``

All parameters are contained in the source file ``spheres.c``. This file is a symlink to one
of the version of the simulator:

- ``spheres_basic.c`` ... the basic version with repulsive forces only
- ``spheres_friction.c`` ... the version with friction
- ``spheres_friction_angular.c`` ... the version with friction and rotation

To select one of the versions for compilation (point the symlink to one of the source code variants), one may type e.g.:

``./Select.sh spheres_basic.c``

Then modify the source file and run ``make`` again.

