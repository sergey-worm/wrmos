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
#include "l4api.h"
#include "list.h"
#include "sys-utils.h" // acc2str
#include "elfloader.h"
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

#include "panic.h"   // FIXME: remove

#include "sparc/processor.h"

// FIXME:  replace me
#ifdef Cfg_arch_sparc
enum { Stack_frame_sz = 96 };
#else
enum { Stack_frame_sz = 0 };
#endif

//--------------------------------------------------------------------------------------------------
//  App-manager
//--------------------------------------------------------------------------------------------------
class App_map_t
{
	struct region_t
	{
		addr_t remote;        // vaddr in remote aspace
		size_t sz;            // size in bytes, may not be aligned
		acc_t  acc;           // access bits
		addr_t local;         // vaddr in local aspace, if 0 --> need allocate before using
		addr_t local_origin;  // if !=0 --> copy data from 'local_origin' to 'local' after allocating

		explicit region_t(addr_t r, size_t s, unsigned a) :
			remote(r), sz(s), acc(a), local(0), local_origin(0)  {}
		explicit region_t(addr_t r, size_t s, unsigned a, addr_t l) :
			remote(r), sz(s), acc(a), local(l), local_origin(0) {}
		addr_t remote_end()  { return remote + sz; }
		bool need_relocate() { return acc & Acc_w; }
		addr_t location()    { return local; } 
	};

	typedef list_t <region_t, 512> regions_t;
	regions_t  _regions;     // memory regions
	L4_thrid_t _owner;       // global thread ID
	addr_t     _entry;       // entry point
	addr_t     _stack_ua;    // stack area address
	size_t     _stack_sz;    // stack area size
	addr_t     _sp;          // initial stack pointer
	L4_fpage_t _kip;         // kip area
	L4_fpage_t _utcb;        // utcb area
	word_t     _name;        // 4 chars

	// thread IDs data
	unsigned _max_threads;   // max threads in app
	uint32_t _busy_thrno;    // busy bits: 0 - free, 1 - busy
	enum { Max_threads_limit = 8 * sizeof(_busy_thrno) };

	// max prio
	uint8_t _max_prio;

	bool _fpu;

public:

	explicit App_map_t(L4_thrid_t id) : _regions(/*dprint_list_app*/), _owner(id), _entry(0), _sp(0)
	{
		//wrm_logd("  App_map_t::ctor:  bid=%x/%d, regs.cap()=%d.\n", _owner.raw(), _owner.number(), _regions.capacity());
	}

	~App_map_t()
	{
		//wrm_logd("  App_map_t::dtor:  id=%x/%d.\n", _owner.raw(), _owner.number());
	}

	App_map_t operator = (const App_map_t& right)
	{
		_regions = right._regions;
		_owner   = right._owner;
		return *this;
	}

	void entry(addr_t v)            { _entry = v; }
	void kip(L4_fpage_t v)          { _kip   = v;      add_vspace(v);   }
	void utcb(L4_fpage_t v)         { _utcb  = v;      add_vspace(v);   }
	void name(const char* v)        { _name = *(word_t*)v; }
	void max_threads(unsigned v)    { _max_threads = v; _busy_thrno = 0x1; assert(v>0 && v<Max_threads_limit); }
	void max_prio(unsigned v)       { _max_prio = v;                    }
	void fpu(bool v)                { _fpu = v;                         }

	void stack(addr_t a, size_t s)
	{
		_stack_ua = a;
		_stack_sz = s;
		_sp = a + s - Stack_frame_sz;
		add_vspace(a, s, Acc_rw);
	}

	L4_thrid_t owner()       const { return _owner; }
	addr_t     entry()       const { return _entry; }
	addr_t     stack_ua()    const { return _stack_ua; }
	addr_t     sp()          const { return _sp;    }
	L4_fpage_t kip()         const { return _kip;   }
	L4_fpage_t utcb()        const { return _utcb;  }
	word_t     name()        const { return _name;  }
	unsigned   max_threads() const { return _max_threads; }
	uint8_t    max_prio()    const { return _max_prio;  }
	bool       fpu()         const { return _fpu;  }

