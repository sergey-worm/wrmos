####################################################################################################
#
#  Rules to generate config files and update ldr-address for krn/sigma0/alpha.
#
####################################################################################################
#---------------------------------------------------------------------------------------------------
#  CONFIG RULES
#---------------------------------------------------------------------------------------------------

cfg_deps = $(prj_file) $(plt_file) $(wrmdir)/cfg/base.cfg

$(cfgdir)/sys-config.h:  $(cfg_deps)
	@$(echo) "$(color_compile)[GEN] $(notdir $(prj_file)) --> $(notdir $@)$(color_off)";
	@mkdir -p $(dir $@)
	@echo -en "\
	/* sys-config.h - generated by mk/syscfg.mk. Don't edit me. */\n\
	\n\
	#ifndef SYS_CONFIG_H\n\
	#define SYS_CONFIG_H\n\
	\n\
	#define Cfg_arch $(arch)\n\
	#define Cfg_arch_$(arch)\n\
	#define Cfg_cpu $(cpu)\n\
	#define Cfg_cpu_$(cpu)\n\
	#define Cfg_plat $(plat)\n\
	#define Cfg_plat_$(plat)\n\
	#define Cfg_brd $(brd)\n\
	#define Cfg_brd_$(brd)\n\
	#define Cfg_max_cpus $(max_cpus)\n\
	#define Cfg_sys_clock_hz $(sys_clock_hz)\n\
	#define Cfg_ram_start $(ram_start)\n\
	#define Cfg_ram_sz $(ram_sz)\n\
	#define Cfg_page_sz  $(page_sz)\n\
	\n\
	#endif // SYS_CONFIG_H" > $@

# uart vaddr for temporary rude mapping, use at initial startup code
# сhoose an address upper kernel + 16 MB and aligned to 2 MB
kuart_va = (((Cfg_krn_vaddr + 0x1000000) & 0xffffffffffe00000) + (Cfg_krn_uart_paddr & 0xfffff))

# intc vaddr for temporary rude mapping, use at initial startup code
# сhoose an address upper kernel + 32 MB and aligned to 2 MB
kintc_va = (((Cfg_krn_vaddr + 0x2000000) & 0xffffffffffe00000) + (Cfg_krn_intc_paddr & 0xfffff))

$(cfgdir)/krn-config.h:  $(cfg_deps)
	@$(echo) "$(color_compile)[GEN] $(notdir $(prj_file)) --> $(notdir $@)$(color_off)"
	@mkdir -p $(dir $@)
	@echo -en "\
	/* krn-config.h - generated by mk/syscfg.mk. Don't edit me. */\n\
	\n\
	#ifndef KRN_CONFIG_H\n\
	#define KRN_CONFIG_H\n\
	\n\
	#include \"sys-config.h\"\n\
	\n\
	/* kernel settings */\n\
	#define Cfg_krn_vaddr $(krn_vaddr)\n\
	#define Cfg_krn_tick_usec $(krn_tick_usec)\n\
	#define Cfg_krn_uart_paddr $(krn_uart_paddr)\n\
	#define Cfg_krn_uart_vaddr $(kuart_va)\n\
	#define Cfg_krn_uart_sz $(krn_uart_sz)\n\
	#define Cfg_krn_uart_bitrate $(krn_uart_bitrate)\n\
	#define Cfg_krn_uart_irq $(krn_uart_irq)\n\
	#define Cfg_krn_intc_paddr $(krn_intc_paddr)\n\
	#define Cfg_krn_intc_vaddr $(kintc_va)\n\
	#define Cfg_krn_intc_sz $(krn_intc_sz)\n\
	#define Cfg_krn_timer_paddr $(krn_timer_paddr)\n\
	#define Cfg_krn_timer_sz $(krn_timer_sz)\n\
	#define Cfg_krn_timer_irq $(krn_timer_irq)\n\
	#define Cfg_krn_timer_lag_usec $(krn_timer_lag_usec)\n\
	\n\
	#endif // KRN_CONFIG_H" > $@

$(cfgdir)/ldr-config.h:  $(cfg_deps)
	@$(echo) "$(color_compile)[GEN] $(notdir $(prj_file)) --> $(notdir $@)$(color_off)"
	@mkdir -p $(dir $@)
	@echo -en "\
	/* ldr-config.h - generated by mk/syscfg.mk. Don't edit me. */\n\
	\n\
	#ifndef LDR_CONFIG_H\n\
	#define LDR_CONFIG_H\n\
	\n\
	#include \"sys-config.h\"\n\
	\n\
	/* loader settings */\n\
	#define Cfg_ldr_uart_paddr $(ldr_uart_paddr)\n\
	#define Cfg_ldr_uart_bitrate $(ldr_uart_bitrate)\n\
	\n\
	#endif // LDR_CONFIG_H" > $@

#---------------------------------------------------------------------------------------------------
#  LOAD ADDR RULES
#---------------------------------------------------------------------------------------------------

# $1 - file path
# $2 - new value
update_load_addr_file = $(SHELL) -c '\
	echo -en "\
		/* $(notdir $1) - generated by mk/syscfg.mk. Do not edit me. */\n\
		\#define Cfg_load_addr $2" > $1; \
	if [ "$v" != "@" ]; then echo New load address is $2 for $(target); fi; '

$(cfgdir)/kernel-load-addr.h:  $(cfg_deps)
	@$(echo) "$(color_compile)[GEN] $(notdir $(target)):  $(notdir $(prj_file)) --> $(notdir $@)$(color_off)"
	@mkdir -p $(blddir)/config
	@$(call update_load_addr_file,$@,$(ram_start))
	$(v)if [ "$v" != "@" ]; then echo "Ok:   $(notdir $@)"; fi

# define dependencies between ELFs
dep_sigma0-load-addr.h     = $(rtblddir)/krn/kernel.elf
dep_alpha-load-addr.h      = $(rtblddir)/app/sigma0/sigma0.elf
dep_bootloader-load-addr.h = $(rtblddir)/app/alpha/alpha.elf

$(cfgdir)/sigma0-load-addr.h:      $(dep_sigma0-load-addr.h)
$(cfgdir)/alpha-load-addr.h:       $(dep_alpha-load-addr.h)
$(cfgdir)/bootloader-load-addr.h:  $(dep_bootloader-load-addr.h)

# update Cfg_load_addr value in *-load-addr.h
$(cfgdir)/%-load-addr.h:
	@$(echo) "$(color_compile)[GEN] $(notdir $(target)):  $(notdir $(dep_$(notdir $@))) --> $(notdir $@)$(color_off)"
	@if [ ! -f "$(dep_$(notdir $@))" ]; then \
		echo ERROR:  no file \"$(dep_$(notdir $@))\".; \
		exit 10; \
	fi; \
	str=$$(readelf -lW $(dep_$(notdir $@)) | grep LOAD | tail -n1); \
		arr=($$str); \
		paddr=$${arr[3]}; \
		memsz=$${arr[5]}; \
		new_val=$$(printf "0x%x\n" $$(($$paddr + $$memsz))); \
		new_val=$$(printf "0x%x\n" $$((($$new_val + 0xfff) & ~0xfff))); \
		export new_val; \
		$(call update_load_addr_file,$@,$$new_val)
	$(v)if [ "$v" != "@" ]; then echo "Ok:   $(notdir $@)"; fi
