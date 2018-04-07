####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################▫

objs      :=

ifeq ($(arch),sparc)
  objs    += sparc/hwbp.o
endif

incflags  := -I.
incflags  += -I$(wrmdir)/lib/wlibc/inc
baseflags := -Wall -Werror -pedantic -pedantic-errors
baseflags += -fno-builtin # don't use builtin printf()
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
