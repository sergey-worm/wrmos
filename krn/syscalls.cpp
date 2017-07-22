//##################################################################################################
//
//  L4 syscalls.
//
//##################################################################################################

#include "syscalls.h"
#include "threads.h"
#include "l4syscalls.h"

void syscall_exchange_registers(Thread_t& cur, Entry_frame_t& eframe)
{
	printk("exchange_registers entry:  id=0x%x/%u.\n", cur.globid().raw(), cur.globid().number());

	L4_thrid_t dst     = eframe.scall_exreg_dest();
	word_t     control = eframe.scall_exreg_control();
	word_t     sp      = eframe.scall_exreg_sp();
	word_t     ip      = eframe.scall_exreg_ip();
	L4_thrid_t pager   = eframe.scall_exreg_pager();
	word_t     uhandle = eframe.scall_exreg_uhandle();;
	word_t     flags   = eframe.scall_exreg_flags();

	(void)sp; (void)ip; (void)uhandle;

	printk("exreg:  dst=0x%x/%u, ctl=%c%c%c%c%c%c%c%c%c%c, sp=0x%x, ip=0x%x, pgr=0x%x/%u, uhnd=0x%x, flags=0x%x.\n",
		dst.raw(), dst.number(),
		control & L4_exreg_ctl_d ? 'd' : '-', control & L4_exreg_ctl_h ? 'h' : '-',
		control & L4_exreg_ctl_p ? 'p' : '-', control & L4_exreg_ctl_u ? 'u' : '-',
		control & L4_exreg_ctl_f ? 'f' : '-', control & L4_exreg_ctl_i ? 'i' : '-',
		control & L4_exreg_ctl_s ? 's' : '-', control & L4_exreg_ctl_S ? 'S' : '-',
		control & L4_exreg_ctl_R ? 'R' : '-', control & L4_exreg_ctl_H ? 'H' : '-',
		sp, ip, pager.raw(), pager.number(), uhandle, flags);

	eframe.scall_exreg_result(L4_thrid_t::Nil);  // syscall failed

	// find dst thread
	Thread_t* dst_thr = Threads_t::find(dst);
	if (!dst_thr  ||  dst_thr->task() != cur.task())
	{
		printk("exreg:  ERROR:  no thread with id=0x%x/%u.\n", dst.raw(), dst.number());
		cur.utcb()->error(2);         // UnavailableThread
		return;
	}

	if (control == L4_exreg_ctl_f)
	{
		eframe.scall_exreg_flags(dst_thr->flags());
		dst_thr->flags(flags);
	}
	else
	{
		panic("exreg:  unimplemented control=0x%x, implme!\n", control);
	}

	eframe.scall_exreg_result(dst.is_local() ? dst_thr->globid().raw() : dst_thr->localid().raw());  // syscall success
}

