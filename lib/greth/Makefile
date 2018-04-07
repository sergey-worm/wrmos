####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

objs      := greth.o
incflags  := 
baseflags := -Wall -Werror -pedantic -pedantic-errors
baseflags += -fno-builtin # don't use builtin printf()

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
