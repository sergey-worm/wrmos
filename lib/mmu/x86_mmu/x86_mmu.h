//##################################################################################################
//
//  x86 MMU low-level helpers.
//
//  x86-32 has 2 level of paging:
//
//    Level  Name     TableSize  PageSize  HasMap   HasDir
//        1  PageDir      0x400      4 MB  + (PSE)       +
//        2  PageTab      0x400      4 kB       +        -
//
//  x86-64 has 4 level of paging:
//
//    Level  Name         TableSize  PageSize  HasMap   HasDir
//        1  PageMapLev4      0x200    512 GB       -        +
//        2  PageDirPtr       0x200      1 GB  + (PSE)       +
//        3  PageDir          0x200      2 MB       +        +
//        4  PageTab          0x200      4 kB       +        -
//
//##################################################################################################

#ifndef MMU_H
#define MMU_H

#include "sys_types.h"
#include "sys_proc.h"
#include "wlibc_assert.h"

#if defined (DEBUG) && defined (USE_MMU_ASSERT)
#  define mmu_assert wassert
#else
#  define mmu_assert(expr)
#endif

#define mmu_isalign(val, algn)  (!(val & (algn - 1)))

enum
{
	#ifdef X86_32

	// table size in records
	L1_sz       = 0x400,
	L2_sz       = 0x400,

	// table size in bytes
	L1_bytes  = L1_sz * sizeof(word_t),
	L2_bytes  = L2_sz * sizeof(word_t),

	/**/PDPT_pgsz   = 0x0,         // absent for 32-bit mode
	PDT_pgsz    = 0x400000,    // 4 MB, allow if PS=1
	PT_pgsz     = 0x1000,      // 4 kB

	L1_pgsz     = PDT_pgsz,
	L2_pgsz     = PT_pgsz,
	Paddr_width = 32,

	#else

	// table size in records
	L1_sz       = 0x200,
	L2_sz       = 0x200,
	L3_sz       = 0x200,
	L4_sz       = 0x200,

	// table size in bytes
	L1_bytes  = L1_sz * sizeof(word_t),
	L2_bytes  = L2_sz * sizeof(word_t),
	L3_bytes  = L3_sz * sizeof(word_t),
	L4_bytes  = L4_sz * sizeof(word_t),

	PML4T_pgsz  = 0x8000000000, // 512 GB, note: PML4 table does not support map records
	PDPT_pgsz   = 0x40000000,   // 1 GB, allow if PS=1 (note: extention)
	PDT_pgsz    = 0x200000,     // 2 MB, allow if PS=1
	PT_pgsz     = 0x1000,       // 4 kB

	L1_pgsz     = PML4T_pgsz,
	L2_pgsz     = PDPT_pgsz,
	L3_pgsz     = PDT_pgsz,
	L4_pgsz     = PT_pgsz,

	Paddr_width = 52,

	#endif

	NotCachable   = 0,
	Cachable      = 1,

	Mmu_acc_krx_uno   = 0,
	Mmu_acc_krwx_uno  = 1,
	Mmu_acc_krx_urx   = 2,
	Mmu_acc_krwx_urwx = 3,
	Mmu_acc_max       = 3,
};

inline bool _mmu_acc_user(int acc) { return acc == Mmu_acc_krx_urx   ||  acc == Mmu_acc_krwx_urwx; }
inline bool _mmu_acc_wr(int acc)   { return acc == Mmu_acc_krwx_uno  ||  acc == Mmu_acc_krwx_urwx; }
inline int  _mmu_make_acc(bool user, bool wr) { return (user << 1) | wr; }

//--------------------------------------------------------------------------------------------------
struct PML4T_dir_t
{
	union
	{
		struct
		{
			unsigned present    :  1;  // page is present in RAM
			unsigned writeable  :  1;  // 0 -> read_only, 1 -> read_write
			unsigned user       :  1;  // allow user access
			unsigned cache_mode :  1;  // 0 -> write_back, 1 -> write_through
			unsigned cache_off  :  1;  // disable cache
			unsigned accessed   :  1;  // page was accessed for read or write
			unsigned ignored1   :  1;  //
			unsigned reserved   :  1;  //
			unsigned ignored2   :  4;  // may be used by OS for storage
			word_t   addr       :  Paddr_width - 12;  // PDPT addr
		};
		word_t raw;
	};

