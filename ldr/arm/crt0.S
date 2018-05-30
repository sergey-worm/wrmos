//##################################################################################################
//
//  crt0.S - entry point, trap table, etc.
//
//##################################################################################################

#include "sys-config.h"

#define MASTER_CPU_STACK_SZ 0x1000
#define SLAVE_CPU_STACK_SZ 0x400

//--------------------------------------------------------------------------------------------------
// low-level entry point
.section .text
.global _start
_start:

	// read CPU ID
	mrc p15, 0, r0, c0, c0, 5         // get MPIDR
	and r0, r0, #3                    // lower 2 bits
	cmp r0, #0
	bne 1f

	// set stack for master CPU
	ldr sp, =init_stack_svc
	b 2f
1:

	// halt CPU if ID exceeds Cfg_max_cpus
	cmp r0, #Cfg_max_cpus
	bge halt

	// set stack for slave CPU
	ldr sp, =init_stack_svc
	sub sp, sp, #MASTER_CPU_STACK_SZ  // skip CPU #0 stack
	mov r1, #SLAVE_CPU_STACK_SZ       // prepare skip-size
	sub r0, r0, #1                    //
	mul r1, r1, r0                    //
	sub sp, r1                        // skip slave CPUs stacks
2:
	bl main


halt:

	wfe
	b  halt

//--------------------------------------------------------------------------------------------------
.section .bss
.align 3 // align to 8-bytes (8=2^3)
.space (MASTER_CPU_STACK_SZ + SLAVE_CPU_STACK_SZ * (Cfg_max_cpus - 1))
init_stack_svc:
