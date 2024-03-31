# Digithell HyperGeneric Makefile
# (module)
# (C) 2005 Digithell, Inc. (Pavel Strachota)
# =====================================

include ../../_settings/settings.mk

# -------------------------------------

MODULENAME = mprintf


# uncomment to activate MPI rank-0-only printing functions
# (incompatible with virtual rank mapping in some Digithell applications)

#CC = mpicc
#LD = mpicc
#MACRO_DEFINITIONS := $(MACRO_DEFINITIONS) -D PARA


# -------------------------------------

$(MODULENAME).o : $(MODULENAME).c $(INC_PATH)/$(MODULENAME).h $(SETTINGS)
	$(CC) $(CC_FLAGS) $(INC_COMMAND) -c $(MODULENAME).c

# -------------------------------------

.PHONY : clean
clean :
	rm -f *.o