void syscall_thread_control(Thread_t& cur, Entry_frame_t& eframe)
{
	printk("thread_control entry:  id=0x%x/%u.\n", cur.globid().raw(), cur.globid().number());

	L4_thrid_t dst   = eframe.scall_thctl_dest();
	L4_thrid_t space = eframe.scall_thctl_space();
	L4_thrid_t sched = eframe.scall_thctl_scheduler();
	L4_thrid_t pager = eframe.scall_thctl_pager();
	addr_t     utcb  = eframe.scall_thctl_utcb_location();

	printk("tctl:  dst=0x%x/%u, space=0x%x/%u, sched=0x%x/%u, pager=0x%x/%u, utcb=0x%x.\n",
		 dst.raw(), dst.number(), space.raw(), space.number(),
		 sched.raw(), sched.number(),  pager.raw(), pager.number(), utcb);

	eframe.scall_thctl_result(0);   // syscall failed

	// check privileges
	L4_thrid_t cur_id = cur.globid();
	if (cur_id != thrid_sigma0()  &&  cur_id != thrid_roottask())
	{
		printk("tctl:  ERROR:  no privilege.\n");
		cur.utcb()->error(1);  // NoPrivilege
		return;
	}

	// extention - set interrupt handler
	if (thrid_is_int(dst)  &&  dst.number() != Cfg_krn_timer_irq)
	{
		printk("tctl:  set INT handler.\n\n\n");
		// TODO:  check that pager is user and exist
		Int_thread_t* ithr = Threads_t::int_thread(dst.number());
		assert(ithr);
		ithr->handler(pager);
		eframe.scall_thctl_result(1);  // syscall success
		return;
	}

	// check dst
	if (!thrid_is_global_user(dst))
	{
		printk("tctl:  ERROR:  wrong dst=0x%x/%u.\n", dst.raw(), dst.number());
		cur.utcb()->error(2);  // UnavailableThread
		return;
	}

	// check utcb location
	if (0/*TODO*/)
	{
		printk("tctl:  ERROR:  invalid UTCB location.\n");
		cur.utcb()->error(6);  // InvalidUtcb
		return;
	}

	// find dst thread
	Thread_t* dst_thr = Threads_t::find(dst.number()); // dst may exists or not

	printk("tctl:  dst_thr=%x/%s, dst=0x%x/%u, space=0x%x.\n",
		dst_thr, dst_thr ? dst_thr->name() : "---", dst.raw(), dst.number(), space.raw());

	// thread creation
	if (!space.is_nil() && !dst_thr)
	{
		printk("tctl:  Creation of new thread.\n");

		// check sched - should exists for creation
		if (sched.is_nil())
		{
			printk("tctl:  ERROR:  schedeler thread ID should be passed for thread creation.\n");
			cur.utcb()->error(4);  // InvalidSched
			return;
		}

		// check pager - should be Nil for aspace creation
		if (dst == space  &&  !pager.is_nil())
		{
			printk("tctl:  ERROR:  pager should be Nil for aspace creation.\n");
			cur.utcb()->error(3);  // InvalidSpace
			return;
		}

		char name[8];
		snprintf(name, sizeof(name), "%04u", dst.number());

		Task_t* tsk = 0;
		if (dst == space) // aspace creation
		{
			printk("tctl:  Creation of new aspace.\n");
			tsk = &Tasks_t::create();
			tsk->name(name);
		}
		else
		{
			// task my not exists
			Thread_t* space_thr = Threads_t::find(space);
			if (space_thr)
				tsk = space_thr->task();

			// check aspace - task should be configured if thread will be activate
			if (!pager.is_nil()           &&                  // thread will be activate and
				(!tsk                     ||                  // task does not exist or
				 !tsk->is_configured()    ||                  // task is not configured or
				 !tsk->is_able_to_activate_new_thread()))     // task is not able to activate new thread
			{
				if (!tsk || !tsk->is_configured())
					force_printk("tctl:  ERROR:  activation in not configured aspace.\n");
				else
					force_printk("tctl:  ERROR:  task is not able to activate new thread, maxthr=%u.\n",
						tsk->threads_max());
				cur.utcb()->error(3);  // InvalidSpace
				return;
			}
		}

		dst_thr = &Threads_t::create(dst, sched, pager, tsk, cur.prio(), name);

		// set utcb_location if need
		if (utcb != -1)
		{
			// utcb should be mapped to cur task
			paddr_t pa = cur.task()->walk(utcb, Cfg_page_sz);
			assert(pa);
			dst_thr->utcb_location(pa);
		}

		if (!pager.is_nil())
			dst_thr->activate();
	}
	// thread modification - ver-bits of thrid, sched, pager, aspace
	else if (!space.is_nil() && dst_thr)
	{
		printk("tctl:  Modification of existing thread 0x%x.\n", dst_thr);

		// TODO:  abort any ongoing IPC operations

		// find task
		Task_t* tsk = 0;
		Thread_t* space_thr = Threads_t::find(space);
		if (space_thr)
			tsk = space_thr->task();

		// check aspace - should be configured if thread will be activate
		if (!pager.is_nil() && !dst_thr->is_active() &&  // thread will be activate
			(!tsk || !tsk->is_configured()))              // task not exist or not configured
		{
			printk("tctl:  ERROR:  activation in not configured aspace.\n");
			cur.utcb()->error(3);  // InvalidSpace
			return;
		}

		// check utcb_location
		if (utcb != -1  &&  dst_thr->is_active())
		{
			printk("tctl:  ERROR:  setting utcb location for active thread.\n");
			cur.utcb()->error(6);  // InvalidUtcb
			return;
		}

		dst_thr->globid(dst);  // overwrite version field

		if (!sched.is_nil())
		{
			dst_thr->schedid(sched);
			// NOTE:  scheduler thread must exist when the addressed thread starts executing.
		}

		if (utcb != -1)
		{
			paddr_t pa = cur.task()->walk(utcb, Cfg_page_sz);
			assert(pa);
			dst_thr->utcb_location(pa);
		}

		if (tsk != dst_thr->task())
			dst_thr->task(tsk);

		if (!pager.is_nil())
		{
			dst_thr->pagerid(pager);
			if (!dst_thr->is_active())
				dst_thr->activate();
		}
	}
	// thread deletion
	else if (space.is_nil() && dst_thr)
	{
		// TODO:  delete dst_thr, if it is last thread in Task --> delete task
		panic("tctl:  Thread deletion isn't supported yet.\n");
	}
	else
	{
		printk("tctl:  ERROR:  destination thread is unavailable.\n");
		cur.utcb()->error(2);  // UnavailableThread
		return;
	}

	eframe.scall_thctl_result(1);  // syscall success
}

