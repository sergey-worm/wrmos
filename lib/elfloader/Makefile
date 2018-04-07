####################################################################################################
#
#  Static library makefile. Specify params for base.mk.
#
####################################################################################################

objs      := elfloader.o
incflags  := -I$(cfgdir)
baseflags := -O2 -Wall -Werror
cxxflags  :=

ifeq ($(dbg),1)
  baseflags += -DDEBUG
else
  baseflags += -DNDEBUG
endif

include $(wrmdir)/mk/base.mk
