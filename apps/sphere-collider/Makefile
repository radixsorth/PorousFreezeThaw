# Digithell HyperGeneric Makefile
# (application, AVS compliant)
# (C) 2005-2006 Digithell, Inc. (Pavel Strachota)
# =====================================

include ../../_settings/settings.mk

CC = mpicc
CPP = mpic++
LD = mpicc

# compile with OpenMP support
CC_FLAGS := $(CC_FLAGS) $(CC_OMP)
LD_FLAGS := $(LD_FLAGS) $(LD_OMP)
RKSOLVER = RK_MPI_SAsolver_hybrid

MACRO_DEFINITIONS := $(MACRO_DEFINITIONS) -D __USE_VFORK

# -------------------------------------
# Here enter the names without any extensions or paths:
# (Uncomment or add as many lines as necessary)

# Application name
APPNAME = spheres

# Used modules:
MODULE1 = $(RKSOLVER)
MODULE2 =
MODULE3 =
MODULE4 =
MODULE5 =

# Used additional user libraries:
LIB1 = 
LIB2 = 
LIB3 = 

# Used additional system libraries
# (this is copied onto the linker command line, thus use
# the appropriate syntax, e.g SYS_LIBS = -lxxxx -lyyyy )
SYS_LIBS = -lnetcdf $(CPP_RUNTIME_LIB)

# -------------------------------------
# Module & library path specification:
# (Uncomment or add as many lines as necessary)

MODULE1_OBJ = $(MOD_PATH)/$(MODULE1)/$(MODULE1).o
MODULE2_OBJ = $(MOD_PATH)/$(MODULE2)/$(MODULE2).o
MODULE3_OBJ = $(MOD_PATH)/$(MODULE3)/$(MODULE3).o
MODULE4_OBJ = $(MOD_PATH)/$(MODULE4)/$(MODULE4).o
MODULE5_OBJ = $(MOD_PATH)/$(MODULE5)/$(MODULE5).o

LIB1_A = $(LIB_PATH)/lib$(LIB1).a
LIB2_A = $(LIB_PATH)/lib$(LIB2).a
LIB3_A = $(LIB_PATH)/lib$(LIB3).a

# -------------------------------------
# Concatenation:
# (The commented out lines show how to construct the variables)

#MODULE_OBJS = $(MODULE1_OBJ) $(MODULE2_OBJ) $(MODULE3_OBJ) $(MODULE4_OBJ) $(MODULE5_OBJ)
MODULE_OBJS = $(MODULE1_OBJ)

#SPEC_LIBS = $(LIB1_A) $(LIB2_A) $(LIB3_A)
SPEC_LIBS =

# -------------------------------------
# The following section is not to be modified:

# main is the target that is to be brought up to date;
# this concept ensures that the modules and libraries sections
# are always performed. It contains only recursive make commands
# that update modules that this application depends on.
# (thus we do not need special prerequisites for each module,
# which would normally have to be included, even though
# they are already contained in the makefile of the respective
# module)

# the application must depend on all modules and libraries it uses,
# as it must be linked with them whenever they get updated.

# the main application module does not depend on header files
# that do not belong to any module (such as mydefs.h)

.PHONY: modules libraries main clean
main: modules libraries $(APPNAME)

# main application binary
$(APPNAME) : $(APPNAME).o $(MODULE_OBJS) $(SPEC_LIBS) $(SETTINGS) _AVScurrentv.h
# increase the version number
	./_AVSversionInc < _AVScurrentv.h > _AVStmp ; mv -f _AVStmp _AVScurrentv.h
# compile the AVS version retrieval functions
	$(CC) $(CC_FLAGS) $(INC_COMMAND) -c _AVS.c
# link the application binary
	$(LD) $(LD_FLAGS) $(LIB_COMMAND) -o $(APPNAME) $(APPNAME).o _AVS.o $(MODULE_OBJS) $(SPEC_LIBS) $(SYS_LIBS) $(LIBS)

# main application module
$(APPNAME).o : $(APPNAME).c $(SETTINGS)
	$(CC) $(CC_FLAGS) $(INC_COMMAND) -c $(APPNAME).c

# AVS header file (this creates the file if it's not present - e.g. after Git repository cloning)
_AVScurrentv.h :
	cp ../../_template/AVS/_AVScurrentv.h.template _AVScurrentv.h

# -------------------------------------
# Here follows the list of module make commands
# (Uncomment or add as many lines as necessary)

modules:
	cd $(MOD_PATH)/$(MODULE1); $(MAKE)
#	cd $(MOD_PATH)/$(MODULE2); $(MAKE)
#	cd $(MOD_PATH)/$(MODULE3); $(MAKE)
#	cd $(MOD_PATH)/$(MODULE4); $(MAKE)
#	cd $(MOD_PATH)/$(MODULE5); $(MAKE)

libraries:
#	cd $(LIBSOURCE_PATH)/$(LIB1); $(MAKE)
#	cd $(LIBSOURCE_PATH)/$(LIB2); $(MAKE)
#	cd $(LIBSOURCE_PATH)/$(LIB3); $(MAKE)

# -------------------------------------

clean:
	rm -f *.o
	rm -f $(APPNAME)