void syscall_thread_switch(Thread_t& cur, Entry_frame_t& eframe)
{
	L4_thrid_t dst = eframe.scall_thsw_dest();

	//printk("thread_switch entry:  id=0x%x/%u, dst=0x%x/%u.\n",
	//	cur.globid().raw(), cur.globid().number(), dst.raw(), dst.number());

	// find dst thread
	if (dst.is_nil())
	{
		// ordinary scheduling
		Sched_t::switch_to_next();
		cur.timeslice(0);
	}
	else
	{
		Thread_t* dst_thr = Threads_t::find(dst);
	
		if (!dst_thr)
		{
			// ordinary scheduling
			Sched_t::switch_to_next();
			cur.timeslice(0);
		}
		else
		{
			// extraordinary scheduling
			panic("TODO:   donates remaining timeslice to dest.");
		}
	}
}

void syscall_schedule(Thread_t& cur, Entry_frame_t& eframe)
{
	printk("schedule entry:  id=0x%x/%u.\n", cur.globid().raw(), cur.globid().number());

	L4_thrid_t dst         = eframe.scall_sched_dest();
	word_t     time_ctl    = eframe.scall_sched_time_ctl();
	word_t     proc_ctl    = eframe.scall_sched_proc_ctl();
	word_t     prio        = eframe.scall_sched_prio();
	word_t     preempt_ctl = eframe.scall_sched_preem_ctl();

	printk("schd:  dst=0x%x/%u, tmctl=0x%x, procctl=0x%x, prio=%u, preemtctl=0x%x.\n",
		 dst.raw(), dst.number(), time_ctl, proc_ctl, prio, preempt_ctl);

	eframe.scall_sched_result(0);  // syscall failed

	// find dst thread
	Thread_t* dst_thr = Threads_t::find(dst);

	if (!dst_thr)
	{
		printk("schd:  ERROR:  no thread with id=0x%x/%u.\n", dst.raw(), dst.number());
		cur.utcb()->error(2);  // UnavailableThread
		return;
	}

	if (dst_thr->schedid() != cur.globid())
	{
		printk("schd:  ERROR:  cur is not dest->sched.\n");
		cur.utcb()->error(1);  // NoPrivilege
		return;
	}

	// check incoming params
	if (//TODO:  check time_ctl     ||
	    //TODO:  check proc_ctl     ||
	    prio > cur.prio()  // ||
	    //TODO:  check preempt_ctl
	    )
	{
		printk("schd:  ERROR:  invalid incoming parameter.\n");
		cur.utcb()->error(5);  // InvalidParameter
		return;
	}

	// set time control if need
	if (time_ctl != -1)
	{
		panic("IMPLEMENT ME:  set time_control.\n");
	}

	// set processor control if need
	if (proc_ctl != -1)
	{
		panic("IMPLEMENT ME:  set processor_control.\n");
	}

	// set priority if need
	if (prio != -1)
	{
		dst_thr->prio(prio);
	}

	// set preemption control
	if (preempt_ctl != -1)
	{
		panic("IMPLEMENT ME:  set preemption_control.\n");
	}

	// return thread state
	/*
		Inactive       =            0,  // inactive thread
		Active         =            1,  // active thread, start by receiving a starg_msg from pager
		Ready          =            2,  // thread ready to execute
		Send_ipc       = Snd_mask | 0,  // thread blocked in IpcSnd phase
		Send_pfault    = Snd_mask | 1,  // thread blocked for send pf_msg to pager
		Receive_ipc    = Rcv_mask | 0,  // thread blocked in IpcRcv phase
		Receive_pfault = Rcv_mask | 1   // thread blocked for receive map/grant msg from pager
	*/
	switch (dst_thr->state())
	{
		case Thread_t::Idle:            panic("ImpMe"); eframe.scall_sched_result(2);  break;  //
		case Thread_t::Inactive:        eframe.scall_sched_result(2);  break;  // inactive
		case Thread_t::Active:          eframe.scall_sched_result(3);  break;  // running
		case Thread_t::Ready:           eframe.scall_sched_result(3);  break;  // running
		case Thread_t::Send_ipc:        eframe.scall_sched_result(4);  break;  // pending_send(4) / sending(5)
		case Thread_t::Send_pfault:     eframe.scall_sched_result(4);  break;  // pending_send(4) / sending(5)
		case Thread_t::Receive_ipc:     eframe.scall_sched_result(6);  break;  // waiting_receive(6) / receiving(7)
		case Thread_t::Receive_pfault:  eframe.scall_sched_result(6);  break;  // waiting_receive(6) / receiving(7)
	}

	// return time control
	eframe.scall_sched_time_ctl(-1);   // TODO:  set current time control
}

