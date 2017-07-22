//##################################################################################################
//
//  crt0.S - entry point, trap table, etc.
//
//##################################################################################################

.equ  NO_IRQ,     0x80              // mask to disable IRQ
.equ  NO_FIQ,     0x40              // mask to disable FIQ
.equ  NO_INT,     (NO_IRQ | NO_FIQ) // mask to disable IRQ and FIQ
.equ  FIQ_MODE,   0x11
.equ  IRQ_MODE,   0x12
.equ  SVC_MODE,   0x13
.equ  SYS_MODE,   0x1F
.equ  SYS_MODE,   0x1F
.equ  USR_MODE,   0x10

#define STACK_PTR_SVC       0x81000000
#define STACK_PTR_SYS       0x82000000
#define STACK_PTR_IRQ       0x83000000

//--------------------------------------------------------------------------------------------------
// low-level entry point
.section .text
.global _start
_start:

	ldr sp, =init_stack_svc
	bl main

halt:
//	wfe
	b  halt

//--------------------------------------------------------------------------------------------------
.section .bss
.align 8
.space 0x1000
init_stack_svc:
#init_stack_usr:
#.space 0x1000
#init_stack:
