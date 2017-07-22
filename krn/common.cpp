//##################################################################################################
//
//  Common kernel functions.
//
//##################################################################################################

#include "thread.h"
#include "sched.h"

Thread_t* cur_thr() { return Sched_t::current(); }
const char* cur_thr_name() { return Sched_t::current() ? Sched_t::current()->name() : "----"; }

void process_pfault(Thread_t* fault_thr, word_t fault_addr, word_t fault_access, word_t fault_inst)
{
	printk("pfault:  addr=0x%x, access=%d, inst=0x%x.\n", fault_addr, fault_access, fault_inst);

	assert(Sched_t::current() == fault_thr);

	Thread_t* pgr = fault_thr->pager();
	if (!pgr)
		panic("Pagefault:  no pager.");

	if (pgr->state() == Thread_t::Receive_ipc)
	{
		// send pagefault msg
		printk("pfault:  pager wait msg:  pgr_id=%u, pgr_state=%s.\n", pgr->id(), pgr->state_str());
		L4_utcb_t* pgr_utcb = (L4_utcb_t*) pgr->kutcb();
		pgr_utcb->msgtag().proto_label(L4_msgtag_t::Pagefault);
		pgr_utcb->msgtag().pfault_access(fault_access);
		pgr_utcb->msgtag().untyped(2);
		pgr_utcb->msgtag().typed(0);
		pgr_utcb->mr[1] = fault_addr;
		pgr_utcb->mr[2] = fault_inst;
		pgr->entry_frame()->scall_ipc_from(fault_thr->globid().raw());
		pgr->state(Thread_t::Ready);
		fault_thr->pf_save(fault_addr, fault_access, fault_inst);
		fault_thr->state(Thread_t::Receive_pfault);
		Sched_t::switch_to(pgr);
	}
	else
	{
		// wait
		fault_thr->pf_save(fault_addr, fault_access, fault_inst);
		fault_thr->state(Thread_t::Send_pfault);
		Sched_t::switch_to_next();
	}
}

void kdb_console_entry_wrapper(bool krn_mode, bool error_entry, const char* prompt);

// for kernel panic()
extern "C" void break_execution(const char* str)
{
	bool krn_mode = true;
	bool error_entry = true;
	kdb_console_entry_wrapper(krn_mode, error_entry, str);
}
