####################################################################################################▫
#
#  Base rules for some targets.
#
#  Next params SHOULD be set by top level Makefile:
#
#    blddir     = build
#    target     = target
#    objs       = xxx.o
#
#  Next params MAY be set by top level Makefile:
#
#    incflags   = -Iinc
#    baseflags  =
#    asflags    =
#    cflags     =
#    cxxflags   =
#    ldflags    =
#    libs       = -lm
#    use_libgcc = 1
#
####################################################################################################
#---------------------------------------------------------------------------------------------------
# configure
#---------------------------------------------------------------------------------------------------
# make options
MAKEFLAGS += --no-builtin-rules      # disable the built-in implicit rules (printk)
MAKEFLAGS += --no-builtin-variables  # disable the built-in variable settings

# build environment
gccprefix ?= set-gccprefix-please #$(arch)-linux-
blddir    ?= build

echo=/bin/echo -e

color_off=\e[0m
color_black=\e[0;30m
color_red=\e[0;31m
color_green=\e[0;32m
color_yellow=\e[0;33m
color_blue=\e[0;34m
color_purple=\e[0;35m
color_cyan=\e[0;36m
color_white=\e[0;37m

color_compile=$(color_cyan)
color_ok=$(color_green)
color_err=$(color_red)

# verbose
ifneq ($V,1)
v = @
endif

# prepare incoming params to use in rules
# add -Mx to generate *.d files
baseflags += -MD -MP $(incflags)
ifdef arch_ver
  baseflags += -march=$(arch_ver)
endif
asflags   += $(baseflags)
cflags    += $(baseflags) -std=gnu99
cxxflags  += $(baseflags) #-std=c++11
ldflags   += --warn-common
objects   := $(addprefix $(blddir)/,$(objs))
# add path to libgcc if need
ifdef use_libgcc
  libs += $(shell $(gccprefix)gcc -print-libgcc-file-name)
endif
#---------------------------------------------------------------------------------------------------
# ~ configure
#---------------------------------------------------------------------------------------------------
#---------------------------------------------------------------------------------------------------
# rules
#---------------------------------------------------------------------------------------------------
all:
	@#$(echo) "libs=$(libs)"
	@#$(echo) "objs=$(objs)"
	@#$(echo) "objects=$(objects)"
	@#$(echo) "blddir=$(blddir)"
	make prebuild-target
	@mkdir -p $(blddir)
	make $(blddir)/$(target)
	make postbuild-target
	@$(echo) "$(color_ok)[BUILD] $(target), ok.$(color_off)"

# executing before building of target
# may be re-defined in top-level Makefile
prebuild-target:

# executing after building of target
# may be re-defined in top-level Makefile
postbuild-target:

# make disk image with size rounded up to MByte
$(blddir)/%.img:  $(blddir)/%.bin Makefile
	@$(echo) "$(color_compile)[DISK IMAGE] $< --> $@$(color_off)"
	dd if=/dev/zero of=$@ bs=1M count=$$(($$(du -b $< | awk '{print $$1}') / 1024 / 1024 + 8))
	dd if=$< of=$@ conv=notrunc

$(blddir)/%.bin:  $(blddir)/%.elf Makefile
	@$(echo) "$(color_compile)[OBJCOPY] $< --> $@$(color_off)"
	$(v)$(gccprefix)objcopy -Obinary  $< $@

$(blddir)/%.elf:  $(objects) $(blddir)/link.ld $(libs)
	@$(echo) "$(color_compile)[LD] $(notdir $(objects) $(libs)) link.ld --> $@$(color_off)"
	$(v)$(gccprefix)ld $(objects) --start-group $(libs) $(ldflags) --end-group -T$(blddir)/link.ld -o $@

$(blddir)/link.ld:  $(blddir)/link.ld.S Makefile
	@$(echo) "$(color_compile)[CC] $< --> $@$(color_off)"
	$(v)$(gccprefix)gcc $(baseflags) -E -P -C $< -o $@ 

# use:
# a) local arch link file
# b) if it is absent - common local link file
# c) if it is absent - app-link file
$(blddir)/link.ld.S:
	@if [ -a $(arch)/link.ld.S ] ; then \
		$(echo) "$(color_compile)[CP] $(arch)/link.ld.S --> $@$(color_off)"; \
		cp $(arch)/link.ld.S $@; \
	elif [ -a link.ld.S ] ; then \
		$(echo) "$(color_compile)[CP] link.ld.S --> $@$(color_off)"; \
		cp link.ld.S $@; \
	else \
		$(echo) "$(color_compile)[CP] ../link.ld.S --> $@$(color_off)"; \
		cp $(wrmdir)/app/link.ld.S $@; \
	fi;

$(blddir)/lib%.a:  $(objects) Makefile
	@$(echo) "$(color_compile)[AR] $(notdir $(objects)) --> $@$(color_off)"
	$(v)$(gccprefix)ar -r $@ $(objects) #2>/dev/null

$(blddir)/%.o:  %.cpp Makefile
	@mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[C++] $< --> $@$(color_off)"
	$(v)$(gccprefix)g++ $(cxxflags) -c $< -o $@

$(blddir)/%.o:  %.cc Makefile
	@mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[C++] $< --> $@$(color_off)"
	$(v)$(gccprefix)g++ $(cxxflags) -c $< -o $@

$(blddir)/%.o:  %.c Makefile
	@mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[CC] $< --> $@$(color_off)"
	$(v)$(gccprefix)gcc $(cflags) -c $< -o $@

$(blddir)/%.o:  %.S Makefile
	$(v)mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[AS] $< --> $@$(color_off)"
	$(v)$(gccprefix)gcc $(asflags) -c $< -o $@

clean:  preclean-target
	@$(echo) "$(color_compile)[RM] clean$(color_off)"
	$(v)rm -f $(objects) $(blddir)/link.ld $(blddir)/link.ld.S \
		$(basename $(blddir)/$(target)).*

# executing before clearing of target
# may be re-defined in top-level Makefile
preclean-target:

# special built-in target to disable intermediate files deletion (*.d, *.o, ...)
.SECONDARY:
#---------------------------------------------------------------------------------------------------
# ~ rules
#---------------------------------------------------------------------------------------------------
