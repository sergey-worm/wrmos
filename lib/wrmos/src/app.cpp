//##################################################################################################
//
//  Thread - to create and manage by thread.
//
//##################################################################################################

#include "wrm_app.h"
#include "wrm_thr.h"
#include "wrm_mpool.h"
#include "wrm_log.h"
#include "wrm_appcfg.h"
#include "wrm_ramfs.h"
#include "l4_api.h"
#include "list.h"
#include "sys_utils.h"
#include "sys_proc.h"
#include "elfloader.h"
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include "wlibc_panic.h"


void* region_list_malloc(size_t sz_req, size_t* sz_rep)
{
	L4_fpage_t fpage = wrm_pgpool_alloc(round_up(sz_req, Cfg_page_sz));
	//wrm_logd("%s:  sz_req=%zu, sz_rep=%lu.\n", __func__, sz_req, fpage.size());
	assert(!fpage.is_nil());
	if (fpage.is_nil())
		return 0;
	*sz_rep = fpage.size();
	return (void*)fpage.addr();
}

//--------------------------------------------------------------------------------------------------
//  App-manager
//--------------------------------------------------------------------------------------------------
class Thr_t
{
public:

	int        state;
	long       terminate_code;
	L4_fpage_t utcb;
	addr_t     stack_addr;
	size_t     stack_sz;

	Thr_t() : state(Wrm_thr_state_free) {}
};

class Thrs_t
{
	Thr_t    _threads[32];
	unsigned _threads_max;

public:

	void max(unsigned threads_max)
	{
		assert(threads_max <= sizeof(_threads)/sizeof(_threads[0]));
		_threads_max = threads_max;
		for (unsigned i=0; i<_threads_max; ++i)
			_threads[i].state = Wrm_thr_state_free;
	}

	unsigned max() const
	{
		return _threads_max;
	}

	// find free and return thread index
	int alloc(L4_fpage_t utcb, addr_t stack_addr, size_t stack_sz)
	{
		for (unsigned i=0; i<_threads_max; ++i)
		{
			Thr_t* t = &_threads[i];
			if (t->state == Wrm_thr_state_free)
			{
				t->state      = Wrm_thr_state_busy;
				t->utcb       = utcb;
				t->stack_addr = stack_addr;
				t->stack_sz   = stack_sz;
				return i;
			}
		}
		return -1;
	}

	// return thread state
	int state(unsigned thr_index)
	{
		if (thr_index >= _threads_max)
			return -1;
		return _threads[thr_index].state;
	}

	// set terminate code and update state
	int terminate(unsigned thr_index, long term_code)
	{
		if (thr_index >= _threads_max)
			return -1;
		Thr_t* t = &_threads[thr_index];
		if (t->state != Wrm_thr_state_busy)
			return -2;
		t->terminate_code = term_code;
		t->state = Wrm_thr_state_done;
		return 0;
	}

	// return thread params and update state
	int free(unsigned thr_index, L4_fpage_t* utcb, addr_t* stack_addr, size_t* stack_sz, long* term_code)
	{
		if (thr_index >= _threads_max)
			return -1;
		Thr_t* t = &_threads[thr_index];
		if (t->state != Wrm_thr_state_done)
			return -2;
		*utcb       = t->utcb;
		*stack_addr = t->stack_addr;
		*stack_sz   = t->stack_sz;
		*term_code  = t->terminate_code;
		t->state    = Wrm_thr_state_free;
		return 0;
	}
};

class App_t
{
	struct region_t
	{
		addr_t remote;         // vaddr in remote aspace
		size_t sz;             // size in bytes, may not be aligned
		acc_t  acc;            // access bits
		addr_t local;          // vaddr in local aspace, if 0 --> need allocate before using
		addr_t local_origin;   // if !=0 --> copy data from 'local_origin' to 'local' after allocating

		//explicit region_t(addr_t r, size_t s, unsigned a) :
		//	remote(r), sz(s), acc(a), local(0), local_origin(0)  {}
		explicit region_t(addr_t r, size_t s, unsigned a, addr_t l=0, addr_t lo=0) :
			remote(r), sz(s), acc(a), local(l), local_origin(lo) {}
		addr_t remote_end()  { return remote + sz; }
		bool need_relocate() { return acc & Acc_w; }
		addr_t location()    { return local; } 
	};

