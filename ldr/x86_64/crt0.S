//##################################################################################################
//
//  boot0 code and data - first sector of drive (512 bytes, MBR), loaded by BIOS at 0x7c00
//
//##################################################################################################

#define CODE_16  0x08
#define DATA_16  0x10
#define CODE_64  0x18
#define DATA_64  0x20

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

	// 64-bit specific (further boot1)

	call check_cpu

	call set_page_tables

	// enter long mode
	mov $0xa0, %eax                   // set PAE and PGE bits
	or $0x200, %eax                   // set OSFXSR bit - allow SSE/%xmm, gcc uses them for -O2
	mov %eax, %cr4

	mov $0xc0000080, %ecx             // read from the EFER MSR
	rdmsr
	or $0x100, %eax                   // EFER.LME = 1
	wrmsr

	mov %cr0, %eax                    // activate long mode
	or $0x80000001, %eax              // cr0.pg/pe = 1
	mov %eax, %cr0

	ljmp $CODE_64, $LongMode          // load CS with 64-bit segment and flush icache

.code64
LongMode:

	mov $DATA_64, %ax
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
#include "../x86/mbr-real.inc"

gdt_begin:
	// initial 1:1 mapping
	.quad 0x0000000000000000          // 0x00  null descriptor
	.quad 0x000f9b000000ffff          // 0x08  16-bit code
	.quad 0x000f92000000ffff          // 0x10  16-bit data
	.quad 0x00209a0000000000          // 0x18  64-bit code
	.quad 0x0000920000000000          // 0x20  64-bit data
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
set_page_tables:

	#define PML4 0x4000
	#define L1TB 0x5000
	#define L2TB 0x6000
	#define PAGE_PRESENT (1 << 0)
	#define PAGE_WRITE   (1 << 1)
	#define PAGE_PTE     (1 << 7)

	// set zeros
	mov $PML4, %edi
	mov $0xc00, %ecx     // sizeof PML4, L1TP and L2TB together in longs
	xor %eax, %eax
	cld
	rep stosl            // store %eax at [%es:%edi] %ecx times

	// set L2TB first 8 entry (2 MB * 8 entry)
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 0 * 0x200000), %eax
	mov %eax, L2TB + 0x00
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 1 * 0x200000), %eax
	mov %eax, L2TB + 0x08
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 2 * 0x200000), %eax
	mov %eax, L2TB + 0x10
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 3 * 0x200000), %eax
	mov %eax, L2TB + 0x18
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 4 * 0x200000), %eax
	mov %eax, L2TB + 0x20
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 5 * 0x200000), %eax
	mov %eax, L2TB + 0x28
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 6 * 0x200000), %eax
	mov %eax, L2TB + 0x30
	mov $(PAGE_PRESENT | PAGE_WRITE | PAGE_PTE | 7 * 0x200000), %eax
	mov %eax, L2TB + 0x38

	// set L1TB 0-th entry
	mov $(PAGE_PRESENT | PAGE_WRITE), %eax
	or $L2TB, %eax
	mov %eax, L1TB

	// set PML4 0-th entry
	mov $(PAGE_PRESENT | PAGE_WRITE), %eax
	or $L1TB, %eax
	mov %eax, PML4

	// set PML4
	mov $PML4, %eax
	mov %eax, %cr3

	ret

//--------------------------------------------------------------------------------------------------
// Checks whether CPU supports long mode or not.
// Returns with carry set if CPU doesn't support long mode.
.code16
check_cpu:

	// Check whether CPUID is supported or not.
	pushf                            // Get flags in EAX register.
	pop %eax
	mov %eax, %ecx
	xor $0x200000, %eax
	push %eax
	popf

	pushf
	pop %eax
	xor %ecx, %eax
	shr $21, %eax
	and $1, %eax                     // Check whether bit 21 is set or not. If EAX now contains 0, CPUID isn't supported.
	push %ecx
	popf

	test %eax, %eax
	jz .NoLongMode

	mov $0x80000000, %eax
	cpuid

	cmp $0x80000001, %eax             // Check whether extended function 0x80000001 is available are not.
	jb .NoLongMode                    // If not, long mode not supported.

	mov $0x80000001, %eax
	cpuid
	test $(1 << 29), %edx             // Test if the LM-bit, is set or not.
	jz .NoLongMode                    // If not Long mode not supported.

	ret

.NoLongMode:
	mov $msg_no_long_mode, %ax
	call print
1:
	jmp 1b

msg_no_long_mode:
	.asciz "[boot0]  ERROR: no 64-bit.\n\r"


//--------------------------------------------------------------------------------------------------
.global do_bioscall
do_bioscall:
.code64

	#define Entry_frame_sz     (8 + 4*8)   /* ra_sz + regs_sz */
	#define Bioscall_frame_sz  0x10        /* sizeof(Bioscall_frame_t) */

	// now we are in 64-bit protected mode
	push %rax
	push %rcx
	push %rdx
	push %rbx

nop; nop; nop
	push $CODE_16
nop; nop; nop
	push $1f
nop; nop; nop
	lretq
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

	// go to real mode, cr0.ep/pg = 0
	mov %cr0, %eax
	and  $0x7ffffffe, %eax
	mov  %eax, %cr0

	ljmp $0, $2f
2:
	// now we are in 16-bit real mode
	// reload segment registers
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss

	//-------------------
	// execute bios call
	//-------------------
	add $Entry_frame_sz, %sp   // set sp to bioscall frame
	pop %ax                    // get intno from frame
	mov %al, intno             // set intno to execute
	jmp 3f                     // sync pipeline
3:
	// read bioscall frame
	pop %bx
	pop %dx
	pop %cx
	pop %ax
	sub $(Entry_frame_sz + Bioscall_frame_sz), %sp   // restore sp
	// execute int instrustion
	.byte 0xcd           // int opcode
intno:
	.byte 0              // int number
	// read flags
	pushf
	pop tmp_flags
	add $(Entry_frame_sz + Bioscall_frame_sz), %sp   // set sp to bioscall frame again
	// set syscall frame
	push %ax
	push %cx
	push %dx
	push %bx
	push tmp_flags
	sub $(Entry_frame_sz), %sp         // restore sp
	//--------------------------
	// end of execute bios call
	//--------------------------

	// now we are in 16 bit real mode

	// disable interrupts
	cli

	// go to 64-bit protected mode
	// cr0.pe = 1
	mov  %cr0, %eax
	or   $0x80000001, %eax
	mov  %eax, %cr0

	// go to cs:eip, 64-bit protected mode
	ljmp $CODE_64, $4f
.code64
4:
	// now we are in 64-bit protected mode
	// set data segment registers
	mov $DATA_64, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss

	pop %rbx
	pop %rdx
	pop %rcx
	pop %rax
	ret

tmp_flags:
	.word 0
