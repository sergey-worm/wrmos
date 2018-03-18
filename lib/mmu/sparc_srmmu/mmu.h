//##################################################################################################
//
//  Sparc MMU low-level helpers.
//
//  Where are 3 level of paging and Context Table (L0):
//
//    Level  Name     TableSize  PageSize  HasMap   HasDir
//        0  context     0x100       4 GB       -        +
//        1  region      0x100      16 MB       +        +
//        2  segment      0x40     256 kB       +        +
//        3  page         0x40       4 kB       +        -
//
//##################################################################################################

#ifndef MMU_H
#define MMU_H

#include "sys_proc.h"

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
	Ctx_sz = 0x100,
	L1_sz  = 0x100,
	L2_sz  =  0x40,
	L3_sz  =  0x40,

	// table size in bytes
	Ctx_bytes = Ctx_sz * sizeof(word_t),
	L1_bytes  = L1_sz  * sizeof(word_t),
	L2_bytes  = L2_sz  * sizeof(word_t),
	L3_bytes  = L3_sz  * sizeof(word_t),

	L1_pgsz   = 0x1000000,  //  16 MB
	L2_pgsz   = 0x40000,    // 256 kB
	L3_pgsz   = 0x1000,     //   4 kB

	NotCachable   = 0,
	Cachable      = 1,

	Mmu_acc_kro_uro   = 0,
	Mmu_acc_krw_urw   = 1,
	Mmu_acc_krx_urx   = 2,
	Mmu_acc_krwx_urwx = 3,
	Mmu_acc_kxo_uxo   = 4,
	Mmu_acc_krw_uro   = 5,
	Mmu_acc_krx_uno   = 6,
	Mmu_acc_krwx_uno  = 7,
	Mmu_acc_max       = 7,
	Mmu_acc_mask      = 7,

	Probe_l3      = 0,
	Probe_l2      = 1,
	Probe_l1      = 2,
	Probe_ctx     = 3,
	Probe_entire  = 4,

	Flush_l3      = 0,
	Flush_l2      = 1,
	Flush_l1      = 2,
	Flush_ctx     = 3,
	Flush_entire  = 4,

	Reg_ctrl  = 0x000,
	Reg_ctxtb = 0x100,
	Reg_ctx   = 0x200,
	Reg_fsr   = 0x300,
	Reg_far   = 0x400,

	Et_inv    = 0,
	Et_ptd    = 1,
	Et_pte    = 2,
	Et_mask   = 0x3,
};

struct Ptd
{
	unsigned ptp : 30;
	unsigned et  :  2;
};

struct Pte
{
	unsigned ppn : 24;
	unsigned c   :  1;
	unsigned m   :  1;
	unsigned r   :  1;
	unsigned acc :  3;
	unsigned et  :  2;
};

//--------------------------------------------------------------------------------------------------

inline void flush_dcache()
{
	Proc::sta(0, 0, Proc::Asi_flush_dcache);
}

inline void mmu_tlb_flush()
{
	Proc::sta(0, 0, Proc::Asi_flush_mmu_tlb);
	//Proc::sta(1, 0x400, Proc::Asi_flush_mmu_tlb);  // in HelenOS and Embox are used 0x400, why?
}

//--------------------------------------------------------------------------------------------------
//  Table cells
//--------------------------------------------------------------------------------------------------
// make dir record
inline void mmu_set_dir(unsigned level, word_t* cell, paddr_t tb)
{
	(void)level;
	mmu_assert((level == 0  &&  mmu_isalign(tb, L1_bytes))  ||
	           (level == 1  &&  mmu_isalign(tb, L2_bytes))  ||
	           (level == 2  &&  mmu_isalign(tb, L3_bytes)));
	*cell = ((tb >> 4) & ~(3)) | Et_ptd;
}

// make map record
inline void mmu_set_map(unsigned level, word_t* cell, paddr_t pa, unsigned access, unsigned cachable)
{
	(void)level;
	mmu_assert(mmu_isalign(pa, 0x1000)); // least 4 kB (for L2 and L3 tables)
	mmu_assert(access <= Mmu_acc_max);
	mmu_assert(cachable <= 1);
	*cell = ((pa >> 4) & 0xffffff00) | (cachable << 7) | (access << 2) | Et_pte;
}

// make invalid cell
inline void mmu_set_inv(unsigned level, word_t* cell)
{
	(void)level;
	*cell = Et_inv;
}

