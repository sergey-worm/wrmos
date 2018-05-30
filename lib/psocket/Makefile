####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

objs       := psocket.o
incflags   := -I$(cfgdir)
incflags   += -I$(wrmdir)/lib/l4
incflags   += -I$(wrmdir)/lib/wrmos
incflags   += -I$(wrmdir)/lib/sys
incflags   += -I$(wrmdir)/lib/sys/$(arch)
baseflags  := -O2 -Wall -Werror
baseflags  += -fno-builtin # don't use builtin printf()

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
