//##################################################################################################
//
//  Arch - arch specifics kernel functions.
//
//##################################################################################################

#include "arch.h"
#include "sys_stack.h"
#include "sys_proc.h"

void arch_init()
{
	// nothing for sparc
}

void arch_set_timer_va(long va)
{
	extern unsigned timer_va;
	timer_va = va;
}

void arch_set_ksp(long ksp)
{
	extern unsigned cur_ksp;
	cur_ksp = ksp;
}

long arch_get_ksp()
{
	extern unsigned cur_ksp;
	return cur_ksp;
}

void __attribute__((section(".user.text"))) arch_user_invoke()
{
	// NOTE:  We assume that this func does not have epilogue (preserving stack for regwin)
	//        at the start.
	//        Do not use any C-code here to preserve adding epilogue by compiler!

	// clean inc regs
	asm volatile
	(
		"mov %g0, %i0       \n"
		"mov %g0, %i1       \n"
		"mov %g0, %i2       \n"
		"mov %g0, %i3       \n"
		"mov %g0, %i4       \n"
		"mov %g0, %i5       \n"
		"mov %g0, %i6       \n"
		"mov %g0, %i7       \n"
	);

	asm volatile
	(
		#if 0
		"ld  [ %sp ], %g7   \n" // get UTCB address
		#else
		"ld  [ %sp ], %l0   \n" // get UTCB address
		"set 0xff000000, %l1\n"
		"st %l0, [%l1]      \n "
		#endif
		"add %sp, 4, %sp    \n" // correct sp after pop start address
	);

	// read start address from top of the stack
	asm volatile
	(
		"ld  [ %sp ], %o7   \n" // get start address
		"add %sp, 4, %sp    \n" // correct sp after pop start address
	);

	// read initial sp from top of the stack
	asm volatile
	(
		"ld  [ %sp ], %o5   \n" // get initial sp
		"add %sp, 4, %sp    \n" // correct sp after pop sp
	);

	word_t flags = Stack::pop();
	(void) flags;

	// set new PSR
	word_t psr = Proc::psr();
	psr &= ~(0xf << 8);         // enable IRQ
	psr &= ~(0x1 << 7);         // user mode
	psr &= ~(1 << 12);          // disable fpu, enabling my be inside fp_disabled trap

	Proc::psr(psr);             // switch to user mode

	// set sp and go to thread's start
	asm volatile
	(
		"jmp %o7         \n"    // go to thread entry point
		" mov %o5, %sp   \n"    // set initial stack
	);
}

