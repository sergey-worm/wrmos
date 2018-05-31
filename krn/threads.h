//##################################################################################################
//
//  Threads container.
//
//##################################################################################################

#ifndef THREADS_T
#define THREADS_T

#include "krn-config.h"
#include "kconfig.h"
#include "thread.h"
#include "sched.h"
#include "l4_kdbops.h"
#include "l4_ipcerr.h"


class Threads_t
{
	enum
	{
		Thread_number_min = Kcfg::Ints_max,
		Thread_number_max = Kcfg::Ints_max + Kcfg::Threads_max - 1,
	};

	static Int_thread_t _int_threads [Kcfg::Ints_max];
	static Thread_t     _threads     [Kcfg::Threads_max];

	typedef uint32_t bits_t;
	static bits_t    _ready_bits[(Thread_t::Prio_max+1) / (sizeof(bits_t)*8)];
	static threads_t _ready_threads[Thread_t::Prio_max+1]; // threads by prio
	static threads_t _send_threads;                        // send ipc threads with infinity timeout
	static threads_t _timeout_waiting_snd_threads;         // threads ordered by expire time and prio
	static threads_t _timeout_waiting_rcv_threads;         // threads ordered by expire time and prio

private:

	static_assert((Thread_t::Prio_max+1) % (sizeof(bits_t)*8) == 0);
	static const char* separated_str(unsigned long long val)
	{
		static char str[32];
		if (val < 1000)
			snprintf(str, sizeof(str), "%llu", val);
		else if (val < 1000*1000)
			snprintf(str, sizeof(str), "%llu.%03llu", val/1000, val%1000);
		else if (val < 1000*1000*1000)
			snprintf(str, sizeof(str), "%llu.%03llu.%03llu", val/1000/1000, val/1000%1000, val%1000);
		return str;
	}

	static threads_t* ready_threads(unsigned prio)
	{
		wassert(prio <= Thread_t::Prio_max);
		return &_ready_threads[prio];
	}

public:

	static void dump()
	{
		L4_clock_t now = SystemClock_t::sys_clock(__func__);
		L4_clock_t exec = 0;

		printf("  ##   id  name  task  prio/max  cpu,%%  state           partner       no-act,us  signal\n");
		for (unsigned i=0; i<sizeof(_threads)/sizeof(_threads[i]); ++i)
		{
			Thread_t* it = _threads + i;
			if (it->state() == Thread_t::Idle)
				continue;

			char partner[16] = "";
			if (it->state() == Thread_t::Send_ipc  ||  it->state() == Thread_t::Receive_ipc)
			{
				L4_thrid_t partn = it->state()==Thread_t::Send_ipc ? it->ipc_to() : it->ipc_from_spec();
				if (partn.is_nil())
					snprintf(partner, sizeof(partner), "nil");
				else if (partn.is_any())
					snprintf(partner, sizeof(partner), "any");
				else if (partn.is_any_local())
					snprintf(partner, sizeof(partner), "any_loc");
				else
					snprintf(partner, sizeof(partner), "%u", partn.number());
			}

			unsigned promile = 1000 * it->tmspan_exec() / now;
			exec += it->tmspan_exec();

			unsigned long long no_activity_us = Sched_t::current()==&*it ? 0 : (now - it->tmpoint_suspend());

			printf(" %3d  %3d  %4s    %2u   %3u/%3u   %2u.%u  %-14s  %7s  %14s  %6d\n",
					it->id(), it->globid().number(), it->name(), it->task()->id(), it->prio(),
					it->prio_max(), promile/10, promile%10, it->state_str(), partner,
					separated_str(no_activity_us), it->signal_pending());
		}
		printf("\n");
		printf(" uptime,us:    %s\n", separated_str(now));
		printf(" exectime,us:  %s\n", separated_str(exec));
		printf(" diff,us:      %s\n", separated_str(now - exec));
	}

