/*
	link.ld.S - linker script for sparc eabi that will be preprocessed to link.ld
	SEE:  http://www.math.utah.edu/docs/info/ld_3.html

	Next params should be defined as gcc params (-DCfg_<param>=0x123)
		Cfg_ram_start
		Cfg_ram_sz

	NOTE:
	1. .gnu.linkonce.*     - need for vague linking, when the GNU linker sees two input sections
	                         with the same name, and the name starts with ".gnu.linkonce.", the
                             linker will only keep one copy and discard the other;
	2. SORT(.text.*)       - sort matches by name;
	3. KEEP(*(.bss.crt0))  - keep the symbols in the section even if symbols are not referenced,
	                         for --gc-sections.

    vi: set ft=ld:
*/

#include "sys-config.h"
#include "bootloader-load-addr.h"


ENTRY(_start)


MEMORY
{
	#define Load_area_sz (Cfg_ram_sz - (Cfg_load_addr - Cfg_ram_start))
	load : ORIGIN = Cfg_load_addr, LENGTH = Load_area_sz
}

SECTIONS
{
	.ramfs :
	{
		_ramfs_start = .;
		*ramfs.generate.o(.first) /* locate file headers at the start of section */
		*ramfs.generate.o(*)
		. = ALIGN(Cfg_page_sz);
		_ramfs_end = .;
	} > load

	_bootloader_start = .;

	.text :
	{
		_text_start = .;
		*(.text)
		*(.text.*)

		. = ALIGN(8);
		_ctors_start = .;
		KEEP(*(.ctors))       /* sparc, x86 */
		KEEP(*(.init_array))  /* arm */
		_ctors_end = .;

		_text_end = .;
	} > load

	.rodata :
	{
		_rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		_rodata_end = .;
	} > load

	.data :
	{
		_data_start = .;
		*(.data)
		*(.got)               /* .got need fo libgcc */
		_data_end = .;
	} > load

	.bss :
	{
		_bss_start = .;
		*(.bss)
		*(.bss.*)
		_bss_end = .;
	} > load

	. = ALIGN(Cfg_page_sz);
	_bootloader_end = .;

	/DISCARD/ :
	{
		*(.dtors)
		*(.fini_array)
		*(.note)
		*(.note.ABI-tag)
		*(.comment)
		*(.eh_frame)
		*(.dummy)
		*(.debug*)
	}
}