	void dump()
	{
		wrm_logd("  Dump app_map:  id=0x%x/%u, name=%.4s, ep=0x%x, sp=0x%x, regions=%u:\n",
			_owner.raw(), _owner.number(), &_name, _entry, _sp, _regions.size());
		for (regions_t::citer_t it=_regions.cbegin(); it!=_regions.cend(); ++it)
		{
			wrm_logd("    ptr=0x%x, rem=0x%08x, sz=0x%08x, acc=%s, loc=0x%08x, loc_orig=0x%08x\n",
				&*it, it->remote, it->sz, acc2str(it->acc), it->local, it->local_origin);
		}
	}

	// 0 - not three thread IDs
	unsigned alloc_thrno()
	{
		for (unsigned i=0; i<_max_threads; ++i)
		{
			// find free bit
			if (!((_busy_thrno >> i) & 0x1))
			{
				_busy_thrno |= 1 << i;
				return _owner.number() + i;
			}
		}
		return 0;
	}

	bool free_thrno(unsigned thrno)
	{
		if (thrno < _owner.number()  ||  thrno >= (_owner.number() + _max_threads))
		{
			assert(false && "wrong thread num");
			return 0;
		}
		_busy_thrno &= ~(1 << (thrno - _owner.number()));
		return true;
	}

	bool get_thread_numbers(unsigned* thrno_begin, unsigned* thrno_end)
	{
		*thrno_begin = _owner.number();
		*thrno_end   = _owner.number() + _max_threads;
		return true;
	}

private:

	// find memory region that contents [va,sz,acc]
	region_t* find(addr_t rem_va, size_t sz, acc_t acc)
	{
		addr_t rem_end = rem_va + sz;
		for (regions_t::iter_t it=_regions.begin(); it!=_regions.end(); ++it)
			if (rem_va >= it->remote  &&  rem_end <= it->remote_end()  &&  acc == (acc & it->acc))
				return &*it;
		return 0;
	}

	// find free memory for app args
	// free memory may be only once in .data section
	bool find_remote_free_space(addr_t* rem_va, size_t* rem_sz)
	{
		*rem_va = 0;
		*rem_sz = 0;
		for (regions_t::iter_t it=_regions.begin(); it!=_regions.end(); ++it)
		{
			if (!is_aligned(it->sz, Cfg_page_sz))
			{
				assert(it->acc == Acc_rw);
				*rem_va = it->remote_end();
				*rem_sz = align_up(it->remote_end(), Cfg_page_sz) - it->remote_end();
				it->sz += *rem_sz;
				return true;
			}
		}
		return false;
	}

public:

	// add remote aspace
	// 'sz' may not be aligned
	// split_wr - split writable regs, used for adding from ELF parsing
	void add_vspace(addr_t rem_va, size_t sz, acc_t acc, bool split_wr = false)
	{
		//wrm_logd("%s() - rem_va=0x%x, sz=0x%x, acc=%d.\n", __func__, rem_va, sz, acc);
		assert(rem_va);
		assert(sz);
		assert(acc);
		assert(is_aligned(rem_va, Cfg_page_sz));
		region_t* reg = find(rem_va, sz, Acc_nil); // check crossing
		if (reg)
			l4_kdb("Attempt to add region for app, but region cross existing region.");
		if (split_wr && (acc & Acc_w))
		{
			// writable region --> split region to Pg_sz regions to alloacte later
			for (addr_t va=rem_va; va<(rem_va+sz); va+=Cfg_page_sz)
			{
				size_t s = (va+Cfg_page_sz)>(rem_va + sz) ? get_offset(sz, Cfg_page_sz) : Cfg_page_sz;
				assert(s);
				_regions.push_back(region_t(va, s, acc));
			}
		}
		else
		{
			_regions.push_back(region_t(rem_va, sz, acc));
		}
	}

	// add remote aspace
	void add_vspace(L4_fpage_t fp)
	{
		add_vspace(fp.addr(), fp.size(), fp.access());
	}