	static void dump_cpuusage()
	{
		L4_clock_t now = SystemClock_t::sys_clock(__func__);
		L4_clock_t exec = 0;

		printf("  ##   id  name  all,%%    usr,%%  krn,%%     kentr,%%  kwork,%%  kexit,%%"
		       "     1,%%   2,%%   3,%%\n");
		for (unsigned i=0; i<sizeof(_threads)/sizeof(_threads[i]); ++i)
		{
			Thread_t* it = _threads + i;
			if (it->state() == Thread_t::Idle)
				continue;
			unsigned all    = 10000 * it->tmspan_exec()   / now;
			unsigned usr    = 10000 * it->tmspan_uexec()  / now;
			unsigned krn    = 10000 * it->tmspan_kexec()  / now;
			unsigned kentry = 10000 * it->tmspan_kentry() / now;
			unsigned kwork  = 10000 * it->tmspan_kwork()  / now;
			unsigned kexit  = 10000 * it->tmspan_kexit()  / now;
			unsigned kwork1 = 10000 * it->tmspan_kwork1() / now;
			unsigned kwork2 = 10000 * it->tmspan_kwork2() / now;
			unsigned kwork3 = 10000 * it->tmspan_kwork3() / now;
			exec += it->tmspan_exec();

			printf(" %3d  %3d  %4s  %2u.%02u    %2u.%02u  %2u.%02u       %2u.%02u    %2u.%02u    %2u.%02u"
			       "    %u.%02u  %u.%02u  %u.%02u\n",
					it->id(), it->globid().number(), it->name(),
					all/100, all%100, usr/100, usr%100, krn/100, krn%100,
					kentry/100, kentry%100, kwork/100, kwork%100, kexit/100, kexit%100,
					kwork1/100, kwork1%100, kwork2/100, kwork2%100, kwork3/100, kwork3%100);
		}
		printf("\n");
		printf(" uptime,us:    %s\n", separated_str(now));
		printf(" exectime,us:  %s\n", separated_str(exec));
		printf(" diff,us:      %s\n", separated_str(now - exec));
	}

	static void kdb_threads_entry(L4_kdb_thread_info_t* threads, unsigned sz)
	{
		L4_clock_t now = SystemClock_t::sys_clock(__func__);
		unsigned cnt = 0;

		for (unsigned i=0; i<sizeof(_threads)/sizeof(_threads[i]) && cnt!=sz; ++i)
		{
			Thread_t* it = _threads + i;
			if (it->state() == Thread_t::Idle)
				continue;

			threads[cnt].id               = it->globid().number();
			threads[cnt].prio             = it->prio();
			threads[cnt].thr_uptime_usec  = it->tmspan_exec();
			threads[cnt].sys_uptime_usec  = now;
			threads[cnt].no_activity_usec = Sched_t::current()==&*it ? 0 : now - it->tmpoint_suspend();
			strncpy(threads[cnt].name, it->name(), 8);
			cnt++;
		}
		if (cnt != sz)
			threads[cnt].id = 0;  // end-of-list marker
	}

	static void remap_user_utcb(Task_t* tsk)
	{
		for (unsigned i=0; i<sizeof(_threads)/sizeof(_threads[i]); ++i)
		{
			Thread_t* t = _threads + i;
			if (t->task() == tsk)
			{
				paddr_t utcb_pa = tsk->walk((addr_t)t->kutcb(), Cfg_page_sz);
				tsk->map((addr_t)t->uutcb(), utcb_pa, Cfg_page_sz, Acc_utcb, Cachable);
			}
		}
	}

	static void alloc_kstack()
	{
		for (unsigned i=0; i<sizeof(_threads)/sizeof(_threads[i]); ++i)
		{
			_threads[i].alloc_kstack();
		}
	}

	static Int_thread_t* int_thread(unsigned irq)
	{
		return irq < Kcfg::Ints_max  ?  &_int_threads[irq]  :  0;
	}

	// create user thread
	static Thread_t* create(L4_thrid_t globid, L4_thrid_t sched, L4_thrid_t pager, Task_t* tsk, uint8_t prio, const char* name)
	{
		printk("new thread:  id=0x%lx/%u, prio=%u, name=%s.\n", globid.raw(), globid.number(), prio, name);
		wassert(globid.number() >= Thread_number_min  &&  globid.number() <= Thread_number_max);
		Thread_t* thr = &_threads[globid.number() - Thread_number_min];
		wassert(thr->state() == Thread_t::Idle);
		thr->task(tsk);
		thr->name(name);
		thr->globid(globid);
		thr->schedid(sched);
		thr->pagerid(pager);
		thr->prio(prio);
		thr->state(Thread_t::Inactive);
		return thr;
	}

