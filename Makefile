####################################################################################################
#
#  Makefile - root makefile for WrmOS.
#
####################################################################################################

SHELL = /bin/bash
echo  = /bin/echo -e

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

color_bld=$(color_white)
color_ok=$(color_green)
color_err=$(color_red)

# verbose
ifneq ($V,1)
v = @
endif

extdir   = $(realpath $E)
blddir   = $(realpath $B)
prj_file = $(P)

include $(prj_file)

help:
	@echo "WrmOS build system."
	@echo ""
	@echo "Use:"
	@echo "  make build   P=<path/*.prj> B=<build-dir> [ E=<external-dir> ]"
	@echo "  make rebuild P=<path/*.prj> B=<build-dir> [ E=<external-dir> ]"
	@echo "  make clean   P=<path/*.prj> B=<build-dir> [ E=<external-dir> ]"
	@echo ""
	@echo "Examples:"
	@echo "  make build   P=cfg/prj/hello-qemu-leon3.prj B=../build-hello -j"
	@echo "  make rebuild P=cfg/prj/hello-qemu-leon3.prj B=../build-hello"
	@echo "  make clean   P=cfg/prj/hello-qemu-leon3.prj B=../build-hello"
	@echo ""


# default

build: build-all
clean: clean-all

# all

build-all:
	$(v)make build-configs blddir=$B
	$(v)make build-libs
	$(v)make build-kernel
	$(v)make build-apps
	$(v)make build-bootloader


clean-all:
	$(v)make clean-configs
	$(v)make clean-libs
	$(v)make clean-kernel
	$(v)make clean-apps
	$(v)make clean-bootloader

rebuild-all:
	$(v)make clean
	$(v)make build

# os

build-os:
	$(v)make build-configs blddir=$B
	$(v)make build-libs
	$(v)make build-kernel
	#$(v)make build-apps

clean-os:
	$(v)make clean-configs blddir=$B
	$(v)make clean-libs    blddir=$B
	$(v)make clean-kernel  blddir=$B gccprefix=$(gccprefix)
	#$(v)make clean-apps   blddir=$B gccprefix=$(gccprefix)

rebuild-os:
	$(v)make clean-os
	$(v)make build-os

# img

build-img:
	$(v)make build-apps
	$(v)make build-bootloader


clean-img:
	$(v)make clean-apps
	$(v)make clean-bootloader

rebuild-img:
	$(v)make clean-img
	$(v)make build-img

# rules for configs
include mk/syscfg.mk

build-configs:
	@$(echo) "$(color_bld)+-----------------+$(color_off)"
	@$(echo) "$(color_bld)|  Build configs  |$(color_off)"
	@$(echo) "$(color_bld)+-----------------+$(color_off)"
	@# rules in syscfg.mk
	make $(blddir)/config/sys-config.h
	make $(blddir)/config/krn-config.h
	make $(blddir)/config/ldr-config.h
	make $(blddir)/config/next-load-addr.h

clean-configs:
	@$(echo) "$(color_bld)+-----------------+$(color_off)"
	@$(echo) "$(color_bld)|  Clean configs  |$(color_off)"
	@$(echo) "$(color_bld)+-----------------+$(color_off)"
	$(v)rm -f $(blddir)/config/sys-config.h
	$(v)rm -f $(blddir)/config/krn-config.h
	$(v)rm -f $(blddir)/config/ldr-config.h
	$(v)rm -f $(blddir)/config/next-load-addr.h

# path may be:
# 1) lib/name     - internal app
# 2) ext:lib/name - external app
build-libs:
	@for lib in $(lib_paths); do \
		line="$$(for i in `seq 1 $${#lib}`; do echo -n -; done)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		$(echo) "$(color_bld)|  Build lib $$lib  |$(color_off)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		arr=($$(echo $$lib | tr ":" " ")); \
		if [ $${arr[0]} == "ext" ]; then \
			dir=$(extdir)/; \
			name=$${arr[1]}; \
		else \
			dir=""; \
			name=$${arr[0]}; \
		fi; \
		make -C $$dir$$name V=$V blddir=$(blddir)/$$name \
			cfgdir=$(blddir)/config arch=$(arch) arch_ver=$(arch_ver) dbg=$(lib_dbg) \
			wrmdir=$$(pwd) gccprefix=$(gccprefix); \
		if [ "$$?" != "0" ]; then exit 10; fi; \
		echo; \
	done

