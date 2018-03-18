//##################################################################################################
//
//  High-level kernel entry points.
//  This funcs calls from asm code.
//
//##################################################################################################

#include "sched.h"
#include "kentry.h"

extern "C" void arm_entry_undef(addr_t spsr, addr_t inst)
{
	force_printk("undef_trap:  spsr=0x%lx, inst=0x%lx.\n", spsr, inst);
	while (1);
}

extern "C" void arm_entry_iabort(addr_t spsr, addr_t inst)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock(__func__));

	addr_t inst_fault_addr = 0;
	asm volatile ("mrc  p15, 0, %0, c6, c0, 2" : "=r"(inst_fault_addr)); // IFAR

	addr_t inst_fault_status = 0;
	asm volatile ("mrc  p15, 0, %0, c5, c0, 1" : "=r"(inst_fault_status)); // IFSR

	force_printk("iabort_trap:  spsr=0x%lx, inst=0x%lx, fault_addr=0x%lx, fault_status=0x%lx.\n",
		spsr, inst, inst_fault_addr, inst_fault_status);

	bool krn_mode = (spsr & 0x1f) != 0x10;

	//printk("pfault:  addr=0x%x, status=0x%x, inst=0x%x.\n", fault_addr, fault_status, fault_inst);

	if (krn_mode)
		panic("Pagefault in kernel mode.");

	//Sched_t::current()->entry_frame()->dump();

	printk("user:  sp=0x%lx, lr=0x%lx.\n", Proc::usp(), Proc::ulr());

	kentry_pagefault(inst_fault_addr, Acc_x, inst);

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock(__func__));
}

extern "C" void arm_entry_dabort(addr_t spsr, addr_t inst)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock(__func__));

	addr_t data_fault_addr = 0;
	asm volatile ("mrc  p15, 0, %0, c6, c0, 0" : "=r"(data_fault_addr));  // DFAR

	addr_t data_fault_status = 0;
	asm volatile ("mrc  p15, 0, %0, c5, c0, 0" : "=r"(data_fault_status)); // DFSR

	force_printk("dabort_trap:  spsr=0x%lx, inst=0x%lx, fault_addr=0x%lx, fault_status=0x%lx.\n",
		spsr, inst, data_fault_addr, data_fault_status);

	bool krn_mode = (spsr & 0x1f) != 0x10; //Proc::psr() & (1<<6); // psr.ps

	//printk("pfault:  addr=0x%x, status=0x%x, inst=0x%x.\n", fault_addr, fault_status, fault_inst);

	if (krn_mode)
		panic("Pagefault in kernel mode.");

	//Sched_t::current()->entry_frame()->dump();

	printk("user:  sp=0x%lx, lr=0x%lx.\n", Proc::usp(), Proc::ulr());

	int access = (data_fault_status >> 11) & 1  ? Acc_w : Acc_r;

	kentry_pagefault(data_fault_addr, access, inst);

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock(__func__));
}

extern "C" void arm_entry_reset(addr_t spsr, addr_t inst)
{
	force_printk("reset_trap:  spsr=0x%lx, inst=0x%lx.\n", spsr, inst);
	while (1);
}

extern "C" void arm_entry_irq(addr_t spsr, addr_t inst)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock(__func__));

	(void)spsr;
	(void)inst;
	unsigned irq = Intc::irq();
	kentry_irq(irq);

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock(__func__));
}

static void dprint1(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("dbg:  ");
	vprintf(fmt, args);
	va_end(args);
}

extern "C" void arm_entry_fiq(addr_t spsr, addr_t inst)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock(__func__));

	printf("fiq_trap:  spsr=0x%lx, inst=0x%lx.\n", spsr, inst);
	Intc::dump(dprint1);
	while (1);

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock(__func__));
}

extern "C" void arm_entry_syscall(word_t inst)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock(__func__));

	(void) inst;
	kentry_syscall();

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock(__func__));
}

