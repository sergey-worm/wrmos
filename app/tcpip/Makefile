####################################################################################################
#
#  Makefile for user application.
#  External vars my be:
#    arch      - target arch
#    dbg       - debug flag
#    cfgdir    - path to dir that contents sys-config.h
#    blddir    - path to dir that will content build result
#
####################################################################################################

blddir     ?= build
target     ?= tcpip.elf
objs       := main.o
incflags   := -I$(cfgdir)
incflags   += -I../../lib/l4
incflags   += -I../../lib/sys
incflags   += -I../../lib/sys/$(arch)
incflags   += -I../../lib/list
incflags   += -I../../lib/wrmos/inc
incflags   += -I../../lib/psocket
incflags   += -I../../lib/cbuf
baseflags  := -O2 -Wall -Werror
cxxflags   := -std=c++11 -fno-rtti -fno-exceptions
ldflags    := --fatal-warnings
libs       :=
libs       += $(blddir)/../../lib/l4/libl4.a
libs       += $(blddir)/../../lib/sys/libsys.a
libs       += $(blddir)/../../lib/wrmos/libwrmos.a
libs       += $(blddir)/../../lib/wlibc/libwlibc.a
libs       += $(blddir)/../../lib/wstdc++/libwstdc++.a
use_libgcc := 1 # for snprintf in libwlibc.a

ifeq ($(dbg),1)
  baseflags += -DCfg_debug
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
