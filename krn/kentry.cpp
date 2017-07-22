//##################################################################################################
//
//  Highest-level kernel entry points.
//  This funcs calls from arch/entry.cpp
//
//##################################################################################################

#include "syscalls.h"
#include "types.h"
#include "sched.h"
#include "l4syscalls.h"
#include "threads.h"

void process_pfault(Thread_t* fault_thr, word_t fault_addr, word_t fault_access, word_t fault_inst);

static void kern_timer_tick()
{
	// update system clock
	SystemClock_t::tick();

	Thread_t* cur = Sched_t::current();

	// update timeslice of current thread
	L4_clock_t now = SystemClock_t::sys_clock(__func__);
	cur->tmevent_tick(now);

	#if 0
	// this code steal char from serial driver, but it is useful for debug
	// check Esc key
	int c = Uart::getch();
	if (c == 0x1b)
		kdb_console_entry_wrapper(false, false, "Escape key is pressed.");
	#endif

	Thread_t* next1 = Threads_t::check_ipc_timeouts(now); // may be hi-prio
	Thread_t* next2 = 0;                                  // same-prio

	if (!cur->timeslice())
		next2 = *Threads_t::timeslice_expired(cur);

	if (next1  &&  next1->prio_max() > cur->prio_max())
		Sched_t::switch_to(next1);  // expired timeout for hi-prio thread
	else if (next2)
		Sched_t::switch_to(next2);  // expiret timeslice
}

void kentry_pagefault(word_t fault_addr, word_t fault_access, word_t fault_inst)
{
	process_pfault(Sched_t::current(), fault_addr, fault_access, fault_inst);
}

void kentry_syscall()
{
	Thread_t* cur = Sched_t::current();

	Entry_frame_t* eframe = cur->entry_frame();

	printk("syscall:  %s:  scall=0x%x.\n", cur->name(), eframe->scall_number());
	//eframe->dump(true);

	switch (eframe->scall_number())
	{
		case L4_syscall_kernel_interface:
		{
			// put KIP virt addr to entry_frame
			eframe->scall_kip_base_addr(cur->user_kip());
			printk("kip entry:  kip=%x.\n", cur->user_kip());
			break;
		}
		case L4_syscall_exchange_registers:
		{
			syscall_exchange_registers(*cur, *eframe);
			break;
		}
		case L4_syscall_thread_control:
		{
			syscall_thread_control(*cur, *eframe);
			break;
		}
		case L4_syscall_system_clock:
		{
			// TODO:  now syscall duration 4-5 usec, do in ASM and compare duration
			L4_clock_t clock = SystemClock_t::sys_clock(__func__);
			eframe->scall_sclock(clock);
			break;
		}
		case L4_syscall_thread_switch:
		{
			syscall_thread_switch(*cur, *eframe);
			break;
		}
		case L4_syscall_schedule:
		{
			syscall_schedule(*cur, *eframe);
			break;
		}
		case L4_syscall_ipc:
		{
			syscall_ipc(*cur, *eframe);
			break;
		}
		case L4_syscall_lipc:
		{
			panic("Unsupported syscall:  lipc.\n");
			break;
		}
		case L4_syscall_unmap:
		{
			panic("Unsupported syscall:  unmap.\n");
			break;
		}
		case L4_syscall_space_control:
		{
			syscall_space_control(*cur, *eframe);
			break;
		}
		case L4_syscall_processor_control:
		{
			panic("Unsupported syscall:  processor_control.\n");
			break;
		}
		case L4_syscall_memory_control:
		{
			syscall_memory_control(*cur, *eframe);
			break;
		}
		case L4_syscall_kdb:
		{
			syscall_kdb(*cur, *eframe);
			break;
		}
		default:
		{
			printk("\n");
			eframe->dump();
			panic("Unknown syscall:  syscall=0x%x.\n", eframe->scall_number());
		}
	}
}

void kentry_irq(unsigned irq)
{
	Intc::clear(irq);
	if (irq == Cfg_krn_timer_irq)
	{
		Timer::irq_ack();
		kern_timer_tick();
	}
	else
	{
		// route irq to waiting thread
		Intc::mask(irq);  // disable interrupt until it re-enable msg
		Int_thread_t* ithr = Threads_t::int_thread(irq);
		assert(ithr);
		L4_thrid_t h = ithr->handler();
		assert(!h.is_nil());
		Thread_t* dst = Threads_t::find(h);
		assert(dst);

		if (dst->state() == Thread_t::Receive_ipc  &&  dst->is_irq_acceptable(irq))
		{
			L4_utcb_t* utcb = Sched_t::current()->task()==dst->task() ? dst->utcb() : dst->kutcb();
			utcb->msgtag().proto_label(L4_msgtag_t::Interrupt);
			utcb->msgtag().proto_nulls();
			utcb->msgtag().untyped(0);
			utcb->msgtag().typed(0);
			dst->entry_frame()->scall_ipc_from(thrid_int(irq).raw()); // from

			dst->state(Thread_t::Ready);

			if (dst->prio_max() > Sched_t::current()->prio_max())
				Sched_t::switch_to(dst);
		}
		else // receiver is not ready to ipc
		{
			ithr->pending(true);
		}
	}
}

