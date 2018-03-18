####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir     ?= build
target     ?= libpsocket.a
objs       := psocket.o
incflags   := -I$(cfgdir)
incflags   += -I../l4
incflags   += -I../wrmos
incflags   += -I../sys
incflags   += -I../sys/$(arch)
baseflags  := -O2 -Wall -Werror
baseflags  += -fno-builtin # don't use builtin printf()

ifeq ($(dbg),1)
  baseflags += -DDEBUG
#else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
