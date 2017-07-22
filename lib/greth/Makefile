####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir    ?= build
target    ?= libgreth.a
objs      := greth.o
incflags  := 
baseflags := -Wall -Werror -pedantic -pedantic-errors
baseflags += -fno-builtin # don't use builtin printf()

ifeq ($(dbg),1)
  baseflags += -DCfg_debug
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