void syscall_space_control(Thread_t& cur, Entry_frame_t& eframe)
{
	printk("space_control entry:\n");

	L4_thrid_t space      = eframe.scall_sctl_space_spec();
	word_t     control    = eframe.scall_sctl_control();
	L4_fpage_t kip_area   = L4_fpage_t::create(eframe.scall_sctl_kip_area());
	L4_fpage_t utcb_area  = L4_fpage_t::create(eframe.scall_sctl_utcb_area());
	L4_thrid_t redirector = eframe.scall_sctl_redirector();

	eframe.scall_sctl_result(0);   // syscall failed

	(void) control;  // unused now

	// check privileges
	L4_thrid_t cur_id = cur.globid();
	if (cur_id != thrid_sigma0()  &&  cur_id != thrid_roottask())
	{
		cur.utcb()->error(1);  // NoPrivilege
		return;
	}

	// find space
	Thread_t* thr = 0;
	if (thrid_is_global_user(space))
		thr = Threads_t::find(space);
	else if (space.is_local())
		thr = &cur;

	if (!thr || !thr->task())
	{
		cur.utcb()->error(3);  // InvalidSpace
		return;
	}

	Task_t* tsk = thr->task();

	// ignore kip_area and utcb_area if exists active threads
	if (!tsk->threads_active())
	{
		if (!is_aligned(utcb_area.addr(), Cfg_page_sz)  ||  !is_aligned(utcb_area.size(), Cfg_page_sz)  ||
			utcb_area.access() != Acc_rw)
		{
			printk("ERROR:  sctl:  unavailable UTCB area:  addr=0x%x, sz=0x%x.\n", utcb_area.addr(), utcb_area.size());
			cur.utcb()->error(6);  // Invalid UTCB area
			return;
		}

		if (!is_aligned(kip_area.addr(), Cfg_page_sz)  ||  kip_area.size() != Cfg_page_sz  ||
			kip_area.access() != Acc_rx     ||  kip_area.is_cross(utcb_area))
		{
			printk("ERROR:  sctl:  unavailable KIP area:  addr=0x%x, sz=0x%x.\n", kip_area.addr(), kip_area.size());
			cur.utcb()->error(7);  // Invalid KIP area
			return;
		}

		// TODO:  check that kip_area and utcb_area should be in user-accessible part of an address space

		tsk->kip_area(kip_area);
		tsk->utcb_area(utcb_area);
	}

	if (!redirector.is_nil())
	{
		// XXX:  should redirector exist?
		tsk->redirector(redirector);
	}

	eframe.scall_sctl_result(1); // syscall success
}

