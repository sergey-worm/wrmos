####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

objs       := console.o
incflags   := -I$(cfgdir)
incflags   += -I$(wrmdir)/lib/l4/inc
incflags   += -I$(wrmdir)/lib/wrmos/inc
incflags   += -I$(wrmdir)/lib/wlibc/inc
incflags   += -I$(wrmdir)/lib/sys
incflags   += -I$(wrmdir)/lib/sys/$(arch)
baseflags  := -O2 -Wall -Werror
baseflags  += -fno-builtin # don't use builtin printf()
baseflags  += -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
#else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