// get paddr from dir record
inline paddr_t mmu_get_dir_pa(unsigned level, word_t* cell)
{
	(void)level;
	mmu_assert((*cell & Et_mask) == Et_ptd);
	return (*cell & 0xfffffffc) << 4;
}

// get paddr from map record
inline paddr_t mmu_get_map_pa(unsigned level, word_t* cell)
{
	(void)level;
	mmu_assert((*cell & Et_mask) == Et_pte);
	return (*cell & 0xffffff00) << 4;
}

// get cachable attribute from map record
inline int mmu_get_map_cached(unsigned level, word_t* cell)
{
	(void)level;
	mmu_assert((*cell & Et_mask) == Et_pte);
	return (*cell >> 7) & 0x1;
}

// get access attribute from map record
inline const char* mmu_get_map_acc(unsigned level, word_t* cell)
{
	(void)level;
	mmu_assert((*cell & Et_mask) == Et_pte);
	int acc = (*cell >> 2) & Mmu_acc_mask;
	switch (acc)
	{
		case Mmu_acc_kro_uro:    return "kr--_ur--";
		case Mmu_acc_krw_urw:    return "krw-_urw-";
		case Mmu_acc_krx_urx:    return "kr-x_ur-x";
		case Mmu_acc_krwx_urwx:  return "krwx_urwx";
		case Mmu_acc_kxo_uxo:    return "k--x_u--x";
		case Mmu_acc_krw_uro:    return "krw-_ur--";
		case Mmu_acc_krx_uno:    return "kr-x_u---";
		case Mmu_acc_krwx_uno:   return "krwx_u---";
	}
	return "__unknown_mmu_access__";
}

// is cell dir record
inline bool mmu_is_dir(unsigned level, word_t* cell)
{
	(void)level;
	return (*cell & Et_mask) == Et_ptd;
}

// is cell map record
inline bool mmu_is_map(unsigned level, word_t* cell)
{
	(void)level;
	return (*cell & Et_mask) == Et_pte;
}

// is cell invalid record
inline bool mmu_is_inv(unsigned level, word_t* cell)
{
	(void)level;
	return (*cell & Et_mask) == Et_inv;
}

// convert table index to vaddr base
inline addr_t mmu_index2va(unsigned level, unsigned index)
{
	mmu_assert((level==1 && index<L1_sz) || (level==2 && index<L2_sz) || (level==3 && index<L3_sz));
	if (level == 1)
		return index * L1_pgsz;
	else
	if (level == 2)
		return index * L2_pgsz;
	else
	if (level == 3)
		return index * L3_pgsz;

	return -1;
}

//--------------------------------------------------------------------------------------------------
//  ~ Table cells
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
inline void mmu_zero(word_t* tb, unsigned sz_bytes)
{
	mmu_assert((mmu_isalign((addr_t)tb, 1024)  &&  sz_bytes == 1024)  ||  // l0 or l1 table
	           (mmu_isalign((addr_t)tb, 256)   &&  sz_bytes ==  256));    // l2 or l3 table
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
	return Proc::lda(Reg_ctrl, Proc::Asi_mmu_reg);
}

inline void mmu_reg_ctrl(word_t ctrl)
{
	Proc::sta(ctrl, Reg_ctrl, Proc::Asi_mmu_reg);
}

// read value of MMU.ctx register
inline word_t mmu_reg_ctx()
{
	return (Proc::lda(Reg_ctx, Proc::Asi_mmu_reg));
}

// write value to MMU.ctx register
inline void mmu_reg_ctx(word_t ctx)
{
	mmu_assert(ctx <= 0xff);
	Proc::sta(ctx, Reg_ctx, Proc::Asi_mmu_reg);
}

// read physphys addr of MMU.ctxtb register
inline word_t mmu_reg_ctxtb()
{
	return (Proc::lda(Reg_ctxtb, Proc::Asi_mmu_reg) << 4);
}

// write phys addr of context_table to MMU.ctxtb register
inline void mmu_reg_ctxtb(paddr_t ctxtb)
{
	mmu_assert(mmu_isalign(ctxtb, 0x400)); // 1024 B for L0 table
	Proc::sta((ctxtb >> 4), Reg_ctxtb, Proc::Asi_mmu_reg);
}
//--------------------------------------------------------------------------------------------------
//  ~ MMU registers access
//--------------------------------------------------------------------------------------------------

#endif // MMU_H
