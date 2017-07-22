//##################################################################################################
//
//  Arch - arch specifics kernel functions.
//
//##################################################################################################

#include "arch.h"
#include "stack.h"
#include "processor.h"
#include "../x86/x86_common.h"

// data for x86 video output (video.h)
unsigned video_x;
unsigned video_y;
uintptr_t video_mem;

void arch_init()
{
	set_gdt();
	set_idt();
	tss.init();
}

void arch_set_timer_va(long va)
{
	(void) va;  // nothing for x86
}

void arch_set_ksp(uintptr_t ksp)
{
	tss.set_ksp(ksp);
}

uintptr_t arch_get_ksp()
{
	return tss.get_ksp();
}

void __attribute__((section(".user.text"))) arch_user_invoke()
{
	// load UTCB address from top of the stack to 0xff000000
	asm volatile
	(
		"pop %rax \n"
	);

	// read start address from top of the stack
	asm volatile
	(
		"pop %rcx"
	);

	// read initial sp from top of the stack
	asm volatile
	(
		"pop %rdx"
	);

	word_t flags = Stack::pop();
	(void)flags;

	// set sel-regs, set return frame on the kstack, call iret
	asm volatile
	(
		"mov $0x23, %ax   \n"  // user data selector
		"mov %ax, %ds     \n"
		"mov %ax, %es     \n"
		"mov %ax, %fs     \n"
		"mov $0x3b, %ax   \n"  // user ku-data selector
		"mov %ax, %gs     \n"
		"push $0x23       \n"  // ss, user data selector
		"push %rdx        \n"  // sp
		"push $0x200      \n"  // rflags:  IF=1 (bit 9)
		"push $0x1b       \n"  // cs, user code selector
		"push %rcx        \n"  // ip
		"iretq            \n"
	);
}

void arch_switch_cpu(addr_t* cur_ksp, addr_t nxt_ksp, word_t nxt_utcb)
{
	// set utcb
	*((word_t*)0xff000000) = nxt_utcb;

	// store to kstack return address and user_sp/lr
	asm volatile
	(
		"push %rax    \n"
		"push %rcx    \n"
		"push %rdx    \n"
		"push %rbx    \n"
		"push %rbp    \n"
		"push %rsi    \n"
		"push %rdi    \n"
		"push %r8     \n"
		"push %r9     \n"
		"push %r10    \n"
		"push %r11    \n"
		"push %r12    \n"
		"push %r13    \n"
		"push %r14    \n"
		"push %r15    \n"
		"push  $1f    \n"
	);

	// save current stack pointer
	*cur_ksp = Proc::sp();

	// switch to new stack
	Proc::sp(nxt_ksp);

	// load restart address from stack ("1:" or user_invoke())
	asm volatile
	(
		"ret        \n"  // jump at restart address from stack
	);

	// next thread restarts from here
	asm volatile ("1:");

	// load  user_sp/lr
	asm volatile
	(
		"pop %r15   \n"
		"pop %r14   \n"
		"pop %r13   \n"
		"pop %r12   \n"
		"pop %r11   \n"
		"pop %r10   \n"
		"pop %r9    \n"
		"pop %r8    \n"
		"pop %rdi   \n"
		"pop %rsi   \n"
		"pop %rbp   \n"
		"pop %rbx   \n"
		"pop %rdx   \n"
		"pop %rcx   \n"
		"pop %rax   \n"
	);
}

void arch_store_floats(void* float_frame)
{
	(void)float_frame;
}

void arch_restore_floats(const void* float_frame)
{
	(void)float_frame;
}
