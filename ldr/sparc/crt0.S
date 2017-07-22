//##################################################################################################
//
//  crt0.S - entry point, trap table, etc.
//
//##################################################################################################

// trap time registers aliases

#define t_psr      l0    /*  Trap time PSR.                                       */
#define t_pc       l1    /*  PC to return from trap. Filled by the hardware.      */
#define t_npc      l2    /*  nPC to return from trap. Filled by the hardware.     */
#define t_wim      l3    /*  Current WIM. Initialized either by trap table entry. */
#define t_tmp1     l4

//-------------------------------------------------------------------------------------------------
// MACROS - some low-level operations.
//-------------------------------------------------------------------------------------------------
.macro store_frame  base
	std %l0, [%\base +  0]
	std %l2, [%\base +  8]
	std %l4, [%\base + 16]
	std %l6, [%\base + 24]
	std %i0, [%\base + 32]
	std %i2, [%\base + 40]
	std %i4, [%\base + 48]
	std %i6, [%\base + 56]
.endm

.macro restore_frame  base
	ldd   [%\base +  0], %l0
	ldd   [%\base +  8], %l2
	ldd   [%\base + 16], %l4
	ldd   [%\base + 24], %l6
	ldd   [%\base + 32], %i0
	ldd   [%\base + 40], %i2
	ldd   [%\base + 48], %i4
	ldd   [%\base + 56], %i6
.endm

// valid for NWIN = 8
.macro rotate_right  val scratch
	srl %\val, 1, %\scratch
	sll %\val, 7, %\val
	or %\val, %\scratch, %\val
	and %\val, 0xff, %\val
.endm

// valid for NWIN = 8
.macro rotate_left  val scratch
	sll %\val, 1, %\scratch
	srl %\val, 7, %\val
	or %\val, %\scratch, %\val
	and %\val, 0xff, %\val
.endm

#define Stack_frame_sz 96

.macro TRAP  handler
	mov %psr, %t_psr
	sethi %hi(\handler), %t_tmp1
	jmp %t_tmp1 + %lo(\handler)
	 mov %wim, %t_wim
.endm

.macro BAD_TRAP
	ta 0
	nop; nop; nop
.endm

.macro BAD_TRAP_4
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
.endm

.macro BAD_TRAP_16
	BAD_TRAP_4
	BAD_TRAP_4
	BAD_TRAP_4
	BAD_TRAP_4
.endm
//-------------------------------------------------------------------------------------------------
// ~ MACROS
//-------------------------------------------------------------------------------------------------

.section .text.init, "ax"

//--------------------------------------------------------------------------------------------------
.align 0x1000  // align to page size
.global leon_traphandlers
leon_traphandlers:                           // description                prio    tt
	BAD_TRAP                                 // reset                         1  0x00
	BAD_TRAP                                 // instruction_access_exception  5  0x01
	BAD_TRAP                                 // illegal_instruction           7  0x02
	BAD_TRAP                                 // privileged_instruction        6  0x03
	BAD_TRAP                                 // fp_disabled                   8  0x04
	TRAP sparc_window_overflow               // window_overflow               9  0x05
	TRAP sparc_window_underflow              // window_underflow              9  0x06
	BAD_TRAP                                 // mem_address_not_aligned      10  0x07
	BAD_TRAP                                 // fp_exception                 11  0x08
	BAD_TRAP                                 // data_access_exception        13  0x09
	BAD_TRAP                                 // tag_overflow                 14  0x0a
	BAD_TRAP                                 // watchpoint_detected           8  0x0b
	BAD_TRAP                                 // ?                                0x0c
	BAD_TRAP                                 // ?                                0x0d
	BAD_TRAP                                 // ?                                0x0e
	BAD_TRAP                                 // ?                                0x0f
	BAD_TRAP_16                                                              //  0x10
	BAD_TRAP_16                                                              //  0x20
	BAD_TRAP_16                                                              //  0x30
	BAD_TRAP_16                                                              //  0x40
	BAD_TRAP_16                                                              //  0x50
	BAD_TRAP_16                                                              //  0x60
	BAD_TRAP_16                                                              //  0x70
	BAD_TRAP_16                                                              //  0x80
	BAD_TRAP_16                                                              //  0x90
	BAD_TRAP_16                                                              //  0xa0
	BAD_TRAP_16                                                              //  0xb0
	BAD_TRAP_16                                                              //  0xc0
	BAD_TRAP_16                                                              //  0xd0
	BAD_TRAP_16                                                              //  0xe0
	BAD_TRAP_16                                                              //  0xf0

//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim
.globl sparc_window_overflow
sparc_window_overflow:

	rotate_right  t_wim t_tmp1  ! new WIM value

	mov %g0, %wim               ! disable WIM traps
	nop; nop; nop               ! 3 delay slots

	save                        ! go to dirty window
	store_frame  sp             ! store window to stack
	restore                     ! go back to trap window

	mov %t_wim, %wim            ! set new WIM, really set after 3 instructions
	nop; nop; nop

	jmp %t_pc                   ! go back to trapped instruction SAVE
	 rett %t_npc

//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim
.globl sparc_window_underflow
sparc_window_underflow:

	rotate_left  t_wim, t_tmp1  ! new WIM value

	mov %g0, %wim               ! disable WIM traps
	nop; nop; nop               ! 3 delay slots

	restore                     ! go to window to restore
	restore                     !
	restore_frame  sp           ! restore window from stack
	save                        !
	save                        ! go back to trap window

	mov %t_wim, %wim            ! set new WIM, really set after 3 instructions
	nop; nop; nop

	jmp %t_pc                   ! go back to trapped instruction RESTORE
	 rett %t_npc

//--------------------------------------------------------------------------------------------------
// low-level entry point
.section .text
.global _start
_start:

	// setup trap handler table
	sethi %hi(leon_traphandlers), %g2
	wr %g2, %tbr

	// set initial PSR and WIM
	mov 0xfe0, %psr        // irq_off, supervisor, trap_on, CWP=0
	mov 2, %wim            // set invalid windows #1

	// set stack pointer
	sethi %hi(init_stack), %sp
	or %sp, %lo(init_stack), %sp

	// reserv stack frame
	sub %sp, Stack_frame_sz, %sp

	call main
	 nop

//--------------------------------------------------------------------------------------------------
.section .bss
.align 8
.space 0x1000
init_stack:
