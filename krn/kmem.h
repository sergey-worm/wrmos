//##################################################################################################
//
//  Types to work with adress spaces and kernel memory.
//
//##################################################################################################

#ifndef KMEM_H
#define KMEM_H

#include "krn-config.h"
#include "sys_types.h"
#include "sys_utils.h"
#include <stdarg.h>
#include "wlibc_panic.h"
#include "ptable.h"
#include "kconfig.h"
#include "list.h"
#include "wlibc_assert.h"

/*
static void dprint_list(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void dprint_list(const char* fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	printk("list:  %s", buf);
	va_end(args);
}
*/

//--------------------------------------------------------------------------------------------------
template <typename Addr, typename Size>
class Region
{
public:
	Addr start;
	Size sz;
	const char* name;
	Region() : start(0), sz(0) {}
	Region(Addr a, Size s, const char* n) : start(a), sz(s), name(n) {}
	inline void set(Addr a, Size s, const char* n) { start = a;  sz = s;  name = n; }
	inline Addr end() { return start + sz; }
	inline bool inside(Addr a, Size s) { return (a >= start) && (a+s <= end()); }
};

//--------------------------------------------------------------------------------------------------
template <typename Addr, typename Size, typename Type, size_t N = 0>
class Address_space_t
{
	struct Desc
	{
		Addr addr;
		Size sz;
		Type type;
		explicit Desc(Addr a, Size s, Type t) : addr(a), sz(s), type(t) {}
		Addr end() { return addr + sz; }
	};

	typedef list_t <Desc, N> Dlist;
	Dlist dlist;  // descriptor list

public:

	// adding mode
	enum Mode
	{
		Combine     = 1, // combine contiguous regions
		NotCombine  = 2
	};

	typedef typename Dlist::iter_t iter_t;
	typedef typename Dlist::citer_t citer_t;
	iter_t inline begin() { return dlist.begin(); }
	iter_t inline end() { return dlist.end(); }
	citer_t inline cbegin() const { return dlist.begin(); }
	citer_t inline cend() const { return dlist.end(); }
	size_t inline size() const { return dlist.size(); }
	size_t inline capacity() const { return dlist.capacity(); }

private:

	// set new element before pos
	// split or combine regions if need
	void add(iter_t pos, Addr addr, Size sz, const Type& type, Mode mode)
	{
		wassert((pos == dlist.end()     ||  addr + sz <= pos->addr)  &&  "cross next region");
		wassert((pos == dlist.begin()                                ||  // pos is first  --> no prev
		        dlist.empty()                                        ||  // list is empty --> no prev
		        (pos == dlist.end()  &&  addr >= dlist.back().end()) ||  // !empty && pos==end() && back-doesn't-cross
		        (pos != dlist.end()  &&  addr >= (pos-1)->end())) &&     // !empty && pos!=end() && prev-doesn't-cross
		       "cross prev region");

		if (mode == NotCombine) // just insert
		{
			if (pos != dlist.end())
				dlist.insert_before(pos, Desc(addr, sz, type));
			else
				dlist.push_back(Desc(addr, sz, type));
		}
		else // Combine
		{
			if (pos != dlist.end())
			{
				// check contiguous with 'pos' and prev elements
				bool pos_cont = ((addr + sz) == pos->addr)  &&  (type == pos->type);
				iter_t prev = pos - 1;
				bool prev_cont = (prev != dlist.end())  &&  ((prev->addr + prev->sz) == addr)  &&  (prev->type == type);

				if (pos_cont && prev_cont)
				{
					prev->sz += sz + pos->sz;
					dlist.erase(pos);
				}
				else if (pos_cont)
				{
					pos->addr -= sz;
					pos->sz += sz;
				}
				else if (prev_cont)
				{
					prev->sz += sz;
				}
				else
					dlist.insert_before(pos, Desc(addr, sz, type));
			}
			else // == end()
			{
				// check contiguous with last element
				iter_t last = dlist.last();
				if ((last != dlist.end())  &&  ((last->addr + last->sz) == addr)  &&  (last->type == type))
					last->sz += sz;
				else
					dlist.push_back(Desc(addr, sz, type));
			}
		}
	}

