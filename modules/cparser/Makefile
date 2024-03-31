# Digithell HyperGeneric Makefile
# (module)
# (C) 2005 Digithell, Inc. (Pavel Strachota)
# =====================================

include ../../_settings/settings.mk

# -------------------------------------

MODULENAME = cparser

# -------------------------------------

$(MODULENAME).o : $(MODULENAME).c $(INC_PATH)/$(MODULENAME).h $(SETTINGS)
	$(CC) $(CC_FLAGS) $(INC_COMMAND) -c $(MODULENAME).c

# -------------------------------------

.PHONY : clean
clean :
	rm -f *.o
