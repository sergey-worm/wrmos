/*
	link.ld.S - linker script for sparc eabi that will be preprocessed to link.ld
	SEE:  http://www.math.utah.edu/docs/info/ld_3.html

	Next params should be defined as gcc params (-DCfg_<param>=<val>)
		Cfg_ram_start
		Cfg_ram_sz

	vi: set ft=ld:
*/

#include "krn-config.h"
#include "next-load-addr.h"

page_sz            = Cfg_page_sz;
_kern_pload        = Cfg_load_addr;
_kern_vaddr        = Cfg_krn_vaddr;
_diff_va_pa        = _kern_vaddr - _kern_pload;

/* FIXME */
_ram_start = Cfg_ram_start;
_ram_sz = Cfg_ram_sz;
/* ~ FIXME */

ENTRY(_start)

MEMORY
{
	#define Phys_area_sz (16 * Cfg_page_sz)   /* 1 pg for sparc, 8 for arm, 16 for x86 */

	#if ((Cfg_load_addr + Phys_area_sz) >= (Cfg_ram_start + Cfg_ram_sz))
	#  error  Load address out of memory range or RAM isn't anouth.
	#endif

	pspace : ORIGIN = Cfg_load_addr, LENGTH = Phys_area_sz

	#define Virt_area_sz (Cfg_ram_sz - (Cfg_load_addr - Cfg_ram_start) - Phys_area_sz)
	vspace : ORIGIN = Cfg_krn_vaddr + Phys_area_sz, LENGTH = Virt_area_sz
}

SECTIONS
{
	/* AT PHYSICAL ADDRESSES */

	.bootstrap :
	{
		_bstrap_start = .;
		*crt0.o(.bootstrap)
		*bootstrap.o(.text)
		*bootstrap.o(.rodata)
		*bootstrap.o(.data)
		*bootstrap.o(.bss)
		_bstrap_end = .;
		. = ALIGN(page_sz);
	} > pspace

	/* AT VIRTUAL ADDRESSES */

	.kip : AT( ADDR(.kip) - _diff_va_pa )
	{
		_kip_start = .;
		*kip.o(.kip)
		. = ALIGN(page_sz);
		_kip_end = .;
	} > vspace

	.text : AT( ADDR(.text) - _diff_va_pa )
	{
		_text_start = .;
		*crt0.o(.ivt)
		*bootstrap.o(.virt.text)
		*(.text)
		*(.text.*)

		. = ALIGN(8);
		_ctors_start = .;
		KEEP(*(.ctors))       /* sparc, x86 */
		KEEP(*(.init_array))  /* arm */
		_ctors_end = .;

		_text_end = .;
		. = ALIGN(page_sz);
	} > vspace

	.user.text : AT( ADDR(.user.text) - _diff_va_pa )
	{
		_user_text_start = .;
		*(.user.text)
		_user_text_end = .;
		. = ALIGN(page_sz);
	} > vspace

	.rodata :  AT( ADDR(.rodata) - _diff_va_pa )
	{
		_rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		_rodata_end = .;
		. = ALIGN(page_sz);
	} > vspace 

	.data :  AT( ADDR(.data) - _diff_va_pa )
	{
		_data_start = .;
		*(.data)
		*(.data.*)

		_bss_start = .;
		*(.bss)
		*(.bss.*)
		_bss_end = .;

		_data_end = .;
		. = ALIGN(page_sz);
	} > vspace

	_kern_vend = .; /* DELETME ??? */

	/DISCARD/ :
	{
		*(.dtors)
		*(.note)
		*(.note.ABI-tag)
		*(.comment)
		*(.eh_frame)
		*(.dummy)
		*(.debug*)
		*(.ARM.exidx*) /* for arm unwinding */
		*(.fini_array)
	}
}