	// remove peace or whole element by 'pos'
	// return the next element
	iter_t remove(iter_t pos, Addr addr, Size sz)
	{
		wassert(pos != dlist.end());
		wassert((pos->addr <= addr)  &&  (pos->addr + pos->sz >= addr + sz)  &&  "out of region");

		iter_t ret = pos;
		Addr eend = addr + sz;  // eend cause end() is busy =)
		Addr pos_end = pos->addr + pos->sz;

		if (pos->addr == addr  &&  pos_end == eend)  // whole element
		{
			ret = dlist.erase(pos);
		}
		else if (pos->addr == addr)  // peace at the beginning
		{
			pos->addr += sz;
			pos->sz -= sz;
		}
		else if (pos_end == eend)  // peace at the end
		{
			pos->sz -= sz;
			++ret;
		}
		else // peace in the middle
		{
			dlist.insert_before(pos, Desc(pos->addr, addr - pos->addr, pos->type));
			pos->addr = eend;
			pos->sz = pos_end - eend;
		}
		return ret;
	}

public:

	Address_space_t() : dlist(/*dprint_list*/) {}

	// add new region, region should not cross existing ones
	void add(Addr addr, Size sz, Type type, Mode mode)
	{
		// find position of next-busy-region after addr
		iter_t it;
		for (it=dlist.begin(); it!=dlist.end(); ++it)
			if (it->addr >= addr)
				break;

		if (it != dlist.end()  &&  addr+sz > it->addr)
		{
			panic("Added region crosses the existing regions.");
		}

		add(it, addr, sz, type, mode);
	}

	// insert region, region may be new or be inside the existing ones
	// return result type of region
	Type insert(Addr addr, Size sz, Type type, Mode mode)
	{
		// find position of next-busy-region after [addr,sz]
		// or region that contents [addr,sz]
		iter_t it;
		for (it=dlist.begin(); it!=dlist.end(); ++it)
		{
			if (addr >= it->addr  &&  addr+sz <= it->end())  // contents
			{
				type |= it->type;                            // extend type by OR
				it = remove(it, addr, sz);                   // remove to add with new type
				break;
			}
			if (it->addr >= addr)                            // next busy region
			{
				if (addr+sz > it->addr)
					panic("Inserted region partially crosses the existing regions.");
				break;
			}
		}

		add(it, addr, sz, type, mode);
		return type;
	}

	// return address if success or 0 if fail
	Addr find_free(Size sz, Addr align, Addr space_start, Size space_sz)
	{
		// find position of next-busy-region after space_start
		iter_t it = dlist.begin();
		for ( ; it!=dlist.end(); ++it)
			if (it->addr >= space_start)
				break;

		// find not busy space
		Addr space_end = space_start + space_sz;
		Addr cur = round_up(space_start, align);
		for ( ; it!=dlist.end(); ++it)
		{
			if (cur >= space_end  ||  cur + sz > space_end)
			{
				cur = 0; // not found
				break;
			}
			if (cur + sz <= it->addr)
				break; // found
			cur = round_up(it->addr + it->sz, align);
		}

		// check space_end
		if (cur  &&  ((cur + sz) > space_end))
			cur = 0; // not found

		//wassert(cur && "could not find free region");

		return cur;
	}

	// return address if success or 0 if fail
	Addr alloc(Size sz, Addr align, const Type& type, Mode mode, Addr space_start, Size space_sz)
	{
		Addr addr = find_free(sz, align, space_start, space_sz);
		if (addr)
		{
			// now 'it' point to next-busy-region or to dlist.end();
			add(addr, sz, type, mode);
		}
		return addr;
	}

	// find region that started from 'addr'
	iter_t find(Addr addr)
	{
		iter_t it = dlist.begin();
		for ( ; it!=dlist.end(); ++it)
		{
			if (addr == it->addr)
				break;             // found
			if (it->addr > addr)
			{
				it = dlist.end();  // not found
				break;
			}
		}
		return it;
	}

	// find region that contain peace [addr, sz]
	iter_t find(Addr addr, Size sz)
	{
		iter_t it = dlist.begin();
		for ( ; it!=dlist.end(); ++it)
		{
			Addr e = addr + sz;
			if (addr >= it->addr  &&  e <= it->end())
				break;             // found
			if (it->addr >= e)
			{
				it = dlist.end();  // not found
				break;
			}
		}
		return it;
	}

