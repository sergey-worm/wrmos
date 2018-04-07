####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

objs      := src/api.o
objs      += src/$(arch)/api.o
incflags  := -I$(cfgdir)
incflags  += -Iinc
incflags  += -I$(wrmdir)/lib/sys
incflags  += -I$(wrmdir)/lib/sys/$(arch)
incflags  += -I$(wrmdir)/lib/wlibc/inc
baseflags := -O0 -Wall -Werror
cflags    :=
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
