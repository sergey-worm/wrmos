####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

files     := string.o ctype.o stdio.o assert.o panic.o tests.o time.o poll.o ioctl.o uio.o
files     += errno.o stdlib.o unistd.o wchar.o wctype.o locale.o link.o isnan.o fixme_c++.o
files     += wlibc_cb.o
files     += fixme.o
files     += # libm.o  # use libm instead
objs      := $(addprefix src/,$(files))
incflags  := -Iinc
incflags  += -I$(cfgdir)
incflags  += -I$(wrmdir)/lib/l4/inc
incflags  += -I$(wrmdir)/lib/sys
incflags  += -I$(wrmdir)/lib/sys/$(arch)
baseflags := -O2 -Wall -Werror
cflags    := -std=gnu99
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