	inline paddr_t paddr()
	{
		return addr << 12;
	}

	inline void set(paddr_t tb)
	{
		raw        = 0;        //
		addr       = tb >> 12; //
		cache_off  = 0;        // enable for dir entry
		cache_mode = 0;        // 0 -> write_back, 1 -> write_through
		user       = 1;        // allow user access for PTD
		writeable  = 1;        // make writable for dir entry
		present    = 1;        //
	}
};

//static_assert(sizeof(PML4T_dir_t) == 8);

//--------------------------------------------------------------------------------------------------
struct PDPT_dir_t
{
	union
	{
		struct
		{
			unsigned present    :  1;  // page is present in RAM
			unsigned writeable  :  1;  // 0 -> read_only, 1 -> read_write
			unsigned user       :  1;  // allow user access
			unsigned cache_mode :  1;  // 0 -> write_back, 1 -> write_through
			unsigned cache_off  :  1;  // disable cache
			unsigned accessed   :  1;  // page was accessed for read or write
			unsigned ignored1   :  1;  //
			unsigned is_map     :  1;  // must be 0 for dir entry
			unsigned ignored2   :  4;  // may be used by OS for storage
			word_t   addr       :  Paddr_width - 12;  // PDT addr
		};
		word_t raw;
	};

	inline paddr_t paddr()
	{
		return addr << 12;
	}

	inline void set(paddr_t tb)
	{
		raw        = 0;        //
		addr       = tb >> 12; //
		is_map     = 0;        //
		cache_off  = 0;        // enable for dir entry
		cache_mode = 0;        // 0 -> write_back, 1 -> write_through
		user       = 1;        // allow user access for PTD
		writeable  = 1;        // make writable for dir entry
		present    = 1;        //
	}
};

struct PDPT_map_t
{
	union
	{
		struct
		{
			unsigned present    :  1;  // page is present in RAM
			unsigned writeable  :  1;  // 0 -> read_only, 1 -> read_write
			unsigned user       :  1;  // allow user access
			unsigned cache_mode :  1;  // 0 -> write_back, 1 -> write_through
			unsigned cache_off  :  1;  // disable cache
			unsigned accessed   :  1;  // page was accessed for read or write
			unsigned dirty      :  1;  // 1 -> page has been written
			unsigned is_map     :  1;  // 1 -> map entry, 0 -> dir entry
			unsigned global     :  1;  // 1 -> no TLB updating if CR3 is reset
			unsigned ignored    :  3;  // may be used by OS for storage
			unsigned pat        :  1;  // ???, use 0
			unsigned _reserved  : 17;  //
			word_t   addr       :  Paddr_width - 30;  // page addr
		};
		word_t raw;
	};

	inline paddr_t paddr()
	{
		return addr << 30;
	}

	inline void set(paddr_t pa, unsigned access, unsigned cachable)
	{
		raw        = 0;         //
		addr       = pa >> 21;  // 2 MB page addr
		pat        = 0;         // ???, use 0
		global     = 0;         //
		is_map     = 1;         // must be 1 for l1 PTE
		dirty      = 0;         // 1 -> page has been written
		accessed   = 0;         // page was accessed for read or write
		cache_off  = !cachable; //
		cache_mode = 0;         // 0 -> write_back, 1 -> write_through
		user       = _mmu_acc_user(access);
		writeable  = _mmu_acc_wr(access);
		present    = 1;         //
	}
};

//--------------------------------------------------------------------------------------------------
struct PDT_dir_t
{
	union
	{
		struct
		{
			unsigned present    :  1;  // page is present in RAM
			unsigned writeable  :  1;  // 0 -> read_only, 1 -> read_write
			unsigned user       :  1;  // allow user access
			unsigned cache_mode :  1;  // 0 -> write_back, 1 -> write_through
			unsigned cache_off  :  1;  // disable cache
			unsigned accessed   :  1;  // page was accessed for read or write
			unsigned ignored1   :  1;  //
			unsigned is_map     :  1;  // must be 0 for dir entry
			unsigned ignored2   :  4;  // may be used by OS for storage
			word_t   addr       :  Paddr_width - 12;  // PT addr
		};
		word_t raw;
	};

	inline paddr_t paddr()
	{
		return addr << 12;
	}

