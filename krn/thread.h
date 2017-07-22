//##################################################################################################
//
//  Thread implementation.
//
//##################################################################################################

#ifndef THREAD_H
#define THREAD_H

#include "entry_frame.h"
#include "list.h"
#include "l4types.h"
#include "task.h"
#include "stack.h"
#include "sysclock.h"
#include "thrid.h"
#include "tmaccount.h"
#include "arch.h"

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
		// sizes in words
		Canary1_sz  = 32,          // minimum 16 words
		Canary1_val = 0xdeadbeef,
		Canary2_sz  = 32,
		Canary2_val = 0xabadbabe,
		Stack_sz    = 0x300,
		Stack_val   = 0xa5a5a5a5,
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
		Idle           =            0,  // unused thread
		Inactive       =            1,  // inactive thread
		Active         =            2,  // active thread, start by receiving a starg_msg from pager
		Ready          =            3,  // thread ready to execute
		Send_ipc       = Snd_mask | 0,  // thread blocked in IpcSnd phase
		Send_pfault    = Snd_mask | 1,  // thread blocked for send pf_msg to pager
		Receive_ipc    = Rcv_mask | 0,  // thread blocked in IpcRcv phase
		Receive_pfault = Rcv_mask | 1   // thread blocked for receive map/grant msg from pager
	};

private:

	// TODO: may be enough store 'timeout' and 'partner'
	struct Ipc_t
	{
		uint64_t   timeout;           // systime in usec, timeout for current operation (snd/rcv)
		L4_thrid_t to;                // dst for snd phase
		L4_thrid_t from_spec;         // src pattern for rcv phase
		Ipc_t() : timeout(0), to(L4_thrid_t::Nil), from_spec(L4_thrid_t::Nil) {}
	};

	struct Pfault_t
	{
		word_t addr;                  // pfault address
		word_t inst;                  // pfault instruction
		word_t access;                // access permission
		//word_t status;              // MMU status register value
		Pfault_t() : addr(0), inst(0), access(0) {}
	};

	static unsigned _counter;         // for set id

	unsigned _id;                     // XXX:  is it need?
	addr_t _ksp;                      // kernel stack pointer
	char _name[8];                    // thread's name for debug

	unsigned _flags;                  // now use only 1 flag:  FPU=1
	unsigned _fpu_in_use;             // is need store/restore FPU context ?

	// kstack space should be alligned to 8 bytes
	// TODO:  for arm alligned must be 3 (8=2^3)
	unsigned _canary1 [Canary1_sz] __attribute__((aligned(16)));  // canary1 to detect stack overflow
	unsigned _kstack  [Stack_sz]   __attribute__((aligned(16)));  // task's kernel stack
	unsigned _canary2 [Canary2_sz] __attribute__((aligned(16)));  // canary2 to detect stack corruption

	Float_frame_t _float_frame;

	// aspace data
	Task_t* _task;                        // thread's task
	addr_t _utcb_uva;                     // UTCB va for user address space
	addr_t _utcb_kva;                     // UTCB va for kernel address space
	paddr_t _utcb_pa;                     // UTCB location

	// L4 data
	L4_thrid_t  _glob_id;                 // global thread ID
	L4_thrid_t  _sched_id;                // sched thread ID, global or local
	L4_thrid_t  _pager_id;                // pager thread ID, global or local
	Thread_t*   _sched;                   //
	Thread_t*   _pager;                   //
	state_t     _state;                   //
	Ipc_t       _ipc;                     // store active ipc operation data
	Pfault_t    _pfault;                  // store pfault data

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

	// Wrm extention:  sw irq
	bool        _sw_irq_pending;          // flag:  is sowtware irq pending

	threads_t::iter_t _iter;              // iterator for current threads list

	// time accounting
	Time_account_t _tmaccount;            // for accounting timeslice and profile

