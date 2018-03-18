//##################################################################################################
//
//  Helpers for low-level system code.
//
//##################################################################################################

#ifndef SYS_UTILS_H
#define SYS_UTILS_H

#include <stdint.h>
#include <stddef.h>

inline size_t min(size_t a, size_t b) { return a < b ? a : b; }
inline size_t max(size_t a, size_t b) { return a > b ? a : b; }

//--------------------------------------------------------------------------------------------------
// helper macros for work with addresses and sizes
//--------------------------------------------------------------------------------------------------
#define round_up(n, a)    (((n)+((a)-1)) & ~((a)-1))
#define align_up(n, a)    round_up(n, a)
#define round_down(n, a)  ((n) & ~((a)-1))
#define align_down(n, a)  round_down(n, a)
#define get_offset(n, a)  ((n) & ((a)-1))
#define is_aligned(n, a)  !get_offset(n, a)

#define round_pg_up(n)    round_up(n, Cfg_page_sz)
#define align_pg_up(n)    align_up(n, Cfg_page_sz)
#define round_pg_down(n)  round_down(n, Cfg_page_sz)
#define align_pg_down(n)  align_down(n, Cfg_page_sz)
#define get_pg_offset(n)  get_offset(n, Cfg_page_sz)
#define is_pg_aligned(n)  is_aligned(n, Cfg_page_sz)

//--------------------------------------------------------------------------------------------------
// helper funcs for work with log2sz
//--------------------------------------------------------------------------------------------------
inline size_t get_size(size_t log2sz)
{
	return 1 << log2sz;
}

inline size_t get_log2sz(size_t sz_bytes)
{
	if (!sz_bytes)
		return 0;

	// find low bit
	size_t log2sz = 0;
	size_t bytes = sz_bytes;
	while (!(bytes & 0x1))
	{
		bytes >>= 1;
		log2sz += 1;
	}

	// checking
	if (get_size(log2sz) != sz_bytes)
		return 0;

	return log2sz;
}

// get low range
inline size_t get_log2sz_low_range(size_t sz_bytes)
{
	if (!sz_bytes)
		return 0;

	// find top bit
	size_t log2sz = 0;
	for (int i=8*sizeof(size_t)-1; i>=0; --i)
	{
		if (sz_bytes >> i)
		{
			log2sz = i;
			break;
		}
	}

	return log2sz;
}

// get top range
inline size_t get_log2sz_top_range(size_t sz_bytes)
{
	if (!sz_bytes)
		return 0;

	// find top bit
	size_t log2sz = 0;
	for (int i=8*sizeof(size_t)-1; i>=0; --i)
	{
		if (sz_bytes >> i)
		{
			log2sz = i;
			break;
		}
	}

	// and round up if need
	if (get_size(log2sz) != sz_bytes)
		log2sz += 1;

	return log2sz;
}

//--------------------------------------------------------------------------------------------------
// access permissions - rwx
//--------------------------------------------------------------------------------------------------
typedef unsigned acc_t;
enum Acc_t
{
	Acc_r = 1 << 2,
	Acc_w = 1 << 1,
	Acc_x = 1 << 0,
	Acc_rx  = Acc_r | Acc_x,
	Acc_rw  = Acc_r | Acc_w,
	Acc_rwx = Acc_r | Acc_w | Acc_x,
	Acc_nil = 0
};

// buf - at least 4 bytes
inline const char* _acc2str(char* buf, unsigned acc)
{
	buf[0] = acc & Acc_r ? 'r' : '-';
	buf[1] = acc & Acc_w ? 'w' : '-';
	buf[2] = acc & Acc_x ? 'x' : '-';
	buf[3] = '\0';
	return buf;
}

#ifdef __cplusplus

// NOTE:  thread unsafe
inline const char* acc2str(unsigned acc)
{
	static char buf[4];
	return _acc2str(buf, acc);
}

#endif // __cplusplus

//--------------------------------------------------------------------------------------------------
// NOTE:  don't use stack vars in this func, stack will be cleared,
//        use register vars if need.
//--------------------------------------------------------------------------------------------------
__attribute__((always_inline)) inline void init_bss()
{
	// linker vars
	extern unsigned _bss_start;
	extern unsigned _bss_end;

	// bss boundaries
	register uintptr_t addr = (uintptr_t)&_bss_start;
	register size_t    sz   = (uintptr_t)&_bss_end - (uintptr_t)&_bss_start;

	// clear, don't use memset() for this, it may use stack vars
	for (register uintptr_t i=0; i<sz; ++i)
		((char*)addr)[i] = 0x00;
}

#ifdef __cplusplus

//--------------------------------------------------------------------------------------------------
// call ctors from ctor-table
//--------------------------------------------------------------------------------------------------
typedef void(*Ctor_callback_t)(uintptr_t addr);
inline void call_ctors(Ctor_callback_t before_cb = 0, Ctor_callback_t after_cb = 0)
{
	// single entry in the CTOR table
	typedef void(*Ctor_t)(void);

	// linker vars
	extern unsigned _ctors_start;
	extern unsigned _ctors_end;

	// begin and end markers of CTORS table
	const Ctor_t* start = (Ctor_t*) &_ctors_start;
	const Ctor_t* end   = (Ctor_t*) &_ctors_end;

	// call
	for (const Ctor_t* ctor=start; ctor!=end; ctor++)
	{
		if (*ctor)
		{
			if (before_cb)
				before_cb((uintptr_t)*ctor);

			(*ctor)();

			if (after_cb)
				after_cb((uintptr_t)*ctor);
		}
	}
}

//--------------------------------------------------------------------------------------------------
// call dtors from dtor-table
//--------------------------------------------------------------------------------------------------
inline void call_dtors()
{
	// single entry in the DTOR table
	typedef void (*Dtor_t) (void);

	// linker vars
	extern unsigned _dtors_start;
	extern unsigned _dtors_end;

	// begin and end markers of CTORS table
	const Dtor_t* start = (Dtor_t*) &_dtors_start;
	const Dtor_t* end   = (Dtor_t*) &_dtors_end;

	// call
	for (const Dtor_t* dtor=start; dtor!=end; dtor++)
		if (*dtor)
			(*dtor)();
}

#endif // __cplusplus

#endif // SYS_UTILS_H