	inline void set(paddr_t tb)
	{
		raw        = 0;        //
		addr       = tb >> 12; //
		is_map     = 0;        //
		cache_off  = 0;        // enable for PTD
		cache_mode = 0;        // 0 -> write_back, 1 -> write_through
		user       = 1;        // allow user access for PTD
		writeable  = 1;        // make writable for PTD
		present    = 1;        //
	}
};

struct PDT_map_t
{
	union
	{
		struct
		{
			unsigned present    :  1;  // page is present in RAM
			unsigned writeable  :  1;  // 0 -> read_only, 1 -> read_write
			unsigned user       :  1;  // allow user access
			unsigned cache_mode :  1;  // 0 -> write_back, 1 -> write_through
			unsigned cache_off  :  1;  // disable cache
			unsigned accessed   :  1;  // page was accessed for read or write
			unsigned dirty      :  1;  // 1 -> page has been written
			unsigned is_map     :  1;  // 1 -> map entry, 0 -> dir entry
			unsigned global     :  1;  // 1 -> no TLB updating if CR3 is reset
			unsigned ignored    :  3;  // may be used by OS for storage
			unsigned pat        :  1;  // ???, use 0
			#ifdef X86_32
			unsigned addr_hi    :  4;  // page addr, bits [39..32]
			unsigned _nulls     :  5;  // must be 0
			unsigned addr_lo    : 10;  // page addr, bits [31..22]
			#else
			unsigned _nulls_1   :  8;  // must be 0
			unsigned addr_lo    :  32 - 21;           // page addr, bits [31..21]
			unsigned addr_hi    :  Paddr_width - 32;  // page addr, bits [51..32]
			unsigned _nulls_2   : 64 - Paddr_width;
			#endif
		} __attribute__((package));
		word_t raw;
	};

	inline paddr_t paddr()
	{
		#ifdef X86_32
		return ((paddr_t)addr_hi << 32) | (addr_lo << 22);
		#else
		//return addr << 21;
		return ((paddr_t)addr_hi << 32) | (addr_lo << 21);
		#endif
	}

