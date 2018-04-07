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
#include "sigma0-load-addr.h"

page_sz = Cfg_page_sz;

ENTRY(app_entry)

MEMORY
{
	#if (Cfg_load_addr >= (Cfg_ram_start + Cfg_ram_sz))
	#  error  Load address out of memory range, RAM isn't anough.
	#endif

	#define Load_area_sz (Cfg_ram_sz - (Cfg_load_addr - Cfg_ram_start))
	load : ORIGIN = Cfg_load_addr, LENGTH = Load_area_sz
}

PHDRS
{
	sect_load PT_LOAD;
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

		_text_end = .;
		. = ALIGN(page_sz);
	} > load : sect_load

	.rodata :
	{
		_rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		_rodata_end = .;
		. = ALIGN(page_sz);
	} > load

	.data :
	{
		_data_start = .;
		*(.data)
		*(.got)               /* .got need for libgcc */
		_data_end = .;
		. = ALIGN(page_sz);
	} > load

	.bss :
	{
		_bss_start = .;
		*(.bss)
		*(.bss.*)
		_bss_end = .;
		. = ALIGN(page_sz);
	} > load

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
		*(.ARM.extab*)     /* arm */
		*(.ARM.exidx*)     /* arm */
	}
}
