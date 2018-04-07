####################################################################################################
#
#  Makefile - root makefile for WrmOS.
#
####################################################################################################

help:
	@echo "WrmOS build system."
	@echo ""
	@echo "Use:"
	@echo "  make build   P=<path/*.prj> B=<build-dir> [ E=<external-dir> ]"
	@echo "  make clean   P=<path/*.prj> B=<build-dir> [ E=<external-dir> ]"
	@echo ""
	@echo "Examples:"
	@echo "  make build   P=cfg/prj/hello-qemu-leon3.prj B=../build-hello -j"
	@echo "  make clean   P=cfg/prj/hello-qemu-leon3.prj B=../build-hello"
	@echo ""

ifeq ($P,)
$(warning No P=<path/*.prj>. Use "make help".)
endif
ifeq ($B,)
$(warning No B=<build-dir>. Use "make help".)
endif

export extdir   = $(realpath $E)
export blddir   = $(abspath $B)
export rtblddir = $(blddir)
export wrmdir   = $(shell pwd)
export cfgdir   = $(blddir)/config
export prj_file = $(realpath $(P))

include $(prj_file)

ramfs_files = $(foreach f,$(ldr_ramfs),$(word 2,$(subst :, ,$f)))
ramfs_elfs = $(filter %.elf,$(ramfs_files))
ramfs_deps = $(filter-out %sigma0.elf,$(filter-out %kernel.elf,$(ramfs_elfs)))  # to avoid duplicate building

export ramfs_elfs

.PHONY:  configs build clean rebuild $(blddir)/ldr/$(ldr_file)

cfg_files = $(foreach f,sys-config.h krn-config.h ldr-config.h,$(cfgdir)/$f)

configs:  $(cfg_files)
	@$(echo) "Ok:   $(notdir $(cfg_files))"

build:  $(cfg_files)
	$(v)+make $(blddir)/ldr/$(ldr_file)

# check each ELF from $(ramfs_files) and build bootloader
$(blddir)/ldr/$(ldr_file):  $(ramfs_deps)
	@$(echo) "$(color_build)Build $(notdir $@)$(color_off)"
	$(v)+make -C $(wrmdir)/ldr \
		target=$(ldr_file) \
		blddir=$(blddir)/ldr \
		ldr_ramfs="$(ldr_ramfs)" \
		ramfs_files="$(ramfs_files)" \
		dbg=$(ldr_dbg) \
		uart=$(ldr_uart)

clean_elfs = $(ramfs_elfs) $(blddir)/ldr/$(ldr_file)
clean_deps = $(addsuffix .CLEANLIBS,$(clean_elfs)) $(addsuffix .CLEAN,$(clean_elfs))

# clean each ELF from $(ramfs_files), bootloader and configs
clean:  $(clean_deps)
	$(v)rm -f $(cfgdir)/*
	$(v)rm -f $(blddir)/ldr/ramfs.generate.c
	@$(echo) "$(color_ok)Ok:   clean$(color_off)"

rebuild:
	$(v)+make clean
	$(v)+make build

include mk/base.mk