	typedef list_t <region_t, 0> regions_t;
	regions_t  _regions;       // memory regions
	L4_thrid_t _owner;         // global thread ID
	addr_t     _entry;         // entry point
	addr_t     _stack_ua;      // stack area address
	size_t     _stack_sz;      // stack area size
	addr_t     _sp;            // initial stack pointer
	L4_fpage_t _kip;           // kip area
	L4_fpage_t _utcb;          // utcb area
	word_t     _name;          // 4 chars

	// aspaces data
	unsigned _max_aspaces;     // max aspaces in app
	unsigned _aspaces_in_use;  // aspaces in use

	// threads data
	Thrs_t _threads;

	// config params
	uint8_t _max_prio;
	bool    _fpu;
	int     _malloc_strategy;

public:

	explicit App_t(L4_thrid_t id) : _regions(/*dprint_list_regions*/), _owner(id), _entry(0), _sp(0)
	{
		//wrm_logd("  App_t::ctor:  id=%u, regs.cap()=%u.\n", _owner.number(), _regions.capacity());
		_regions.set_allocator(region_list_malloc);
	}

	~App_t()
	{
		//wrm_logd("  App_t::dtor:  id=%u.\n", _owner.number());
	}

	App_t operator = (const App_t& right)
	{
		_regions = right._regions;
		_owner   = right._owner;
		return *this;
	}

	void entry(addr_t v)            { _entry = v;                       }
	void kip(L4_fpage_t v)          { _kip   = v;      add_vspace(v);   }
	void utcb(L4_fpage_t v)         { _utcb  = v;      add_vspace(v);   }
	void name(const char* v)        { _name = *(word_t*)v;              }
	void max_aspaces(unsigned v)    { _max_aspaces = v;                 }
	void max_threads(unsigned v)    { _threads.max(v);                  }
	void max_prio(unsigned v)       { _max_prio = v;                    }
	void fpu(bool v)                { _fpu = v;                         }
	void malloc_strategy(int v)     { _malloc_strategy = v;             }

	void stack(addr_t a, size_t s)
	{
		_stack_ua = a;
		_stack_sz = s;
		_sp = a + s;
		add_vspace(a, s, Acc_rw);
	}

	L4_thrid_t  owner()           const { return _owner;           }
	addr_t      entry()           const { return _entry;           }
	addr_t      stack_ua()        const { return _stack_ua;        }
	size_t      stack_sz()        const { return _stack_sz;        }
	addr_t      sp()              const { return _sp;              }
	L4_fpage_t  kip()             const { return _kip;             }
	L4_fpage_t  utcb()            const { return _utcb;            }
	word_t      name()            const { return _name;            }
	const char* name_str()        const { return (char*)&_name;    } // no null terminator !
	unsigned    max_aspaces()     const { return _max_aspaces;     }
	unsigned    max_threads()     const { return _threads.max();   }
	uint8_t     max_prio()        const { return _max_prio;        }
	bool        fpu()             const { return _fpu;             }
	int         malloc_strategy() const { return _malloc_strategy; }

	void dump()
	{
		wrm_logd("  Dump app_map:  id=%u, name=%.4s, ep=0x%lx, sp=0x%lx, regions=%zu:\n",
			_owner.number(), (char*)&_name, _entry, _sp, _regions.size());
		for (regions_t::citer_t it=_regions.cbegin(); it!=_regions.cend(); ++it)
		{
			wrm_logd("    ptr=0x%p, rem=0x%08lx, sz=0x%08zx, acc=%s, loc=0x%08lx, loc_orig=0x%08lx\n",
				&*it, it->remote, it->sz, acc2str(it->acc), it->local, it->local_origin);
		}
	}

	bool alloc_aspace()
	{
		if (_aspaces_in_use + 1 > _max_aspaces)
			return false;
		_aspaces_in_use++;
		return true;
	}

	bool free_aspace()
	{
		if (!(_aspaces_in_use - 1))
			return false; // only one aspace in use
		_aspaces_in_use--;
		return true;
	}

	int thread_alloc(L4_fpage_t utcb, addr_t stack_addr, size_t stack_sz)
	{
		assert(!utcb.is_nil());
		assert(stack_addr);
		assert(stack_sz);
		int thr_index = _threads.alloc(utcb, stack_addr, stack_sz);
		return (thr_index < 0) ? thr_index : (thr_index + _owner.number());
	}

	int thread_state(unsigned thrno)
	{
		assert(thrno >= _owner.number()  &&  thrno < (_owner.number() + _threads.max()));
		return _threads.state(thrno - _owner.number());
	}

	int thread_terminate(unsigned thrno, long term_code)
	{
		assert(thrno >= _owner.number()  &&  thrno < (_owner.number() + _threads.max()));
		return _threads.terminate(thrno - _owner.number(), term_code);
	}

	int thread_free(unsigned thrno, L4_fpage_t* utcb, addr_t* stack_addr, size_t* stack_sz, long* term_code)
	{
		assert(thrno >= _owner.number()  &&  thrno < (_owner.number() + _threads.max()));
		return _threads.free(thrno - _owner.number(), utcb, stack_addr, stack_sz, term_code);
	}

	bool thread_numbers(unsigned* thrno_begin, unsigned* thrno_end)
	{
		*thrno_begin = _owner.number();
		*thrno_end   = _owner.number() + _threads.max();
		return true;
	}

private:

	// find memory region that contents [va,sz,acc]
	regions_t::iter_t find(addr_t rem_va, size_t sz, acc_t acc)
	{
		addr_t rem_end = rem_va + sz;
		for (regions_t::iter_t it=_regions.begin(); it!=_regions.end(); ++it)
			if (rem_va >= it->remote  &&  rem_end <= it->remote_end()  &&  acc == (acc & it->acc))
				return it;
		return _regions.end();
	}

public:

	// add remote aspace
	// 'rem_va' and 'sz' may not be page aligned, 'sz' may be 0
	void add_vspace(addr_t rem_va, size_t sz, acc_t acc)
	{
		//wrm_logd("%s() - rem_va=0x%lx, sz=0x%zx, acc=%d.\n", __func__, rem_va, sz, acc);
		assert(rem_va);
		assert(acc);
		if (!sz)
			return;
		regions_t::iter_t reg = find(rem_va, sz, Acc_nil); // check crossing
		if (reg != _regions.end())
			l4_kdb("Attempt to add region for app, but region cross existing region.");
		_regions.push_back(region_t(rem_va, sz, acc));
	}

	// add remote aspace
	void add_vspace(L4_fpage_t fp)
	{
		add_vspace(fp.addr(), fp.size(), fp.access());
	}

	// remove incoming region and add 2 or 3 new regions, return requested region
	regions_t::iter_t split_to_smaller_regions(regions_t::iter_t reg, addr_t rem_va, addr_t sz)
	{
		//wrm_logw("%s:  split reg:  rem=0x%lx, sz=0x%lx, acc=%d, loc=0x%lx, loc_orig=0x%lx.\n",
		//	__func__, reg->remote, reg->sz, reg->acc, reg->local, reg->local_origin);

		size_t sz_before = round_pg_down(rem_va) - reg->remote;
		bool   tails_in_common_pg = round_pg_down(rem_va + sz - 1) == round_pg_down(reg->remote_end() - 1);
		size_t sz_after  = tails_in_common_pg ? 0 : reg->remote_end() - round_pg_up(rem_va + sz);
		size_t sz_middle = reg->sz - sz_before - sz_after;

		//wrm_logw("%s:  split at sizes:  0x%zx, 0x%zx, 0x%zx.\n", __func__, sz_before, sz_middle, sz_after);

		assert((sz_before + sz_middle + sz_after) == reg->sz);

		if (sz_before)
		{
			_regions.push_back(region_t(reg->remote, sz_before, reg->acc, reg->local, reg->local_origin));
			//wrm_logw("%s:  add bite before:  rem=0x%lx, sz=0x%lx, acc=%d, loc=0x%lx, loc_orig=0x%lx.\n",
			//	__func__, _regions.back().remote, _regions.back().sz, _regions.back().acc,
			//	_regions.back().local, _regions.back().local_origin);
		}

		_regions.push_back(region_t(reg->remote + sz_before, sz_middle, reg->acc, reg->local,
			reg->local_origin ? (reg->local_origin + sz_before) : 0));
		//wrm_logw("%s:  add bite middle:  rem=0x%lx, sz=0x%lx, acc=%d, loc=0x%lx, loc_orig=0x%lx.\n",
		//	__func__, _regions.back().remote, _regions.back().sz, _regions.back().acc,
		//	_regions.back().local, _regions.back().local_origin);
		regions_t::iter_t new_reg = _regions.last();

		if (sz_after)
		{
			_regions.push_back(region_t(reg->remote + sz_before + sz_middle, sz_after, reg->acc, reg->local,
				reg->local_origin ? (reg->local_origin + sz_before + sz_middle) : 0));
			//wrm_logw("%s:  add bite after:   rem=0x%lx, sz=0x%lx, acc=%d, loc=0x%lx, loc_orig=0x%lx.\n",
			//	__func__, _regions.back().remote, _regions.back().sz, _regions.back().acc,
			//	_regions.back().local, _regions.back().local_origin);
		}

		_regions.erase(reg);
		assert(new_reg->remote == round_pg_down(rem_va));
		return new_reg;
	}

public:

