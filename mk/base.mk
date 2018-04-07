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
#    libs       = $(blddir)/lib/xxx/xxx.a  # libs to build and use
#    libs_ext   = path/to/lib.a            # external precompiled libs to use without building
#
####################################################################################################
#---------------------------------------------------------------------------------------------------
#  CONFIGURE
#---------------------------------------------------------------------------------------------------
# make options
MAKEFLAGS += --no-print-directory    # to decrease output
SHELL     := /bin/bash
echo      := /bin/echo -e

# build environment
gccprefix ?= set-gccprefix-please
blddir    ?= set-build-please

ifndef NOCOLOR
color_off      =\e[0m
color_black    =\e[0;30m
color_red      =\e[0;31m
color_green    =\e[0;32m
color_yellow   =\e[1;33m
color_blue     =\e[0;34m
color_purple   =\e[0;35m
color_cyan     =\e[0;36m
color_white    =\e[0;37m
color_black_b  =\e[1;30m
color_red_b    =\e[1;31m
color_green_b  =\e[1;32m
color_yellow_b =\e[1;33m
color_blue_b   =\e[1;34m
color_purple_b =\e[1;35m
color_cyan_b   =\e[1;36m
color_white_b  =\e[1;37m
color_compile=$(color_cyan)
color_build=$(color_blue_b)
color_build_lib=$(color_blue)
color_ok=$(color_green)
color_err=$(color_red)
endif

# verbose
ifneq ($V,1)
v = @
endif

# prepare incoming params to use in rules
# add -Mx to generate *.d files
baseflags += -MD -MP $(incflags)
ifeq ($(arch),x86_64)
  baseflags += -mcmodel=kernel # it need for linking 64-bit kernel
endif
ifdef arch_ver
  baseflags += -march=$(arch_ver)
endif
asflags   += $(baseflags)
cflags    += $(baseflags) #-std=gnu99
cxxflags  += $(baseflags)
ldflags   += --warn-common --fatal-warnings
ldflags   += -zmax-page-size=0x1000 # to don't inflate elf size for 64-bit archs
objects    = $(addprefix $(blddir)/,$(objs))
libgcc     = $(shell $(gccprefix)gcc -print-libgcc-file-name)
dep_libs   = $(foreach f,$(libs),$(abspath $f))

include $(prj_file)

# export some project specifics variables
export plt_uart
export arch
export gccprefix

#---------------------------------------------------------------------------------------------------
#  COMMON RULES
#---------------------------------------------------------------------------------------------------

#  main rule - entry point for current $(target)
all:  $(blddir)/$(target)
	@if [ "$(suffix $(target))" != ".a" ]; then $(echo) "$(color_ok)Ok:   $(target)$(color_off)"; fi

# build-once rule - to sync parallel building
%.ONCE:
	@mkdir -p $(dir $@)
	@+flock $*.LOCK -c "$(MAKE) $*.REAL"

# helper to determine using $(wrmdir) or $(extdir):
# a) if app or lib exists in $(extdir) - use it
# b) else use $(wrmdir)
# $1 - app or lib subpath (app/hello or lib/wlibc
getdir=$$(if [ -d $(extdir)/$1 ]; then echo $(extdir)/$1; else echo $(wrmdir)/$1; fi)

include $(wrmdir)/mk/syscfg.mk

#---------------------------------------------------------------------------------------------------
#  ELF CHECK RULES
#---------------------------------------------------------------------------------------------------

.PHONY:  $(foreach f,$(ramfs_elfs),$f.CHECK)

# $1 - path to elf
define create_elf_rules
$1:  $1.CHECK
	@# this line must be here, else $1 is not considered renewed

$1.CHECK:
	@+make $$(subst .CHECK,.ONCE,$$@)
endef

# create rules for all ELFs from ramfs_elfs
# not use then $(target) is bootloader.xxx to decrease re-checking of ELFs
ifneq ($(target),$(ldr_file))
$(foreach f,$(filter-out $(blddir)/$(target),$(ramfs_elfs)),$(eval $(call create_elf_rules,$f)))
endif

$(rtblddir)/krn/kernel.elf.REAL:
	@$(echo) "$(color_build)Chk:  $(notdir $(basename $@))$(color_off)"
	$(v)+make -C $(wrmdir)/krn \
		target=kernel.elf blddir=$(rtblddir)/krn dbg=$(krn_dbg) \
		klog=$(krn_log) uart=$(krn_uart) timer=$(krn_timer) intc=$(krn_intc) mmu=$(krn_mmu) \
		ram_start=$(ram_start)

$(rtblddir)/app/%.elf.REAL:
	@$(echo) "$(color_build)Chk:  $(notdir $(basename $@))$(color_off)"
	$(v)+make -C $(call getdir,app/$(dir $*)) \
		target=$(notdir $(basename $@)) blddir=$(abspath $(dir $@)) dbg=$(app_dbg)

#---------------------------------------------------------------------------------------------------
#  LIB CHECK RULES
#---------------------------------------------------------------------------------------------------

.PHONY:  $(foreach f,$(dep_libs),$f.CHECK)

# $1 - path to lib
define create_lib_rules
$1:  $1.CHECK
	@# this line must be here, else $1 is not considered renewed

$1.CHECK:
	@+make $$(subst .CHECK,.ONCE,$$@)
endef

# create rules for current $(dep_libs)
$(foreach f,$(filter-out $(blddir)/$(target),$(dep_libs)),$(eval $(call create_lib_rules,$f)))

$(rtblddir)/lib/%.a.REAL:
	@$(echo) "$(color_build)Chk:$(color_build_lib)  $(notdir $(basename $@))$(color_off)"
	$(v)+make -C $(call getdir,lib/$(dir $*)) \
		target=$(notdir $(basename $@)) blddir=$(abspath $(dir $@)) dbg=$(lib_dbg)

#---------------------------------------------------------------------------------------------------
#  BUILD RULES
#---------------------------------------------------------------------------------------------------

# make disk image with size rounded up to MByte
$(blddir)/%.img:  $(blddir)/%.bin
	@$(echo) "$(color_compile)[IMG] $(notdir $(target)):  $(notdir $<) --> $(notdir $@)$(color_off)"
	$(v)dd if=/dev/zero of=$@ bs=1M count=$$(($$(du -b $< | awk '{print $$1}') / 1024 / 1024 + 8)) 2>/dev/null
	$(v)dd if=$< of=$@ conv=notrunc 2>/dev/null

$(blddir)/%.bin:  $(blddir)/%.elf
	@$(echo) "$(color_compile)[BIN] $(notdir $(target)):  $(notdir $<) --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)objcopy -Obinary  $< $@

$(blddir)/%.elf:  $(objects) $(blddir)/link.ld $(dep_libs) $(libs_ext)
	@$(echo) "$(color_compile)[LD]  $(notdir $(target)):  $(notdir $(objects) $(libs)) link.ld --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)ld $(objects) --start-group $(dep_libs) $(libs_ext) $(libgcc) $(ldflags) --end-group -T$(blddir)/link.ld -o $@

$(blddir)/link.ld:  $(blddir)/link.ld.S $(ldaddr_file)
	@$(echo) "$(color_compile)[CC]  $(notdir $(target)):  $(notdir $<) --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)gcc $(baseflags) -E -P -C $< -o $@ 

# use:
# a) local arch link file
# b) if it is absent - common local link file
# c) if it is absent - app-link file
$(blddir)/link.ld.S:
	@mkdir -p $(dir $@)
	@if [ -a $(arch)/link.ld.S ] ; then \
		$(echo) "$(color_compile)[CP]  $(notdir $(target)):  $(arch)/link.ld.S --> $(notdir $@)$(color_off)"; \
		cp $(arch)/link.ld.S $@; \
	elif [ -a link.ld.S ] ; then \
		$(echo) "$(color_compile)[CP]  $(notdir $(target)):  link.ld.S --> $(notdir $@)$(color_off)"; \
		cp link.ld.S $@; \
	else \
		$(echo) "$(color_compile)[CP]  $(notdir $(target)):  ../link.ld.S --> $(notdir $@)$(color_off)"; \
		cp $(wrmdir)/app/link.ld.S $@; \
	fi;

$(blddir)/lib%.a:  $(objects)
	@$(echo) "$(color_compile)[AR]  $(notdir $(target)):  $(notdir $(objects)) --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)ar -r $@ $(objects) 2>/dev/null

-include $(objects:.o=.d)

$(blddir)/%.o:  %.cpp
	@mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[C++] $(notdir $(target)):  $(notdir $<) --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)g++ $(cxxflags) -c $< -o $@

$(blddir)/%.o:  %.cc $(cc_deps)
	@mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[C++] $(notdir $(target)):  $(notdir $<) --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)g++ $(cxxflags) -c $< -o $@

$(blddir)/%.o:  %.c
	@mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[CC]  $(notdir $(target)):  $(notdir $<) --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)gcc $(cflags) -c $< -o $@

$(blddir)/%.o:  %.S
	$(v)mkdir -p $(dir $@)
	@$(echo) "$(color_compile)[AS]  $(notdir $(target)):  $(notdir $<) --> $(notdir $@)$(color_off)"
	$(v)$(gccprefix)gcc $(asflags) -c $< -o $@

#---------------------------------------------------------------------------------------------------
#  CLEAN RULES
#---------------------------------------------------------------------------------------------------

# common rule to clean:  app, lib, kernel, bootloader
%.CLEAN:
	$(v)+make -C $(call getdir,$(subst $(rtblddir)/,,$(dir $@))) clean-files \
		target=$(notdir $(basename $@)) \
		blddir=$(dir $@)

# common rule to clean libs for:  app, lib, kernel, bootloader
%.CLEANLIBS:
	$(v)+make -C $(call getdir,$(subst $(rtblddir)/,,$(dir $@))) clean-libs \
		target=$(notdir $(basename $@)) \
		blddir=$(dir $@)

# clean files for elf-target, must be double-colon rule to top level Makefile may extend it
clean-files::
	@$(echo) "$(color_compile)[CLN] $(notdir $(target))$(color_off)"
	$(v)rm -f $(objects) $(objects:.o=.d) $(blddir)/link.* $(basename $(blddir)/$(target)).*

# clean libs for elf-target, must be double-colon rule to LIB may extend it
clean-libs::
	$(v)+if [ "$(dep_libs)" != "" ]; then make $(addsuffix .CLEAN,$(dep_libs)); fi

# special built-in target to disable intermediate files deletion (*.d, *.o, ...)
.SECONDARY:
