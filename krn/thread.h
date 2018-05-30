//##################################################################################################
//
//  Thread implementation.
//
//##################################################################################################

#ifndef THREAD_H
#define THREAD_H

#include "list.h"
#include "sys_eframe.h"
#include "sys_stack.h"
#include "l4_types.h"
#include "l4_syscalls.h"
#include "task.h"
#include "sysclock.h"
#include "thrid.h"
#include "tmaccount.h"
#include "arch.h"
#include "wlibc_assert.h"

class Thread_t;
typedef list_t <Thread_t*, Kcfg::Threads_max> threads_t;

// helper for threads_t
static inline bool is_exist(threads_t* list, Thread_t* thr)
{
	for (threads_t::iter_t it=list->begin(); it!=list->end(); ++it)
		if ((*it) == thr)
			return true;
	return false;
}

// helper for threads_t
static inline bool is_exist(threads_t* list, threads_t::iter_t iter)
{
	for (threads_t::iter_t it=list->begin(); it!=list->end(); ++it)
		if (it == iter)
			return true;
	return false;
}

/*
static void dump(threads_t* list)
{
	unsigned cnt = 0;
	printk("  list=0x%x, sz=%u:\n", list, list->size());
	for (threads_t::iter_t it=list->begin(); it!=list->end(); ++it, ++cnt)
		printk("    %u.  thr=%s, prio=%u.\n", cnt, (*it)->name(), (*it)->prio());
}
*/

// helpers to call Threads_t members
threads_t::iter_t threads_add_ready(Thread_t* thr);
threads_t::iter_t threads_del_ready(threads_t::iter_t it);
threads_t::iter_t threads_add_send(Thread_t* thr);
threads_t::iter_t threads_del_send(threads_t::iter_t it);
threads_t::iter_t threads_add_rcv_timeout_waiting(Thread_t* thr);
threads_t::iter_t threads_del_rcv_timeout_waiting(threads_t::iter_t it);
threads_t::iter_t threads_add_snd_timeout_waiting(Thread_t* thr);
threads_t::iter_t threads_del_snd_timeout_waiting(threads_t::iter_t it);
threads_t::iter_t threads_timeslice_expired();
Thread_t*         threads_find(L4_thrid_t id);

// helpers to call Sched_t members
Thread_t* cur_thr();
const char* cur_thr_name();

//--------------------------------------------------------------------------------------------------
class Thread_t
{
	enum
	{
		Stack_32bit_val = 0xa5a5a5a5,
		Stack_sz        = Cfg_page_sz,
	};

public:

	enum
	{
		Prio_min = 0,
		Prio_max = 255,

		Snd_mask = 0x10,
		Rcv_mask = 0x20
	};

	enum state_t
	{
		Idle              =            0,  // unused thread
		Inactive          =            1,  // inactive thread
		Active            =            2,  // active thread, start by receiving a starg_msg from pager
		Ready             =            3,  // thread ready to execute
		Send_ipc          = Snd_mask | 0,  // thread blocked in IpcSnd phase
		Send_pfault       = Snd_mask | 1,  // thread blocked for send pfault_msg to pager
		Send_exception    = Snd_mask | 2,  // thread blocked for send except_msg to exc-handler
		Receive_ipc       = Rcv_mask | 0,  // thread blocked in IpcRcv phase
		Receive_pfault    = Rcv_mask | 1,  // thread blocked for receive map/grant msg from pager
		Receive_exception = Rcv_mask | 2   // thread blocked for receive except_msg from exc-handler
	};

private:

	addr_t _kstack_area;

	// TODO: may be enough store 'timeout' and 'partner'
	struct Ipc_t
	{
		uint64_t   timeout;           // systime in usec, timeout for current operation (snd/rcv)
		L4_thrid_t to;                // dst for snd phase
		L4_thrid_t from_spec;         // src pattern for rcv phase
		Ipc_t() : timeout(0), to(L4_thrid_t::Nil), from_spec(L4_thrid_t::Nil) {}
		void clear() { timeout = 0; to = from_spec = L4_thrid_t::Nil; }
	};

