//##################################################################################################
//
//  Module to do paging with differents page table structures.
//
//##################################################################################################

#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#define USE_MMU_ASSERT
#include "mmu.h"
#include "types.h"
#include "printk.h"
#include "sys-utils.h"
#include <string.h>

addr_t kmem_alloc(size_t sz, addr_t align);
void kmem_free(void* va);

paddr_t kmem_paddr(void* va, size_t sz);
paddr_t kmem_paddr(addr_t va, size_t sz);
void* kmem_vaddr(paddr_t pa, size_t sz);

// alloc aligned memory or die
inline word_t* create_table(size_t sz)
{
	word_t* tb = (word_t*) kmem_alloc(sz, sz);
	if (!tb)
		panic("%s:  faild to allocate table:  sz=%lx.", __func__, sz);
	memset(tb, 0x0, sz);
	return tb;
}

//--------------------------------------------------------------------------------------------------
template <int Table_level, int Addr_index_hi, int Addr_index_low, typename Child_ptable>
class Ptable
{
public:

	//  Virtual address scheme:
	//
	//  31     hi         low                 0
	//  |------*-----------*------------------|
	//           addr_index    addr_offset

	// integer constants
	enum
	{
		Level       = Table_level,
		Index_hi    = Addr_index_hi,
		Index_low   = Addr_index_low,
		Table_sz    = (1 << (Addr_index_hi - Addr_index_low + 1)), // size in records
		Table_bytes = Table_sz * sizeof(word_t),                   // size in bytes
	};

	// UL integer constants
	enum
	{
		Index_mask  = ((1ul << (Addr_index_hi - Addr_index_low + 1)) - 1) << Addr_index_low,
		Offset_mask = ((1ul << Addr_index_low) - 1),
		Page_sz     = 1ul << Addr_index_low,
	};

	// ULL integer constants
	enum
	{
		Aspace_sz   = 1ull << (Addr_index_hi + 1),
	};

private:

	word_t table [Table_sz]; // table data

	// use in unmap() to free table memory if empty
	// NOTE:  now no checking for child_table.empty()
	inline bool empty()
	{
		for (unsigned i=0; i<Table_sz; ++i)
			if (mmu_is_inv(Level, table + i))
				return false;
		return true;
	}

public:

	// print tables structure
	static void print_structure()
	{
		printk("  lev=%u:  hi=%ld, lo=%ld, index_mask=0x%lx, offset_mask=0x%lx, pg_sz=0x%lx, tb_sz=0x%lx.\n",
			Level,  Addr_index_hi, Addr_index_low, Index_mask, Offset_mask, Page_sz, Table_sz);

		Child_ptable::print_structure();
	}

	void dump(addr_t start_va)
	{
		enum { Indent = Level - 1 };

		for (int i=0; i<Table_sz; ++i)
		{
			word_t* cell = &table[i];
			if (mmu_is_inv(Level, cell))
			{
				// nothing
			}
			else if (mmu_is_dir(Level, cell))
			{
				printk("%*s L%u:  i=%3d:  dir:  start_va=0x%lx\n", Indent, "", Level, i, start_va + mmu_index2va(Level, i));
				paddr_t child_pa = mmu_get_dir_pa(Level, cell);
				size_t child_sz = Child_ptable::Table_bytes;
				Child_ptable* child = (Child_ptable*) kmem_vaddr(child_pa, child_sz);
				if (!child)
					panic("Faild to get child table:  lev=%d, child_sz=%lx.", Level, child_sz);
				child->dump(start_va + mmu_index2va(Level, i));
			}
			else if (mmu_is_map(Level, cell))
			{
				printk("%*s L%u:  i=%3d:  map:  va=0x%lx, pa=0x%llx, sz=0x%lx, cached=%u, acc=%s.\n",
					Indent, "", Level, i,
					start_va + mmu_index2va(Level, i), mmu_get_map_pa(Level, cell), Page_sz,
					mmu_get_map_cached(Level, cell), mmu_get_map_acc(Level, cell));
			}
			else
				panic("Unknown cell type:  lev=%u, cell=0x%x.", Level, *cell);
		}
	}

	inline void init()
	{
		//printk("Ptable::%s:  lev=%ld.\n", __func__, Level);
		mmu_zero(table, Table_bytes);
	}

