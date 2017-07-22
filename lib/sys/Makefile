####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################▫

blddir    ?= build
target    ?= libsys.a
objs      := panic.o

ifeq ($(arch),sparc)
  objs    += sparc/hwbp.o
endif

incflags  := -I.
baseflags := -Wall -Werror -pedantic -pedantic-errors
baseflags += -fno-builtin # don't use builtin printf()
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(arch),x86_64)
  baseflags  += -mcmodel=kernel  # FIXME:  it need for 64-bit kernel
endif

ifeq ($(dbg),1)
  baseflags += -DCfg_debug
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