	// page fault data
	struct Pfault_t
	{
		word_t  addr;                 // pfault address
		word_t  inst;                 // pfault instruction
		word_t  access;               // access permission
		Pfault_t() : addr(0), inst(0), access(0) {}
		void clear() { addr = inst = access = 0; }
	};

	// exception fault data
	struct Efault_t
	{
		int     type;                 // exc type
		word_t  pfault_addr_and_acc;  // pfault addr and access
		Efault_t() : type(0), pfault_addr_and_acc(0) {}
		void clear() { type = pfault_addr_and_acc = 0; }
	};

	static unsigned _counter;         // for set id

	unsigned _id;                     // XXX:  is it need?
	addr_t _ksp;                      // kernel stack pointer
	char _name[8];                    // thread's name for debug

	unsigned _flags;                  // now use only 1 flag:  FPU=1
	unsigned _fpu_in_use;             // is need store/restore FPU context ?

	Float_frame_t _float_frame __attribute__((aligned(16)));

	// aspace data
	Task_t*     _task;                    // thread's task
	addr_t      _utcb_uva;                // UTCB va for user address space
	addr_t      _utcb_kva;                // UTCB va for kernel address space
	paddr_t     _utcb_pa;                 // UTCB location

	// L4 data
	L4_thrid_t  _glob_id;                 // global thread ID
	L4_thrid_t  _sched_id;                // sched thread ID, global or local
	L4_thrid_t  _pager_id;                // pager thread ID, global or local
	Thread_t*   _sched;                   //
	Thread_t*   _pager;                   //
	state_t     _state;                   //
	Ipc_t       _ipc;                     // store active normal-ipc operation data
	Pfault_t    _pfault;                  // store active pfault-ipc operation data
	Efault_t    _efault;                  // store active exc-ipc    operation data

	// sched data
	struct InheritedPrio_t
	{
		L4_thrid_t owner;
		unsigned   prio;
		InheritedPrio_t(L4_thrid_t o, unsigned p) : owner(o), prio(p) {}
		inline bool operator < (InheritedPrio_t other) const { return prio < other.prio; };
	};
	typedef list_t <InheritedPrio_t, 2> inhprios_t;
	unsigned    _prio;                    // thread priority
	inhprios_t  _inherited_prios;         // some threads inerit priority for prio inversion
	L4_thrid_t  _prio_heir;               // this thread inerited selth prio to _prio_heir

	// Wrm extention:  signal
	bool        _signal_pending;          // flag:  is signal pending

	int         _entry_type;              // 1 - syscall, 2 - pfault, 3 - kpfault, 4 - irq

	threads_t::iter_t _iter;              // iterator for current threads list

	L4_clock_t  _update_timeslice_point;  // time point
	unsigned    _remaning_timeslice;      // time stamp

	// time accounting
	Time_account_t _tmaccount;            // for accounting timeslice and profile

public:

	void print_kstack()
	{
		printf("stack:  sz=%u:\n", Stack_sz);
		for (unsigned i=0; i<Stack_sz/sizeof(uint32_t); ++i)
			printf("  0x%08x  %8x\n", _kstack_area + i * sizeof(uint32_t), ((uint32_t*)(_kstack_area))[i]);
		printf("\n");
	}

	unsigned unused_kstack_sz()
	{
		for (unsigned i=0; i<Stack_sz/sizeof(uint32_t); ++i)
			if (((uint32_t*)(_kstack_area))[i] != Stack_32bit_val)
				return i * sizeof(uint32_t);
		return Stack_sz;
	}

	// for replacement new in list_t
	static void* operator new(size_t sz, Thread_t* thr)
	{
		(void) sz;
		return thr;
	}

	explicit Thread_t() : _kstack_area(0), _id(_counter++), _ksp(0/*(addr_t)_kstack + sizeof(_kstack)*/), _flags(0), _fpu_in_use(0),
	                      _task(0), _utcb_uva(-1), _utcb_kva(-1), _utcb_pa(-1),
	                      _glob_id(L4_thrid_t::Nil), _sched_id(L4_thrid_t::Nil), _pager_id(L4_thrid_t::Nil),
	                      _sched(0), _pager(0), _state(Idle),
	                      _ipc(), _pfault(), _efault(),
	                      _prio(0), _prio_heir(L4_thrid_t::Nil), _signal_pending(false),
	                      _update_timeslice_point(0), _remaning_timeslice(0), _tmaccount(_name)
	{
		_name[0] = 0;
		//printk("Thread::ctor:  id=%d, _kstack=0x%x, sz=%u, _ksp=0x%x.\n", _id, _kstack_area, Stack_sz, _ksp);
	}

	void name(const char* n, size_t sz = sizeof(_name)-1)
	{
		unsigned len = min(sizeof(_name)-1, sz);
		strncpy(_name, n, len);
		_name[len] = '\0';

		// XXX:  set user accesible thread name (wrm extention)
		if (kutcb() != (void*)-1)
			memcpy(kutcb()->tls, _name, 4);
	}

	// allocate kstack and remap it to separate vspace with guard between stacks
	void alloc_kstack()
	{
		wassert(!_kstack_area && !_ksp);
		addr_t  va = kmem_alloc(Stack_sz, Cfg_page_sz);
		paddr_t pa = kmem_paddr(va, Stack_sz);
		addr_t  new_va = Aspace::kmap_kstack(pa, Stack_sz);
		// TODO:  Aspace::kunmap(va, Stack_sz);
		_kstack_area = new_va;
		_ksp = _kstack_area + Stack_sz;

		// init stack values for debug
		for (unsigned i=0; i<Stack_sz/sizeof(uint32_t); ++i)
			((uint32_t*)(_kstack_area))[i] = Stack_32bit_val;
	}

	inline unsigned        id()              const { return _id;       }
	inline addr_t          ksp()             const { return _ksp;      }
	inline addr_t          kstack_area()     const { return _kstack_area; }
	inline size_t          kstack_sz()       const { return Stack_sz;  }
	inline addr_t          kentry_sp()       const { return _kstack_area + Stack_sz; }
	inline const char*     name()            const { return _name;     }
	inline unsigned        flags()           const { return _flags;    }
	inline unsigned        fpu_in_use()      const { return _fpu_in_use; }
	inline Task_t*         task()            const { return _task;     }
	inline L4_thrid_t      globid()          const { return _glob_id;  }
	inline L4_thrid_t      localid()         const { return _utcb_uva; }
	inline L4_thrid_t      schedid()         const { return _sched_id; }
	inline L4_thrid_t      pagerid()         const { return _pager_id; }
	inline Thread_t*       sched()           const { return _sched;    }
	inline Thread_t*       pager()           const { return _pager;    }
	inline bool            is_active()       const { return _state != Inactive; }
	inline uint8_t         prio()            const { return _prio;     }
	inline L4_thrid_t      prio_heir()       const { return _prio_heir; }
	inline bool            signal_pending()  const { return _signal_pending; }
	inline int             entry_type()      const { return _entry_type; }
	threads_t::iter_t      iter()            const { return _iter; }

	inline void ksp(addr_t v)                  { _ksp        = v; }
	inline void fpu_in_use(unsigned v)         { _fpu_in_use = v; }
	inline void task(Task_t* v)                { _task       = v; }
	inline void globid(L4_thrid_t v)           { _glob_id    = v; }
	inline void schedid(L4_thrid_t v)          { _sched_id   = v; }
	inline void pagerid(L4_thrid_t v)          { _pager_id   = v; }
	inline void sched(Thread_t* v)             { _sched      = v; }
	inline void pager(Thread_t* v)             { _pager      = v; }
	inline void prio_heir(L4_thrid_t v)        { _prio_heir  = v; }
	inline void signal_pending(bool v)         { _signal_pending = v; }
	inline void entry_type(int v)              { _entry_type = v; }
	inline void iter(threads_t::iter_t v)      { _iter       = v; }

	// timeslice account
	unsigned timeslice()               const { return _remaning_timeslice; }
	void     timeslice(unsigned v)           { _remaning_timeslice = v; }
	void     timeslice_start(L4_clock_t now) { _update_timeslice_point = now; }

	void timeslice_stop(L4_clock_t now)
	{
		L4_clock_t exec_time = now - _update_timeslice_point;
		_remaning_timeslice -= min(exec_time, _remaning_timeslice);
		_update_timeslice_point = 0;
	}

	void timeslice_update(L4_clock_t now)
	{
		L4_clock_t exec_time = now - _update_timeslice_point;
		_remaning_timeslice -= min(exec_time, _remaning_timeslice);
		_update_timeslice_point = now;
	}

	// time account funcs
	inline L4_clock_t tmspan_exec()               const { return _tmaccount.tmspan_exec(); }
	inline L4_clock_t tmspan_uexec()              const { return _tmaccount.tmspan_uexec(); }
	inline L4_clock_t tmspan_kexec()              const { return _tmaccount.tmspan_kexec(); }
	inline L4_clock_t tmspan_kentry()             const { return _tmaccount.tmspan_kentry(); }
	inline L4_clock_t tmspan_kwork()              const { return _tmaccount.tmspan_kwork(); }
	inline L4_clock_t tmspan_kexit()              const { return _tmaccount.tmspan_kexit(); }
	inline L4_clock_t tmspan_kwork1()             const { return _tmaccount.tmspan_kwork1(); }
	inline L4_clock_t tmspan_kwork2()             const { return _tmaccount.tmspan_kwork2(); }
	inline L4_clock_t tmspan_kwork3()             const { return _tmaccount.tmspan_kwork3(); }
	inline L4_clock_t tmpoint_suspend()           const { return _tmaccount.tmpoint_suspend(); }
	inline void       tmevent_tick(L4_clock_t c)        { return _tmaccount.tmevent_tick(c); }
	inline void       tmevent_resume(L4_clock_t c)      { return _tmaccount.tmevent_resume(c); }
	inline void       tmevent_kexit_start(L4_clock_t c) { return _tmaccount.tmevent_kexit_start(c); }
	inline void       tmevent_kentry_end(L4_clock_t c)  { return _tmaccount.tmevent_kentry_end(c); }
	inline void       tmevent_suspend(L4_clock_t c)     { return _tmaccount.tmevent_suspend(c); }
	inline void       tmevent_kwork_1s(L4_clock_t c)    { return _tmaccount.tmevent_kwork_1s(c); }
	inline void       tmevent_kwork_1e(L4_clock_t c)    { return _tmaccount.tmevent_kwork_1e(c); }
	inline void       tmevent_kwork_2s(L4_clock_t c)    { return _tmaccount.tmevent_kwork_2s(c); }
	inline void       tmevent_kwork_2e(L4_clock_t c)    { return _tmaccount.tmevent_kwork_2e(c); }
	inline void       tmevent_kwork_3s(L4_clock_t c)    { return _tmaccount.tmevent_kwork_3s(c); }
	inline void       tmevent_kwork_3e(L4_clock_t c)    { return _tmaccount.tmevent_kwork_3e(c); }

	void prio(uint8_t v)
	{
		// ready list is unique for every priority
		// delete item for current ready list
		if (_state == Ready)
			_iter = threads_del_ready(_iter);

		_prio = v;

		// and add to new ready list
		if (_state == Ready)
			_iter = threads_add_ready(this);
	}

	void flags(word_t f)
	{
		if ((_flags & L4_flags_fpu)  &&  !(f & L4_flags_fpu))
		{
			// disable FPU for thread
			fpu_in_use(false);
			entry_frame()->disable_fpu();
		}
		//force_printk_uart("flags:  0x%lx -> 0x%lx\n", _flags, f);
		_flags = f;
	}

	static const char* state_str(int state)
	{
		switch (state)
		{
			case Idle:               return "idle";
			case Inactive:           return "inactive";
			case Active:             return "active";
			case Ready:              return "ready";
			case Send_ipc:           return "send_ipc";
			case Send_pfault:        return "send_pfault";
			case Send_exception:     return "send_exc";
			case Receive_ipc:        return "receive_ipc";
			case Receive_pfault:     return "receive_pfault";
			case Receive_exception:  return "receive_exc";
		}
		return "__unknown_state__";
	}
	const char* state_str()    const { return state_str(_state);     }

