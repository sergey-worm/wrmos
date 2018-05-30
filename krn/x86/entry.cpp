//##################################################################################################
//
//  High-level kernel entry points.
//  This funcs calls from asm code.
//
//##################################################################################################

#include "sched.h"
#include "kentry.h"
#include <assert.h>

enum
{
	Exc_div_by_zero     =  0,
	Exc_debug           =  1,
	Exc_nmi             =  2,
	Exc_breakpoint      =  3,
	Exc_overflow        =  4,
	Exc_bound_range     =  5,
	Exc_inv_opcode      =  6,
	Exc_dev_not_avail   =  7,
	Exc_double_fault    =  8,
	Exc_cop_seg_or      =  9,
	Exc_inv_tss         = 10,
	Exc_seg_not_pres    = 11,
	Exc_stack_fault     = 12,
	Exc_gprot_fault     = 13,
	Exc_page_fault      = 14,
	Exc_x87_fpu_err     = 16,
	Exc_alignment_check = 17,
	Exc_machine_check   = 18,
	Exc_simd_fpu_exc    = 19
};

const char* exc2str(unsigned exc)
{
	switch (exc)
	{
		case Exc_div_by_zero:      return "div_by_zero";
		case Exc_debug:            return "debug";
		case Exc_nmi:              return "nmi";
		case Exc_breakpoint:       return "breakpoint";
		case Exc_overflow:         return "overflow";
		case Exc_bound_range:      return "bound_range";
		case Exc_inv_opcode:       return "inv_opcode";
		case Exc_dev_not_avail:    return "dev_not_avail";
		case Exc_double_fault:     return "double_fault";
		case Exc_cop_seg_or:       return "cop_seg_or";
		case Exc_inv_tss:          return "inv_tss";
		case Exc_seg_not_pres:     return "seg_not_pres";
		case Exc_stack_fault:      return "stack_fault";
		case Exc_gprot_fault:      return "gprot_fault";
		case Exc_page_fault:       return "page_fault";
		case Exc_x87_fpu_err:      return "x87_fpu_err";
		case Exc_alignment_check:  return "alignment_check";
		case Exc_machine_check:    return "machine_check";
		case Exc_simd_fpu_exc:     return "simd_fpu_exc";
	}
	return "__unknown_exc__";
}

struct Selector_ecode_t
{
	enum
	{
		Gdt  = 0,
		Idt  = 1,
		Ldt  = 2,
		Idt2 = 3,
	};

	union
	{
		struct
		{
			unsigned ext       :  1;  // exception originated externally to the processor
			unsigned tbl       :  2;  // table type
			unsigned index     : 13;  // selector index
			unsigned reserved  : 16;
		};
		uint32_t raw;
	};

	const char* table()
	{
		switch (tbl)
		{
			case Gdt:   return "gdt";
			case Idt:   return "idt";
			case Ldt:   return "ldt";
			case Idt2:  return "idt2";
		}
	}

	Selector_ecode_t(uint32_t val) : raw(val) {}
};

static word_t arch_get_access(word_t ecode)
{
	enum
	{
		Present    = 1 << 0,
		Write      = 1 << 1,
		User       = 1 << 2,  // CPL=3
		Res_write  = 1 << 3,  // reading a 1 in a reserved field
		Inst_fetch = 1 << 4,

		// FIXME:  use thread acc declared
		Acc_r = 1 << 2,
		Acc_w = 1 << 1,
		Acc_x = 1 << 0
	};

	if (ecode & Present)
	{
		// NOTE:  it is normal, it is mean that page was mapped with one access but
		//        required another, need just to remap this page

		//printf("pfault.ecode=0x%lx contents Present bit:\n", ecode);
		//Sched_t::current()->task()->dump();
		//panic("IMPME:  pfault.ecode=0x%lx contents Present bit. What can I do?\n", ecode);
	}

	if (ecode & Res_write)
		panic("IMPME:  pfault.ecode=0x%lx contents Reserved_write bit. What can I do?\n", ecode);

	if (!(ecode & User))
		panic("Pfault in kernel mode, ecode=0x%lx.\n", ecode);

	int acc = 0;

	if (ecode & Write)
		acc |= Acc_w;
	else if (ecode & Inst_fetch)
		acc |= Acc_x;
	else
		acc |= Acc_r;

	return acc;
}