	// find local_va for remote_va
	// allocate memory from pool if need
	addr_t get_location(addr_t rem_va, addr_t sz, acc_t acc, acc_t* acc_max = 0)
	{
		//wrm_logd("%s:  req:  0x%lx - 0x%lx, sz=0x%lx, acc=%d.\n",
		//	__func__, rem_va, rem_va + sz, sz, acc);

		regions_t::iter_t reg = find(rem_va, sz, acc);
		if (reg != _regions.end())
		{
			//wrm_logw("%s:  found:    0x%lx - 0x%lx, sz=0x%zx, acc=%d, loc=0x%lx, loc_orig=0x%lx.\n",
			//	__func__, reg->remote, reg->remote_end(), reg->sz, reg->acc, reg->local, reg->local_origin);
			assert(rem_va >= reg->remote  &&  (rem_va + sz) <= reg->remote_end());
			addr_t local = reg->location();
			if (!local)
			{
				/* TODO:  implement me
				if (malloc_strategy() == Wrm_app_cfg_t::Malloc_on_startup)
					wrm_loge("%s:  Malloc_on_startup but no location.\n", __func__);
				*/

				// split to smaller regions if need
				if (sz < reg->sz)
					reg = split_to_smaller_regions(reg, rem_va, sz);

				// allocate
				L4_fpage_t fp = wrm_pgpool_alloc(round_pg_up(reg->sz));
				assert(!fp.is_nil());
				if (fp.is_nil())
					return 0;

				// FIXME:  allocated page may cross several regions, but reg->local set only for one
				reg->local = fp.addr();

				// initialize if need
				if (reg->local_origin)
				{
					// copy all page instead of reg->sz, cause page may cross another regions
					memcpy((void*)round_pg_down(reg->local), (void*)round_pg_down(reg->local_origin), round_pg_up(reg->sz));
					//wrm_logw("memcpy:  local=%p, orig=0x%p, sz=0x%zx.\n",
					//	(void*)reg->local, (void*)reg->local_origin, round_pg_up(reg->sz));
				}
			}

			if (acc_max)
				*acc_max = reg->acc;

			size_t offset = rem_va - reg->remote;
			assert(reg->location());
			return reg->location() + offset;
		}
		return 0;
	}

	// do not create new regions, just set location for existing
	// incomming region may contents many existing regions
	void set_location_from_elf(addr_t rem_va, size_t sz, addr_t location)
	{
		//wrm_logd("%s:  rem=0x%lx, sz=0x%zx, loc=0x%lx.\n", __func__, rem_va, sz, location);

		// set location
		addr_t rem_end = rem_va + sz;
		for (regions_t::iter_t it=_regions.begin(); it!=_regions.end(); ++it)
		{
			if (it->remote >= rem_va  &&  it->remote_end() <= rem_end)
			{
				assert(is_pg_aligned(rem_va));
				//assert(is_pg_aligned(sz));  // sz may not be page aligned
				assert(is_pg_aligned(location));
				assert(rem_va);
				assert(sz);
				assert(location);

				size_t offset = it->remote - rem_va;
				addr_t local = location + offset;

				if (it->need_relocate()) // writable region, will allocate later
				{
					if (0 && malloc_strategy() == Wrm_app_cfg_t::Malloc_on_startup)
					{
						// TODO:  implement me
						assert(it->sz == Cfg_page_sz);
						// allocate memory
						L4_fpage_t fp = wrm_pgpool_alloc(it->sz);
						if (fp.is_nil())
							panic("%s:  no mem.\n", __func__);
						it->local = fp.addr();

						// initialize if need
						if (local)
							memcpy((void*)it->local, (void*)local, it->sz);
					}
					else
					{
						// allocating will be later on pagefault
						it->local_origin = local;
					}
				}
				else
				{
					it->local = local;
				}
			}
		}
	}

	// add vspace and set location for it, sz may be > Cfg_page_sz
	// return error code
	int add_location(addr_t rem_va, L4_fpage_t location)
	{
		//wrm_logw("%s:  rem=0x%lx, loc=0x%lx, sz=0x%lx.\n",
		//	__func__, rem_va, location.addr(), location.size());

		regions_t::iter_t reg = find(rem_va, location.size(), Acc_nil); // check crossing
		if (reg != _regions.end())
		{
			wrm_loge("%s:  region (va=0x%lx, sz=0x%lx) cross existing region.\n",
				__func__, rem_va, location.size());
			return -1;
		}
		_regions.push_back(region_t(rem_va, location.size(), location.access()));
		region_t& r = _regions.back();
		r.local = location.addr();
		return 0;
	}

	// add app arguments to memory map
	void args(const char* name, const Wrm_app_cfg_t::arg_t* args, addr_t args_page_rem_va)
	{
		assert(_sp && "Stack is absent.");

		// locate args in memory:
		//   arg1_ptr, arg2_ptr, ... , argN_ptr
		//   arg1_str, arg2_str, ... , argN_str

		// calculate argc, first arg is app name
		unsigned argc = 1;
		const Wrm_app_cfg_t::arg_t* arg = args;
		while ((*arg)[0])
		{
			argc++;
			arg++;
		}

		// alloc memory for args_page_rem_va
		L4_fpage_t fp = wrm_pgpool_alloc(Cfg_page_sz);
		assert(!fp.is_nil());
		if (fp.is_nil())
			panic("DEBUG ME");
		add_location(args_page_rem_va, fp);

		addr_t rem_va = args_page_rem_va;
		addr_t loc_va = fp.addr();
		size_t sz = Cfg_page_sz;
		int diff_loc_rem = loc_va - rem_va;

		//wrm_logd("argspace:  va=0x%x, sz=0x%x.\n", rem_va, sz);

		// store argv
		unsigned argv_sz = argc * sizeof(char*);
		if (sz < argv_sz)
		{
			panic("IMPLEMENT ME 1:  allocate page and set SZ and LOC_VA!");
			// set: rem_va, loc_va, sz, diff_loc_rem
		}
		char** argv = (char**)loc_va;
		loc_va += argv_sz;
		sz -= argv_sz;

		// store app name
		unsigned name_sz = strlen(name) + sizeof('\0');
		if (sz < name_sz)
		{
			panic("IMPLEMENT ME 2:  allocate page and set SZ and LOC_VA!");
			// set: loc_va, sz, diff_loc_rem
		}
		argv[0] = (char*)(loc_va - diff_loc_rem);
		memcpy((void*)loc_va, name, name_sz);
		loc_va += name_sz;
		sz -= name_sz;

		// store args one by one
		arg = args;
		unsigned cnt = 1;
		while ((*arg)[0])
		{
			size_t arg_sz = strlen(*arg) + sizeof('\0');
			if (sz < arg_sz)
			{
				panic("IMPLEMENT ME 3:  allocate page and set SZ and LOC_VA!");
				// set: loc_va, sz, diff_loc_rem
			}
			argv[cnt++] = (char*)(loc_va - diff_loc_rem);
			memcpy((void*)loc_va, *arg, arg_sz);
			loc_va += arg_sz;
			sz -= arg_sz;
			arg++;
		}

		size_t stack_params_sz = 3 * sizeof(word_t);  // 3 params
		addr_t stack_location = get_location(_sp - stack_params_sz, stack_params_sz, Acc_rw);
		assert(stack_location);
		word_t* sp_location = (word_t*)(stack_location + stack_params_sz);
		//wrm_logd("sp_location=0x%p, sp=0x%lx.\n", sp_location, _sp);
		sp_location[-1] = (addr_t)argv - diff_loc_rem;
		sp_location[-2] = argc;
		sp_location[-3] = _fpu;
		_sp -= stack_params_sz;
		//wrm_logd("argc=0x%lx, argv=0x%lx.\n\n", sp_location[-2], sp_location[-1]);
	}
};

class Apps_t
{
	typedef list_t <App_t, 8> apps_t;
	apps_t _apps;

public:

	Apps_t() : _apps(/*dprint_list_apps*/) {}

	void dump(const char* mark = "")
	{
		wrm_logd("Dump apps (%zu)%s:\n", _apps.size(), mark);
		for (apps_t::iter_t it=_apps.begin(); it!=_apps.end(); ++it)
			it->dump();
	}