	inline void set(paddr_t pa, unsigned access, unsigned cachable)
	{
		raw        = 0;         //
		#ifdef X86_32
		addr_lo    = pa >> 22;  //
		_nulls     = 0;         // must be 0
		addr_hi    = pa >> 32;  //
		#else
		addr_lo    = pa >> 21;  // 2 MB page addr
		addr_hi    = pa >> 32;  // 2 MB page addr
		_nulls_1   = 0;         // must be 0
		_nulls_2   = 0;         // must be 0
		#endif
		pat        = 0;         // ???, use 0
		global     = 0;         //
		is_map     = 1;         // must be 1 for l1 PTE
		dirty      = 0;         // 1 -> page has been written
		accessed   = 0;         // page was accessed for read or write
		cache_off  = !cachable; //
		cache_mode = 0;         // 0 -> write_back, 1 -> write_through
		user       = _mmu_acc_user(access);
		writeable  = _mmu_acc_wr(access);
		present    = 1;         //
	}
};
static_assert(sizeof(PDT_map_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
struct PT_map_t
{
	union
	{
		struct
		{
			unsigned present    :  1;  // page is present in RAM
			unsigned writeable  :  1;  // 0 -> read_only, 1 -> read_write
			unsigned user       :  1;  // allow user access
			unsigned cache_mode :  1;  // 0 -> write_back, 1 -> write_through
			unsigned cache_off  :  1;  // disable cache
			unsigned accessed   :  1;  // page was accessed for read or write
			unsigned dirty      :  1;  // 1 -> page has been written
			unsigned pat        :  1;  // ???, use 0
			unsigned global     :  1;  // 1 -> no TLB updating if CR3 is reset
			unsigned _ignored   :  3;  // may be used by OS for storage
			word_t   addr       :  Paddr_width - 12;  // page addr
		};
		word_t raw;
	};

	inline paddr_t paddr()
	{
		return addr << 12;
	}

	inline void set(paddr_t pa, unsigned access, unsigned cachable)
	{
		raw        = 0;         //
		addr       = pa >> 12;  //
		global     = 0;         //
		pat        = 0;         // ???, use 0
		dirty      = 0;         // 1 -> page has been written
		accessed   = 0;         // page was accessed for read or write
		cache_off  = !cachable; //
		cache_mode = 0;         // 0 -> write_back, 1 -> write_through
		user       = _mmu_acc_user(access);
		writeable  = _mmu_acc_wr(access);
		present    = 1;         //
	}
};

//--------------------------------------------------------------------------------------------------
//  Table cells
//--------------------------------------------------------------------------------------------------

// make dir entry
inline void _mmu_set_pml4t_dir(word_t* cell, paddr_t tb)
{
	mmu_assert(mmu_isalign(tb, 0x1000)); // 4 kB aligned
	PML4T_dir_t entry;
	entry.set(tb);
	*cell = entry.raw;
}

// make dir entry
inline void _mmu_set_pdpt_dir(word_t* cell, paddr_t tb)
{
	mmu_assert(mmu_isalign(tb, 0x1000)); // 4 kB aligned
	PDPT_dir_t entry;
	entry.set(tb);
	*cell = entry.raw;
}

// make map entry
inline void _mmu_set_pdpt_map(word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	mmu_assert(mmu_isalign(pa, PDPT_pgsz));
	mmu_assert(access <= Mmu_acc_max);
	mmu_assert(cachable <= 1);
	PDPT_map_t entry;
	entry.set(pa, access, cachable);
	*cell = entry.raw;
}

// make dir entry
inline void _mmu_set_pdt_dir(word_t* cell, paddr_t tb)
{
	mmu_assert(mmu_isalign(tb, 0x1000)); // 4 kB aligned
	PDT_dir_t entry;
	entry.set(tb);
	*cell = entry.raw;
}

// make map entry
inline void _mmu_set_pdt_map(word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	mmu_assert(mmu_isalign(pa, PDT_pgsz));
	mmu_assert(access <= Mmu_acc_max);
	mmu_assert(cachable <= 1);
	PDT_map_t entry;
	entry.set(pa, access, cachable);
	*cell = entry.raw;
}

// make map entry
inline void _mmu_set_pt_map(word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	mmu_assert(mmu_isalign(pa, PT_pgsz));
	mmu_assert(access <= Mmu_acc_max);
	mmu_assert(cachable <= 1);
	PT_map_t entry;
	entry.set(pa, access, cachable);
	*cell = entry.raw;
}

//--------------------------------------------------------------------------------------------------
//  ~ Table cells
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  API
//--------------------------------------------------------------------------------------------------
// is this table level supportes map records?
inline addr_t mmu_has_map(unsigned level)
{

	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return 1;
	else
	if (level == 2)
		return 1;

	#else // 64

	mmu_assert(level == 1  ||  level == 2  ||  level == 3  ||  level == 4);
	if (level == 1)
		return 0;
	else
	if (level == 2)
		return 1;  // TODO:  check PSE supporting
	else
	if (level == 3)
		return 1;
	else
	if (level == 4)
		return 1;

	#endif
	return 0;
}

// make dir entry
inline void mmu_set_dir(unsigned level, word_t* cell, paddr_t tb)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  mmu_isalign(tb, L2_bytes));
	if (level == 1)
		_mmu_set_pdt_dir(cell, tb);

	#else

	mmu_assert((level == 1  ||  mmu_isalign(tb, L2_bytes)) ||
	           (level == 2  ||  mmu_isalign(tb, L3_bytes)) ||
	           (level == 3  ||  mmu_isalign(tb, L4_bytes)));
	if (level == 1)
		_mmu_set_pml4t_dir(cell, tb);
	else if (level == 2)
		_mmu_set_pdpt_dir(cell, tb);
	else if (level == 3)
		_mmu_set_pdt_dir(cell, tb);

	#endif
}

// make map entry
inline void mmu_set_map(unsigned level, word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	#ifdef X86_32

	mmu_assert((level == 1  ||  mmu_isalign(pa, L1_pgsz)) ||
	           (level == 2  ||  mmu_isalign(pa, L2_pgsz)));
	if (level == 1)
		_mmu_set_pdt_map(cell, pa, access, cachable); // NOTE:  L1 has map record only for PSE=1
	else if (level == 2)
		_mmu_set_pt_map(cell, pa, access, cachable);

	#else

	mmu_assert((level == 2  ||  mmu_isalign(pa, L2_pgsz)) ||
	           (level == 3  ||  mmu_isalign(pa, L3_pgsz)) ||
	           (level == 4  ||  mmu_isalign(pa, L4_pgsz)));
	if (level == 2)
		_mmu_set_pdpt_map(cell, pa, access, cachable); // NOTE:  L2 has map record only for PSE=1
	else if (level == 3)
		_mmu_set_pdt_map(cell, pa, access, cachable);
	else if (level == 4)
		_mmu_set_pt_map(cell, pa, access, cachable);

	#endif
}

