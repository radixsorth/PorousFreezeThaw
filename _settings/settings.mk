# Digithell HyperGeneric Makefile System
# (common variable definitions)
# (C) 2005-2009 Digithell, Inc. (Pavel Strachota)
# ===============================================

# IMPORTANT: GNU Make is required to process Digithell HyperGeneric Makefile System

# 0) The TARGET_SYSTEM variable
# -----------------------------
# This defines the target platform. Compiler and library variables are set up
# depending on the value of this variable.
# Currently supported target platforms:
# --> linux
# --> hp_ux
# NOTE: The default is "linux"
TARGET_SYSTEM = linux

# #############################################################################

# 1) The BASE_DEPTH variable
# --------------------------
# this is the depth of the source directory relative to the base
# program directory, where all directories like 'lib' or 'include'
# are placed. Assume this base directory is called 'Progs'
# If your source is located at 'Progs/apps/my_app/' , then the depth
# will be 2, which means you must perform 'cd ../../' to reach the
# base directory. You must redefine this variable if your source
# is located in different depth.
# (e.g if it's in '/Progs/apps/tests/test1', set BASE_DEPTH to ../../../)

# Also don't forget to change the path to this file in the initial include
# statement. (Alternatively, if you define BASE_DEPTH first, you can use it
# as a part of the include path.)

# NOTE: Since all variables that make use of BASE_DEPTH are recursively
# expanded (see make variable flavors - GNU make help), you can redefine
# BASE_DEPTH variable after the inclusion of this file. Conversely, you may
# also define it before you include this file, as the ?= assignment operator
# is used here.
BASE_DEPTH ?= ../../

# #############################################################################

# 2) The SETTINGS variable
# ------------------------
# when the settings or one of the listed include files change, EVERYTHING
# should be rebuilt. Every target therefore depends on these files.
# HINT: if you want to force rebuild, you can perform the command:
# touch settings.mk
SETTINGS = $(BASE_DEPTH)_settings/settings.mk $(BASE_DEPTH)include/common.h $(BASE_DEPTH)include/mathspec.h

# #############################################################################

# 3) The MAKE variable: what command should be called as a make program)
# (applies to 'make' calls from inside other Makefiles)
# ----------------------------------------------------------------------
# If the MAKE variable is not explicitly set here, it contains the full path
# to the make program that is currently processing the makefile. Use '$(MAKE)'
# instead of 'make' when executing another make from within a makefile to ensure
# that the same make program is called for makefiles at all levels.

# MAKE = gmake

# #############################################################################

# 4) The CC, CPP and LD variables: what compiler and linker to use
# ----------------------------------------------------------------
# Generally, it is wise to use a compiler for linking purposes as well. If we use
# a standalone linker, we'll have to involve many libraries and initialization code
# objects explicitly in the link. Thus, the LD variable serves mainly in cases where
# we want to decide whether to use a C or C++ compiler for linking. If you want to
# use a C compiler to link C++ modules, you will have to provide the linker with
# C++ runtime library location (see section 8).

# for the GNU C/C++ compiler:
CC	= gcc
CPP	= g++
LD	= gcc

# NOTE:	If you want to compile C/C++ code using a C++ compiler, place the following lines
#	into the makefile that you have obtained from the template, right below the
#	inclusion of settings.mk.
#	CC = $(CPP)
#	CC_FLAGS = $(CPP_FLAGS)

# for the Intel C/C++ optimizing compiler (tested with Version 9.0):
# CC	= icc
# CPP	= icpc
# LD	= icc

# for the C/C++ compiler on the HP-UX PA-RISC system
ifeq ($(TARGET_SYSTEM),hp_ux)
  CC	= c89
  CPP	= aCC
  LD	= c89
endif

# #############################################################################