# path may be:
# 1) lib/name     - internal app
# 2) ext:lib/name - external app
clean-libs:
	@for lib in $(lib_paths); do \
		line="$$(for i in `seq 1 $${#lib}`; do echo -n -; done)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		$(echo) "$(color_bld)|  Clean lib $$lib  |$(color_off)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		arr=($$(echo $$lib | tr ":" " ")); \
		if [ $${arr[0]} == "ext" ]; then \
			dir=$(extdir)/; \
			name=$${arr[1]}; \
		else \
			dir=""; \
			name=$${arr[0]}; \
		fi; \
		make -C $$dir$$name clean V=$V blddir=$(blddir)/$$name arch=$(arch) wrmdir=$$(pwd); \
		echo; \
	done

build-kernel:
	@$(echo) "$(color_bld)+----------------+$(color_off)"
	@$(echo) "$(color_bld)|  Build kernel  |$(color_off)"
	@$(echo) "$(color_bld)+----------------+$(color_off)"
	$(v)make -C $(krn_path) V=$V target=$(krn_elf) blddir=$(blddir)/$(krn_path) \
		cfgdir=$(blddir)/config arch=$(arch) arch_ver=$(arch_ver) dbg=$(krn_dbg) \
		klog=$(krn_log) uart=$(krn_uart) timer=$(krn_timer) intc=$(krn_intc) mmu=$(krn_mmu) \
		gccprefix=$(gccprefix)

clean-kernel:
	@$(echo) "$(color_bld)+----------------+$(color_off)"
	@$(echo) "$(color_bld)|  Clean kernel  |$(color_off)"
	@$(echo) "$(color_bld)+----------------+$(color_off)"
	$(v)make -C $(krn_path) V=$V target=$(krn_elf) blddir=$(blddir)/$(krn_path) arch=$(arch) \
		arch_ver=$(arch_ver) uart=$(krn_uart) timer=$(krn_timer) intc=$(krn_intc) mmu=$(krn_mmu) clean

# path may be:
# 1) app/hello     - internal app
# 2) ext:app/hello - external app
build-apps:
	@echo extdir=$(extdir)
	@for app in $(app_paths); do \
		line="$$(for i in `seq 1 $${#app}`; do echo -n -; done)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		$(echo) "$(color_bld)|  Build app $$app  |$(color_off)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		arr=($$(echo $$app | tr ":" " ")); \
		if [ $${arr[0]} == "ext" ]; then \
			dir=$(extdir)/; \
			name=$${arr[1]}; \
		else \
			dir=""; \
			name=$${arr[0]}; \
		fi; \
		make -C $$dir$$name V=$V blddir=$(blddir)/$$name cfgdir=$(blddir)/config \
			arch=$(arch) arch_ver=$(arch_ver) dbg=$(app_dbg) wrmdir=$$(pwd) extdir=$(extdir) \
			gccprefix=$(gccprefix) plt_uart=$(plt_uart); \
		if [ "$$?" != "0" ]; then exit 10; fi; \
		echo; \
	done

# path may be:
# 1) app/hello     - internal app
# 2) ext:app/hello - external app
clean-apps:
	@for app in $(app_paths); do \
		line="$$(for i in `seq 1 $${#app}`; do echo -n -; done)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		$(echo) "$(color_bld)|  Clean app $$app  |$(color_off)"; \
		$(echo) "$(color_bld)+--------------$$line+$(color_off)"; \
		arr=($$(echo $$app | tr ":" " ")); \
		if [ $${arr[0]} == "ext" ]; then \
			dir=$(extdir)/; \
			name=$${arr[1]}; \
		else \
			dir=""; \
			name=$${arr[0]}; \
		fi; \
		make -C $$dir$$name clean V=$V blddir=$(blddir)/$$name arch=$(arch) wrmdir=$$(pwd); \
		echo; \
	done

# extract ramfs files to get dependences and make target
build-bootloader:
	@$(echo) "$(color_bld)+-------------------+$(color_off)"
	@$(echo) "$(color_bld)|  Build booloader  |$(color_off)"
	@$(echo) "$(color_bld)+-------------------+$(color_off)"
	@for item in $(ldr_ramfs); do \
		arr=($$(echo $$item | tr ":" " ")); \
		files="$$files $${arr[1]}"; \
	done; \
	make -C $(ldr_path) V=$V target=$(ldr_file) blddir=$(blddir)/$(ldr_path) \
		cfgdir=$(blddir)/config arch=$(arch) arch_ver=$(arch_ver) \
		ldr_ramfs="$(ldr_ramfs)" ramfs_files="$$files" dbg=$(ldr_dbg) uart=$(ldr_uart) \
		gccprefix=$(gccprefix)

clean-bootloader:
	@$(echo) "$(color_bld)+--------------------+$(color_off)"
	@$(echo) "$(color_bld)|  Clean bootloader  |$(color_off)"
	@$(echo) "$(color_bld)+--------------------+$(color_off)"
	$(v)make -C $(ldr_path) V=$V target=$(ldr_file) blddir=$(blddir)/$(ldr_path) arch=$(arch) \
		uart=$(ldr_uart) clean