	App_t* find(L4_thrid_t id)
	{
		for (apps_t::iter_t it=_apps.begin(); it!=_apps.end(); ++it)
		{
			unsigned it_num_s = it->owner().number();
			unsigned it_num_e = it->owner().number() + it->max_threads();
			if (id.number() >= it_num_s  &&  id.number() < it_num_e)
				return &*it;
		}
		return 0;
	}

	App_t* add_app(L4_thrid_t id)
	{
		_apps.push_back(App_t(id));
		return &_apps.back();
	}
};
static Apps_t apps;

//--------------------------------------------------------------------------------------------------
//  ~ App-manager
//--------------------------------------------------------------------------------------------------
/*
static unsigned dprint_elf(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wrm_logd("elf:  ");
	vprintf(fmt, args);
	va_end(args);
	return 0;
}
*/
// callback func for elf_foreach()
static void process_elf_shdr(size_t label, addr_t addr, size_t sz, unsigned access,
                             const char* name, int is_progbits_or_nobits)
{
	//wrm_logd("app:  shdr:  label=0x%zx, addr=0x%08lx, sz=0x%08zx, acc=%s, prog/no-bits=%d, name=%s.\n",
	//	label, addr, sz, acc2str(access), is_progbits_or_nobits, name);

	if (is_progbits_or_nobits)
		((App_t*)label)->add_vspace(addr, sz, access);
}

// callback func for elf_foreach()
static void process_elf_phdr(size_t label, addr_t vaddr, addr_t paddr, size_t sz,
                             addr_t location, int is_load)
{
	//wrm_logd("app:  phdr:  label=0x%zx, va=0x%lx, pa=0x%lx, sz=0x%zx, location=0x%lx, load=%d.\n",
	//	label, vaddr, paddr, sz, location, is_load);

	if (is_load)
		((App_t*)label)->set_location_from_elf(vaddr, sz, location);
}

static App_t* prepare_app_memory_map(const Wrm_app_cfg_t* cfg, L4_thrid_t id, addr_t elf_addr, size_t elf_sz)
{
	//wrm_logd("app:  %s:  id=%u, stack_sz=0x%x.\n", __func__, id.number(), cfg->stack_sz);

	App_t* map = apps.add_app(id);
	assert(map);
	map->name(cfg->short_name);
	map->max_aspaces(cfg->max_aspaces);
	map->max_threads(cfg->max_threads);
	map->max_prio(cfg->max_prio);
	map->fpu(cfg->fpu);
	map->malloc_strategy(cfg->malloc_strategy);

	// preprocess elf - build memory map in apps
	//wrm_logd("app:  prepare elf.\n");
	addr_t entry = 0;
	int rc = elf_foreach((const uint8_t*)elf_addr, elf_sz, process_elf_shdr, process_elf_phdr, (size_t)map, &entry, 0/*dprint_elf*/);
	//wrm_logd("app:  prepare elf done, rc=%d.\n", rc);
	if (rc)
		l4_kdb("ELF file is not valid.");

	// set entry point
	map->entry(entry);

	size_t guard_space = Cfg_page_sz;
	addr_t va = 0x1000000;  // start of user text/data

	// set stack area
	size_t ssz = round_pg_up(cfg->stack_sz);
	va -= ssz + guard_space;
	map->stack(va, ssz);

	// set kip area
	va -= Cfg_page_sz + guard_space;
	L4_fpage_t kip_area = L4_fpage_t::create(va, Cfg_page_sz, Acc_rx);
	map->kip(kip_area);

	// set utcb area for app->max_threads()
	size_t area_sz = get_size(get_log2sz_top_range(cfg->max_threads * Cfg_page_sz));
	va -= area_sz + guard_space;
	va = round_down(va, area_sz);
	L4_fpage_t utcb_area = L4_fpage_t::create(va, area_sz, Acc_rw);
	assert(!utcb_area.is_nil());
	map->utcb(utcb_area);

	// set args
	va -= Cfg_page_sz + guard_space;
	map->args(cfg->name, cfg->args, va);

	return map;
}