// make invalid cell
inline void mmu_set_inv(unsigned level, word_t* cell)
{
	#ifdef X86_32
	mmu_assert(level == 1  ||  level == 2);
	#else
	mmu_assert(level == 1  ||  level == 2  ||  level == 3  ||  level == 4);
	#endif
	(void) level;
	*cell = 0;
}

// get paddr from dir record
inline paddr_t mmu_get_dir_pa(unsigned level, word_t* cell)
{
	#ifdef X86_32

	mmu_assert(level == 1  &&  !((PDT_dir_t*)cell)->is_map);
	if (level == 1)
		return ((PDT_dir_t*)cell)->paddr();

	#else

	mmu_assert((level == 1) ||
	           (level == 2  &&  !((PDPT_dir_t*)cell)->is_map) ||
	           (level == 3  &&  !((PDT_dir_t*)cell)->is_map));
	if (level == 1)
		return ((PML4T_dir_t*)cell)->paddr();
	else
	if (level == 2)
		return ((PDPT_dir_t*)cell)->paddr();
	else
	if (level == 3)
		return ((PDT_dir_t*)cell)->paddr();

	#endif
	return -1;
}

// get paddr from map record
inline paddr_t mmu_get_map_pa(unsigned level, word_t* cell)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return ((PDT_map_t*)cell)->paddr();
	else
	if (level == 2)
		return ((PT_map_t*)cell)->paddr();

	#else // 64

	mmu_assert(level == 2  ||  level == 3  ||  level == 4);
	if (level == 2)
		return ((PDPT_map_t*)cell)->paddr();
	else
	if (level == 3)
		return ((PDT_map_t*)cell)->paddr();
	else
	if (level == 4)
		return ((PT_map_t*)cell)->paddr();

	#endif
	return -1;
}

// get cachable attribute from map record
inline int mmu_get_map_cached(unsigned level, word_t* cell)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return !((PDT_map_t*)cell)->cache_off;
	else
	if (level == 2)
		return !((PDT_map_t*)cell)->cache_off;

	#else // 64

	mmu_assert(level == 2  ||  level == 3  ||  level == 4);
	if (level == 2)
		return !((PDPT_map_t*)cell)->cache_off;
	else
	if (level == 3)
		return !((PDT_map_t*)cell)->cache_off;
	else
	if (level == 4)
		return !((PDT_map_t*)cell)->cache_off;

	#endif
	return -1;
}

// get access attribute from map record
inline const char* mmu_get_map_acc(unsigned level, word_t* cell)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	int acc = 0;
	if (level == 1)
		acc = _mmu_make_acc(((PDT_map_t*)cell)->user, ((PDT_map_t*)cell)->writeable);
	else
	if (level == 2)
		acc = _mmu_make_acc(((PT_map_t*)cell)->user, ((PT_map_t*)cell)->writeable);

	#else // 64

	mmu_assert(level == 2  ||  level == 3  ||  level == 4);
	int acc = 0;
	if (level == 2)
		acc = _mmu_make_acc(((PDPT_map_t*)cell)->user, ((PDPT_map_t*)cell)->writeable);
	else
	if (level == 3)
		acc = _mmu_make_acc(((PDT_map_t*)cell)->user, ((PDT_map_t*)cell)->writeable);
	else
	if (level == 4)
		acc = _mmu_make_acc(((PT_map_t*)cell)->user, ((PT_map_t*)cell)->writeable);

	#endif
	switch (acc)
	{
		case Mmu_acc_krx_uno:    return "krx_uno";
		case Mmu_acc_krwx_uno:   return "krwx_uno";
		case Mmu_acc_krx_urx:    return "krx_urx";
		case Mmu_acc_krwx_urwx:  return "krwx_urwx";
	}
	return "__unknown_mmu_access__";
}