	// free peace or whole region
	void free(Addr addr, Size sz)
	{
		iter_t it = find(addr, sz);
		if (it != dlist.end())
			remove(it, addr, sz);
	}

	// free whole region
	void free(iter_t pos)
	{
		if (pos != dlist.end())
			dlist.erase(pos);
	}

	// set type of whole or the peace of 'pos' element
	// split or combine regions if need
	void set_type(iter_t pos, Addr addr, Size sz, const Type& type, Mode mode)
	{
		wassert(pos != dlist.end());
		wassert((pos->addr <= addr)  &&  (pos->addr + pos->sz >= addr + sz));
		wassert(pos->type != type);

		// NOTE:  may to optimize to preserve add/del new item
		/*
		if (pos->sz == sz)  // whole region
		{
			pos->type = type;
			// TODO:  combine if need
		}
		else // common case
		*/
		{
			iter_t it = remove(pos, addr, sz);
			add(it, addr, sz, type, mode);
		}
	}
};

#include "l4_types.h"
#include "acc2mmuacc.h"

//--------------------------------------------------------------------------------------------------
// Virtual address space.
class Aspace
{
public:

	struct Type
	{
		kacc_t acc;
		bool cached;
		explicit Type(kacc_t a, bool c) : acc(a), cached(c) {}
		inline bool operator == (const Type& t) const { return acc==t.acc && cached==t.cached; }
		inline bool operator != (const Type& t) const { return !operator==(t); }
		inline Type operator |= (const Type& t) { acc = (kacc_t)(acc | t.acc);  return *this; }

		const char* str() const
		{
			static char buf[10];
			buf[0] = cached       ? 'c' : '-';
			buf[1] = 'k';
			buf[2] = acc & Acc_kr ? 'r' : '-';
			buf[3] = acc & Acc_kw ? 'w' : '-';
			buf[4] = acc & Acc_kx ? 'x' : '-';
			buf[5] = 'u';
			buf[6] = acc & Acc_ur ? 'r' : '-';
			buf[7] = acc & Acc_uw ? 'w' : '-';
			buf[8] = acc & Acc_ux ? 'x' : '-';
			buf[9] = '\0';
			return buf;
		}
	};

private:

	typedef Address_space_t <addr_t, size_t, Type, 512> aspace_t;
	typedef Region <addr_t, size_t> vregion_t;
	struct ranges_t
	{
		vregion_t kmem;              // kernel mem space, map phys:0xf0000000
		vregion_t kutcbs;            // kernel aspace for utcbs
		vregion_t kstacks;           // kernel aspace for kstacks
		vregion_t kheap;             // kernel aspace for heap
		vregion_t kio;               // kernel io space
		vregion_t usr;               // user space (mem and io)
	};

	// common data for all addr spaces
	static aspace_t _kspace;         // kernel virt space, common for all contexts
	static ranges_t _ranges;         // virtual space regions
	static psize_t  _diff_kva_kpa;   // difference between kernel virt and phys addresses
	static unsigned _cur_mmu_ctxid;  // optimisation:  don't set mm_ctx if switching to ctx=0 (idle)

	// individual addr space data
	Pgtab    _pgtab;                 // root page table
	aspace_t _uspace;                // user virt space
	unsigned _id;                    // id

	enum
	{
		Kio_uart_pg_pa = round_pg_down(Cfg_krn_uart_paddr),
		Kio_uart_pg_sz = round_pg_up(Cfg_krn_uart_paddr + Cfg_krn_uart_sz) - Kio_uart_pg_pa,

		Kio_intc_pg_pa = round_pg_down(Cfg_krn_intc_paddr),
		Kio_intc_pg_sz = round_pg_up(Cfg_krn_intc_paddr + Cfg_krn_intc_sz) - Kio_intc_pg_pa,

		Kio_timer_pg_pa = round_pg_down(Cfg_krn_timer_paddr),
		Kio_timer_pg_sz = round_pg_up(Cfg_krn_timer_paddr + Cfg_krn_timer_sz) - Kio_timer_pg_pa
	};

private:

	static void setup_kdiff()
	{
		extern int _diff_va_pa;                // linker var
		_diff_kva_kpa = (size_t)&_diff_va_pa;  // difference between virt and phys addresses
	}

