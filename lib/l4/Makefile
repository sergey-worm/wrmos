####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir    ?= build
target    ?= libl4.a
objs      := $(arch)/l4api.o
incflags  := -I$(cfgdir) -I. -I../uart -I../sys -I../sys/$(arch)
incflags  += -I../../lib/wlibc/inc
baseflags := -O0 -Wall -Werror
cflags    :=
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DCfg_debug
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