	// create kernel thread
	static void create_kthread_and_go(void* entry_point, Task_t* tsk, const char* name) __attribute__((noreturn))
	{
		L4_thrid_t globid = L4_thrid_t::create_global(get_kip()->thread_info.system_base());
		Thread_t* thr = create(globid, globid, globid, tsk, Thread_t::Prio_min, name);
		thr->state(Thread_t::Ready);
		thr->tmevent_resume(SystemClock_t::sys_clock(__func__));
		Sched_t::current(thr);
		Proc::set_new_stack_area_and_jump(thr->kstack_area(), thr->kstack_sz(), (addr_t)entry_point);
	}

	static Thread_t* find(unsigned thrid_num)
	{
		if (thrid_num >= Thread_number_min  &&  thrid_num <= Thread_number_max)
		{
			Thread_t* t = &_threads[thrid_num - Thread_number_min];
			return t->state() == Thread_t::Idle ? 0 : t;
		}
		return 0;
	}

	// XXX:  It need to find by local ID.
	//       NOTE:  My kernel implementation works faster with global ID,
	//              may be need discard works with local ID inside kernel?
	static Thread_t* find_by_utcb(addr_t utcb)
	{
		for (unsigned i=0; i<sizeof(_threads)/sizeof(_threads[i]); ++i)
		{
			Thread_t* t = _threads + i;
			if (t->task() == Sched_t::current()->task()  &&  utcb == (addr_t)t->uutcb())
				return t->state() == Thread_t::Idle ? 0 : t;
		}
		dump();
		panic("%s:  no thread with such utcb=0x%lx.\n", __func__, utcb);
		return 0;
	}

	static Thread_t* find(L4_thrid_t id)
	{
		if (id.is_global())
		{
			return find(id.number());
		}
		else if (id.is_local())
		{
			#if 1
			return find_by_utcb(id.raw());
			#else
			// if corrupt ID or id-thread in alien aspace -> PF, bug
			return find(((L4_utcb_t*)id.raw())->global_id().number());
			#endif
		}
		return 0;
	}

	static bool _is_good_sender(const Thread_t& rcv, L4_thrid_t from_spec, const Thread_t* snd,
	                            bool* propagated, bool* use_local_id)
	{
		// check state
		wassert(snd->is_snd_state());

		bool local = snd->task() == rcv.task();     // is 'snd' local thread for 'rcv'

		// check partner destination
		wassert(!snd->ipc_to().is_nil());
		wassert(!snd->ipc_to().is_any());
		wassert(!snd->ipc_to().is_any_local());
		if (!(local  &&  snd->ipc_to() == rcv.localid())  &&
		    !(           snd->ipc_to() == rcv.globid()))
			return false;

		const Thread_t* sender = snd;

		// if propagate send operation
		if (snd->kutcb()->msgtag().propagated())
		{
			const Thread_t* virt_sender = Threads_t::find(snd->kutcb()->sender());
			// if propagation permit
			if (virt_sender &&
			    (snd->task() == virt_sender->task()  ||
			     snd->task() == rcv.task()))
			{
				sender = virt_sender;
				local = sender->task() == rcv.task();         // is 'virt_sender' local thread for 'rcv'
				*propagated = true;
			}
		}

		// check rcv from_spec
		if (!from_spec.is_any()  &&                           // 'from_spec' is not any thread
			!(local  &&  from_spec.is_any_local())        &&  // 'from_spec' is not any_local thread
			!(local  &&  from_spec == sender->localid())  &&  // 'from_spec' is not equal local_id
			!(           from_spec == sender->globid()))      // 'from_spec' is not equal global_id
			return false;

		// set local or global flag for from field
		if (local  &&  (from_spec.is_any_local()  ||  from_spec == sender->localid()))
			*use_local_id = true;

		return true;
	}

private:

	static Thread_t* find_highest_prio_sender(threads_t* list, const Thread_t* rcv, L4_thrid_t from_spec,
	                                   bool* propagated, bool* use_local_id)
	{
		Thread_t* res = 0;
		for (threads_t::iter_t it=list->begin(); it!=list->end(); ++it)
		{
			//printk("%s:  thr=%s, state=%s.\n", __func__, (*it)->name(), (*it)->state_str());

			if (!_is_good_sender(*rcv, from_spec, *it, propagated, use_local_id))
				continue;

			if (!res  ||  res->prio_max() < (*it)->prio_max())
				res = *it;
		}
		return res;
	}

public:

	// find partner for receive thread
	// TODO:  find max prio thread
	static Thread_t* find_sender(const Thread_t& rcv, L4_thrid_t from_spec, bool* propagated, bool* use_local_id)
	{
		*propagated = false;
		*use_local_id = false;

		// if receiver wants specific thread
		Thread_t* t = find(from_spec);
		if (t)
		{
			// check state
			if (!t->is_snd_state())
				return 0;

			if (!_is_good_sender(rcv, from_spec, t, propagated, use_local_id))
				return 0;

			return t;
		}

		// if receiver wants any or any_local thread
		bool prop_1  = false;
		bool prop_2  = false;
		bool locid_1 = false;
		bool locid_2 = false;
		Thread_t* snd1 = find_highest_prio_sender(&_timeout_waiting_snd_threads, &rcv, from_spec, &prop_1, &locid_1);
		Thread_t* snd2 = find_highest_prio_sender(&_send_threads, &rcv, from_spec, &prop_2, &locid_2);

		if (snd1 && snd2)
		{
			if (snd1->prio_max() < snd2->prio_max())
				snd1 = 0;
			else
				snd2 = 0;
		}

		if (snd1)
		{
			*propagated = prop_1;
			*use_local_id = locid_1;
			return snd1;
		}

		if (snd2)
		{
			*propagated = prop_2;
			*use_local_id = locid_2;
			return snd2;
		}

		return 0;
	}

	// find interrupt partner for receive thread
	static Int_thread_t* find_int_sender(const Thread_t& rcv, L4_thrid_t from_spec)
	{
		wassert(thrid_is_int(from_spec));
		Int_thread_t* ithr = int_thread(from_spec.number());
		wassert(ithr);
		if (ithr->is_active() && ithr->is_pending() && ithr->handler()==rcv.globid())
			return ithr;
		return 0;
	}

	static threads_t::iter_t add_ready(Thread_t* thr)
	{
		unsigned prio = thr->prio();
		threads_t* que = ready_threads(prio);
		wassert(!is_exist(que, thr));
		que->push_back(thr);
		unsigned bits_group = prio / (sizeof(bits_t)*8);
		_ready_bits[bits_group] |= 1 << (prio - bits_group * (sizeof(bits_t)*8));
		return que->last();
	}

	static threads_t::iter_t del_ready(threads_t::iter_t it)
	{
		unsigned prio = (*it)->prio();
		threads_t* que = ready_threads(prio);
		wassert(is_exist(que, it));
		que->erase(it);
		if (que->empty())
		{
			unsigned bits_group = prio / (sizeof(bits_t)*8);
			_ready_bits[bits_group] &= ~(1 << (prio - bits_group * (sizeof(bits_t)*8)));
		}
		return que->end();
	}

	// replace cur thread at the end of que and set new timeslice
	static threads_t::iter_t timeslice_expired(Thread_t* thr, threads_t* que = 0)
	{
		wassert(!thr->timeslice());
		wassert(thr->state() == Thread_t::Ready);
		if (!que)
			que = ready_threads(thr->prio());
		thr->timeslice(Kcfg::Timeslice_usec);
		threads_t::iter_t it = thr->iter();
		if (que->size() > 1)
		{
			// replace at the end of que
			que->push_back(*it);
			thr->iter(que->last());
			que->erase(it);
		}
		return que->begin();
	}

	// get next Ready thread
	static Thread_t* get_highest_prio_ready_thread()
	{
		// find max prio thread
		for (int i=sizeof(_ready_bits)/sizeof(_ready_bits[0])-1; i>=0; --i)
		{
			if (_ready_bits[i])
			{
				for (int j=sizeof(bits_t)*8-1; j>=0; --j)
				{
					if ((_ready_bits[i] >> j) & 0x1)
					{
						unsigned prio = sizeof(bits_t)*8*i + j;
						threads_t* que = ready_threads(prio);
						wassert(que->size());
						threads_t::iter_t it = que->begin();
						if (!(*it)->timeslice())
							it = timeslice_expired(*it, que);
						return *it;
					}
				}
			}
		}
		panic("no ready threads");
		return 0;
	}