// is cell dir record
inline bool mmu_is_dir(unsigned level, word_t* cell)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return ((PDT_map_t*)cell)->present  &&  !((PDT_map_t*)cell)->is_map;

	#else // 64

	mmu_assert(level == 1  ||  level == 2  ||  level == 3  ||  level == 4);
	if (level == 1)
		return ((PML4T_dir_t*)cell)->present;
	else
	if (level == 2)
		return ((PDPT_map_t*)cell)->present  &&  !((PDPT_map_t*)cell)->is_map;
	else
	if (level == 3)
		return ((PDT_map_t*)cell)->present  &&  !((PDT_map_t*)cell)->is_map;

	#endif
	return 0;
}

// is cell map record
inline bool mmu_is_map(unsigned level, word_t* cell)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return ((PDT_map_t*)cell)->present  &&  ((PDT_map_t*)cell)->is_map;
	else
	if (level == 2)
		return ((PT_map_t*)cell)->present;

	#else // 64

	mmu_assert(level == 1  ||  level == 2  ||  level == 3  ||  level == 4);
	if (level == 1)
		return 0;
	else
	if (level == 2)
		return ((PDPT_map_t*)cell)->present  &&  ((PDPT_map_t*)cell)->is_map;
	else
	if (level == 3)
		return ((PDT_map_t*)cell)->present  &&  ((PDT_map_t*)cell)->is_map;
	else
	if (level == 4)
		return ((PT_map_t*)cell)->present;

	#endif
	return 0;
}

// is cell invalid record
inline bool mmu_is_inv(unsigned level, word_t* cell)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return !((PDT_map_t*)cell)->present;
	else
	if (level == 2)
		return !((PT_map_t*)cell)->present;

	#else // 64

	mmu_assert(level == 1  ||  level == 2  ||  level == 3  ||  level == 4);
	if (level == 1)
		return !((PML4T_dir_t*)cell)->present;
	else
	if (level == 2)
		return !((PDPT_map_t*)cell)->present;
	else
	if (level == 3)
		return !((PDT_map_t*)cell)->present;
	else
	if (level == 4)
		return !((PT_map_t*)cell)->present;

	#endif
	return 0;
}

// convert table index to vaddr base
inline addr_t mmu_index2va(unsigned level, unsigned index)
{
	#ifdef X86_32

	mmu_assert(level == 1  ||  level == 2);
	mmu_assert(index < 0x400);
	if (level == 1)
		return index * L1_pgsz;
	else
	if (level == 2)
		return index * L2_pgsz;

	#else // 64

	mmu_assert(level == 1  ||  level == 2  ||  level == 3  ||  level == 4);
	mmu_assert(index < 0x200);
	if (level == 1)
		return index * L1_pgsz + (index < 0x100 ? 0 : 0xffff000000000000);
	else
	if (level == 2)
		return index * L2_pgsz;
	else
	if (level == 3)
		return index * L3_pgsz;
	else
	if (level == 4)
		return index * L4_pgsz;

	#endif
	return 0;
}

//--------------------------------------------------------------------------------------------------
//  ~ API
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
inline void mmu_zero(word_t* tb, unsigned sz_bytes)
{
	mmu_assert(mmu_isalign((addr_t)tb, 0x1000)  &&  sz_bytes == 0x1000);
	// try to use double instruction
	uint64_t* ptr = (uint64_t*)(void*)tb;
	unsigned sz = sz_bytes / sizeof(uint64_t);
	for (unsigned i=0; i<sz; ++i)
		ptr[i] = 0;
}

//--------------------------------------------------------------------------------------------------
//  MMU registers access
//--------------------------------------------------------------------------------------------------

inline void mmu_root_table(word_t val)
{
	Proc::cr3(val);
}

inline void mmu_tlb_flush()
{
	// NOTE:  don't need after mmu_root_table()
	Proc::cr3(Proc::cr3());
}

inline void mmu_enable_pse()
{
	#ifdef X86_32

	Proc::cr4(Proc::cr4() | 0x10);  // enable PSE (page size extention) to use L1 mapping

	#else // 64

	Proc::cr4(Proc::cr4() | 0x10);  // enable PSE (page size extention) to use L2 mapping

	#endif
}

inline void mmu_enable_paging()
{
	Proc::cr0(Proc::cr0() | (1 << 31));
}

//--------------------------------------------------------------------------------------------------
//  ~ MMU registers access
//--------------------------------------------------------------------------------------------------

#endif // MMU_H
