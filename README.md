# PorousFreezeThaw
Numerical solvers for simulation of freezing and thawing of water in porous media at pore scale

This repository contains supplementary materials for the manuscript

**Pavel Strachota - Three-dimensional numerical simulation of water freezing and thawing in a container filled with glass beads**

The directory structure is as follows:

```
├── porous-freeze-thaw-simulator              # materials for the phase field simulations of freezing and thawing in porous media
│   ├── results                               # results (case settings files "Params", log files, visualizations)
│   │   ├── 100_low-resolution
│   │   ├── 200_medium-resolution
│   │   └── 400_high-resolution
│   └── source-code                           # source code of the simulator for POSIX-compatible systems
│       ├── apps                
│           ├── intertrack-hybrid-S-freezing  # <-- this directory contains the simulator itself
│       └── ......
└── sphere-collider                           # materials for the sphere dynamics simulator
    ├── results                               # results: simulator versions for different situations, final positions of sphere centers, animations
    │   ├── 100-spheres-for-porous-bed
    │   ├── 100-spheres-high_elevation
    │   └── 200-spheres-for-porous-bed
    └── source-code
        ├── plots
        ├── spheres.m                         # the simulator itself (MATLAB script)
        ├── spheres_prod.m                    # several simulator variants (also contained in the individual subdirectories of the results dir.)
        ├── spheres_p100.m
        └── spheres_test.m
```

## Sphere collision simulations

Run the ``spheres.m`` script using a recent MATLAB distribution (anything newer than R2020a will do). The source code is heavily commented.
The script requires that a ``plots`` directory exists in its path where the visualization of the simulation is stored in the form of a series of images.

## Freezing and thawing simulations in porous media

The simulator is written in C and C++. For easy compilation, the following software environment is required:

- Linux operating system
- any recent ``gcc`` compiler with OpenMP support
- an MPI library with development files, such as OpenMPI
- ``make``

To build the software, run ``make_all`` in the ``porous-freeze-thaw-simulator/source-code`` directory. The binary of the simulator will be
placed in ``porous-freeze-thaw-simulator/source-code/apps/intertrack-hybrid-S-freezing``

To run the simulation with the default parameters settings (the ``Params`` file), issue the command

```
./Run M N
```  

where ``M`` is the number of MPI ranks and ``N`` is the number of OpenMP threads. If ``N`` is omitted, it defaults to 1.
If both ``M`` and ``N`` are omitted, one single-threaded process is launched.

The positions of the sphere centers are contained in the ``data/spheres_positions.txt`` file. This path is currently hardcoded in ``equation.c``
