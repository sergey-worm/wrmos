//##################################################################################################
//
//  ARM MMU low-level helpers.
//
//  Where are 2 level of paging:
//
//    Level  Name     TableSize  PageSize  HasMap   HasDir
//        1  section    0x1000       1 MB       +        +
//        2  page        0x100       2 kB       +        -
//
//##################################################################################################

#ifndef MMU_H
#define MMU_H

#include "sys_types.h"

#if defined (DEBUG) && defined (USE_MMU_ASSERT)
# include "wlibc_assert.h"
# define mmu_assert wassert
#else
# define mmu_assert(expr)
#endif

#define mmu_isalign(val, algn)  (!(val & (algn - 1)))

enum
{
	// table size in records
	L1_sz     = 0x1000,
	L2_sz     = 0x100,

	// table size in bytes
	L1_bytes  = L1_sz * sizeof(word_t),
	L2_bytes  = L2_sz * sizeof(word_t),

	L1_pgsz   = 0x100000,  // 1 MB
	L2_pgsz   = 0x1000,    // 4 kB

	NotCachable   = 0,
	Cachable      = 1,

	Mmu_acc_kno_uno   = 0,
	Mmu_acc_krwx_uno  = 1,
	Mmu_acc_krwx_urx  = 2,
	Mmu_acc_krwx_urwx = 3,
	Mmu_acc_mask      = 3,
	Mmu_acc_max       = 3,

	Et_l1_inv = 0,
	Et_l1_dir = 1,
	Et_l1_map = 2,
	Et_l2_inv = 0,
	Et_l2_map = 2,
	Et_mask   = 0x3,

};

struct L1_map_t
{
	union
	{
		struct
		{
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			unsigned addr    : 12;
			unsigned _nulls  :  8;
			unsigned acc     :  2;
			unsigned _null_1 :  1;
			unsigned domain  :  4;
			unsigned _null_2 :  1;
			unsigned c       :  1;
			unsigned b       :  1;
			unsigned type    :  2;
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned type    :  2;
			unsigned b       :  1;
			unsigned c       :  1;
			unsigned _null_1 :  1;
			unsigned domain  :  4;
			unsigned _null_2 :  1;
			unsigned acc     :  2;
			unsigned _nulls  :  8;
			unsigned addr    : 12;
			#endif
		};
		unsigned raw;
	};
};

struct L1_dir_t
{
	union
	{
		struct
		{
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			unsigned addr    : 22;
			unsigned _null   :  1;
			unsigned domain  :  4;
			unsigned _nulls  :  3;
			unsigned type    :  2;
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned type    :  2;
			unsigned _nulls  :  3;
			unsigned domain  :  4;
			unsigned _null   :  1;
			unsigned addr    : 22;
			#endif
		};
		unsigned raw;
	};
};

struct L2_map_t
{
	union
	{
		struct
		{
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			unsigned addr    : 20;
			unsigned acc3    :  2;
			unsigned acc2    :  2;
			unsigned acc1    :  2;
			unsigned acc0    :  2;
			unsigned c       :  1;
			unsigned b       :  1;
			unsigned type    :  2;
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned type    :  2;
			unsigned b       :  1;
			unsigned c       :  1;
			unsigned acc0    :  2;
			unsigned acc1    :  2;
			unsigned acc2    :  2;
			unsigned acc3    :  2;
			unsigned addr    : 20;
			#endif
		};
		unsigned raw;
	};
};

inline void mmu_tlb_flush()
{
	asm volatile("mcr p15, 0, r0, c8, c7, 0"); // TLBIALL▫
}

//--------------------------------------------------------------------------------------------------
//  Table cells
//--------------------------------------------------------------------------------------------------
// make dir record
inline void mmu_set_dir(unsigned level, word_t* cell, paddr_t tb)
{
	mmu_assert(level == 1  &&  mmu_isalign(tb, L2_bytes));
	if (level == 1)
	{
		L1_dir_t dir;
		dir.raw    = 0;
		dir.addr   = tb >> 10;
		dir.domain = 0;  // FIXME
		dir.type   = 1;  // ptable
		*cell = dir.raw;
	}
}

// make map record
inline void _mmu_set_l1_map(word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	L1_map_t map;
	map.raw    = 0;
	map.addr   = pa >> 20;
	map.acc    = access;
	map.domain = 0; // FIXME
	map.c      = cachable;
	map.b      = cachable;
	map.type   = 2;  // section
	*cell = map.raw;
	(void) access;
	(void) cachable;
}

// make map record
inline void _mmu_set_l2_map(word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	L2_map_t map;
	map.raw    = 0;
	map.addr   = pa >> 12;
	map.acc0   = access;
	map.c      = cachable;
	map.b      = cachable;
	map.type   = 2;  // section
	*cell = map.raw;
	(void) access;
	(void) cachable;
}

// make map record
inline void mmu_set_map(unsigned level, word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	mmu_assert((level == 1  &&  mmu_isalign(pa, L1_pgsz))  ||
	           (level == 2  &&  mmu_isalign(pa, L2_pgsz)));
	mmu_assert(access <= Mmu_acc_max);
	mmu_assert(cachable <= 1);
	if (level == 1)
		_mmu_set_l1_map(cell, pa, access, cachable);
	else
	if (level == 2)
		_mmu_set_l2_map(cell, pa, access, cachable);
}

// make invalid cell
inline void mmu_set_inv(unsigned level, word_t* cell)
{
	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		*cell = 0;
	else
	if (level == 2)
		*cell = 0;
}

