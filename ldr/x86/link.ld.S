/*
	link.ld.S - linker script, it will be preprocessed to link.ld
    vi: set ft=ld:
*/

#include "sys-config.h"
#include "next-load-addr.h"

boot0_addr = 0x7c00; /* BIOS loads first 512 bytes (MBR) at 0x7c00 and execute it */
boot0_sz   = 0x200;
boot1_addr = boot0_addr + boot0_sz;
boot1_sz   = 0x2000;
diff_load_boot = Cfg_load_addr - boot1_addr - boot1_sz;

ENTRY(_start)

MEMORY
{
	boot0_space :  ORIGIN = boot0_addr,  LENGTH = boot0_sz
	boot1_space :  ORIGIN = boot1_addr,  LENGTH = boot1_sz

	#define Load_area_sz (Cfg_ram_sz - (Cfg_load_addr - Cfg_ram_start))
	load : ORIGIN = Cfg_load_addr, LENGTH = Load_area_sz
}

SECTIONS
{
	.boot0 :
	{
		*(.boot0);
		. = ALIGN(512) - 2;
		SHORT(0xaa55);
	} > boot0_space

	.boot1 :
	{
		*crt0.o(.boot1);
		*crt1.o(.text*)
		*crt1.o(.rodata*)
		*crt1.o(.data*)
		*crt1.o(.bss*)
	} > boot1_space

	.ramfs : AT( ADDR(.ramfs) - diff_load_boot)
	{
		_ramfs_start = .;
		*ramfs.generate.o(.first) /* locate file headers at the start of section */
		*ramfs.generate.o(*)
		. = ALIGN(Cfg_page_sz);
		_ramfs_end = .;
	} > load

	_bootloader_start = .;

	.text : AT( ADDR(.text) - diff_load_boot)
	{
		_text_start = .;
		*(.text)
		*(.text.*)

		. = ALIGN(8);
		_ctors_start = .;
		KEEP(*(.ctors))
		_ctors_end = .;

		_text_end = .;
	} > load

	.rodata : AT( ADDR(.rodata) - diff_load_boot)
	{
		_rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		_rodata_end = .;
	} > load

	.data : AT( ADDR(.data) - diff_load_boot)
	{
		_data_start = .;
		*(.data)
		*(.got)             /* .got need fo libgcc */
		_data_end = .;
	} > load

	.bss : AT( ADDR(.bss) - diff_load_boot)
	{
		_bss_start = .;
		*(.bss)
		*(.bss.*)
		_bss_end = .;
	} > load

	. = ALIGN(Cfg_page_sz);
	_bootloader_end = .;

	.end_of_image :
	{
		/* goto next 512-byte segment and set 'end of image' lable, it need for loader */
		. = ALIGN(512);
		LONG(0xdeadbabe);
	} > load

	/* not needed sections */
	/DISCARD/ :
	{
		*(.dtors)
		*(.note)
		*(.note.ABI-tag)
		*(.comment)
		*(.eh_frame)
		*(.dummy)
		*(.debug*)
	}
}