	inline word_t* get_cell(addr_t va)
	{
		return table + ((va & Index_mask) >> Addr_index_low);
	}

	void set_dir(addr_t va, unsigned dir_level, void* dir_tb)
	{
		//printk("%s:  L%u:  va=0x%lx, dir_lev=%ld, dir_tb=0x%lx.\n", __func__, Level, va, dir_level, dir_tb);
		word_t* cell = get_cell(va);
		if (Level + 1 == dir_level)
		{
			//printk("%s:  L%u:  set dir record.\n", __func__, Level);
			if (!mmu_is_inv(Level, cell))
				panic("%s:  L%u:  cell is not invalid.\n", __func__, Level);
			mmu_set_dir(Level, cell, kmem_paddr(dir_tb, /*FIXME*/Table_bytes/*FIXME*/));
		}
		else
		{
			//printk("%s:  L%u:  go to child table.\n", __func__, Level);

			Child_ptable* child = 0;
			size_t child_sz = Child_ptable::Table_bytes;
			if (mmu_is_inv(Level, cell))
			{
				//printk("%s:  L%u:  create new child table.\n", __func__, Level);
				// create new table
				child = (Child_ptable*) create_table(child_sz);
				mmu_set_dir(Level, cell, kmem_paddr(child, child_sz));
			}
			else if (mmu_is_dir(Level, cell))
			{
				paddr_t child_pa = mmu_get_dir_pa(Level, cell);
				child = (Child_ptable*) kmem_vaddr(child_pa, child_sz);
			}
			else if (mmu_is_map(Level, cell))
				panic("%s:  L%u:  unexpected map record.\n", __func__, Level);
			else
				panic("Unknown cell type:  lev=%lu, cell=0x%lx.", Level, *cell);

			child->set_dir(va, dir_level, dir_tb);
		}
	}

	void map(addr_t vaddr, paddr_t paddr, size_t size, unsigned access, unsigned cachable, bool allow_over_map)
	{
		//printk("Ptable::%s:  lev=%u:  va=%lx, pa=%llx, sz=0x%x.\n", __func__, Level, vaddr, paddr, size);

		assert(mmu_isalign(vaddr, Cfg_page_sz));
		assert(mmu_isalign(paddr, Cfg_page_sz));
		assert(mmu_isalign(size, Cfg_page_sz));
		assert(size <= Aspace_sz);
		assert(access < 8);
		assert(cachable < 2);

		addr_t va = vaddr;
		size_t rest = size;
		while (rest)
		{
			word_t* cell = get_cell(va);
			//printk("Ptable::%s:  lev=%u:  va=%lx, cell=%lx/%lx, *cell=%lx.\n",
			//	__func__, Level, va, cell, (va & Index_mask) >> Addr_index_low, *cell);

			if (!(va & Offset_mask)  &&  rest >= Page_sz) // whole page
			{
				//printk("Ptable::%s:  lev=%u:  va=%lx, pa=%llx, sz=0x%lx, cache=%u, acc=%u.\n",
				//	__func__, Level, va, paddr + va - vaddr, Page_sz, cachable, access);

				// L4:  Pages already mapped in the mappee’s address space that would conflict
				//      with new mappings are implicitly unmapped before new pages are mapped.
				if (!allow_over_map  &&  mmu_is_map(Level, cell))
					panic("%s:  over mapping while allow_over_map=false:  lev=%u, va=%x.", __func__, Level, va);
				if (mmu_is_dir(Level, cell))
					panic("IMPLME:  over mapping, need unmap old mapping:  lev=%u, va=%x.", Level, va);

				mmu_set_map(Level, cell, paddr + va - vaddr, access, cachable);
				va += Page_sz;
				rest -= Page_sz;
			}
			else
			{
				//printk("Ptable::%s:  lev=%u:  use next ptable level.\n", __func__, Level);

				// get child page
				Child_ptable* child = 0;
				size_t child_sz = Child_ptable::Table_bytes;
				if (mmu_is_inv(Level, cell))
				{
					// create new table
					child = (Child_ptable*) create_table(child_sz);
					mmu_set_dir(Level, cell, kmem_paddr(child, child_sz));
				}
				else if (mmu_is_dir(Level, cell))
				{
					paddr_t child_pa = mmu_get_dir_pa(Level, cell);
					child = (Child_ptable*) kmem_vaddr(child_pa, child_sz);
				}
				else if (mmu_is_map(Level, cell))
					panic("Attempt to map, walk to next level ptable, but already mapped:  lev=%lu, va=%lx.", Level, va);
				else
					panic("Unknown cell type:  lev=%lu, cell=0x%lx.", Level, *cell);

				// map in child page
				size_t len = min(rest, Page_sz - (va & Offset_mask));
				child->map(va, paddr + va - vaddr, len, access, cachable, allow_over_map);

				va += len;
				rest -= len;
			}
		}
	}