	bool inline is_snd_state() const { return _state & Snd_mask;     }
	bool inline is_rcv_state() const { return _state & Rcv_mask;     }

	inline void ipc_to(L4_thrid_t v)        { _ipc.to = v;           }
	inline void ipc_from_spec(L4_thrid_t v) { _ipc.from_spec = v;    }

	inline L4_thrid_t ipc_to()        const { return _ipc.to;        }
	inline L4_thrid_t ipc_from_spec() const { return _ipc.from_spec; }

	inline void save_rcv_phase(L4_time_t timeout, L4_thrid_t from_spec)
	{
		wassert(timeout.is_rel());
		_ipc.timeout = timeout.is_never() ? -1 : SystemClock_t::sys_clock(__func__) + timeout.rel_usec();
		_ipc.from_spec = from_spec;
		state(Receive_ipc);
		//printk("-- from_spec=0x%x/%u, timeout=%llu, expired=%llu.\n",
		//	from_spec.raw(), from_spec.number(), timeout.rel_usec(), _ipc.timeout);
	}

	inline void save_snd_phase(L4_time_t timeout, L4_thrid_t to)
	{
		wassert(timeout.is_rel());
		_ipc.timeout = timeout.is_never() ? -1 : SystemClock_t::sys_clock(__func__) + timeout.rel_usec();
		_ipc.to = to;
		state(Send_ipc);
		//printk("-- to=0x%x/%u, timeout=%llu, expired=%llu.\n",
		//	to.raw(), to.number(), timeout.rel_usec(), _ipc.timeout);
	}

	inline L4_clock_t timeout() { return _ipc.timeout; }

	void pf_save(word_t fault_addr, word_t fault_access, word_t fault_inst)
	{
		_pfault.addr   = fault_addr;
		_pfault.access = fault_access;
		_pfault.inst   = fault_inst;
		//_ipc.to = _pager_id;
	}

	inline word_t  pf_addr()       const { return _pfault.addr;       }
	inline word_t  pf_inst()       const { return _pfault.inst;       }
	inline word_t  pf_access()     const { return _pfault.access;     }

	inline void exc_save(int exc_type, word_t pfault_addr_and_acc)
	{
		_efault.type                = exc_type;
		_efault.pfault_addr_and_acc = pfault_addr_and_acc;
	}

	inline int    exc_type() const { return _efault.type;                }
	inline word_t exc_pf()   const { return _efault.pfault_addr_and_acc; }

	void inherit_prio_dump()
	{
		for (inhprios_t::iter_t it=_inherited_prios.begin(); it!=_inherited_prios.end(); ++it)
			printk("inh_prio_dump:  owner=%u, prio=%u.\n", it->owner.number(), it->prio);
	}

	inline inhprios_t::iter_t inherit_prio_find(L4_thrid_t owner)
	{
		for (inhprios_t::iter_t it=_inherited_prios.begin(); it!=_inherited_prios.end(); ++it)
			if (it->owner == owner)
				return it;
		return _inherited_prios.end();
	}

	void inherit_prio_add(L4_thrid_t owner, unsigned priority)
	{
		printk("inh_prio_add:  iam=%u:  owner=%u, prio=%u, inh_prio_list_sz=%zu.\n",
			globid().number(), owner.number(), priority, _inherited_prios.size());
		wassert(owner != globid());
		wassert(inherit_prio_find(owner) == _inherited_prios.end());
		_inherited_prios.insert_sort(InheritedPrio_t(owner, priority));
	}

	void inherit_prio_del(L4_thrid_t owner, unsigned priority)
	{
		printk("inh_prio_del:  iam=%u:  owner=%u, prio=%u, inh_prio_list_sz=%zu.\n",
			globid().number(), owner.number(), priority, _inherited_prios.size());
		(void) priority;
		inhprios_t::iter_t it = inherit_prio_find(owner);
		wassert(owner != globid());
		wassert(it != _inherited_prios.end());
		wassert(it->prio == priority);
		_inherited_prios.erase(it);
	}

