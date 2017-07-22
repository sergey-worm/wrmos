####################################################################################################
#
#  Kernel makefile.
#
#  Make rules is located in make.mk. Here need to define params for it:
#  $(blddir), $(target), $(objs), $(incflags), $(libs), etc.
#  See base.mk for additional info.
#
####################################################################################################

#------------------------
#  pre-configure params  
#------------------------

use_baseflags  := -nostdlib -nostartfiles -ffreestanding -nodefaultlibs
use_baseflags  += -fno-builtin -fomit-frame-pointer
use_baseflags  += -fno-exceptions
use_baseflags  += -O2

use_warns  := -Wall -Wextra -Wshadow -Wcast-align -Wwrite-strings
use_warns  += -Wredundant-decls
use_warns  += -Wno-attributes -Wno-deprecated-declarations
use_warns  += -Wno-div-by-zero -Wno-endif-labels -Wfloat-equal
use_warns  += -Wformat=2 -Wno-format-extra-args -Winit-self
use_warns  += -Winvalid-pch -Wmissing-format-attribute
use_warns  += -Wmissing-include-dirs -Wno-multichar
use_warns  += -Wredundant-decls -Wshadow
use_warns  += -Wno-sign-compare -Wswitch -Wsystem-headers -Wundef
use_warns  += -Wno-pragmas -Wno-unused-but-set-parameter
use_warns  += -Wno-unused-but-set-variable -Wno-unused-result
use_warns  += -Wwrite-strings -Wdisabled-optimization -Wpointer-arith
use_warns  += -Werror

ifeq ($(arch),x86_64)
  use_baseflags  += -mcmodel=kernel  # FIXME:  it need for 64-bit kernel
endif

ifeq ($(dbg),1)
  use_baseflags += -g           # add GDB info
  use_baseflags += -DCfg_debug  # define dbg macro
  use_baseflags += -DDEBUG
else
  use_baseflags += -DNDEBUG
  use_warns     += -Winline     # to check code size limit
endif

ifeq ($(klog),1)
  use_baseflags += -DCfg_klog   # loging output to kernel log sybsystem
endif

use_libs   += wlibc/libwlibc.a
use_libs   += wstdc++/libwstdc++.a
use_libs   += sys/libsys.a
use_libs   += shell/libshell.a

#------------------------------
#  prepare params for base.bk  
#------------------------------

blddir     ?= build
target     ?= kernel.elf
objs       := $(arch)/crt0.o
objs       += $(arch)/bootstrap.o
objs       += $(arch)/arch.o
objs       += $(arch)/entry.o
objs       += thread.o
objs       += threads.o
objs       += singletons.o
objs       += kip.o
objs       += printk.o
objs       += kentry.o
objs       += syscalls.o
objs       += ipc.o
objs       += startup.o
objs       += common.o
incflags   := -I.
incflags   += -I$(cfgdir)
incflags   += -I$(arch)
incflags   += -I../lib/sys/$(arch)
incflags   += -I../lib/sys
incflags   += -I../lib/list
incflags   += -I../lib/l4
incflags   += -I../lib/wlibc/inc
incflags   += -I../lib/shell
incflags   += -I../lib/cbuf
incflags   += -I../lib/uart/$(uart)
incflags   += -I../lib/timer/$(timer)
incflags   += -I../lib/intc/$(intc)
incflags   += -I../lib/mmu/$(mmu)
baseflags  += $(use_baseflags) $(use_warns)
cxxflags   := -fno-rtti # to avoid undefined reference to `vtable ...'
#cxxflags  += -std=c++11
ldflags    := --fatal-warnings
ldflags    += -zmax-page-size=0x1000
libs       += $(addprefix $(blddir)/../lib/,$(use_libs))
use_libgcc := 1 # for snprintf in libwlibc.a

include ../mk/base.mk
include ../mk/syscfg.mk

postbuild-target: update-next-load-addr