	// TODO:  do case than mapped L2 and unmaping l3 --> need:
	//          - map item -> dir item, and do lower mappings
	//          - unmap from lower table
	void unmap(addr_t vaddr, size_t size)
	{
		//printk("Ptable::%s:  lev=%u:  va=%lx, sz=0x%lx.\n", __func__, Level, vaddr, size);

		assert(mmu_isalign(vaddr, Cfg_page_sz));
		assert(mmu_isalign(size, Cfg_page_sz));
		assert(size <= Aspace_sz);

		addr_t va = vaddr;
		size_t rest = size;
		while (rest)
		{
			word_t* cell = get_cell(va);
			size_t len = 0;
			//printk("Ptable::%s:  lev=%lu:  va=%lx, cell=%lx/%2lx, cell_val=0x08%lx.\n",
			//	__func__, Level, va, cell, (va & Index_mask) >> Addr_index_low, *cell);

			if (mmu_is_dir(Level, cell))
			{
				// get child page
				paddr_t child_pa = mmu_get_dir_pa(Level, cell);
				Child_ptable* child = (Child_ptable*) kmem_vaddr(child_pa, Child_ptable::Table_bytes);

				// unmap in child page
				len = min(rest, Page_sz - (va & Offset_mask));
				child->unmap(va, len);
			}
			else if (mmu_is_map(Level, cell))
			{
				if (rest < Page_sz)
					panic("Unmap high-level table but need low-level:  lev=%u, cell_val=0x08%lx.", Level, *cell);
				len = Page_sz;
			}
			else if (mmu_is_inv(Level, cell))
			{
				panic("Unmap but all or part of region is not mapped:  lev=%u, cell_val=0x08%lx.", Level, *cell);
			}

			mmu_set_inv(Level, cell);

			va += len;
			rest -= len;
		}
	}

	// find pa and check virt contiguous for size
	// TODO:  find virt contig memory for SZ and ACC -- if (acc & req_acc == req_acc) ...
	// TODO:  return PA and found SZ
	// TODO:  in another place do get_sg_list()
	paddr_t walk(addr_t vaddr, size_t size)
	{
		//printk("Ptable::%s:  lev=%lu:  va=%lx, sz=0x%lx.\n", __func__, Level, vaddr, size);

		assert(mmu_isalign(vaddr, Cfg_page_sz));
		assert(mmu_isalign(size, Cfg_page_sz));
		assert(size <= Aspace_sz);

		paddr_t pa = 0;
		addr_t va = vaddr;
		size_t rest = size;
		while (rest)
		{
			word_t* cell = get_cell(va);
			size_t len = 0;
			//printk("Ptable::%s:  lev=%lu:  va=%lx, cell=%lx/%2lx, cell_val=0x08%lx.\n",
			//	__func__, Level, va, cell, (va & Index_mask) >> Addr_index_low, *cell);

			if (mmu_is_inv(Level, cell))
			{
				return 0;  // mem region does not mapped
			}
			else if (mmu_is_dir(Level, cell))
			{
				// get child page
				paddr_t child_pa = mmu_get_dir_pa(Level, cell);
				Child_ptable* child = (Child_ptable*) kmem_vaddr(child_pa, Child_ptable::Table_bytes);

				// walk in child page
				len = min(rest, Page_sz - (va & Offset_mask));
				paddr_t res = child->walk(va, len);
				if (!res)
					return 0;
				if (!pa)
					pa = res;
			}
			else if (mmu_is_map(Level, cell))
			{
				len = min(rest, Page_sz - (va & Offset_mask));
				if (!pa)
					pa = mmu_get_map_pa(Level, cell) + (va & Offset_mask);
			}
			else
				panic("Unknown cell type:  lev=%lu, cell=0x%lx.", Level, *cell);

			va += len;
			rest -= len;
		}
	
		return pa;
	}
};

