//##################################################################################################
//
//  ...
//
//##################################################################################################

#ifndef TASK_H
#define TASK_H

#include "kmem.h"
#include "kkip.h"
#include "wlibc_assert.h"
#include "arch_data.h"

//--------------------------------------------------------------------------------------------------
class Task_t
{
	static unsigned _counter;         // for set id

	unsigned    _id;                  //
	char        _name[8];             // space's name for debug
	Aspace      _aspace;              // virtual address space
	L4_fpage_t  _kip_area;            // KIP area for user address space
	L4_fpage_t  _utcb_area;           // UTCBs area for user address space
	L4_thrid_t  _redirector;          //
	unsigned    _threads_max;         //
	unsigned    _threads_act;         //
	Arch_task_t _arch;                //

public:

	// for replacement new in list_t
	static void* operator new(size_t sz, Task_t* tsk)
	{
		(void) sz;
		return tsk;
	}

	explicit Task_t() : _id(_counter++), _aspace(_id), _threads_max(0), _threads_act(0)
	{
		_kip_area = L4_fpage_t::create_nil();
		_utcb_area = L4_fpage_t::create_nil();
		_name[0] = 0;

		#if defined (Cfg_arch_arm) or defined (Cfg_arch_x86) or defined (Cfg_arch_x86_64)  or defined (Cfg_arch_sparc)
		// for arm utcb address locates at 0xff000000
		// for x86 utcb address locates at %gs:0, use 0xff000000 for it
		// TODO:  one page per CPU is enough I think
		addr_t kupage_va = kmem_alloc(Cfg_page_sz, Cfg_page_sz);
		paddr_t kupage_pa = kmem_paddr(kupage_va, Cfg_page_sz);
		#if 1
		// FIXME:  use kernal map, now bug
		map(0xff000000, kupage_pa, Cfg_page_sz, Acc_kw_ur, Cachable);
		#else
		_aspace.kmap(0xff000000, kupage_pa, Cfg_page_sz, Acc_kw_ur, Cachable);
		#endif
		// FIXME:  for x86_64 think about address
		#endif

		printk("Task::ctor:  id=%d.\n", _id);
	}

	void name(const char* n, size_t sz = sizeof(_name) - 1)
	{
		unsigned len = min(sizeof(_name) - 1, sz);
		strncpy(_name, n, len);
		_name[len] = '\0';
	}

	const char* name()  const { return _name; }
	unsigned id()       const { return _id;   }

	inline L4_thrid_t redirector()   const { return _redirector;  }
	inline unsigned threads_max()    const { return _threads_max; }
	inline unsigned threads_active() const { return _threads_act; }

	inline void redirector(L4_thrid_t r) { _redirector = r; }

	void map(addr_t va, paddr_t pa, size_t sz, kacc_t acc, unsigned cachable)
	{
		printk("Task::map:  id=%d, va=0x%08lx, pa=0x%llx, sz=0x%08zx, acc=%s.\n",
			_id, va, (long long)pa, sz, Aspace::Type(acc, cachable).str());
		_aspace.map(va, pa, sz, acc, cachable);
	}

	void unmap(addr_t va, size_t sz)
	{
		printk("Task::unmap:  id=%d, va=0x%08lx, sz=0x%zx.\n", _id, va, sz);
		_aspace.unmap(va, sz);
	}

	paddr_t walk(addr_t va, size_t sz)
	{
		printk("Task::walk:  id=%d, va=0x%08lx, sz=0x%zx.\n", _id, va, sz);
		return _aspace.walk(va, sz);
	}

	inline paddr_t walk(void* p, size_t sz)  {  return walk((addr_t)p, sz);  }

	addr_t find_free_uspace(size_t sz, size_t align)
	{
		return _aspace.find_free_uspace(sz, align);
	}


private:

	// used than first task's thread is starting
	void map_kip()
	{
		paddr_t pa = kmem_paddr(get_kip(), Cfg_page_sz);
		_aspace.map(_kip_area.addr(), pa, Cfg_page_sz, Acc_ukip, Cachable);
	}

public:

	inline void kip_area(L4_fpage_t fpage)
	{
		wassert(!_threads_act);
		wassert(!fpage.is_nil());
		_kip_area  = fpage;
	}

	inline void utcb_area(L4_fpage_t fpage)
	{
		wassert(!_threads_act);
		_utcb_area = fpage;
		_threads_max = fpage.size() / Cfg_page_sz;
	}

	inline L4_fpage_t kip_area()  { return _kip_area;  }

	// alloc utcb area for next thread
	inline addr_t alloc_utcb_area()
	{
		wassert(_utcb_area.addr() && _utcb_area.size());
		return _threads_act < _threads_max  ?  _utcb_area.addr() + _threads_act * Cfg_page_sz  :  0;
	}

	inline bool is_able_to_activate_new_thread()
	{
		return _threads_max - _threads_act;
	}

	inline void thread_activated() // callback
	{
		if (!_threads_act)
		{
			// aspace should be already configured
			wassert(_kip_area.addr() && _kip_area.size());
			map_kip();
		}
		wassert(_utcb_area.addr() && _utcb_area.size());
		wassert(_threads_act < _threads_max);
		_threads_act++;
	}

	inline addr_t user_kip()
	{
		return _kip_area.addr();
	}

	inline void set_current(bool force = false)
	{
		_aspace.set_current(force);
		_arch.set_current();
	}

	bool is_configured()
	{
		return _kip_area.addr() && _kip_area.size() && _utcb_area.addr() && _utcb_area.size();
	}

	inline bool is_inside_acc(L4_fpage_t fp)
	{
		return _aspace.is_inside_acc(fp);
	}

	// return 'cached' attribute if OK or negative value if 'is_inside_acc' failed
	inline int cached(L4_fpage_t fp)
	{
		return _aspace.cached(fp);
	}

	// return memory attributes if OK or negative value if alien memory
	inline int attributes(L4_fpage_t fp)
	{
		return _aspace.attributes(fp);
	}

	inline void dump()
	{
		_aspace.dump();
	}

	inline void set_ioperm_all(int enable)
	{
		_arch.set_ioperm_all(enable);
	}

	inline void set_ioperm(unsigned port, unsigned sz, int enable)
	{
		_arch.set_ioperm(port, sz, enable);
	}

	inline int is_ioperm(unsigned port, unsigned sz, int enable)
	{
		return _arch.is_ioperm(port, sz, enable);
	}
};

//--------------------------------------------------------------------------------------------------
class Tasks_t
{
	typedef list_t <Task_t, 16> tasks_t;
	static tasks_t _tasks;

public:

	static Task_t& create()
	{
		Task_t& tsk = _tasks.push_back();
		return tsk;
	}
};

#endif // TASK_H
