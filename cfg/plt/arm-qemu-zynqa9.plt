####################################################################################################
#
#  vi: set ft=make:
#
#  Platform config.
#
#  This file is including in *prj file.
#
#  Based on it will be generate files:  krn-config.h, ldr-config.h, sys-config.h.
#
####################################################################################################

#---------------------------------------------------------------------------------------------------
#  system params
#---------------------------------------------------------------------------------------------------

arch               = arm
arch_ver           = armv7-a
cpu                = cortex_a9
plat               = zynq
brd                = qemu_zynqa9
max_cpus           = 1
sys_clock_hz       = 200000000   # 200 MHz
ram_start          = 0x0
ram_sz             = 0x40000000  # 1 GB
  ram_sz           = 0x800000    # FIXME should be 1 GB, but Alpha-Sigma too long startup FIXME
page_sz            = 0x1000

#---------------------------------------------------------------------------------------------------
#  kernel params
#---------------------------------------------------------------------------------------------------

krn_vaddr          = 0xf0000000
krn_tick_usec      = 10000

krn_uart_paddr     = 0xe0000000  # uart0
krn_uart_sz        = 0x100
krn_uart_bitrate   = 115200
krn_uart_irq       = 59          # uart0

krn_intc_paddr     = 0xf8f00000
krn_intc_sz        = 0x2000

krn_timer_paddr    = 0xf8f00200
krn_timer_sz       = 0x100
krn_timer_irq      = 27
krn_timer_lag_usec = 1000        # qemu-zynqa9 has timer lag

#---------------------------------------------------------------------------------------------------
#  bootloader params
#---------------------------------------------------------------------------------------------------

ldr_uart_paddr     = 0xe0000000
ldr_uart_bitrate   = 115200

#---------------------------------------------------------------------------------------------------
#  Base devices
#---------------------------------------------------------------------------------------------------

plt_uart           = arm_cadence
plt_intc           = arm_gic
plt_timer          = arm_a9gtimer
plt_mmu            = arm_mmu