//--------------------------------------------------------------------------------------------------
// it is used to make invalid child Ptable to terminate hierarchy
class Ptable_terminator
{
public:
	enum { Table_sz = 0, Table_bytes = 0 };
	static void print_structure() { return; }
	void dump(addr_t) { panic("dump"); }
	void init() { panic("init"); }
	void map(addr_t, paddr_t, size_t, unsigned, unsigned, bool) { panic("map"); }
	void unmap(addr_t, size_t) { panic("unmap"); }
	paddr_t walk(addr_t, size_t) { panic("walk");  return 0; }
	void set_dir(addr_t, unsigned, void*) { panic("set_dir"); }
};

#if defined(Cfg_arch_sparc)
//            level [indx bits]
  typedef Ptable <3, 17, 12, Ptable_terminator> Ptable_l3;
  typedef Ptable <2, 23, 18, Ptable_l3> Ptable_l2;
  typedef Ptable <1, 31, 24, Ptable_l2> Ptable_l1;
  typedef Ptable_l2 Ktab_t;
  #define USE_CTXTB
#elif defined(Cfg_arch_arm)
  //          level [indx bits]
  typedef Ptable <2, 19, 12, Ptable_terminator> Ptable_l2;
  typedef Ptable <1, 31, 20, Ptable_l2> Ptable_l1;
  typedef Ptable_l2 Ktab_t;
#elif defined(Cfg_arch_x86)
  //          level [indx bits]  child ptable
  typedef Ptable <2, 21, 12, Ptable_terminator> Ptable_l2;
  typedef Ptable <1, 31, 22, Ptable_l2> Ptable_l1;
  typedef Ptable_l2 Ktab_t;
#elif defined(Cfg_arch_x86_64)
  //          level [indx bits]  child ptable
  typedef Ptable <4, 20, 12, Ptable_terminator> Ptable_l4;
  typedef Ptable <3, 29, 21, Ptable_l4> Ptable_l3;
  typedef Ptable <2, 38, 30, Ptable_l3> Ptable_l2;
  typedef Ptable <1, 47, 39, Ptable_l2> Ptable_l1;
  typedef Ptable_l4 Ktab_t;
#else
# error Unsupported arch
#endif

//--------------------------------------------------------------------------------------------------
class Pgtab
{
	enum
	{
		Kspace_sz = 16 * 0x100000,                                // use <= 16 MB for kspace
		Ktbs      = Kspace_sz / Ktab_t::Aspace_sz,                // number of Ktab_t's  for kspace
		Kern_va   = round_down(Cfg_krn_vaddr, Ktab_t::Aspace_sz), // start of kernel virt space
	};

	static_assert(is_aligned(Kspace_sz, Ktab_t::Aspace_sz));
	static_assert(is_aligned(Kern_va, Ktab_t::Aspace_sz));

	// common for all contexts data
	static Ktab_t* kerntb[Ktbs];      // l2 ptables for kernel space

	#ifdef USE_CTXTB
	static word_t* ctxtb;             // context table
	#endif

	// individual context data
	Ptable_l1* roottb;                // root page table
	unsigned ctxid;                   // may be used as ctx num in ctxtb

public:

	// initialize common contexts data (kernel address space)
	static void kinit()
	{
		printk("Pgtab::%s:  hello.\n", __func__);

		assert(!kerntb[0] && "Already inited.");

		#ifdef USE_CTXTB
		// create context ptable (L0)
		ctxtb = create_table(Ctx_bytes);
		#endif

		// create kernel tables
		for (unsigned i=0; i<Ktbs; ++i)
			kerntb[i] = (Ktab_t*) create_table(Ktab_t::Table_bytes);

		printk("Pgtab::%s:  done.\n", __func__);
	}