public:

	void print_kstack(const char* name, uint32_t* area, size_t sz)
	{
		printf("stack:  name=%s, sz=%u:\n", name, sz);
		for (unsigned i=0; i<sz; ++i)
			printf("  0x%08x  %8x\n", area + i, area[i]);
		printf("\n");
	}

	bool check_canary()
	{
		bool err = 0;

		for (unsigned i=0; i<Canary1_sz; ++i)
		{
			if (_canary1[i] != Canary1_val)
			{
				printk("ERROR:  kstack overflow, thr_id=%u.\n", _id);
				err = 1;
			}
		}

		for (unsigned i=0; i<Canary2_sz; ++i)
		{
			if (_canary2[i] != Canary2_val)
			{
				printk("ERROR:  kstack corrupt, thr_id=%u.\n", _id);
				err = 1;
			}
		}

		if (_ksp < (addr_t)_kstack  ||  _ksp > ((addr_t)_kstack + sizeof(_kstack)))
		{
			printk("ERROR:  ksp corrupt, thr_id=%u:  kstack=[%x, %x), ksp=%x\n",
				_id, _kstack, (addr_t)_kstack + sizeof(_kstack), _ksp);
			err = 1;
		}

		if (err)
		{
			print_kstack("canary1", _canary1, Canary1_sz);
			print_kstack("area",    _kstack,  Stack_sz);
			print_kstack("canary2", _canary2, Canary2_sz);
			printk("kstack error detected.\n");
		}
		return !err;
	}

	unsigned unused_kstack_sz()
	{
		for (unsigned i=0; i<Stack_sz; ++i)
			if (_kstack[i] != Stack_val)
				return i * sizeof(_kstack[0]);
		return Stack_sz * sizeof(_kstack[0]);
	}

	// for replacement new in list_t
	static void* operator new(size_t sz, Thread_t* thr)
	{
		(void) sz;
		return thr;
	}

	explicit Thread_t() : _id(_counter++), _ksp((addr_t)_kstack + sizeof(_kstack)), _flags(0), _fpu_in_use(0),
	                      _task(0), _utcb_uva(-1), _utcb_kva(-1), _utcb_pa(-1),
	                      _glob_id(L4_thrid_t::Nil), _sched_id(L4_thrid_t::Nil), _pager_id(L4_thrid_t::Nil),
	                      _sched(0), _pager(0), _state(Idle),
	                      _ipc(), _pfault(),
	                      _prio(0), _prio_heir(L4_thrid_t::Nil), _sw_irq_pending(false), _tmaccount(_name)
	{
		_name[0] = 0;

		for (unsigned i=0; i<Canary1_sz; ++i)
			_canary1[i] = Canary1_val;

		for (unsigned i=0; i<Stack_sz; ++i)
			_kstack[i] = Stack_val;

		for (unsigned i=0; i<Canary2_sz; ++i)
			_canary2[i] = Canary2_val;

		// NOTE:  _ksp now is pointing to invalid space,
		//        alloc space before pushing data!

		printk("Thread::ctor:  id=%d, _kstack=0x%x, sz=%u, _ksp=0x%x.\n", _id, _kstack, sizeof(_kstack), _ksp);
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

	inline unsigned        id()             const { return _id;       }
	inline addr_t          ksp()            const { return _ksp;      }
	inline addr_t          kentry_sp()      const { return (addr_t)_kstack + sizeof(_kstack); }
	inline const char*     name()           const { return _name;     }
	inline unsigned        flags()          const { return _flags;    }
	inline unsigned        fpu_in_use()     const { return _fpu_in_use; }
	inline Task_t*         task()           const { return _task;     }
	inline L4_thrid_t      globid()         const { return _glob_id;  }
	inline L4_thrid_t      localid()        const { return _utcb_uva; }
	inline L4_thrid_t      schedid()        const { return _sched_id; }
	inline L4_thrid_t      pagerid()        const { return _pager_id; }
	inline Thread_t*       sched()          const { return _sched;    }
	inline Thread_t*       pager()          const { return _pager;    }
	inline bool            is_active()      const { return _state != Inactive; }
	inline uint8_t         prio()           const { return _prio;     }
	inline L4_thrid_t      prio_heir()      const { return _prio_heir; }
	inline bool            sw_irq_pending() const { return _sw_irq_pending; }
	threads_t::iter_t      iter()           const { return _iter; }

	inline void ksp(addr_t v)                  { _ksp        = v; }
	inline void fpu_in_use(unsigned v)         { _fpu_in_use = v; }
	inline void task(Task_t* v)                { _task       = v; }
	inline void globid(L4_thrid_t v)           { _glob_id    = v; }
	inline void schedid(L4_thrid_t v)          { _sched_id   = v; }
	inline void pagerid(L4_thrid_t v)          { _pager_id   = v; }
	inline void sched(Thread_t* v)             { _sched      = v; }
	inline void pager(Thread_t* v)             { _pager      = v; }
	inline void prio_heir(L4_thrid_t v)        { _prio_heir  = v; }
	inline void sw_irq_pending(bool v)         { _sw_irq_pending = v; }
	inline void iter(threads_t::iter_t v)      { _iter       = v; }
	inline void timeslice(unsigned v)          { _tmaccount.timeslice(v); }

	// time account funcs
	inline unsigned   timeslice()                 const { return _tmaccount.timeslice(); }
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

	inline void prio(uint8_t v)
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
		// now FPU flag only supported (flag 0x1)
		// disable FPU if need
		if (_flags == 1  &&  f == 0)
		{
			fpu_in_use(false);
			entry_frame()->disable_fpu();
		}
		_flags = f;
	}

	static const char* state_str(int state)
	{
		switch (state)
		{
			case Idle:            return "idle";
			case Inactive:        return "inactive";
			case Active:          return "active";
			case Ready:           return "ready";
			case Send_ipc:        return "send_ipc";
			case Send_pfault:     return "send_pfault";
			case Receive_ipc:     return "receive_ipc";
			case Receive_pfault:  return "receive_pfault";
		}
		return "__unknown_state__";
	}
	const char* state_str()    const { return state_str(_state); }

	bool inline is_snd_state() const { return _state & Snd_mask; }
	bool inline is_rcv_state() const { return _state & Rcv_mask; }

	inline L4_thrid_t ipc_to()        const { return _ipc.to; }
	inline L4_thrid_t ipc_from_spec() const { return _ipc.from_spec; }

	inline void save_rcv_phase(L4_time_t timeout, L4_thrid_t from_spec)
	{
		assert(timeout.is_rel());
		_ipc.timeout = timeout.is_never() ? -1 : SystemClock_t::sys_clock(__func__) + timeout.rel_usec();
		_ipc.from_spec = from_spec;
		state(Receive_ipc);
		//printk("-- from_spec=0x%x/%u, timeout=%llu, expired=%llu.\n",
		//	from_spec.raw(), from_spec.number(), timeout.rel_usec(), _ipc.timeout);
	}
	
	inline void save_snd_phase(L4_time_t timeout, L4_thrid_t to)
	{
		assert(timeout.is_rel());
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

		_ipc.to = _pager_id;
	}
	
	inline word_t pf_addr()      const { return _pfault.addr;    }
	inline word_t pf_inst()      const { return _pfault.inst;    }
	inline word_t pf_access()    const { return _pfault.access;  }

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
		printk("inh_prio_add:  iam=%u:  owner=%u, prio=%u, inh_prio_list_sz=%u.\n",
			globid().number(), owner.number(), priority, _inherited_prios.size());
		assert(owner != globid());
		assert(inherit_prio_find(owner) == _inherited_prios.end());
		_inherited_prios.insert_sort(InheritedPrio_t(owner, priority));
	}

	void inherit_prio_del(L4_thrid_t owner, unsigned priority)
	{
		printk("inh_prio_del:  iam=%u:  owner=%u, prio=%u, inh_prio_list_sz=%u.\n",
			globid().number(), owner.number(), priority, _inherited_prios.size());
		(void) priority;
		inhprios_t::iter_t it = inherit_prio_find(owner);
		assert(owner != globid());
		assert(it != _inherited_prios.end());
		assert(it->prio == priority);
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
		assert(state() == Receive_ipc);
		assert(_ipc.from_spec != L4_thrid_t::Nil);

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
		assert(state() == Receive_ipc);
		assert(_ipc.from_spec != L4_thrid_t::Nil);

		// check rcv from
		if (_ipc.from_spec.is_any()  ||                           // 'from' is any thread
			_ipc.from_spec == L4_thrid_t::create_global(irq, 1))  // 'from' is equal
			return true;

		return false;
	}

	void activate()
	{
		printk("activate thread id=%u.\n", _id);
		assert(!is_active() && "activate:  already active.");
		assert(_utcb_pa && "activate:  no utcb location.");
		assert(_task->is_configured() && "activate:  aspace has not been configured.");

		// map utcb
		addr_t utcb_uva = task()->alloc_utcb_area();
		assert(utcb_uva);
		_task->map(utcb_uva, _utcb_pa, Cfg_page_sz, Acc_utcb, Cachable);
		_utcb_uva = utcb_uva;

		_task->thread_activated();
		state(Active);
	}

	void start(addr_t entry_ip, addr_t sp)
	{
		assert(_state==Active && "Attemt to start inactivate thread.");

		if (_id != 1) // is not sigma0
		{
			assert(!_pager_id.is_nil() && "Attemt to start thread without pager.");
			assert(!_sched_id.is_nil() && "Attemt to start thread without scheduler.");

			// XXX
			// define pager, scheduler
			_pager = threads_find(_pager_id);
			_sched = threads_find(_sched_id);
			assert(_pager && _sched);

			L4_utcb_t* p = cur_thr()->task()==task() ? utcb() : kutcb();
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
		printk("%s:  0x%x/%u:  state:  %s -> %s.\n", _name, globid().raw(), globid().number(), state_str(), state_str(s));
		assert(_state != s);

		// delete from cur list if need
		if (_state == Ready)
			_iter = threads_del_ready(_iter);
		else
		if (_state == Send_ipc  &&  _ipc.timeout != -1)
			_iter = threads_del_snd_timeout_waiting(_iter);
		else
		if (_state == Send_ipc)
			_iter = threads_del_send(_iter);
		else
		if (_state == Receive_ipc  &&  _ipc.timeout != -1)
			_iter = threads_del_rcv_timeout_waiting(_iter);

		_state = s;

		// add to new list if need
		if (s == Ready)
			_iter = threads_add_ready(this);
		else
		if (s == Send_ipc  &&  _ipc.timeout != -1)
			_iter = threads_add_snd_timeout_waiting(this);
		else
		if (s == Send_ipc)
			_iter = threads_add_send(this);
		else
		if (s == Receive_ipc  &&  _ipc.timeout != -1)
			_iter = threads_add_rcv_timeout_waiting(this);
	}

	inline L4_utcb_t*  utcb() const { return (L4_utcb_t*) _utcb_uva; }  // used for intra aspace access
	inline L4_utcb_t* kutcb() const { return (L4_utcb_t*) _utcb_kva; }  // used for other aspace access, don't forget flush dcache
	inline paddr_t utcb_location() { return _utcb_pa; }

	inline void utcb(addr_t va)
	{
		assert(!is_active() && "Attemt to set utcb_va for already activate thread.");
		_utcb_uva = va;
	}

	inline void utcb_location(paddr_t pa)
	{
		assert(!is_active() && "Attemt to set utcb_pa for already activate thread.");
		_utcb_pa = pa;
		_utcb_kva = Aspace::kmap_utcb(pa);
		assert(_utcb_kva);

		// set user accesible thread name (wrm extention)
		memcpy(kutcb()->tls, _name, 4);
	}

	Entry_frame_t* entry_frame()
	{
		addr_t addr = (addr_t)_kstack + sizeof(_kstack) - sizeof(Entry_frame_t);
		return (Entry_frame_t*) addr;
	}

	void set_initial_stack_frame(addr_t entry, addr_t sp)
	{
		printk("%s:  entry=%x, sp=%x, _ksp=%x, utcb=%x.\n", __func__, entry, sp, _ksp, _utcb_uva);

		assert(_ksp);
		assert(_utcb_uva);
		assert(entry);

		Stack::push(&_ksp, _flags);                   // user_invoke() will use it from the stack
		Stack::push(&_ksp, sp);                       // user_invoke() will use it from the stack
		Stack::push(&_ksp, entry);                    // user_invoke() will use it from the stack
		Stack::push(&_ksp, _utcb_uva);                // user_invoke() will use it from the stack
		Stack::push(&_ksp, (word_t)arch_user_invoke); // context_switch() will use it from the stack
	}

	void store_floats()
	{
		if (fpu_in_use())
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
		_globid(L4_thrid_t::create_global(_intno, 1)),
		_handler(L4_thrid_t::Nil),
		_pending(false) {}

	inline void handler(L4_thrid_t h)
	{
		assert(h.is_nil() || thrid_is_global_user(h));
		_handler = h;
		_pending = false;
		if (h.is_nil())
			Intc::mask(_intno);
		//else                           DELME
		//	Intc::unmask(_intno);        DELME
	}

	inline void pending(bool p)       { _pending = p;              }
	inline unsigned intno()     const { return _intno;             }
	inline L4_thrid_t globid()  const { return _globid;            }
	inline L4_thrid_t handler() const { return _handler;           }
	inline bool is_active()     const { return !_handler.is_nil(); }
	inline bool is_pending()    const { return _pending;           }
};

#endif // THREAD_H
