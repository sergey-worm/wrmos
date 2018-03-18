####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir    ?= build
target    ?= libwstdc++.a
objs      := stdc++.o
incflags  := -I../sys
incflags  += -I../wlibc/inc
baseflags += -O2 -Wall -Werror
cxxflags  += -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(arch),x86_64)
  baseflags  += -mcmodel=kernel  # it need for 64-bit kernel
endif

ifeq ($(dbg),1)
  baseflags += -DDEBUG
#else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