	// find local_va for remote_va
	// allocate memory from pool if need
	addr_t location(addr_t rem_va, addr_t sz, acc_t acc)
	{
		region_t* reg = find(rem_va, sz, acc);
		if (reg)
		{
			addr_t local = reg->location();
			if (!local)
			{
				// allocate
				L4_fpage_t fp = wrm_mpool_alloc(round_up(reg->sz, Cfg_page_sz));
				assert(!fp.is_nil());
				if (fp.is_nil())
					return 0;
				reg->local = fp.addr();

				// initialize if need
				if (reg->local_origin)
					memcpy((void*)reg->local, (void*)reg->local_origin, reg->sz);
			}

			size_t offset = rem_va - reg->remote;
			assert(reg->location());
			return reg->location() + offset;
		}
		return 0;
	}

	// 'sz' may not be aligned
	// incomming region may contents many existing regions
	void set_location_from_elf(addr_t rem_va, size_t sz, addr_t location)
	{
		assert(is_aligned(rem_va, Cfg_page_sz));
		assert(is_aligned(location, Cfg_page_sz));
		addr_t rem_end = rem_va + sz;
		for (regions_t::iter_t it=_regions.begin(); it!=_regions.end(); ++it)
		{
			if (it->remote >= rem_va  &&  it->remote_end() <= rem_end)
			{
				size_t offset = it->remote - rem_va;
				addr_t local = location + offset;

				if (it->need_relocate()) // writable region, will allocate later
				{
					assert(it->sz <= Cfg_page_sz && "writable regs must be splited to Page_size regs");
					it->local_origin = local;
				}
				else
				{
					it->local = local;
	
				}
			}
		}
	}

	// add app arguments to memory map
	void args(const char* name, const Wrm_app_cfg_t::arg_t* args)
	{
		assert(_sp && "Stack is absent.");

		// TODO:
		// 1. add to link.lds.S vars:  _text_unused_start/end
		//                             _rodata_unused_start/end
		//                             _data_unused_start/end
		// 2. use this information in App_map
		// 3. put args at .rodata (not .data)
		// ~TODO

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

		// check free space in existing regions
		addr_t rem_va = 0;
		addr_t loc_va = 0;
		size_t sz = 0;
		int diff_loc_rem = 0;
		if (find_remote_free_space(&rem_va, &sz))
		{
			loc_va = location(rem_va, sz, Acc_rw);
			diff_loc_rem = loc_va - rem_va;
			assert(loc_va);
		}

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
		addr_t stack_location = location(_sp - stack_params_sz, stack_params_sz, Acc_rw);
		assert(stack_location);
		word_t* sp_location = (word_t*)(stack_location + stack_params_sz);
		//wrm_logd("sp_location=0x%x, sp=0x%x.\n", (unsigned)sp_location, _sp);
		sp_location[-1] = (addr_t)argv - diff_loc_rem;
		sp_location[-2] = argc;
		sp_location[-3] = _fpu;
		_sp -= stack_params_sz;
		//wrm_logd("argc=0x%x, argv=0x%x.\n\n", sp_location[-2], sp_location[-1]);
	}
};

class App_maps_t
{
	typedef list_t <App_map_t, 4> apps_t;
	apps_t _apps;

public:

	App_maps_t() : _apps(/*dprint_list_apps*/) {}

	void dump(const char* mark = "")
	{
		wrm_logd("Dump app_maps (%u)%s:\n", _apps.size(), mark);
		for (apps_t::iter_t it=_apps.begin(); it!=_apps.end(); ++it)
			it->dump();
	}

	App_map_t* find(L4_thrid_t id)
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

	App_map_t* add_app(L4_thrid_t id)
	{
		_apps.push_back(App_map_t(id));
		return &_apps.back();
	}
};
static App_maps_t app_maps;

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
static void process_elf_shdr(size_t label, uintptr_t addr, size_t sz, unsigned access,
                      const char* name, int is_progbits)
{
	//wrm_logd("shdr:  label=0x%x, addr=0x%08x, sz=0x%08x, acc=%s, progbits=%d, name=%s.\n",
	//	label, addr, sz, acc2str(access), is_progbits, name);

	if (is_progbits)
		((App_map_t*)label)->add_vspace(addr, sz, access, true);
}

