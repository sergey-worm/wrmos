####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir    ?= build
target    ?= libwlibc.a
files     := string.o ctype.o stdio.o assert.o tests.o
files     += errno.o stdlib.o unistd.o
files     += fixme.o
files     += # libm.o  # use libm instead
objs      := $(addprefix src/,$(files))
incflags  := -Iinc
incflags  += -I$(cfgdir)
incflags  += -I../../lib/l4
incflags  += -I../../lib/sys
incflags  += -I../../lib/sys/$(arch)
baseflags := -O2 -Wall -Werror
cflags    := -std=gnu99
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions
libs      :=

ifeq ($(arch),x86_64)
  baseflags  += -mcmodel=kernel  # it need for 64-bit kernel
endif

ifeq ($(dbg),1)
  baseflags += -DCfg_debug
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
