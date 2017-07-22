//##################################################################################################
//
//  boot0 code and data - first sector of drive (512 bytes, MBR), loaded by BIOS at 0x7c00
//
//##################################################################################################

#define CODE_16  0x08
#define DATA_16  0x10
#define CODE_32  0x18
#define DATA_32  0x20

.section .boot0, "ax"
.code16

.global _start
_start:

	// reload %cs
	ljmp $0, $1f
1:

	// store boot device ID
	mov %dl, boot_device_id

	// init segment descriptors
	mov %cs, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %fs
	mov %ax, %gs

	// stack area [0x500, 0x7a00), space [0x7a00, 0x7c00) is reserved for load disk sector
	mov $0x7a00, %sp

	// print hello
	call serial_init
	mov  $msg_hello, %ax
	call print

	// load boot1 from disk to RAM
	mov  $msg_load_boot1, %ax
	call print
	call load_boot1
	mov  $msg_done, %ax
	call print

	// switch to protected mode
	mov  $msg_switch_to_pm, %ax
	call print

	// enable A20
	in  $0x92, %al
	or  $2, %al
	out %al, $0x92

	// set GDT in GDTR
	lgdt gdtr_data

	// disable interrupts
	cli

	// disable unmaskable interrupts
	in  $0x70, %al
	or  $0x80, %al
	out %al, $0x70

	// 32-bit specific

	// switch to protected mode:  cr0.pe = 1
	mov  %cr0, %eax
	or   $1, %al
	mov  %eax, %cr0

	// go to cs:eip, 32-bit protected mode
	ljmp $CODE_32, $2f
.code32
2:
	// init data segment registers
	mov $DATA_32, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss

	// call C-code
	call boot1_main
3:
	jmp 3b

// include real mode helper functions
#include "mbr-real.inc"

gdt_begin:
	// initial 1:1 mapping
	.quad 0x0000000000000000          // 0x00  null descriptor
	.quad 0x000f9b000000ffff          // 0x08  16-bit code
	.quad 0x000f92000000ffff          // 0x10  16-bit data
	.quad 0x00cf9b000000ffff          // 0x18  32-bit code
	.quad 0x00cf92000000ffff          // 0x20  32-bit data
gdt_end:

// data for GDT register
gdtr_data:
	.word gdt_end - gdt_begin - 1
	.long gdt_begin

//--------------------------------------------------------------------------------------------------
//  boot1 code and data - sectors [2..9] of drive (0x1000 bytes), loaded booted by boot0
//--------------------------------------------------------------------------------------------------
.section .boot1, "ax"

//--------------------------------------------------------------------------------------------------
.global do_bioscall
do_bioscall:
.code32

	// now we are in 32-bit protected mode

	#define Entry_frame_sz     (4 + 4*4)   /* ra_sz + regs_sz */
	#define Bioscall_frame_sz  0x10        /* sizeof(Bioscall_frame_t) */

	// store some regs
	push %eax
	push %ecx
	push %edx
	push %ebx

	ljmp $CODE_16, $1f     // reload %cs, switch to 16-bit mode
1:
.code16
	// now we are in 16-bit protected mode
	// reload segment registers
	mov $DATA_16, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss

	// go to protected mode, cr0.ep = 0
	mov  %cr0, %eax
	and  $0xfe, %al
	mov  %eax, %cr0
	ljmp $0, $2f
2:
	// now we are in 16-bit real mode
	// reload segment registers
	mov %cs, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss

	//-------------------
	// execute bios call
	//-------------------
	add $(Entry_frame_sz), %sp   // set sp to bioscall frame
	pop %ax                      // get intno from frame
	mov %al, intno               // set intno to execute
	jmp 3f                       // sync pipeline
3:
	// read bioscall frame
	pop %bx
	pop %dx
	pop %cx
	pop %ax
	sub $(Entry_frame_sz + Bioscall_frame_sz), %sp  // restore sp
	// execute int instrustion
	.byte 0xcd           // int opcode
intno:
	.byte 0              // int number
	// read flags
	pushf
	pop (tmp_flags)
	add $(Entry_frame_sz + Bioscall_frame_sz), %sp   // set sp to bioscall frame again
	// set bioscall frame
	push %ax
	push %cx
	push %dx
	push %bx
	push tmp_flags
	sub $(Entry_frame_sz), %sp         // restore sp
	//--------------------------
	// end of execute bios call
	//--------------------------

	// disable interrupts
	cli

	// now we are in 16-bit real mode

	// go to 32-bit protected mode, cr0.pe = 1
	mov  %cr0, %eax
	or   $1, %al
	mov  %eax, %cr0

	// go to cs:eip, 32-bit protected mode
	ljmp $CODE_32, $4f

.code32
4:
	// now we are in 32-bit protected mode
	// set data segment registers
	mov $DATA_32, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss

	// restore used regs
	pop %ebx
	pop %edx
	pop %ecx
	pop %eax
	ret

tmp_flags:
	.word 0
