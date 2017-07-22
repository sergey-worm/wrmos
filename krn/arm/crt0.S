//##################################################################################################
//
//  Kernel low-level asm code.
//
//##################################################################################################

.equ  SVC_MODE,   0x13
.equ  ABT_MODE,   0x17
.equ  UND_MODE,   0x1b

.equ  NO_IRQ,     0x80              // mask to disable IRQ
.equ  NO_FIQ,     0x40              // mask to disable FIQ

.macro exception_entry
	/**/
	mrs sp, cpsr                // get cpsr, use sp as tmp register
	orr sp, sp, #0x40           // disable fiq
	msr cpsr, sp
	/*~*/
	ldr sp, =cur_ksp            // get address of current kernel sp
	ldr sp, [sp]                // set kernel sp
	stmdb  sp!, {r0-r12, lr}    // store context and klr
	mrs r0, spsr                // get spsr
	stmdb sp!, {r0}             // store spsr
	sub sp, sp, #4              // align to 8 bytes
.endm

.macro exception_exit
	add sp, sp, #4              // skip
	ldmia sp!, {r0}             // load spsr
	msr spsr, r0                // restore spsr
	ldmia sp!, {r0-r12, lr}     // restore context and klr
.endm

.macro exception_entry_fiq
	ldr sp, =cur_ksp            // get address of current kernel sp
	ldr sp, [sp]                // set kernel sp
	stmdb  sp!, {r0-r7, lr}     // store context and klr
	mrs r0, spsr                // get spsr
	stmdb sp!, {r0}             // store spsr
.endm

.macro exception_exit_fiq
	ldmia sp!, {r0}             // load spsr
	msr spsr, r0                // restore spsr
	ldmia sp!, {r0-r7, lr}      // restore context and klr
.endm

.section .text.init, "ax"

.align 5  // trap handler table, must be aligned 32 (2^5)
.global arm_vector_handlers
arm_vector_handlers:

	ldr pc, =handler_reset     // 0x00  Reset
	ldr pc, =handler_undef     // 0x04  Undefined Instruction
	ldr pc, =handler_swi       // 0x08  Software Interrupt
	ldr pc, =handler_pabort    // 0x0c  Prefetch Abort
	ldr pc, =handler_dabort    // 0x10  Data Abort
	nop                        // 0x14  Reserved
	ldr pc, =handler_irq       // 0x18  IRQ
	ldr pc, =handler_fiq       // 0x1c  FIQ


handler_reset:

	mrs r0, spsr
	sub r1, lr, #4
	bl arm_entry_reset
	b .

handler_undef:

	exception_entry
	mrs r0, spsr
	sub r1, lr, #4
	bl arm_entry_undef
	exception_exit
	movs pc, lr                 // return

handler_swi:

	exception_entry             //
	sub r0, lr, #4              // instruction
	bl arm_entry_syscall         //
	exception_exit              //
	movs pc, lr                 // return

handler_pabort:

	exception_entry
	mrs r0, spsr
	sub r1, lr, #4
	bl arm_entry_iabort
	exception_exit
	subs pc, lr, #4             // return

handler_dabort:

	exception_entry
	mrs r0, spsr
	sub r1, lr, #8
	bl arm_entry_dabort
	exception_exit
	subs pc, lr, #8             // return

handler_irq:

	exception_entry
	mrs r0, spsr
	sub r1, lr, #4
	bl arm_entry_irq
	exception_exit
	subs pc, lr, #4             // return

handler_fiq:

	exception_entry_fiq
	mrs r0, spsr
	sub r1, lr, #4
	bl arm_entry_fiq
	exception_exit_fiq
	subs pc, lr, #4             // return


// kernel stack pointer for current thread
.section .data
.global timer_va
.global cur_ksp
.global cur_kentry_start
.global cur_kexit_end
timer_va:          .long 0  // timer virtual address, it needs for time accounting
cur_ksp:           .long 0  // kernel stack pointer for current thread
cur_kentry_start:  .long -1 // kernel entry timer value for current thread
cur_kexit_end:     .long -1 // kernel exit  timer value for current thread


//--------------------------------------------------------------------------------------------------▫
// low-level entry point
.section .bootstrap  // work at phys addresses
.global _start
_start:

	// set VBAR
	ldr r0, =arm_vector_handlers
	mcr p15, 0, r0, c12, c0, 0

	// set SVC stack and go
	msr cpsr, #(SVC_MODE | NO_IRQ | NO_FIQ)
	ldr sp, =bootstrap_stack
	bl bootstrap

.align 3  // sp must be 8-byte aligned (2^3)
.space 0x400
bootstrap_stack:

