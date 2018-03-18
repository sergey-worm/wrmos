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
	// nothing for arm
}

void arch_set_timer_va(long va)
{
	extern unsigned timer_va;
	timer_va = va;
}

void arch_set_ksp(uintptr_t ksp)
{
	extern unsigned cur_ksp;
	cur_ksp = ksp;
}

uintptr_t arch_get_ksp()
{
	extern unsigned cur_ksp;
	return cur_ksp;
}

void __attribute__((section(".user.text"))) arch_user_invoke()
{
	// load UTCB address from top of the stack to 0xff000000
	asm volatile
	(
		"ldr  r3, [ sp ]    \n" // get UTCB address
		"mov  r4, #0xff000000  \n" // 
		"str  r3, [ r4 ]    \n" // 
		"add  sp, sp, #4    \n" // correct sp after pop start address
	);

	// read start address from top of the stack
	asm volatile
	(
		"ldr  r5, [ sp ]    \n" // get start address
		"add  sp, sp, #4    \n" // correct sp after pop start address
	);

	// read initial sp from top of the stack
	asm volatile
	(
		"ldr  r6, [ sp ]    \n" // get initial sp
		"add  sp, sp, #4    \n" // correct sp after pop start address
	);

	word_t flags = Stack::pop();
	(void)flags;

	asm volatile ("msr  cpsr_c, #0x10"); // usr

	// set sp and go to thread's start
	asm volatile
	(
		"mov  sp, r6     \n"    // set initial stack
		"mov  pc, r5     \n"    // go to thread entry point
	);
}

void arch_switch_cpu(addr_t* cur_ksp, addr_t nxt_ksp, word_t nxt_utcb)
{
	// set utcb
	*((word_t*)0xff000000) = nxt_utcb;

	// store user and kernel contexts and return address on kstack
	asm volatile
	(
		"stmdb sp!, {sp, lr}^          \n" // store user registers
		"stmdb sp!, {r0-r12, sp, lr}   \n" // store kernel registers
		"adr   lr, 1f                  \n" // prepare return address
		"push  {lr}                    \n" // store return address
	);

	// XXX:   Is nees store/restore all registers r0-r15 ?!
	// TODO:  store/resore user registers if SPSR.mode = User

	// save current stack pointer
	*cur_ksp = Proc::sp();

	// switch to new stack
	Proc::sp(nxt_ksp);

	// load restart address from stack ("1:" or user_invoke())
	asm volatile
	(
		"pop  {pc}                     \n"  // get restart address
	);

	// next thread restarts from here
	asm volatile ("1:");

	// restore user and kernel contexts
	asm volatile
	(
		"ldmia sp!, {r0-r12, sp, lr}    \n"  // restore kernel registers
		"ldmia sp!, {sp, lr}^           \n"  // restore user registars
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