// callback func for elf_foreach()
static void process_elf_phdr(size_t label, uintptr_t vaddr, uintptr_t paddr, size_t sz,
                      uintptr_t location, int is_load)
{
	//wrm_logd("phdr:  label=0x%x, va=0x%x, pa=0x%x, sz=0x%x, location=0x%x, load=%d.\n",
	//	label, vaddr, paddr, sz, location, is_load);

	if (is_load)
		((App_map_t*)label)->set_location_from_elf(vaddr, sz, location);
}

static App_map_t* prepare_app_memory_map(const Wrm_app_cfg_t* cfg, L4_thrid_t id, addr_t elf_addr, size_t elf_sz)
{
	App_map_t* map = app_maps.add_app(id);
	assert(map);
	map->name(cfg->short_name);
	map->max_threads(cfg->max_threads);
	map->max_prio(cfg->max_prio);
	map->fpu(cfg->fpu);

	// preprocess elf - build memory map in app_maps
	addr_t entry = 0;
	int res = elf_foreach((const uint8_t*)elf_addr, elf_sz, process_elf_shdr, process_elf_phdr, (size_t)map, &entry, 0/*dprint_elf*/);
	if (res)
		l4_kdb("ELF file is not valid.");

	// set entry point
	map->entry(entry);

	size_t guard_space = Cfg_page_sz;
	addr_t va = 0x1000000;  // start of user text/data

	// set stack area
	va -= Cfg_page_sz + guard_space;
	map->stack(va, Cfg_page_sz);

	//set kip area
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
	map->args(cfg->name, cfg->args);

	return map;
}

static void create_aspace_and_start_app_thread(App_map_t* app)
{
	L4_thrid_t my = l4_utcb()->global_id();
	//wrm_logi("%s:  my=0x%x/%d, dst=0x%x/%d, ep=0x%x, sp=0x%x.\n", __func__,
	//	my.raw(), my.number(), app->owner().raw(), app->owner().number(), app->entry(), app->sp());

	// create application's thread/aspace via ThreadControl

	L4_thrid_t space = app->owner();  // =dest for aspace creation
	L4_thrid_t sched = my;
	L4_thrid_t pager = L4_thrid_t::Nil;
	addr_t     utcb_location = app->location(app->utcb().addr(), app->utcb().size(), Acc_rw);
	assert(utcb_location);

	int rc = l4_thread_control(app->owner(), space, sched, pager, utcb_location);
	//wrm_logi("l4_thread_control() - rc=%u.\n", rc);
	if (rc)
		panic("l4_thread_control() - failed, rc=%u.", rc);

	// set thread params via Schedule

	rc = l4_schedule(app->owner(), -1, -1, app->max_prio(), -1);
	//wrm_logi("l4_schedule() - rc=%u.\n", rc);
	if (rc)
		panic("l4_schedule() - failed, rc=%u.", rc);


	// configure application's aspace via SpaceControl

	word_t control = 0; // FIXME
	L4_thrid_t redirector = L4_thrid_t::Nil;

	rc = l4_space_control(app->owner(), control, app->kip(), app->utcb(), redirector);
	//wrm_logi("l4_space_control() - rc=%u.\n", rc);
	if (rc)
		panic("l4_space_control() - failed, rc=%u.", rc);

	// activate aplication's thread via ThreadControl

	space = app->owner();
	sched = L4_thrid_t::Nil;
	pager = my;

	rc = l4_thread_control(app->owner(), space, sched, pager, -1);
	//wrm_logi("l4_thread_control() - rc=%u.\n", rc);
	if (rc)
		panic("l4_thread_control() - failed, rc=%u.", rc);

	// start app by sending Thread_start msg

	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.ipc_label(L4_msgtag_t::Thread_start);
	tag.untyped(3);
	tag.typed(0);
	utcb->mr[0] = tag.raw();;
	utcb->mr[1] = app->entry();
	utcb->mr[2] = app->sp();
	utcb->mr[3] = app->name();

	rc = l4_send(app->owner(), L4_time_t::Zero);
	//wrm_logi("l4_send(start_msg) - rc=%u.\n", rc);
	if (rc)
		panic("l4_send(start_msg) - rc=%u.\n", rc);
}