	// WA:  noinline, if this function is inline, for arm-linux-gcc something going wrong
	static void __attribute__((noinline)) setup_kernel_map()
	{
		// linker vars
		extern int _kip_start;
		extern int _kip_end;
		extern int _text_start;
		extern int _text_end;
		extern int _user_text_start;
		extern int _user_text_end;
		extern int _rodata_start;
		extern int _rodata_end;
		extern int _data_start;
		extern int _data_end;
		extern int _bss_start;
		extern int _bss_end;

		// virt regions
		addr_t va_kip    = (addr_t)&_kip_start;
		size_t sz_kip    = (addr_t)&_kip_end - (addr_t)&_kip_start;
		addr_t va_text   = (addr_t)&_text_start;
		size_t sz_text   = (addr_t)&_text_end - (addr_t)&_text_start;
		addr_t va_utext  = (addr_t)&_user_text_start;
		size_t sz_utext  = (addr_t)&_user_text_end - (addr_t)&_user_text_start;
		addr_t va_rodata = (addr_t)&_rodata_start;
		size_t sz_rodata = (addr_t)&_rodata_end - (addr_t)&_rodata_start;
		addr_t va_data   = (addr_t)&_data_start;
		size_t sz_data   = (addr_t)&_data_end - (addr_t)&_data_start;
		addr_t va_bss    = (addr_t)&_bss_start;
		size_t sz_bss    = (addr_t)&_bss_end - (addr_t)&_bss_start;

		sz_kip    = round_pg_up(sz_kip);
		sz_text   = round_pg_up(sz_text);
		sz_utext  = round_pg_up(sz_utext);
		sz_rodata = round_pg_up(sz_rodata);
		sz_data   = round_pg_up(sz_data);
		sz_bss    = round_pg_up(sz_bss);

		// map kernel memory
		kmap(va_kip,    kmem_paddr(va_kip,    sz_kip),    sz_kip,    Acc_kkip);
		kmap(va_text,   kmem_paddr(va_text,   sz_text),   sz_text,   Acc_ktext);
		kmap(va_utext,  kmem_paddr(va_utext,  sz_utext),  sz_utext,  Acc_kutext);
		kmap(va_rodata, kmem_paddr(va_rodata, sz_rodata), sz_rodata, Acc_krodata);
		kmap(va_data,   kmem_paddr(va_data,   sz_data),   sz_data,   Acc_kdata);
		kmap(va_bss,    kmem_paddr(va_bss,    sz_bss),    sz_bss,    Acc_kdata);

		// map kernel io space
		// if lowest bit 1 - don't map
		if (!(Cfg_krn_uart_paddr & 1))
			kmap(kuart_pg_va(),  round_pg_down(Cfg_krn_uart_paddr),  Kio_uart_pg_sz,  Acc_kio, NotCachable);
		if (!(Cfg_krn_intc_paddr & 1))
			kmap(kintc_pg_va(),  round_pg_down(Cfg_krn_intc_paddr),  Kio_intc_pg_sz,  Acc_kio, NotCachable);
		if (!(Cfg_krn_timer_paddr & 1))
			kmap(ktimer_pg_va(), round_pg_down(Cfg_krn_timer_paddr), Kio_timer_pg_sz, Acc_kio, NotCachable);
	}

public:

	// init kernel address space
	static void kinit()
	{
		printk("Vspace::%s:  hello.\n", __func__);

		if (_ranges.kmem.start || _ranges.kmem.sz)
			panic("Vspace already inited.");

		enum
		{
			Guard = Cfg_page_sz,  // guard page between regions
			Kstack_max = 2 * Cfg_page_sz,

			Usr_va     = 0x10000000,                         // all vspace from 0x10000000 till Cfg_krn_vaddr
			Usr_sz     = Cfg_krn_vaddr - Usr_va - Guard,     //
			Kmem_va    = Cfg_krn_vaddr,                      //
			Kmem_sz    = 0x400000,                           // 4 MB, may be bigger if need
			Kio_va     = Kmem_va + Kmem_sz + Guard,          //
			Kio_sz     = Kio_uart_pg_sz + Kio_intc_pg_sz + Kio_timer_pg_sz + 2*Guard,
			Kutcbs_va  = Kio_va + Kio_sz + Guard,            // area to map utcb in kernel space
			Kutcbs_sz  = Kcfg::Threads_max * Cfg_page_sz,    //
			Kstacks_va = Kutcbs_va + Kutcbs_sz + Guard,      // kernel stacks area
			Kstacks_sz = Kcfg::Threads_max * (Kstack_max + Guard),  //
			Kheap_va   = Kstacks_va + Kstacks_sz + Guard,    // kernel heap area
			Kheap_sz   = 0x100000                            //
		};

		_ranges.usr.set(Usr_va, Usr_sz, "usr");
		_ranges.kmem.set(Kmem_va, Kmem_sz, "kmem");
		_ranges.kio.set(Kio_va, Kio_sz, "kio");
		_ranges.kutcbs.set(Kutcbs_va, Kutcbs_sz, "kutcbs");
		_ranges.kstacks.set(Kstacks_va, Kstacks_sz, "kstacks");
		_ranges.kheap.set(Kheap_va, Kheap_sz, "kheap");

		setup_kdiff();
		Pgtab::kinit();
		setup_kernel_map();
	}

	static addr_t kuart_pg_va()  { return _ranges.kio.start; }
	static addr_t kintc_pg_va()  { return kuart_pg_va() + Kio_uart_pg_sz + Cfg_page_sz; } // + 1 guard pg
	static addr_t ktimer_pg_va() { return kintc_pg_va() + Kio_intc_pg_sz + Cfg_page_sz; } // + 1 guard pg

	static paddr_t kmem_paddr(addr_t va, size_t sz)
	{
		wassert(_ranges.kmem.inside(va, sz));
		(void)sz;
		return va - _diff_kva_kpa;
	}

	static paddr_t kmem_paddr(void* va, size_t sz) { return kmem_paddr((addr_t)va, sz); }

	static addr_t kmem_vaddr(paddr_t pa, size_t sz)
	{
		addr_t va = pa + _diff_kva_kpa;
		wassert(_ranges.kmem.inside(va, sz));
		(void)sz;
		return va;
	}

	static void kdump()
	{
		printk("Dump kernel vspace:\n");
		printk("  kernel (%zu/%zu):\n", _kspace.size(), _kspace.capacity());
		for (aspace_t::citer_t it=_kspace.cbegin(); it!=_kspace.cend(); ++it)
			printk("  [%08lx - %08lx)  sz=0x%08zx,  %s.\n", it->addr, it->addr + it->sz, it->sz, it->type.str());
	}

	void dump()
	{
		printk("Dump vspace, id=%u:\n", _id);
		printk("  kernel (%zu/%zu):\n", _kspace.size(), _kspace.capacity());
		for (aspace_t::citer_t it=_kspace.cbegin(); it!=_kspace.cend(); ++it)
			printk("  [%08lx - %08lx)  sz=0x%08zx,  %s.\n", it->addr, it->addr + it->sz, it->sz, it->type.str());

		printk("  user (%zu/%zu):\n", _uspace.size(), _uspace.capacity());
		for (aspace_t::citer_t it=_uspace.cbegin(); it!=_uspace.cend(); ++it)
			printk("  [%08lx - %08lx)  sz=0x%08zx,  %s.\n", it->addr, it->addr + it->sz, it->sz, it->type.str());

		_pgtab.dump();
	}

	explicit Aspace(unsigned id) : _pgtab(id)
	{
		printk("Aspace::%s:  hello, id=%u.\n", __func__, id);
		_id = id;
	}

	~Aspace()
	{
		printk("Aspace::%s:  by, id=%u.\n", __func__, _id);
	}

	inline void set_current(bool force = false)
	{
		// optimize switching from/to idle
		bool need_set = _id != 0  &&  _id != _cur_mmu_ctxid; // not idle and not cur
		if (force || need_set)
		{
			_pgtab.set_current();
			_cur_mmu_ctxid = _id;
		}
	};

	inline bool is_cur_aspace()
	{
		return _cur_mmu_ctxid == _id;
	}

private:

	// convert virtual memory access to user-access-permission
	static inline unsigned acc2uacc(kacc_t acc)
	{
		return acc & Acc_urwx;
	}

public:

	// convert user-access-permission to virtual memory access
	static inline kacc_t uacc2acc(unsigned uacc)
	{
		return (kacc_t)(uacc & Acc_urwx);
	}

private:

	static void kmap(addr_t va, paddr_t pa, size_t sz, kacc_t acc, unsigned cachable = Cachable)
	{
		Type type(acc, cachable==Cachable);
		printk("Aspace::%s:  va=0x%08lx, pa=0x%08llx, sz=0x%08zx, acc=%s.\n", __func__,
			va, (long long)pa, sz, type.str());
		wassert(is_aligned(va, Cfg_page_sz));
		wassert(is_aligned(pa, Cfg_page_sz));
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(va);
		wassert(pa);
		wassert(sz);

		// TODO:  check that region is Kernel

		_kspace.add(va, sz, type, aspace_t::Combine);
		Pgtab::kmap(va, pa, sz, acc2mmuacc(acc), cachable);
	}

public:

	static void kunmap(addr_t va, size_t sz)
	{
		printk("Aspace::%s:  va=0x%08lx, sz=0x%08zx.\n", __func__, va, sz);
		wassert(is_aligned(va, Cfg_page_sz));
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(va);
		wassert(sz);

		// TODO:  check that region is Kernel

		_kspace.free(va, sz);
		Pgtab::kunmap(va, sz);
	}

public:

	static addr_t kmap_utcb(paddr_t pa)
	{
		printk("Aspace::%s:  pa=0x%llx.\n", __func__, (long long)pa);
		wassert(is_aligned(pa, Cfg_page_sz));
		wassert(pa);

		addr_t kva = _kspace.alloc(Cfg_page_sz, Cfg_page_sz, Type(Acc_kutcb, Cachable), aspace_t::NotCombine,
		                           _ranges.kutcbs.start, _ranges.kutcbs.sz);
		if (!kva)
			kdump();
		wassert(kva && "no kernel memory");
		printk("Aspace::%s:  pa=0x%llx --> kutcb=%lx.\n", __func__, (long long)pa, kva);
		if (kva)
			Pgtab::kmap(kva, pa, Cfg_page_sz, acc2mmuacc(Acc_kutcb), Cachable);
		return kva;
	}

	static void kunmap_utcb(addr_t va)
	{
		printk("Aspace::%s:  va=0x%lx.\n", __func__, va);
		kunmap(va, Cfg_page_sz);
	}

	static addr_t kmap_kstack(paddr_t pa, size_t sz)
	{
		printk("Aspace::%s:  pa=0x%llx, sz=0x%zx.\n", __func__, (long long)pa, sz);
		wassert(is_aligned(pa, Cfg_page_sz));
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(sz <= 0x2000); // FIXME
		wassert(sz);
		wassert(pa);

		printk("Aspace::%s:  region:  0x%lx .. 0x%lx.\n", __func__,
			_ranges.kstacks.start, _ranges.kstacks.start + _ranges.kstacks.sz);

		addr_t kva = _kspace.alloc(sz + 0x1000/*Guard FIXME*/, Cfg_page_sz, Type(Acc_kdata, Cachable), aspace_t::NotCombine,
		                           _ranges.kstacks.start, _ranges.kstacks.sz);
		if (!kva)
			kdump();
		wassert(kva && "no kernel memory");
		printk("Aspace::%s:  pa=0x%llx --> kstack=%lx.\n", __func__, (long long)pa, kva);
		if (kva)
			Pgtab::kmap(kva, pa, Cfg_page_sz, acc2mmuacc(Acc_kdata), Cachable);
		return kva;
	}

	static addr_t alloc_kspace(size_t sz)
	{
		printk("Aspace::%s:  sz=0x%zx.\n", __func__, sz);
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(sz);

		printk("Aspace::%s:  region:  0x%lx .. 0x%lx.\n", __func__,
			_ranges.kheap.start, _ranges.kheap.start + _ranges.kheap.sz);

		addr_t va = _kspace.alloc(sz, Cfg_page_sz, Type(Acc_kdata, Cachable), aspace_t::NotCombine,
		                          _ranges.kheap.start, _ranges.kheap.sz);
		if (!va)
			kdump();
		wassert(va && "no kernel memory");
		printk("Aspace::%s:  va=%lx.\n", __func__, va);
		return va;
	}