static void create_aspace_and_start_app_thread(App_t* app)
{
	L4_thrid_t alpha = l4_utcb()->global_id();
	//wrm_logi("app:  %s:  my=%u, dst=%u, ep=0x%lx, sp=0x%lx.\n", __func__, alpha.number(),
	//	app->owner().number(), app->entry(), app->sp());

	// alloc new aspace
	int rc = wrm_app_alloc_aspace(app->owner());
	if (rc)
		panic("%s:  wrm_app_alloc_aspace() - rc=%d.\n", __func__, rc);

	// alloc thread number
	unsigned newno = 0;
	rc = wrm_app_thr_alloc(app->owner(), &newno, app->utcb(), app->stack_ua(), app->stack_sz());
	if (rc)
		panic("%s:  wrm_app_thr_alloc() - rc=%d.\n", __func__, rc);
	L4_thrid_t newid = L4_thrid_t::create_global(newno);
	assert(newid == app->owner());

	// create application's thread/aspace via ThreadControl
	L4_thrid_t space = newid;  // =dest for aspace creation
	L4_thrid_t sched = alpha;
	L4_thrid_t pager = L4_thrid_t::Nil;
	addr_t     utcb_location = app->get_location(app->utcb().addr(), app->utcb().size(), Acc_rw);
	assert(utcb_location);
	rc = l4_thread_control(newid, space, sched, pager, utcb_location);
	if (rc)
		panic("%s:  l4_thread_control() - rc=%d.", __func__, rc);

	// set thread params via Schedule
	rc = l4_schedule(newid, -1, -1, app->max_prio(), -1);
	if (rc)
		panic("l4_schedule() - rc=%d.", rc);

	// configure application's aspace via SpaceControl
	word_t control = 0;
	L4_thrid_t redirector = L4_thrid_t::Nil;
	rc = l4_space_control(newid, control, app->kip(), app->utcb(), redirector);
	if (rc)
		panic("l4_space_control() - rc=%d.", rc);

	// activate aplication's thread via ThreadControl
	space = newid;
	sched = L4_thrid_t::Nil;
	pager = alpha;
	rc = l4_thread_control(newid, space, sched, pager, -1);
	if (rc)
		panic("%s:  l4_thread_control() - rc=%d.", __func__, rc);

	// start app by sending Thread_start msg
	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.ipc_label(L4_msgtag_t::Thread_start);
	tag.propagated(false);
	tag.untyped(3);
	tag.typed(0);
	utcb->mr[0] = tag.raw();;
	utcb->mr[1] = app->entry();
	utcb->mr[2] = app->sp();
	utcb->mr[3] = app->name();
	rc = l4_send(newid, L4_time_t::Zero);
	if (rc)
		panic("l4_send(start_msg) - rc=%d.\n", rc);
}

//--------------------------------------------------------------------------------------------------
// C API
//--------------------------------------------------------------------------------------------------

extern "C" int wrm_app_create(L4_thrid_t id, const Wrm_app_cfg_t* cfg)
{
	//wrm_logi("app:  create:  %s:  %s:\n", cfg->name, cfg->filename);

	const char* prefix = "ramfs:/";
	int prefix_len = strlen(prefix);
	if (strncmp(cfg->filename, prefix, prefix_len))
		return -1;  // wrong filename

	addr_t elf_addr = 0;
	size_t elf_sz = 0;
	const char* file_name = cfg->filename + prefix_len; // skip fs prefix

	int rc = wrm_ramfs_get_file(file_name, &elf_addr, &elf_sz);
	//wrm_logi("app:  elf_file:  addr=0x%lx, sz=0x%zx.\n", elf_addr, elf_sz);
	if (rc)
	{
		wrm_loge("app:  file=%s not found.\n", file_name);
		return -2;  // file not found
	}

	App_t* app_map = prepare_app_memory_map(cfg, id, elf_addr, elf_sz);
	create_aspace_and_start_app_thread(app_map);
	return 0;
}

extern "C" int wrm_app_add_location(L4_thrid_t id, addr_t rem_va, L4_fpage_t location)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%lx/%u.\n", id.raw(), id.number());
		return -1;
	}
	int rc = app->add_location(rem_va, location);
	if (rc)
	{
		wrm_loge("app=%.4s:  add_location(va=0x%lx, sz=0x%lx, loc=0x%lx) - rc=%d.\n",
			app->name_str(), rem_va, location.size(), location.addr(), rc);
		return -2;
	}
	return 0;
}

