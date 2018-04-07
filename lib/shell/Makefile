####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################▫

objs      := shell.o
incflags  := -I$(wrmdir)/lib/list
incflags  += -I$(wrmdir)/lib/sys
incflags  += -I$(wrmdir)/lib/wlibc/inc
baseflags := -O2 -Wall -Werror
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