	// add ordered by timeout and prio
	static threads_t::iter_t add_send(Thread_t* thr)
	{
		wassert(!is_exist(&_send_threads, thr));
		_send_threads.push_back(thr);
		return _send_threads.last();
	}

	static threads_t::iter_t del_send(threads_t::iter_t it)
	{
		wassert(is_exist(&_send_threads, it));
		_send_threads.erase(it);
		return _send_threads.end();
	}
private:

	// add ordered by timeout and prio
	static threads_t::iter_t add_timeout_waiting(Thread_t* thr, threads_t* list)
	{
		wassert(!is_exist(list, thr));
		for (threads_t::iter_t it=list->begin(); it!=list->end(); ++it)
		{
			if ((*it)->timeout() > thr->timeout()  ||
				((*it)->timeout() == thr->timeout()  &&  (*it)->prio() < thr->prio()))
			{
				list->insert_before(it, thr);
				return it-1;
			}
		}
		list->push_back(thr);
		return list->last();
	}

	static threads_t::iter_t del_timeout_waiting(threads_t::iter_t it, threads_t* list)
	{
		wassert(is_exist(list, it));
		list->erase(it);
		return list->end();
	}

	// return max prio thread with expired timeouts
	static Thread_t* check_ipc_timeouts(L4_clock_t now, threads_t* list)
	{
		Thread_t* next = 0;
		for (threads_t::iter_t it=list->begin(); it!=list->end(); )
		{
			Thread_t* t = *it;
			wassert(t->state() == Thread_t::Send_ipc  ||  t->state() == Thread_t::Receive_ipc);

			if (t->timeout() > now)
				break;

			if (!next)
				next = t;

			int phase = t->state() == Thread_t::Send_ipc ? L4_snd_phase : L4_rcv_phase;
			L4_utcb_t* utcb = t->utcb();
			utcb->ipc_error_code(L4_ipcerr_t(phase, L4_ipc_timeout));
			L4_msgtag_t tag = utcb->msgtag();
			tag.ipc_set_failed();
			utcb->msgtag(tag);

			threads_t::iter_t next_it = it + 1;
			t->state(Thread_t::Ready);  // inside 'it' will be invalidate
			it = next_it;

			// remove inherited prio
			if (!t->prio_heir().is_nil())
			{
				Thread_t* thr = threads_find(t->prio_heir());  // XXX optimize it
				wassert(thr);
				thr->inherit_prio_del(t->globid(), t->prio_max());
				t->prio_heir(L4_thrid_t::Nil);
			}
		}
		return next;
	}

public:

	static threads_t::iter_t add_rcv_timeout_waiting(Thread_t* thr)
	{
		wassert(thr->state() == Thread_t::Receive_ipc);
		return add_timeout_waiting(thr, &_timeout_waiting_rcv_threads);
	}

	static threads_t::iter_t add_snd_timeout_waiting(Thread_t* thr)
	{
		wassert(thr->state() == Thread_t::Send_ipc);
		return add_timeout_waiting(thr, &_timeout_waiting_snd_threads);
	}

	static threads_t::iter_t del_rcv_timeout_waiting(threads_t::iter_t it)
	{
		wassert((*it)->state() == Thread_t::Receive_ipc);
		return del_timeout_waiting(it, &_timeout_waiting_rcv_threads);
	}

	static threads_t::iter_t del_snd_timeout_waiting(threads_t::iter_t it)
	{
		wassert((*it)->state() == Thread_t::Send_ipc);
		return del_timeout_waiting(it, &_timeout_waiting_snd_threads);
	}

	// return max prio thread with expired timeouts
	static Thread_t* check_ipc_timeouts(L4_clock_t now)
	{
		Thread_t* next1 = check_ipc_timeouts(now, &_timeout_waiting_snd_threads);
		Thread_t* next2 = check_ipc_timeouts(now, &_timeout_waiting_rcv_threads);
		if (next1 && next2)
			return next1->prio_max() > next2->prio_max() ? next1 : next2;
		if (next1)
			return next1;
		if (next2)
			return next2;
		return 0;
	}
};

#endif // THREADS_T
