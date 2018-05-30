####################################################################################################
#
#  Makefile for user application.
#  External vars my be:
#    arch      - target arch
#    dbg       - debug flag
#    cfgdir    - path to dir that contents sys-config.h
#    blddir    - path to dir that will content build result
#    target    - target elf file name (hello.elf)
#
####################################################################################################

objs       := main.o
incflags   := -I$(cfgdir)
baseflags  := -O2 -Wall -Werror
cxxflags   := -std=c++11 -fno-rtti -fno-exceptions
ldflags    :=
libs       := $(rtblddir)/lib/l4/libl4.a
libs       += $(rtblddir)/lib/wrmos/libwrmos.a
libs       += $(rtblddir)/lib/wlibc/libwlibc.a
libs       += $(rtblddir)/lib/wstdc++/libwstdc++.a

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