# 5) The CC_FLAGS and CPP_FLAGS variables: compiler options
# ---------------------------------------------------------
# the following section marked by stars describes useful compiler options
# ***************************************************************
#
# compiler options for C compilation and C++ compilation:
# If you change them in this file, everything necessary will be rebuilt
# (see the beginning of this file).
#
# A) generally useful compiler options:
# ---------------------------------------
# -g ..........	include debug information (for ICC, this DISABLES OPTIMIZATION)
# -s ..........	exclude relocation and symbol information
# -c99 ........	needed for the Intel C++ compiler when <mathimf.h> is used
# -std=c99 ....	the same as -c99, works also with the gcc compiler
# NOTE: Selecting a C99 standard influences a wide set of available features. The <features.h> file
#	is a quite comprehensive source of information on that.
#
# B) GCC optimization options:
# ------------------------------
# -O0 .........	don't optimize (default)
# -O, -O1 .....	optimize for speed
# -O2 .........	more speed optimizations
# -O3 .........	yet more speed optimizations
# -Os .........	optimize for size
# NOTE: If you specify more than one option, only the last one is effective.
#
# C) ICC general optimization options:
# ------------------------------------
# -O0 .........	don't optimize (default)
# -O1 .........	optimize for complex programs with a little time spent in loops
# -O, -O2 .....	optimize for speed (default)
# -O3 .........	more aggressive than -O2, suitable for programs with heavy usage
#		of loops and floating point math
# -mp ......... Maintain floating point precision (disable some optimizations)
# -mp1 ........ Improve floating point precision (disable fewer optimizations
#		than -mp)
# -openmp ..... enable support for OpenMP parallelization directives
#
# D) ICC processor-specific optimization:
# ---------------------------------------
# -x<codes> ... generate code exclusively for a processor type
# -ax<codes> ..	generate code specialized for processor type (but backwards
#		compatible). <codes> may be one or more of:
#	K ..... Intel Pentium III
#	W ..... Intel Pentium 4
#	N ..... Intel Pentium 4 + new not processor-specific optimizations
#	B ..... Intel Pentium M + new not processor-specific optimizations
#	P ..... Intel Pentium 4 with SSE3 + new optimizations
#		Example: -axNB
# -march=<cpu>  Generate code exclusively for the given CPU. <cpu> is one of:
#		pentiumpro, pentiumii, pentiumiii, pentium4
# -mcpu=<cpu>	Optimize for the given CPU. <cpu> is one of:
#   pentium ...	Optimize for Intel Pentium
#   pentiumpro	Optimize for Intel Pentium Pro, Pentium II and Pentium III
#   pentium4 ..	Optimize for Intel Pentium 4 (default)
#   itanium ...	Optimize for Intel Itanium processor
#   itanium2 ..	Optimize for Intel Itanium 2 processor
#
# ***************************************************************
MACRO_DEFINITIONS = -D __GNU_SYSTEM
CC_FLAGS = $(MACRO_DEFINITIONS) -O1 -std=c99
CPP_FLAGS = $(MACRO_DEFINITIONS) -O1

ifeq ($(TARGET_SYSTEM),hp_ux)
  MACRO_DEFINITIONS = -D __HPUX_SYSTEM -D __DISABLE_NAN_HANDLING -D __EXPLICIT_BYTE_ORDER -D __USE_MATH_FL_MACROS
  CC_FLAGS = $(MACRO_DEFINITIONS) +e +O3 +Oall +DD64
  CPP_FLAGS = $(MACRO_DEFINITIONS) +O3 +DD64
endif

# #############################################################################

# 6) The LD_FLAGS variable: linker options
# ----------------------------------------
LD_FLAGS = -O1

ifeq ($(TARGET_SYSTEM),hp_ux)
LD_FLAGS = +O3 +Oall +DD64
endif

# #############################################################################

# 7) The OpenMP switches
# ----------------------
# Add these switches to your CC_FLAGS / LD_FLAGS in order to use OpenMP.
# You can use the __OPENMP macro to test whether your application is compiled
# with OpenMP support enabled. Note that there may be other macros predefined
# by the compiler when OpenMP is used (e.g. the _OPENMP macro with the icc compiler)
#
# The __OPENMP31 macro indicates that the features introduced in OpenMP 3.1 can
# be used (e.g. the "max" reduction operation).
CC_OMP = -fopenmp -D __OPENMP -D __OPENMP31
LD_OMP = -fopenmp

# for GCC (also accepted by icc version 13.0 and above)
#CC_OMP = -fopenmp -D __OPENMP -D __OPENMP31
#LD_OMP = -fopenmp


ifeq ($(TARGET_SYSTEM),hp_ux)
  CC_OMP = +Oopenmp -D __OPENMP
  LD_OMP = +Oopenmp
endif

# #############################################################################

# 8) The LIBS variable: default libraries
# ---------------------------------------
# -lm stands for libm, the standard C math library
# -limf stands for libimf, the Intel optimized math library
# NOTE: -limf does not have to be explicitly involved in the link
LIBS = -lm

# #############################################################################

# 9) The CPP_RUNTIME_LIB variable: the main C++ runtime library
# -------------------------------------------------------------
# This library is not automatically included in the application
# makefile template. You should place it into the SYS_LIBS variable
# definition in each makefile that builds an application with C++ code.

# On some systems, it is easier to use the C++ compiler for linking
# than to explicitly involve this library in the link.
# In such case, don't declare this variable.
CPP_RUNTIME_LIB = -lstdc++

# --> C++ runtime libraries for HP-UX: -lCsup -lstd (-lstd seems unnecessary)
ifeq ($(TARGET_SYSTEM),hp_ux)
  CPP_RUNTIME_LIB = -lCsup
endif

# #############################################################################

# 10) The INC_PATH and INC_COMMAND variables: relative path to headers
# from the source directory
# --------------------------------------------------------------------
INC_PATH = $(BASE_DEPTH)include
INC_COMMAND = -I $(INC_PATH)

# the definitions below follow the same path scheme:

# #############################################################################

# 11) The MOD_PATH variable: path where modules are placed
# -------------------------------------------------------
# this is used by application makefiles
MOD_PATH = $(BASE_DEPTH)modules

# #############################################################################

# 12) The LIB_PATH and LIB_COMMAND variables: search path for additional libraries
# -------------------------------------------------------------------------------
# this is used in the link phase
LIB_PATH = $(BASE_DEPTH)lib
LIB_COMMAND = -L $(LIB_PATH)

# #############################################################################

# 13) The LIBSOURCE_PATH variable: path to the library source modules directory
# -----------------------------------------------------------------------------
LIBSOURCE_PATH = $(BASE_DEPTH)libsource