extern "C" int wrm_app_get_location(L4_thrid_t id, addr_t rem_addr, size_t sz, acc_t acc, L4_fpage_t* loc_fpage)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("pfault from unknown thread 0x%lx/%u.\n", id.raw(), id.number());
		apps.dump();
		return -1;
	}
	acc_t acc_max = 0;
	addr_t local_addr = app->get_location(rem_addr, sz, acc, &acc_max);
	if (!local_addr)
	{
		//wrm_logw("app=%.4s:  location for remote va=0x%x for acc=%u does not found, thr=%u.\n",
		//	app->name_str(), rem_addr, acc, id.number());
		// try to find without acc for debug
		local_addr = app->get_location(rem_addr, sz, Acc_nil, &acc_max);
		//wrm_logw("result for addr without acc:  local_addr=0x%lx, acc_max=%d.\n", local_addr, acc_max);

		// FIXME:  WA for w4linux - allow map with increasing of access rights
		if (local_addr  &&  !memcmp(app->name_str(), "lx", 2)  &&  acc != Acc_x) // allow rwx or rx
		{
			//wrm_logw("WORKAROUND:  allow map with changing/increasing acc:  %d -> %d.\n", acc_max, acc);
			acc_max = acc;  // change/increase access right
		}
		// ~FIXME
		else
		{
			//apps.dump();
			return -2;
		}
	}
	//wrm_logi("found local page:  addr=0x%x, acc=%u, acc_max=%u.\n", local_addr, acc, acc_max);
	*loc_fpage = L4_fpage_t::create(round_pg_down(local_addr), round_pg_up(sz), acc_max);
	if (loc_fpage->is_nil())
	{
		wrm_loge("faild to do fpage, something going wrong.\n");
		return -3;
	}
	return 0;
}

extern "C" int wrm_app_alloc_aspace(L4_thrid_t id)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%lx/%u.\n", id.raw(), id.number());
		return -1;
	}
	bool ok = app->alloc_aspace();
	if (!ok)
	{
		wrm_loge("app=%.4s:  no free aspaces, max=%u.\n", app->name_str(), app->max_aspaces());
		return -2;
	}
	return 0;
}

extern "C" int wrm_app_free_aspace(L4_thrid_t id)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%lx/%u.\n", id.raw(), id.number());
		return -1;
	}
	bool ok = app->free_aspace();
	if (!ok)
	{
		wrm_loge("app=%.4s:  could not free aspace.\n", app->name_str());
		return -2;
	}
	return 0;
}

extern "C" int wrm_app_thr_alloc(L4_thrid_t id, unsigned* thrno, L4_fpage_t utcb, addr_t stack, size_t stack_sz)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("%s:  unknown app thread=%u.\n", __func__, id.number());
		return -10;
	}
	int rc = app->thread_alloc(utcb, stack, stack_sz);
	if (rc < 0)
	{
		wrm_loge("app=%.4s:  no free threads, max=%u, rc=%d.\n", app->name_str(), app->max_threads(), rc);
		return rc;
	}
	*thrno = rc;
	return 0;
}

extern "C" int wrm_app_thr_state(L4_thrid_t id, unsigned thrno)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("%s:  unknown app thread=%u.\n", __func__, id.number());
		return -10;
	}
	return app->thread_state(thrno);
}

extern "C" int wrm_app_thr_terminate(L4_thrid_t id, unsigned thrno, long term_code)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%lx/%u.\n", id.raw(), id.number());
		return -10;
	}
	return app->thread_terminate(thrno, term_code);
}

extern "C" int wrm_app_thr_free(L4_thrid_t id, unsigned thrno, L4_fpage_t* utcb, addr_t* stack,
                                size_t* stack_sz, long* term_code)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%lx/%u.\n", id.raw(), id.number());
		return -10;
	}
	return app->thread_free(thrno, utcb, stack, stack_sz, term_code);
}

extern "C" int wrm_app_thr_numbers(L4_thrid_t id, unsigned* thrno_begin, unsigned* thrno_end)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%lx/%u.\n", id.raw(), id.number());
		return -1;
	}
	bool ok = app->thread_numbers(thrno_begin, thrno_end);
	if (!ok)
	{
		wrm_loge("could not get thread numbers for app=%.4s.\n", app->name_str());
		return -2;
	}
	return 0;
}

extern "C" int wrm_app_max_prio(L4_thrid_t id, unsigned* max_prio)
{
	App_t* app = apps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%lx/%u.\n", id.raw(), id.number());
		return -1;
	}
	*max_prio = app->max_prio();
	return 0;
}
