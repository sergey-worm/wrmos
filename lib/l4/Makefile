####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir    ?= build
target    ?= libl4.a
objs      := src/api.o
objs      += src/$(arch)/api.o
incflags  := -I$(cfgdir) -Iinc -I../uart -I../sys -I../sys/$(arch)
incflags  += -I../../lib/wlibc/inc
baseflags := -O0 -Wall -Werror
cflags    :=
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
