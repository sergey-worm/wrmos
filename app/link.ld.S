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
		KEEP(*(.ctors))       /* sparc, x86 */
		KEEP(*(.init_array))  /* arm        */
		_ctors_end = .;

		_dtors_start = .;
		KEEP(*(.dtors))       /* sparc, x86 */
		KEEP(*(.fini_array))  /* arm        */
		_dtors_end = .;

		_text_end = .;
		. = ALIGN(page_sz);
	} > vspace

	.rodata :
	{
		_rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		*(.gcc_except_table*)
		_rodata_end = .;
		. = ALIGN(page_sz);
	} > vspace

	.data :
	{
		_data_start = .;
		*(.data)
		*(.data.*)
		*(.got*)               /* .got needs for libgcc */
		/*. = ALIGN(page_sz);*/
	} > vspace

	.tdata :
	{
		_tls_start = .;
		_tdata_start = .;
		*(.tdata*)
		_tdata_end = .;
		/*. = ALIGN(page_sz);*/
	} > vspace

	.tbss :
	{
		_tbss_start = .;
		*(.tbss*)
		_tbss_end = .;
		_tls_end = .;
		/*. = ALIGN(page_sz);*/
	} > vspace

	.bss :
	{
		_bss_start = .;
		*(.bss)
		*(.bss.*)
		*(.gnu.linkonce*)
		_bss_end = .;
		/*. = ALIGN(page_sz);*/
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
		*(.ARM.extab*)     /* arm */
		*(.ARM.exidx*)     /* arm */
		*(.gnu.attributes)
	}
}
