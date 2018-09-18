####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

files     := mutex.o cond.o attr.o mutexattr.o pthread.o sched.o
objs      := $(addprefix src/,$(files))
incflags  := -Iinc
incflags  += -I$(cfgdir)
incflags  += -I$(wrmdir)/lib/wlibc/inc
incflags  += -I$(wrmdir)/lib/l4/inc
incflags  += -I$(wrmdir)/lib/sys
incflags  += -I$(wrmdir)/lib/sys/$(arch)
incflags  += -I$(wrmdir)/lib/wrmos/inc
baseflags := -O2 -Wall -Werror
cflags    := -std=gnu99
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
