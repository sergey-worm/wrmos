####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

blddir    ?= build
target    ?= libwrmos.a
srcdir    := src
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
incflags  += -I../sys
incflags  += -I../sys/$(arch)
incflags  += -I../l4
incflags  += -I../list
incflags  += -I../elfloader
incflags  += -I../wlibc/inc
baseflags := -O2 -Wall -Werror
cflags    :=
cxxflags  := -std=c++11 -fno-rtti -fno-exceptions

ifeq ($(dbg),1)
  baseflags += -DCfg_debug
  baseflags += -DDEBUG
#else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