	inline unsigned prio_max()
	{
		if (_inherited_prios.empty())
			return _prio;
		unsigned inh_max = _inherited_prios.back().prio;
		return max(_prio, inh_max);
	}

	bool is_good_sender(const Thread_t* snd, bool* use_local_id) const
	{
		wassert(state() == Receive_ipc);
		wassert(_ipc.from_spec != L4_thrid_t::Nil);

		*use_local_id = false;
		bool local = task() == snd->task();                   // is 'snd' local thread

		// check rcv from
		if (_ipc.from_spec.is_any()                       ||  // 'from' is any thread
			(local  &&  _ipc.from_spec.is_any_local())    ||  // 'from' is any_local thread
			(local  &&  _ipc.from_spec == snd->localid()) ||  // 'from' is local_id
			(           _ipc.from_spec == snd->globid()))     // 'from' is global_id
			return true;

		// set local or global flag for 'from' field
		if (local  &&  (_ipc.from_spec.is_any_local()  ||  _ipc.from_spec == snd->localid()))
			*use_local_id = true;

		return false;
	}

	bool is_irq_acceptable(unsigned irq)
	{
		wassert(state() == Receive_ipc);
		wassert(_ipc.from_spec != L4_thrid_t::Nil);

		// check rcv from
		if (_ipc.from_spec.is_any()  ||                           // 'from' is any thread
			_ipc.from_spec == L4_thrid_t::create_irq(irq))        // 'from' is equal
			return true;

		return false;
	}

	void activate()
	{
		printk("activate thread id=%u.\n", _id);
		wassert(!is_active() && "activate:  already active.");
		wassert(_utcb_pa && "activate:  no utcb location.");
		wassert(_task->is_configured() && "activate:  aspace has not been configured.");

		// map utcb
		addr_t utcb_uva = task()->alloc_utcb_area();
		wassert(utcb_uva);
		_task->map(utcb_uva, _utcb_pa, Cfg_page_sz, Acc_utcb, Cachable);
		_utcb_uva = utcb_uva;

		_task->thread_activated();
		state(Active);
	}

	void start(addr_t entry_ip, addr_t sp)
	{
		wassert(_state==Active && "Attemt to start inactivate thread.");

		if (_id != 1) // is not sigma0
		{
			wassert(!_pager_id.is_nil() && "Attemt to start thread without pager.");
			wassert(!_sched_id.is_nil() && "Attemt to start thread without scheduler.");

			// XXX
			// define pager, scheduler
			_pager = threads_find(_pager_id);
			_sched = threads_find(_sched_id);
			wassert(_pager && _sched);

			L4_utcb_t* p = utcb();
			p->global_id(globid());
		}

		set_initial_stack_frame(entry_ip, sp);
		state(Ready);
	}

	inline addr_t user_kip()
	{
		return task()->user_kip();
	}

	inline state_t state() const { return _state; }
	inline void state(state_t s)
	{
		printk("%s:  %u:  state:  %s -> %s.\n", _name, globid().number(), state_str(), state_str(s));
		wassert(_state != s);

		// delete from cur sched-list if need
		if (_state == Ready)
			_iter = threads_del_ready(_iter);
		else
		if (_state == Send_ipc  &&  _ipc.timeout != -1)
			_iter = threads_del_snd_timeout_waiting(_iter);
		else
		if (_state == Send_ipc  /**/ || _state==Send_pfault || _state==Send_exception /*~*/)
			_iter = threads_del_send(_iter);
		else
		if (_state == Receive_ipc  &&  _ipc.timeout != -1)
			_iter = threads_del_rcv_timeout_waiting(_iter);

		_state = s;

		// add to sched-new list if need
		if (s == Ready)
		{
			_ipc.clear();
			_pfault.clear();
			timeslice(Kcfg::Timeslice_usec);
			_iter = threads_add_ready(this);
		}
		else
		if (s == Send_ipc  &&  _ipc.timeout != -1)
			_iter = threads_add_snd_timeout_waiting(this);
		else
		if (s == Send_ipc /**/ || s==Send_pfault || s==Send_exception /*~*/)
			_iter = threads_add_send(this);
		else
		if (s == Receive_ipc  &&  _ipc.timeout != -1)
			_iter = threads_add_rcv_timeout_waiting(this);

		// remove inherited prio if need
		if (!prio_heir().is_nil())
		{
			Thread_t* thr = threads_find(prio_heir());
			wassert(thr);
			thr->inherit_prio_del(globid(), prio_max());
			prio_heir(L4_thrid_t::Nil);
		}
	}

