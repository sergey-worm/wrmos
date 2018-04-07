####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

files     := app_start.o
files     += log.o
files     += mpool.o
files     += ramfs.o
files     += thr.o
files     += app.o
files     += dev.o
files     += mem.o
files     += slock.o
files     += sem.o
files     += nthr.o
files     += app_user.o
objs      := $(addprefix src/,$(files))
incflags  := -Iinc
incflags  += -I$(cfgdir)
incflags  += -I$(wrmdir)/lib/sys
incflags  += -I$(wrmdir)/lib/sys/$(arch)
incflags  += -I$(wrmdir)/lib/l4/inc
incflags  += -I$(wrmdir)/lib/list
incflags  += -I$(wrmdir)/lib/elfloader
incflags  += -I$(wrmdir)/lib/wlibc/inc
baseflags := -O2 -Wall -Werror
cflags    :=
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DDEBUG
#else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