static void exc_handler(unsigned exc, Entry_frame_t* eframe)
{
	//printk("exc:  exc=%u/%s, frame=0x%x.\n", exc, exc2str(exc), eframe);
	//eframe->dump();

	switch (exc)
	{
		case Exc_div_by_zero:
		case Exc_debug:
		case Exc_nmi:
		case Exc_breakpoint:
		case Exc_overflow:
		case Exc_bound_range:
		case Exc_inv_opcode:
		case Exc_dev_not_avail:
		case Exc_double_fault:
		case Exc_cop_seg_or:
		case Exc_inv_tss:
		case Exc_seg_not_pres:
		case Exc_stack_fault:
		{
			/**/printk("exc:  exc=%u/%s, addr=0x%lx, inst=0x%lx.\n",
			/**/	exc, exc2str(exc), Proc::cr2(), eframe->syscall_frame.ip());
			/**/eframe->dump(printf);

			if (eframe->is_krn_entry())
				panic("exception in kernel mode.");

			panic("IMPLME:  exc=%u/%s.\n", exc, exc2str(exc));
			break;
		}
		case Exc_gprot_fault:
		{
			/**/printk("exc:  exc=%u/%s, inst=0x%lx.\n",
			/**/	exc, exc2str(exc), eframe->syscall_frame.ip());

			Selector_ecode_t e = eframe->error();
			printk("ext=%u, tbl=%u/%s, index=%u.\n", e.ext, e.tbl, e.table(), e.index);

			if (eframe->is_krn_entry())
				panic("exception in kernel mode:  exc=%u/%s.\n", exc, exc2str(exc));

			enum { Opcode_in = 0xec, Opcode_out = 0xee };
			unsigned long ip = eframe->syscall_frame.ip();
			uint8_t opcode = *(uint8_t*)ip;

			if (opcode == Opcode_in  ||  opcode == Opcode_out)
			{
				unsigned port = eframe->dx();
				kentry_pagefault(port, 1<<3/*Acc_ioop*/, ip);
			}
			else
			{
				panic("IMPLME:  exc=%u/%s.\n", exc, exc2str(exc));
			}
			break;
		}
		case Exc_page_fault:
		{
			/**/printk("exc:  exc=%u/%s, addr=0x%lx, inst=0x%lx.\n",
			/**/	exc, exc2str(exc), Proc::cr2(), eframe->syscall_frame.ip());
			if (eframe->is_krn_entry())
			{
				eframe->dump(printf);
				panic("Pagefault in kernel mode:  addr=0x%lx, inst=0x%lx.",
					Proc::cr2(), eframe->syscall_frame.ip());
			}
			word_t fault_addr   = Proc::cr2();
			word_t fault_access = arch_get_access(eframe->error());
			word_t fault_inst   = eframe->syscall_frame.ip();
			kentry_pagefault(fault_addr, fault_access, fault_inst);
			break;
		}
		case Exc_x87_fpu_err:
		case Exc_alignment_check:
		case Exc_machine_check:
		case Exc_simd_fpu_exc:
		{
			/**/printk("exc:  exc=%u/%s, addr=0x%lx, inst=0x%lx.\n",
			/**/	exc, exc2str(exc), Proc::cr2(), eframe->syscall_frame.ip());
			/**/eframe->dump(printf);

			if (eframe->is_krn_entry())
				panic("exception in kernel mode.");

			panic("IMPLME:  exc=%u/%s.\n", exc, exc2str(exc));
			break;
		}
		default:
			printk("exc:  exc=%u/%s, addr=0x%lx, inst=0x%lx.\n",
				exc, exc2str(exc), Proc::cr2(), eframe->syscall_frame.ip());
			eframe->dump(printf);
			panic("unknow exception");
	}
}

static void irq_handler(unsigned irq, Entry_frame_t* eframe)
{
	(void) eframe;
	kentry_irq(irq);
}

static void scall_handler(unsigned trapno, Entry_frame_t* eframe)
{
	//printk("sw irq:  trapno=%u, frame=0x%lx.\n", trapno, eframe);
	//eframe->dump();
	if (trapno == 0x80)
		kentry_syscall();
	else
		panic("IMPLME:  scall trap:  trapno=%u, frame=0x%p.\n", trapno, eframe);
}

extern "C" void x86_entry_trap(word_t gateno, Entry_frame_t* eframe)
{
	Sched_t::current()->tmevent_kentry_end(SystemClock_t::sys_clock(__func__, gateno==0x20));

	//printk_notime("trap:  gateno=0x%lx/%lu, frame=0x%p.\n", gateno, gateno, eframe);
	//eframe->dump(printf);
	//printk("eframe=0x%p, cur_eframe=0x%p.\n", eframe, Sched_t::current()->entry_frame());

	// pfault  ||  idle thread  ||  check kentry_sp
	assert(gateno == Exc_page_fault  ||  !Sched_t::current()->id()  ||  eframe == Sched_t::current()->entry_frame());

	if (gateno < 0x20)
		exc_handler(gateno, eframe);
	else if (gateno >= 0x20  &&  gateno < 0x30)
		irq_handler(gateno - 0x20, eframe);
	else
		scall_handler(gateno, eframe);

	Sched_t::current()->tmevent_kexit_start(SystemClock_t::sys_clock(__func__));
}