//--------------------------------------------------------------------------------------------------
// C API
//--------------------------------------------------------------------------------------------------

extern "C" int wrm_app_create(L4_thrid_t id, const Wrm_app_cfg_t* cfg)
{
	//wrm_logi("start app:  %s:  %s:\n", cfg->name, cfg->filename);

	const char* prefix = "ramfs:/";
	int prefix_len = strlen(prefix);
	if (strncmp(cfg->filename, prefix, prefix_len))
		return -1;  // wrong filename

	addr_t elf_addr = 0;
	size_t elf_sz = 0;
	const char* file_name = cfg->filename + prefix_len; // skip fs prefix

	int rc = wrm_ramfs_get_file(file_name, &elf_addr, &elf_sz);
	//wrm_logi("elf_file:  addr=0x%x, sz=0x%x.\n", elf_addr, elf_sz);
	if (rc)
		return -2;  // file not found

	App_map_t* app_map = prepare_app_memory_map(cfg, id, elf_addr, elf_sz);

	//app_maps.dump();

	create_aspace_and_start_app_thread(app_map);

	return 0;
}

extern "C" int wrm_app_location(L4_thrid_t id, addr_t rem_addr, size_t sz, acc_t acc, L4_fpage_t* loc_fpage)
{
	App_map_t* app = app_maps.find(id);
	if (!app)
	{
		wrm_loge("pfault from unknown thread 0x%x/%u.\n", id.raw(), id.number());
		app_maps.dump();
		return -1;
	}

	addr_t local_addr = app->location(rem_addr, sz, acc);
	if (!local_addr)
	{
		word_t name = app->name();
		wrm_loge("location for remote va=0x%x does not found, thr=%u, app=%.4s.\n",
			rem_addr, id.number(), &name);
		//app_maps.dump();
		return -2;
	}

	//wrm_logi("found local page:  addr=0x%x.\n", local_addr);

	*loc_fpage = L4_fpage_t::create(round_down(local_addr, Cfg_page_sz), Cfg_page_sz, acc);

	if (loc_fpage->is_nil())
	{
		wrm_loge("faild to do fpage, something going wrong.\n");
		return -3;
	}

	return 0;
}

extern "C" int wrm_app_alloc_thrno(L4_thrid_t id, unsigned* thrno)
{
	App_map_t* app = app_maps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%x/%u.\n", id.raw(), id.number());
		return -1;
	}

	*thrno = app->alloc_thrno();
	if (!*thrno)
	{
		wrm_loge("no free thread numbers.\n");
		return -2;
	}

	return 0;
}

extern "C" int wrm_app_free_thrno(L4_thrid_t id, unsigned thrno)
{
	App_map_t* app = app_maps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%x/%u.\n", id.raw(), id.number());
		return -1;
	}

	bool ok = app->free_thrno(thrno);
	if (!ok)
	{
		wrm_loge("could not free thrno, wrong thread number.\n");
		return -2;
	}

	return 0;
}

extern "C" int wrm_app_get_thr_numbers(L4_thrid_t id, unsigned* thrno_begin, unsigned* thrno_end)
{
	App_map_t* app = app_maps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%x/%u.\n", id.raw(), id.number());
		return -1;
	}

	bool ok = app->get_thread_numbers(thrno_begin, thrno_end);
	if (!ok)
	{
		word_t name = app->name();
		wrm_loge("could not get thread numbers for app=%.4s.\n", &name);
		return -2;
	}

	return 0;
}

extern "C" int wrm_app_max_prio(L4_thrid_t id, unsigned* max_prio)
{
	App_map_t* app = app_maps.find(id);
	if (!app)
	{
		wrm_loge("unknown app/thread 0x%x/%u.\n", id.raw(), id.number());
		return -1;
	}
	*max_prio = app->max_prio();
	return 0;
}
