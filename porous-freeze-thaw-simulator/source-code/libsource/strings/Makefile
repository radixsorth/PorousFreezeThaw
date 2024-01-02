# Digithell HyperGeneric Makefile
# (static library)
# (C) 2005 Digithell, Inc. (Pavel Strachota)
# =====================================

include ../../_settings/settings.mk

# -------------------------------------

LIBNAME = strings

# -------------------------------------
MODULE_SOURCES = $(wildcard *.c)
MODULES = $(patsubst %.c,%.o,$(MODULE_SOURCES))

$(LIB_PATH)/lib$(LIBNAME).a : $(MODULES) $(SETTINGS)
	ar -r $(LIB_PATH)/lib$(LIBNAME).a $(filter %.o,$?)

%.o : %.c $(INC_PATH)/$(LIBNAME).h $(SETTINGS)
	$(CC) $(CC_FLAGS) $(INC_COMMAND) -c $<

# -------------------------------------

.PHONY : clean
clean :
	rm -f *.o
	rm -f $(LIB_PATH)/lib$(LIBNAME).a