	static void free_kspace(addr_t va, size_t sz)
	{
		printk("Aspace::%s:  va=0x%lx, sz=0x%zx.\n", __func__, va, sz);
		wassert(is_aligned(va, Cfg_page_sz));
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(va);
		wassert(sz);

		printk("Aspace::%s:  region:  0x%lx .. 0x%lx.\n", __func__,
			_ranges.kheap.start, _ranges.kheap.start + _ranges.kheap.sz);

		_kspace.free(va, sz);
	}

	addr_t find_free_uspace(size_t sz, size_t align)
	{
		printk("Aspace::%s:  sz=0x%zx, align=0x%zx.\n", __func__, sz, align);
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(is_aligned(align, Cfg_page_sz));
		wassert(sz);
		wassert(align);

		addr_t uva = _uspace.find_free(sz, align, _ranges.usr.start, _ranges.usr.sz);
		printk("Aspace::%s:  sz=0x%zx, uva=0x%lx.\n", __func__, sz, uva);
		return uva;
	}

	void map(addr_t va, paddr_t pa, size_t sz, kacc_t acc, unsigned cachable = Cachable)
	{
		//printk("Aspace::%s:  id=%u:  va=%x, pa=%x, sz=0x%x, acc=%s.\n", __func__, _id, va, (int)pa, sz, Type(acc).str());
		wassert(is_aligned(va, Cfg_page_sz));
		wassert(is_aligned(pa, Cfg_page_sz));
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(va);
		wassert(pa);
		wassert(sz);

		// TODO:  check that region is User

#ifdef Cfg_arch_sparc
		if (acc == Acc_uw)
			acc = Acc_urw;  // for sparc 'write' access always allows 'read' access
#endif

		Type type = _uspace.insert(va, sz, Type(acc, cachable == Cachable), aspace_t::Combine);
		_pgtab.map(va, pa, sz, acc2mmuacc(type.acc), cachable);
	}

	// va may be 0
	void unmap(addr_t va, size_t sz)
	{
		//printk("Aspace::%s:  id=%u:  va=%x, sz=0x%x.\n", __func__, _id, va, sz);
		wassert(is_aligned(va, Cfg_page_sz));
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(sz);

		// TODO:  check that region is User

		_uspace.free(va, sz);
		_pgtab.unmap(va, sz);
	}

	paddr_t walk(addr_t va, size_t sz)
	{
		//printk("Aspace::%s:  id=%u:  va=%x, sz=0x%x.\n", __func__, _id, va, sz);
		wassert(is_aligned(va, Cfg_page_sz));
		wassert(is_aligned(sz, Cfg_page_sz));
		wassert(va);
		wassert(sz);

		// TODO:  check that region is User

		return _pgtab.walk(va, sz);
	}

	bool is_inside_acc(L4_fpage_t fp)
	{
		aspace_t::iter_t it = _uspace.find(fp.addr(), fp.size());
		if (it == _uspace.end())
			return false;

		// check access
		if (fp.access() != (fp.access() & acc2uacc(it->type.acc)))
		{
			printk("ERR:  req_acc=%u, reg_acc=%u.\n", fp.access(), acc2uacc(it->type.acc));
			return false;
		}

		return true;
	}

	int cached(L4_fpage_t fp)
	{
		aspace_t::iter_t it = _uspace.find(fp.addr(), fp.size());
		if (it == _uspace.end())
			return -1;

		// check access
		if (fp.access() != (fp.access() & acc2uacc(it->type.acc)))
		{
			printk("ERR:  req_acc=%u, reg_acc=%u.\n", fp.access(), acc2uacc(it->type.acc));
			return -2;
		}

		return it->type.cached;
	}

	int attributes(L4_fpage_t fp)
	{
		aspace_t::iter_t it = _uspace.find(fp.addr(), fp.size());
		if (it == _uspace.end())
			return -1;

		/*
		// check access
		if (fp.access() != (fp.access() & acc2uacc(it->type.acc)))
		{
			printk("req_acc=%u, reg_acc=%u.\n", fp.access(), acc2uacc(it->type.acc));
			return -2;
		}
		*/

		return it->type.acc  |  (it->type.cached << 6);
	}
};

//--------------------------------------------------------------------------------------------------
// Kernel memory poll.
class Kmem
{
public:

	struct Type
	{
		enum type_t
		{
			Free,
			Busy
		};
		type_t type;
		explicit Type(type_t t) : type(t) {}
		inline bool operator == (const Type& t) const { return type == t.type; }
		inline bool operator != (const Type& t) const { return !operator==(t); }
		inline Type operator |= (const Type& t) { type = (type_t)(type | t.type);  return *this; }
		const char* str() const
		{
			switch (type)
			{
				case Free:       return "free";
				case Busy:       return "busy";
			}
			wassert(0);
			return "__err__";
		}
	};

private:

	enum
	{
		Alloc_min_sz = 0x40,              // minumum allocated mem chank
		Pool_init_sz = 256 * Cfg_page_sz  // initial pool size
	};

	typedef Address_space_t <addr_t, size_t, Type, 1024> memory_t;
	static memory_t memory;                       // kernel memory pool
	static uint8_t premapped_mem [Pool_init_sz];  // premapped memory
	static size_t pool_sz;

private:

	static void setup_premapped()
	{
		addr_t va = (addr_t)premapped_mem;
		size_t sz = sizeof(premapped_mem);

		wassert(va && is_aligned(va, Cfg_page_sz));
		wassert(sz && is_aligned(sz, Cfg_page_sz));

		memory.add(va, sz, Type(Type::Free), memory_t::NotCombine);
	}

public:

	static void init()
	{
		printk("Kmem::%s:  hello.\n", __func__);
		setup_premapped();       // now Pgtab is able to alloc mem from Kmem
	}

	static void dump()
	{
		printk("Dump kmemory pool (%zu/%zu), free=0x%zx bytes:\n", memory.size(), memory.capacity(), pool_sz);
		for (memory_t::citer_t it=memory.cbegin(); it!=memory.cend(); ++it)
			printk("  [%08lx - %08lx)  sz=0x%08zx,  %s.\n", it->addr, it->addr + it->sz, it->sz, it->type.str());
	}

	// allocate aligned memory chank
	static addr_t alloc(size_t sz, addr_t align = Alloc_min_sz)
	{
		printk("Kmem::%s:  sz=0x%zx, align=0x%lx, pool_sz=0x%zx.\n", __func__, sz, align, pool_sz);

		wassert(sz && align);

		sz = round_up(sz, Alloc_min_sz);

		// find free memory
		memory_t::iter_t it = memory.begin();
		for ( ; it!=memory.end(); ++it)
		{
			if (it->type.type == Type::Free  &&  it->sz >= sz)
			{
				addr_t aaddr = round_up(it->addr, align);  // aligned addr
				size_t diff = aaddr - it->addr;
				if (it->sz >= diff + sz)
				{
					memory.set_type(it, aaddr, sz, Type(Type::Busy), memory_t::NotCombine); // inside 'it' may be changed
					pool_sz -= sz;
					return aaddr;
				}
			}
		}

		dump();
		panic("No free kernel memory.\n");
		return 0;
	}

	// free previously allocated memory
	static void free(addr_t va)
	{
		printk("Kmem::%s:  va=%lx.\n", __func__, va);

		memory_t::iter_t it = memory.find(va);
		wassert(it != memory.end()  &&  it->type.type == Type::Busy);
		pool_sz += it->sz;
		memory.set_type(it, va, it->sz, Type(Type::Free), memory_t::Combine);
	}
};

// WORKAROUND:  access to Aspace::* and Kmem::*.

inline paddr_t kmem_paddr(void* va, size_t sz)     { return Aspace::kmem_paddr(va, sz); }
inline paddr_t kmem_paddr(addr_t va, size_t sz)    { return Aspace::kmem_paddr(va, sz); }
inline void*   kmem_vaddr(paddr_t pa, size_t sz)   { return (void*)Aspace::kmem_vaddr(pa, sz); }
inline addr_t  kmem_alloc(size_t sz, addr_t align) { return Kmem::alloc(sz, align); }
inline void    kmem_free(void* va)                 { Kmem::free((addr_t)va); }
inline addr_t  kuart_va()                          { return Aspace::kuart_pg_va(); }
inline addr_t  kintc_va()                          { return Aspace::kintc_pg_va(); }
inline addr_t  ktimer_va()                         { return Aspace::ktimer_pg_va(); }

// ~WORKAROUND

#endif // KMEM_H
