####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir     ?= build
target     ?= libconsole.a
objs       := console.o
incflags   := -I$(cfgdir)
incflags   += -I../l4/inc
incflags   += -I../wrmos/inc
incflags   += -I../wlibc/inc
incflags   += -I../sys
incflags   += -I../sys/$(arch)
baseflags  := -O2 -Wall -Werror
baseflags  += -fno-builtin # don't use builtin printf()
baseflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
#else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