// get paddr from dir record
inline paddr_t mmu_get_dir_pa(unsigned level, word_t* cell)
{
	mmu_assert(level == 1);
	mmu_assert((*cell & Et_mask) == Et_l1_dir);
	if (level == 1)
		return ((L1_dir_t*)cell)->addr << 10;
	return -1;
}

// get paddr from map record
inline paddr_t mmu_get_map_pa(unsigned level, word_t* cell)
{
	mmu_assert(level == 1  ||  level == 2);
	mmu_assert((*cell & Et_mask) == Et_l1_map  ||  (*cell & Et_mask) == Et_l2_map);
	if (level == 1)
		return ((L1_map_t*)cell)->addr << 20;
	else
	if (level == 2)
		return ((L2_map_t*)cell)->addr << 12;
	return -1;
}

// get cachable attribute from map record
inline int mmu_get_map_cached(unsigned level, word_t* cell)
{
	mmu_assert(level == 1  ||  level == 2);
	mmu_assert((*cell & Et_mask) == Et_l1_map  ||  (*cell & Et_mask) == Et_l2_map);
	if (level == 1)
		return ((L1_map_t*)cell)->c;
	else
	if (level == 2)
		return ((L2_map_t*)cell)->c;
	return -1;
}

// get access attribute from map record
inline const char* mmu_get_map_acc(unsigned level, word_t* cell)
{
	mmu_assert(level == 1  ||  level == 2);
	mmu_assert((*cell & Et_mask) == Et_l1_map  ||  (*cell & Et_mask) == Et_l2_map);
	int acc = 0;
	if (level == 1)
		acc = ((L1_map_t*)cell)->acc;
	else
	if (level == 2)
		acc = ((L2_map_t*)cell)->acc0;
	switch (acc)
	{
		case Mmu_acc_kno_uno:    return "k---_u---";
		case Mmu_acc_krwx_uno:   return "krwx_u---";
		case Mmu_acc_krwx_urx:   return "krwx_ur-x";
		case Mmu_acc_krwx_urwx:  return "krwx_urwx";
	}
	return "__unknown_mmu_access__";
}

// is cell dir record
inline bool mmu_is_dir(unsigned level, word_t* cell)
{
	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return ((L1_map_t*)cell)->type == Et_l1_dir;
	return 0;
}

// is cell map record
inline bool mmu_is_map(unsigned level, word_t* cell)
{
	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return ((L1_map_t*)cell)->type == Et_l1_map;
	else
	if (level == 2)
		return ((L2_map_t*)cell)->type == Et_l2_map;
	return 0;
}

// is cell invalid record
inline bool mmu_is_inv(unsigned level, word_t* cell)
{
	mmu_assert(level == 1  ||  level == 2);
	if (level == 1)
		return ((L1_map_t*)cell)->type == Et_l1_inv;
	else
	if (level == 2)
		return ((L2_map_t*)cell)->type == Et_l2_inv;
	return 0;
}

// convert table index to vaddr base
inline addr_t mmu_index2va(unsigned level, unsigned index)
{
	mmu_assert((level == 1  &&  index < L1_sz)  ||  (level == 2  &&  index < L2_sz));
	if (level == 1)
		return index * L1_pgsz;
	else
	if (level == 2)
		return index * L2_pgsz;

	return -1;
}

//--------------------------------------------------------------------------------------------------
//  ~ Table cells
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
inline void mmu_zero(word_t* tb, unsigned sz_bytes)
{
	mmu_assert((mmu_isalign((addr_t)tb, L1_bytes)  &&  sz_bytes == L1_bytes)  ||
	           (mmu_isalign((addr_t)tb, L2_bytes)  &&  sz_bytes == L2_bytes));
	// try to use double instruction
	uint64_t* ptr = (uint64_t*)(void*)tb;
	unsigned sz = sz_bytes / sizeof(uint64_t);
	for (unsigned i=0; i<sz; ++i)
		ptr[i] = 0;
}

//--------------------------------------------------------------------------------------------------
//  MMU registers access
//--------------------------------------------------------------------------------------------------
inline word_t mmu_reg_ctrl()
{
	word_t val;
	asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(val));
	return val;
}

inline void mmu_reg_ctrl(word_t val)
{
	asm volatile ("mcr p15, 0, %0, c1, c0, 0" :: "r"(val));
}

inline word_t mmu_reg_ttbr()
{
	word_t val;
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(val));
	return val;
}

inline void mmu_reg_ttbr(word_t val)
{
	mmu_assert(mmu_isalign(val, 0x4000)); // 16 kB for L1 table
	asm volatile ("mcr p15, 0, %0, c2, c0, 0" :: "r"(val));
}

inline void mmu_root_table(word_t val)
{
	mmu_reg_ttbr(val);
}

inline word_t mmu_reg_domain()
{
	word_t val;
	asm volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r"(val));
	return val;
}

inline void mmu_reg_domain(word_t val)
{
	asm volatile ("mcr p15, 0, %0, c3, c0, 0" :: "r"(val));
}

inline word_t mmu_reg_asid()
{
	word_t val;
	asm volatile ("mrc p15, 0, %0, c13, c0, 1" : "=r"(val));
	return val;
}

inline void mmu_reg_asid(word_t val)
{
	asm volatile ("mcr p15, 0, %0, c13, c0, 1" :: "r"(val));
}

//--------------------------------------------------------------------------------------------------
//  ~ MMU registers access
//--------------------------------------------------------------------------------------------------

#endif // MMU_H