void syscall_memory_control(Thread_t& cur, Entry_frame_t& eframe)
{
	printk("memory_control entry:\n");

	word_t control  = eframe.scall_mctl_control();
	word_t attr[4];
	attr[0] = eframe.scall_mctl_attr0();
	attr[1] = eframe.scall_mctl_attr1();
	attr[2] = eframe.scall_mctl_attr2();
	attr[3] = eframe.scall_mctl_attr3();

	eframe.scall_mctl_result(0);   // syscall failed

	// check privileges
	L4_thrid_t cur_id = cur.globid();
	if (cur_id != thrid_sigma0()  &&  cur_id != thrid_roottask())
	{
		cur.utcb()->error(1);  // NoPrivilege
		return;
	}

	// check params
	enum { Fpages_max = 64 };
	unsigned num = (control & ~0x3f) + 1;
	if (num > Fpages_max  ||  attr[0] > 1  ||  attr[1] > 1  || attr[2] > 1  || attr[3] > 1)
	{
		if (num > Fpages_max)
			force_printk("ERROR:  mctl:  too many fpages, num=%u, max=%u.\n", num, Fpages_max);
		else
			force_printk("ERROR:  mctl:  unvalid attribs: 0x%x, 0x%x, 0x%x, 0x%x.\n", attr[0], attr[1], attr[2], attr[3]);
		cur.utcb()->error(5);  // InvalidParameter
		return;
	}

	// set attribs
	for (unsigned i=0; i<num; ++i)
	{
		int attr_index = cur.utcb()->mr[i] & 0x3;
		int cached = attr[attr_index] == 1  ?  NotCachable  :  Cachable;
		L4_fpage_t fp = L4_fpage_t::create(cur.utcb()->mr[i]);

		int cur_attr = cur.task()->attributes(fp);
		if (cur_attr < 0)
		{
			printk("ERROR:  mctl:  alien fpage:  addr=0x%x, sz=0x%x.\n", fp.addr(), fp.size());
			cur.utcb()->error(5);  // InvalidParameter
			return;
		}

		// NOTE 1:  sigma0 and roottask have 1:1 maping
		// NOTE 2:  result acc will be cur | map_acc  -->  use map_acc=0x0
		cur.task()->map(fp.addr(), fp.addr(), fp.size(), Aspace::uacc2acc(0), cached);
	}

	eframe.scall_mctl_result(1); // syscall success
}

void kdb_console_entry_wrapper(bool krn_mode, bool error_entry, const char* prompt);
void syscall_kdb(Thread_t& cur, Entry_frame_t& eframe)
{
	word_t opcode = eframe.scall_kdb_opcode();
	word_t param  = eframe.scall_kdb_param();
	void*  data   = (void*)eframe.scall_kdb_data();
	size_t size   = eframe.scall_kdb_size();

	(void)cur;

	printk("kdb entry:  opcode=%ld, param=%ld, data=0x%lx, sz=%lu.\n", opcode, param, data, size);

	eframe.scall_kdb_result(0);  // return code

	switch (opcode)
	{
		case L4_kdb_console:
		{
			kdb_console_entry_wrapper(false,  param, (const char*)data);
			break;
		}
		case L4_kdb_threads:
		{
			Threads_t::kdb_threads_entry((L4_kdb_thread_info_t*)data, size/sizeof(L4_kdb_thread_info_t));
			break;
		}
		default:
			printk("kdb:  ERROR:  unknown opcode=%ld.\n", opcode);
			eframe.scall_kdb_result(-1);
	}
}

void do_ipc(Thread_t& cur, Entry_frame_t& eframe);

void syscall_ipc(Thread_t& cur, Entry_frame_t& eframe)
{
	do_ipc(cur, eframe);
}

