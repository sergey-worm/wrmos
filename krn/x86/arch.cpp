//##################################################################################################
//
//  Arch - arch specifics kernel functions.
//
//##################################################################################################

#include "arch.h"
#include "sys_stack.h"
#include "sys_proc.h"
#include "x86_common.h"

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

void arch_set_ksp(long ksp)
{
	tss.hw.set_ksp(ksp);
}

long arch_get_ksp()
{
	return tss.hw.get_ksp();
}

void __attribute__((section(".user.text"))) arch_user_invoke()
{
	// load UTCB address from top of the stack to 0xff000000
	asm volatile
	(
		"pop %eax \n"
		"mov %eax, %gs:0 \n"
	);

	// read start address from top of the stack
	asm volatile
	(
		"pop %ecx"
	);

	// read initial sp from top of the stack
	asm volatile
	(
		"pop %edx"
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
		"push $0x23       \n"  // ss
		"push %edx        \n"  // sp
		"push $0x200      \n"  // eflags:  IF=1 (bit 9)
		"push $0x1b       \n"  // cs
		"push %ecx        \n"  // ip
		"iret             \n"
	);
}

word_t xxx;
word_t yyy;

void arch_switch_cpu(addr_t* cur_ksp, addr_t nxt_ksp, word_t nxt_utcb)
{
	// NOTE:        Incomming cur/nxt lay on the stack .
	//              But after store context in assembler code stack will be changed
	//              and cur/nxt from stack will invalid.
	// WORKAROUND:  To force use register instead of stack for cur/nxt call cur/nxt->utcb().
	// FIXME:       Fix this workaround.
	xxx = (word_t) cur_ksp;
	yyy = (word_t) nxt_ksp;

	// set utcb
	*((word_t*)0xff000000) = nxt_utcb;

	// store to kstack CPU context and return address
	asm volatile
	(
		"push %eax      \n"
		"push %ecx      \n"
		"push %edx      \n"
		"push %ebx      \n"
		"push %ebp      \n"
		"push %esi      \n"
		"push %edi      \n"
		"push  $1f      \n"
	);

	// save current stack pointer
	*cur_ksp = Proc::sp();

	// switch to new stack
	Proc::sp(nxt_ksp);

	// load restart address from stack ("1:" or user_invoke())
	asm volatile
	(
		"ret          \n"  // jump at restart address from stack
	);

	// next thread restarts from here
	asm volatile ("1:");

	// load CPU context
	asm volatile
	(
		"pop %edi     \n"
		"pop %esi     \n"
		"pop %ebp     \n"
		"pop %ebx     \n"
		"pop %edx     \n"
		"pop %ecx     \n"
		"pop %eax     \n"
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