void arch_switch_cpu(addr_t* cur_ksp, addr_t nxt_ksp, word_t nxt_utcb)
{
	#if 0
	(void) nxt_utcb;  // unused for sparc, utcb always lay in g7
	#else
	// set utcb
	*((word_t*)0xff000000) = nxt_utcb;
	#endif

	// store all dirty regwins including current
	Proc::flush_regwins();

	// store o-registers (but o6/sp)
	asm volatile
	(
		"sub   %sp, 32, %sp      \n"
		"std   %o0, [%sp +  0]   \n"
		"std   %o2, [%sp +  8]   \n"
		"std   %o4, [%sp + 16]   \n"
		"st    %o7, [%sp + 24]   \n"
	);

	// store g-registers, use o4 as scratch
	asm volatile
	(
		"sub   %sp, 32, %sp    \n"
		"rd  %y, %o4           \n"
		"st  %o4, [%sp +  0]   \n"
		"st  %g1, [%sp +  4]   \n"
		"std %g2, [%sp +  8]   \n"
		"std %g4, [%sp + 16]   \n"
		"std %g6, [%sp + 24]   \n"
	);

	// store return address
	asm volatile
	(
		"sub   %sp, 4, %sp       \n"
		"set   1f, %g1           \n"
		"st    %g1, [%sp]        \n"
	);

	// save current stack pointer
	*cur_ksp = Proc::sp();

	// switch to new stack
	Proc::sp(nxt_ksp);

	// load restart address from stack ("1:" or user_invoke())
	asm volatile
	(
		"ld   [%sp], %g1     \n"
		"jmp  %g1            \n"
		" add  %sp, 4, %sp   \n"
	);

	// next thread restarts from here
	asm volatile ("1:");

	// restore g-registers, use o4 as scratch
	asm volatile
	(
		"ld  [%sp +  0], %o4     \n"
		"ld  [%sp +  4], %g1     \n"
		"ldd [%sp +  8], %g2     \n"
		"ldd [%sp + 16], %g4     \n"
		"ldd [%sp + 24], %g6     \n"
		"wr  %o4, %y             \n"
		"add  %sp, 32, %sp       \n"
	);

	// restore o-registers (but o6/sp)
	asm volatile
	(
		"ldd  [%sp +  0], %o0  \n"
		"ldd  [%sp +  8], %o2  \n"
		"ldd  [%sp + 16], %o4  \n"
		"ld   [%sp + 24], %o7  \n"
		"add  %sp, 32, %sp     \n"
	);

	// restore l/i-registers
	asm volatile
	(
		"ldd  [%sp + 0x00], %l0  \n"
		"ldd  [%sp + 0x08], %l2  \n"
		"ldd  [%sp + 0x10], %l4  \n"
		"ldd  [%sp + 0x18], %l6  \n"
		"ldd  [%sp + 0x20], %i0  \n"
		"ldd  [%sp + 0x28], %i2  \n"
		"ldd  [%sp + 0x30], %i4  \n"
		"ldd  [%sp + 0x38], %i6  \n"
	);
}

void arch_store_floats(void* float_frame)
{
	asm volatile
	(
		"std %%f0,  [%[base] +   0]; "
		"std %%f2,  [%[base] +   8]; "
		"std %%f4,  [%[base] +  16]; "
		"std %%f6,  [%[base] +  24]; "
		"std %%f8,  [%[base] +  32]; "
		"std %%f10, [%[base] +  40]; "
		"std %%f12, [%[base] +  48]; "
		"std %%f14, [%[base] +  56]; "
		"std %%f16, [%[base] +  64]; "
		"std %%f18, [%[base] +  72]; "
		"std %%f20, [%[base] +  80]; "
		"std %%f22, [%[base] +  88]; "
		"std %%f24, [%[base] +  96]; "
		"std %%f26, [%[base] + 104]; "
		"std %%f28, [%[base] + 112]; "
		"std %%f30, [%[base] + 120]; "
		"st  %%fsr, [%[base] + 128]; "
		:: [base]"r"(float_frame)
	);
}

void arch_restore_floats(const void* float_frame)
{
	// now fpu may be disabled, enable it
	word_t psr = Proc::psr();
	Proc::psr(psr | (1<<12));  // psr.ef=1

	asm volatile
	(
		"ldd [%[base] +   0], %%f0;  "
		"ldd [%[base] +   8], %%f2;  "
		"ldd [%[base] +  16], %%f4;  "
		"ldd [%[base] +  24], %%f6;  "
		"ldd [%[base] +  32], %%f8;  "
		"ldd [%[base] +  40], %%f10; "
		"ldd [%[base] +  48], %%f12; "
		"ldd [%[base] +  56], %%f14; "
		"ldd [%[base] +  64], %%f16; "
		"ldd [%[base] +  72], %%f18; "
		"ldd [%[base] +  80], %%f20; "
		"ldd [%[base] +  88], %%f22; "
		"ldd [%[base] +  96], %%f24; "
		"ldd [%[base] + 104], %%f26; "
		"ldd [%[base] + 112], %%f28; "
		"ldd [%[base] + 120], %%f30; "
		"ld  [%[base] + 128], %%fsr; "
		:: [base]"r"(float_frame)
	);
}
