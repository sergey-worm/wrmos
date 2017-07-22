/*
	vi: set ft=ld:

	link.ld.S - linker script for sparc eabi that will be preprocessed to link.ld
	SEE:  http://www.math.utah.edu/docs/info/ld_3.html

	Next params should be defined as gcc params (-DCfg_<param>=<val>)
		Cfg_page_sz
		Cfg_load_addr

	NOTE:
	1. .gnu.linkonce.*     - need for vague linking, when the GNU linker sees two input sections
	                         with the same name, and the name starts with ".gnu.linkonce.", the
                             linker will only keep one copy and discard the other;
	2. SORT(.text.*)       - sort matches by name;
	3. KEEP(*(.bss.crt0))  - keep the symbols in the section even if symbols are not referenced,
	                         for --gc-sections.
*/

#include "sys-config.h"

page_sz = Cfg_page_sz;

ENTRY(_start)

MEMORY
{
	vspace : ORIGIN = 0x10000000, LENGTH = 0xd0000000
}

SECTIONS
{
	.text :
	{
		_text_start = .;
		*(.text)
		*(.text.*)

		. = ALIGN(8);
		_ctors_start = .;
		KEEP(*(.ctors))       /* sparc FIXME */
		KEEP(*(.init_array))  /* arm   FIXME */
		_ctors_end = .;

		_dtors_start = .;
		KEEP(*(.dtors))       /* sparc FIXME */
		KEEP(*(.fini_array))  /* arm   FIXME */
		_dtors_end = .;

		_text_end = .;
		. = ALIGN(page_sz);
	} > vspace

	.rodata :
	{
		_rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		_rodata_end = .;
		. = ALIGN(page_sz);
	} > vspace

	.data :
	{
		_data_start = .;
		*(.data)
		*(.data.*)

		_bss_start = .;
		*(.bss)
		*(.bss.*)
		_bss_end = .;

		/* .got needs for libgcc */
		_got_start = .;
		*(.got)
		_got_end = .;

		_data_end = .;
		/* does not aligne to allow use unused space by application */
	} > vspace

	/* FIXME:  topfw-based failed without .bss */
	.bss :
	{
	} > vspace

	/* not needed sections */
	/DISCARD/ :
	{
		*(.note)
		*(.note.ABI-tag)
		*(.comment)
		*(.eh_frame)
		*(.dummy)
		*(.debug*)

		*(.ARM.extab*)     /* FIXME arm */
		*(.ARM.exidx*)     /* FIXME arm */
	}
}
