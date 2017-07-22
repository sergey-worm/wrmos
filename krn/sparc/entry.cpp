//##################################################################################################
//
//  High-level kernel entry points.
//  This funcs calls from asm code.
//
//##################################################################################################

#include "sched.h"
#include "kentry.h"

static word_t arch_get_access(word_t mmu_fsr)
{
	enum
	{
		Access_type_shift = 5, //
		Access_type_mask  = 7, // 3 bits

		Acc_ur  = 0, // Load from User Data Space
		Acc_kr  = 1, // Load from Supervisor Data Space
		Acc_ux  = 2, // Load/Execute from User Instruction Space
		Acc_kx  = 3, // Load/Execute from Supervisor Instruction Space
		Acc_uw  = 4, // Store to User Data Space
		Acc_kw  = 5, // Store to Supervisor Data Space
		Acc_uiw = 6, // Store to User Instruction Space
		Acc_kiw = 7, // Store to Supervisor Instruction Space

		Acc_r = 1 << 2,
		Acc_w = 1 << 1,
		Acc_x = 1 << 0
	};
	int at = (mmu_fsr >> Access_type_shift) & Access_type_mask;

	switch (at)
	{
		case Acc_ur:    return Acc_r;
		case Acc_kr:    return Acc_r;
		case Acc_ux:    return Acc_x;
		case Acc_kx:    return Acc_x;
		case Acc_uw:    return Acc_w;
		case Acc_kw:    return Acc_w;
		case Acc_uiw:   return Acc_w;
		case Acc_kiw:   return Acc_w;
	}
	panic("Unknown fault_status=0x%x", mmu_fsr);
	return -1;
}

extern "C" void sparc_entry_irq(unsigned irq)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock("irq_entry", irq==Cfg_krn_timer_irq));

	irq = Intc::real_irq(irq);       // for SPARC IRQMP extended interrupt
	kentry_irq(irq);

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock("irq_exit"));
}

extern "C" void sparc_entry_syscall(word_t inst)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock("sc_entry"));

	(void) inst;
	kentry_syscall();

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock("sc_exit"));
}

extern "C" void sparc_entry_pagefault(word_t fault_addr, word_t fault_status, word_t fault_inst)
{
	bool krn_mode = Proc::psr() & (1<<6); // psr.ps // must be first operation in this func !
	if (krn_mode)
		panic("Pagefault in kernel mode:  addr=0x%x, status=0x%x, inst=0x%x, psr=0x%x.\n",
			fault_addr, fault_status, fault_inst, Proc::psr());

	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock("pf_entry"));

	printk("pfault:  addr=0x%x, status=0x%x, inst=0x%x, psr=0x%x.\n",
		fault_addr, fault_status, fault_inst, Proc::psr());
	kentry_pagefault(fault_addr, arch_get_access(fault_status), fault_inst);

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock("pf_exit"));
}

void kdb_console_entry_wrapper(bool krn_mode, bool error_entry, const char* prompt);

extern "C" void sparc_entry_bad_trap(unsigned ttype, unsigned psr, unsigned pc, unsigned npc)
{
	bool krn_mode = Proc::psr() & (1<<6); // psr.ps // must be first operation in this func !
	const char* str = "unknown_trap";
	switch (ttype)
	{
		case 0x00:  str = "reset";                         break;
		case 0x2b:  str = "data_store_error";              break;
		case 0x3c:  str = "instruction_access_MMU_miss";   break;
		case 0x21:  str = "instruction_access_error";      break;
		case 0x20:  str = "r_register_access_error";       break;
		case 0x01:  str = "instruction_access_exception";  break;
		case 0x03:  str = "privileged_instruction";        break;
		case 0x02:  str = "illegal_instruction";           break;
		case 0x04:  str = "fp_disabled";                   break;
		case 0x24:  str = "cp_disabled";                   break;
		case 0x25:  str = "unimplemented_FLUSH";           break;
		case 0x0B:  str = "watchpoint_detected";           break;
		case 0x05:  str = "window_overflow";               break;
		case 0x06:  str = "window_underflow";              break;
		case 0x07:  str = "mem_address_not_aligned";       break;
		case 0x08:  str = "fp_exception";                  break;
		case 0x28:  str = "cp_exception";                  break;
		case 0x29:  str = "data_access_error";             break;
		case 0x2c:  str = "data_access_MMU_miss";          break;
		case 0x09:  str = "data_access_exception";         break;
		case 0x0a:  str = "tag_overflow";                  break;
		case 0x2a:  str = "division_by_zero";              break;
	}

	force_printk("BAD_TRAP:  tt=%s(%u):  psr=0x%x, pc=0x%x, npc=0x%x.\n", str, ttype, psr, pc, npc);
	(void)psr; (void)pc, (void)npc;

	int fixed = 0;
	if (ttype == 0x04) // fp_disabled
	{
		Thread_t* cur = Sched_t::current();
		if (!krn_mode  &&  cur->flags() == 0x1)
		{
			// FPU is allowed for cur thread, enable it
			cur->entry_frame()->enable_fpu();
			cur->fpu_in_use(true);
			fixed = 1;
		}
	}

	if (!fixed)
	{
		bool err_entry = true;
		kdb_console_entry_wrapper(krn_mode, err_entry, "bad trap");
	}
}

