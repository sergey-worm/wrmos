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
target     ?= chello.elf
objs       := main.o
incflags   := -I$(cfgdir)
incflags   += -I../../lib/l4/inc
incflags   += -I../../lib/sys
incflags   += -I../../lib/sys/$(arch)
incflags   += -I../../lib/list
incflags   += -I../../lib/wrmos/inc
incflags   += -I../../lib/console
baseflags  := -O2 -Wall -Werror
cxxflags   := -std=c++11 -fno-rtti -fno-exceptions
ldflags    := --fatal-warnings
ldflags    += -zmax-page-size=0x1000
libs       :=
libs       += $(blddir)/../../lib/l4/libl4.a
libs       += $(blddir)/../../lib/sys/libsys.a
libs       += $(blddir)/../../lib/wrmos/libwrmos.a
libs       += $(blddir)/../../lib/wlibc/libwlibc.a
libs       += $(blddir)/../../lib/wstdc++/libwstdc++.a
libs       += $(blddir)/../../lib/console/libconsole.a
use_libgcc := 1 # for snprintf in libwlibc.a

ifeq ($(dbg),1)
  baseflags += -DDEBUG
#else
  baseflags += -DNDEBUG
endif

include ../../mk/base.mk
