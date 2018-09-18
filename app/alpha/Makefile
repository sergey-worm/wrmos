####################################################################################################
#
#  Makefile for system application 'alpha'.
#  External vars my be:
#    arch      - target arch
#    dbg       - debug flag
#    cfgdir    - path to dir that contents sys-config.h
#    blddir    - path to dir that will content build result
#    target    - target elf file name (alpha.elf)
#
####################################################################################################

objs        := $(arch)/crt0.o main.o
incflags    := -I$(cfgdir)
incflags    += -I$(wrmdir)/lib/l4/inc
incflags    += -I$(wrmdir)/lib/sys
incflags    += -I$(wrmdir)/lib/sys/$(arch)
incflags    += -I$(wrmdir)/lib/containers
incflags    += -I$(wrmdir)/lib/elfloader
incflags    += -I$(wrmdir)/lib/wrmos/inc
incflags    += -I$(wrmdir)/lib/wlibc/inc
baseflags   := -O2 -Wall -Werror
cxxflags    := -std=c++11 -fno-rtti -fno-exceptions
ldflags     := #-nostdlib
libs        := $(rtblddir)/lib/wlibc/libwlibc.a
libs        += $(rtblddir)/lib/wstdc++/libwstdc++.a
libs        += $(rtblddir)/lib/sys/libsys.a
libs        += $(rtblddir)/lib/l4/libl4.a
libs        += $(rtblddir)/lib/wrmos/libwrmos.a
libs        += $(rtblddir)/lib/elfloader/libelfloader.a
ldaddr_file := $(cfgdir)/alpha-load-addr.h

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
