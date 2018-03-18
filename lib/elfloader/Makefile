####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir    ?= build
target    ?= libelfloader.a
objs      := elfloader.o
incflags  += -I$(cfgdir)
baseflags += -O2 -Wall -Werror
cxxflags  +=

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