	inline L4_utcb_t* uutcb() const { return (L4_utcb_t*) _utcb_uva; }  // used for intra aspace access
	inline L4_utcb_t* kutcb() const { return (L4_utcb_t*) _utcb_kva; }  // used for other aspace access, don't forget flush dcache
	inline L4_utcb_t*  utcb() const { return cur_thr()->task()==task() ? uutcb() : kutcb(); }

	inline paddr_t utcb_location() { return _utcb_pa; }

	inline void utcb(addr_t va)
	{
		wassert(!is_active() && "Attemt to set utcb_va for already activate thread.");
		_utcb_uva = va;
	}

	inline void utcb_location(paddr_t pa)
	{
		wassert(!is_active() && "Attemt to set utcb_pa for already activate thread.");
		_utcb_pa = pa;
		_utcb_kva = Aspace::kmap_utcb(pa);
		wassert(_utcb_kva);

		// set user accesible thread name (wrm extention)
		memcpy(kutcb()->tls, _name, 4);
	}

	Entry_frame_t* entry_frame() const
	{
		addr_t addr = (addr_t)_kstack_area + Stack_sz - sizeof(Entry_frame_t);
		return (Entry_frame_t*) addr;
	}

	static void user_invoke()
	{
		arch_user_invoke();
	}

	void set_initial_stack_frame(addr_t entry, addr_t sp)
	{
		printk("%s:  entry=%lx, sp=%lx, _ksp=%lx, utcb=%lx.\n", __func__, entry, sp, _ksp, _utcb_uva);

		wassert(_ksp);
		wassert(_utcb_uva);
		wassert(entry);

		Stack::push(&_ksp, _flags);              // user_invoke() will use it from the stack
		Stack::push(&_ksp, sp);                  // user_invoke() will use it from the stack
		Stack::push(&_ksp, entry);               // user_invoke() will use it from the stack
		Stack::push(&_ksp, _utcb_uva);           // user_invoke() will use it from the stack
		Stack::push(&_ksp, (word_t)user_invoke); // context_switch() will use it from the stack
	}

	void store_floats()
	{
		if (fpu_in_use()  /*TESTME*/ &&  entry_frame()->is_fpu_enabled()/*~TESTME*/)
			arch_store_floats(&_float_frame);
	}

	void restore_floats()
	{
		if (fpu_in_use())
			arch_restore_floats(&_float_frame);
	}

	void context_switch(Thread_t* next);
};

//--------------------------------------------------------------------------------------------------
class Int_thread_t
{
	unsigned   _intno;         // int number
	L4_thrid_t _globid;        // int thread id
	L4_thrid_t _handler;       // global id
	bool       _pending;       // is interrupt happens?

	static unsigned _counter;  // to set int number

public:

	Int_thread_t() :
		_intno(_counter++),
		_globid(L4_thrid_t::create_irq(_intno)),
		_handler(L4_thrid_t::Nil),
		_pending(false) {}

	inline void handler(L4_thrid_t h)
	{
		wassert(h.is_nil() || thrid_is_global_user(h));
		_handler = h;
		_pending = false;
		if (h.is_nil())
			Intc::mask(_intno);
	}

	inline void pending(bool p)       { _pending = p;              }
	inline unsigned intno()     const { return _intno;             }
	inline L4_thrid_t globid()  const { return _globid;            }
	inline L4_thrid_t handler() const { return _handler;           }
	inline bool is_active()     const { return !_handler.is_nil(); }
	inline bool is_pending()    const { return _pending;           }
};

#endif // THREAD_H
