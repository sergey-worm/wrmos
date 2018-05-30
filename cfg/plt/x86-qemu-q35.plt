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

arch               = x86
cpu                = q35
plat               = qemu
brd                = qemu_q35
max_cpus           = 2
sys_clock_hz       = 1193182
ram_start          = 0x100000   # > 1 MB, hi-mem
ram_sz             = 0x800000
page_sz            = 0x1000

#---------------------------------------------------------------------------------------------------
#  kernel params
#---------------------------------------------------------------------------------------------------

krn_vaddr          = 0xf0100000 # > 1 MB, hi-mem
krn_tick_usec      = 10000

krn_uart_paddr     = (0xb8000 + 0x3f8) # video_mem + ioport_COM1_addr
krn_uart_sz        = (0x1000  + 0x8)   # video_sz  + ioport_COM1_sz
krn_uart_bitrate   = 115200
krn_uart_irq       = 4

krn_intc_paddr     = 0x1   # lowest bit 1 - don't need kmap
krn_intc_sz        = 0x0

krn_timer_paddr    = 0x1   # lowest bit 1 - don't need kmap
krn_timer_sz       = 0x0
krn_timer_irq      = 0
krn_timer_lag_usec = 1000  # qemu-x86 has timer lag

#---------------------------------------------------------------------------------------------------
#  bootloader params
#---------------------------------------------------------------------------------------------------

ldr_uart_paddr     = (0xb8000 + 0x3f8) # video_mem + ioport_COM1
ldr_uart_bitrate   = 115200

#---------------------------------------------------------------------------------------------------
#  Base devices
#---------------------------------------------------------------------------------------------------

plt_uart           = x86_8250
plt_intc           = x86_8259  # PIC
plt_timer          = x86_8253  # PIT
plt_mmu            = x86_mmu