	// initialize individual context data
	explicit Pgtab(unsigned ctx) : ctxid(ctx)
	{
		printk("Pgtab::%s:  hello, ctx=%u.\n", __func__, ctx);
		assert(ctx <= 0xff);
		assert(kerntb[0] && "Not inited!");

		// create root ptable
		roottb = (Ptable_l1*) create_table(Ptable_l1::Table_bytes);

		#ifdef USE_CTXTB
		// set roottb in ctxtb
		mmu_set_dir(0, ctxtb + ctxid, kmem_paddr(roottb, Ptable_l1::Table_bytes));
		#endif

		// set kernel tables to root table
		for (unsigned i=0; i<Ktbs; ++i)
			roottb->set_dir(Kern_va + i * Ktab_t::Aspace_sz, Ktab_t::Level, kerntb[i]);
		printk("Pgtab::%s:  done.\n", __func__);
	}

	~Pgtab()
	{
		printk("Pgtab::%s:  hello, ctx=%u.\n", __func__, ctxid);
		printk("TODO:  Table walk and free all tables.\n\n");
		#ifdef USE_CTXTB
		mmu_set_inv(0, ctxtb + ctxid);
		#endif
		kmem_free(roottb);
		printk("Pgtab::%s:  by, ctx=%u.\n", __func__, ctxid);
	}

	void dump()
	{
		printk("Dump page tables:\n");
		roottb->dump(0x0);
	}

	static void print_structure()
	{
		printk("Print tables structure:\n");
		Ptable_l1::print_structure();
	}

	// map in root ptable
	inline void map(addr_t va, paddr_t pa, size_t sz, unsigned access, unsigned cachable)
	{
		// TODO:  assert(not-in-kerntb)
		roottb->map(va, pa, sz, access, cachable, true);
	}

	// unmap in root ptable
	inline void unmap(addr_t va, size_t sz)
	{
		// TODO:  assert(not-in-kerntb)
		roottb->unmap(va, sz);
	}

	// walk in root ptable
	inline paddr_t walk(addr_t va, size_t sz)
	{
		// TODO:  assert(not-in-kerntb)
		return roottb->walk(va, sz);
	}

	// map in kernel ptable
	static inline void kmap(addr_t va, paddr_t pa, size_t sz, unsigned access, unsigned cachable)
	{
		assert(va >= Kern_va  &&  va < (Kern_va + Kspace_sz));
		assert((va+sz) > Kern_va  &&  (va+sz) <= (Kern_va) + Kspace_sz);
		while (sz)
		{
			unsigned index = (va - Kern_va) / Ktab_t::Aspace_sz;
			assert(index < Ktbs);
			size_t rest_to_tb_end = Ktab_t::Aspace_sz - (va & (Ktab_t::Aspace_sz - 1));

			size_t s = is_aligned(va, Ktab_t::Aspace_sz) ? min(sz, Ktab_t::Aspace_sz) :
			                                               min(sz, rest_to_tb_end);
			kerntb[index]->map(va, pa, s, access, cachable, false);
			sz -= s;
			va += s;
			pa += s;
		}
	}

	// unmap in kernel ptable
	static inline void kunmap(addr_t va, size_t sz)
	{
		assert(va >= Kern_va  &&  va < (Kern_va + Kspace_sz));
		assert((va+sz) > Kern_va  &&  (va+sz) <= (Kern_va + Kspace_sz));
		unsigned index = (va - Kern_va) / Ktab_t::Aspace_sz;
		kerntb[index]->unmap(va, sz);
	}

	// walk in kernel ptable
	static inline paddr_t kwalk(addr_t va, size_t sz)
	{
		assert(va >= Kern_va  &&  va < (Kern_va + Kspace_sz));
		assert((va+sz) > Kern_va  &&  (va+sz) <= (Kern_va) + Kspace_sz);
		unsigned index = (va - Kern_va) / Ktab_t::Aspace_sz;
		return kerntb[index]->walk(va, sz);
	}

	// set context table pointer to MMU register
	static void activate()
	{
		#ifdef USE_CTXTB
		mmu_reg_ctxtb(kmem_paddr(ctxtb, Ctx_bytes));
		mmu_tlb_flush();
		#endif
	}

	// set current MMU context number
	void set_current()
	{
		#ifdef USE_CTXTB
		mmu_reg_ctx(ctxid);
		#else
		mmu_root_table(kmem_paddr(roottb, L1_sz * sizeof(word_t)));
		#endif

		mmu_tlb_flush();
	}
};

#endif // PAGE_TABLE_H
