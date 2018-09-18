//##################################################################################################
//
//  L4 data types.
//
//##################################################################################################

#ifndef L4_TYPES_H
#define L4_TYPES_H

#include "sys-config.h"
#include "sys_types.h"
#include "sys_utils.h"

#ifndef __cplusplus

typedef word_t L4_thrid_t;

#else

//--------------------------------------------------------------------------------------------------
// Thread Id.
// According to l4-x2-r6.pdf (sections 2.1)
class L4_thrid_t
{
public:
	enum
	{
		Nil          = 0,
		Any          = (word_t)-1l,
		Any_local    = (word_t)(-1l & ~0x3fl),  // low 6 bits are nulls
		Def_glob_ver = 2, // default version for global thrid, !=0 and !=1
	};

	union
	{
		word_t _raw;
		struct
		{
			enum
			{
				Number_width  = sizeof(word_t) == 4 ? 18 : 32, // 18/32 for 32/64-bit system
				Version_width = sizeof(word_t) == 4 ? 14 : 32, // 14/32 for 32/64-bit system
			};
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			word_t number  : Number_width;
			word_t version : Version_width;  // last 6 bytes must not be 0, if 1 --> irq
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			word_t version : Version_width;  // last 6 bytes must not be 0, if 1 --> irq
			word_t number  : Number_width;
			#endif
		} _global;
		struct
		{
			enum { Number_width = sizeof(word_t) * 8 - 6 }; // 26/58 for 32/64-bit system
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			word_t   number  : Number_width;
			unsigned nulls   : 6;  // should be 0
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned nulls   : 6;  // should be 0
			word_t   number  : Number_width;
			#endif
		} _local;
	};

	L4_thrid_t(word_t v=0) : _raw(v) {}

	bool operator == (const L4_thrid_t& thrid) const { return _raw == thrid._raw; }
	bool operator != (const L4_thrid_t& thrid) const { return _raw != thrid._raw; }

	inline bool is_nil()       const { return _raw == Nil; }
	inline bool is_any()       const { return _raw == Any; }
	inline bool is_any_local() const { return _raw == Any_local; }
	inline bool is_single()    const { return !is_nil() && !is_any() && !is_any_local(); }
	inline bool is_local()     const { return is_single()  &&  _local.nulls == 0x0; }
	inline bool is_global()    const { return is_single()  &&  _local.nulls != 0x0; }
	//inline bool is_irq()     const { return _global.version == 0x1; }

	inline word_t   raw()      const { return _raw; }
	inline unsigned number()   const { return is_global() ? _global.number : _local.number; }
	inline word_t   version()  const { return _global.version; }

	inline void set(word_t v)        { _raw = v; }

	static inline L4_thrid_t create_global(word_t num, word_t ver = Def_glob_ver)
	{
		L4_thrid_t thrid;
		thrid._global.number = num;
		thrid._global.version = ver; // != 0, != 1 if not irq
		return thrid;
	}

	static inline L4_thrid_t create_irq(word_t num)
	{
		L4_thrid_t thrid;
		thrid._global.number = num;
		thrid._global.version = 0x1;
		return thrid;
	}
	/*
	static inline L4_thrid_t create_local(word_t num)
	{
		L4_thrid_t thrid;
		thrid._local.number = num;
		thrid._local.nulls = 0x0;
		return thrid;
	}
	*/
};

static_assert(sizeof(L4_thrid_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// Time.
// According to l4-x2-r6.pdf (sections 3.1)
typedef uint64_t L4_clock_t;  // 1 clock = 1 usec

//--------------------------------------------------------------------------------------------------
// Time.
// According to l4-x2-r6.pdf (sections 3.3)
class L4_time_t
{
public:
	enum
	{
		Never = 0x0000,
		Zero  = 0x0400,

		Mantissa_len = 10,
		Mantissa_max = (1 << Mantissa_len) - 1,         // 1023

		Exponent_len_rel = 5,
		Exponent_max_rel = (1 << Exponent_len_rel) - 1  // 31
	};

	union
	{
		uint16_t _raw;
		struct
		{
			unsigned is_abs   :  1; // must be 0 for period
			unsigned exp      :  5;
			unsigned mantissa : 10;
		} __attribute__((packed, aligned(2))) _period;
		struct
		{
			unsigned is_abs   :  1; // must be 1 for point
			unsigned exp      :  4;
			unsigned clock    :  1;
			unsigned mantissa : 10;
		} __attribute__((packed, aligned(2))) _point;
	};

	L4_time_t(unsigned raw = Zero) : _raw(raw) {}

	inline bool     is_never() const { return _raw == Never;    }
	inline bool     is_zero()  const { return _raw == Zero;     }
	inline bool     is_abs()   const { return _point.is_abs;    }
	inline bool     is_rel()   const { return !_point.is_abs;   }
	inline unsigned rel_man()  const { return _period.mantissa; }
	inline unsigned rel_exp()  const { return _period.exp;      }
	inline void     never()          { _raw = Never;            }
	inline void     zero()           { _raw = Zero;             }

	inline uint64_t rel_usec() const
	{
		if (_raw == Never)
    		return 0;  // wrong call
		else
			return (uint64_t) _period.mantissa << _period.exp;
	}

	inline uint64_t abs_usec()
	{
		asm volatile ("ta 0x0");
		return 0;
	}

	// translate usec to float point format
	void inline rel(uint64_t usec)
	{
		if (!usec)
		{
			zero();
			return;
		}

		if (usec <= Mantissa_max)
		{
			_period.is_abs = 0;
			_period.exp = 0;
			_period.mantissa = usec;
			return;
		}

		// find MSBit
		int pos = 64;
		while (!((usec >> --pos) && 0x1));

		// write mantissa and exponent
		int exp = pos + 1 - Mantissa_len;
		if (exp > Exponent_max_rel)
		{
			// too big value
			zero();
			return;
		}

		_period.is_abs = 0;
		_period.exp = exp;
		_period.mantissa = usec >> exp;  // NOTE:  round down
	}

	void inline abs(uint64_t usec)
	{
		(void) usec;
		asm volatile ("ta 0x0");
	}

	static inline L4_time_t create_rel(uint64_t usec)
	{
		L4_time_t time;
		time.rel(usec);
		return time;
	}
} __attribute__((packed, aligned(2)));

static_assert(sizeof(L4_time_t) == 2);

//--------------------------------------------------------------------------------------------------
// Ipc timeouts.
// According to l4-x2-r6.pdf (sections 5.6)
class L4_timeouts_t
{
public:
	union
	{
		word_t _raw;
		struct
		{
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			L4_time_t snd;
			L4_time_t rcv;
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			L4_time_t rcv;
			L4_time_t snd;
			#endif
		} _val;
	};
	L4_timeouts_t(L4_time_t s, L4_time_t r) { _val.snd = s, _val.rcv = r; }
	L4_timeouts_t(word_t raw) { _val.snd = raw >> 16, _val.rcv = raw & 0xffff; }
	inline L4_time_t snd() const { return _val.snd; }
	inline L4_time_t rcv() const { return _val.rcv; }
	inline word_t    raw() const { return _raw; }
};

static_assert(sizeof(L4_timeouts_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// Access permissions.
// According to l4-x2-r6.pdf (sections 4.1)
class L4_access_t
{
public:
	enum
	{
		Acc_r = 1 << 2,
		Acc_w = 1 << 1,
		Acc_x = 1 << 0,
		Acc_rw  = Acc_r | Acc_w,
		Acc_rwx = Acc_r | Acc_w | Acc_x
	};

	word_t _val;
};

static_assert(sizeof(L4_access_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// Flex page.
// According to l4-x2-r6.pdf (sections 4.1)
class L4_fpage_t
{
public:
	enum
	{
		Nil      = 0x0,
		Complete = 1 << 4,
		Ioport   = 0x26
	};
	enum
	{
		Acc_r = 1 << 2,
		Acc_w = 1 << 1,
		Acc_x = 1 << 0,
		Acc_rx  = Acc_r | Acc_x,
		Acc_rw  = Acc_r | Acc_w,
		Acc_rwx = Acc_r | Acc_w | Acc_x,
		Size_min = Cfg_page_sz
	};


	union
	{
		word_t _raw;
		struct
		{
			enum { Base_width = sizeof(word_t) * 8 - 10 }; // 22/54 for 32/64-bit system
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			long     base  : Base_width; // base addr >> 10
			unsigned size  : 6;          // size, bytes = 1 << size
			unsigned null  : 1;          // should be 0
			unsigned rwx   : 3;          // access
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned rwx   : 3;          // access
			unsigned null  : 1;          // should be 0
			unsigned size  : 6;          // size, bytes = 1 << size
			long     base  : Base_width; // base addr >> 10
			#endif
		} _mem;
		struct
		{
			enum { Base_width = sizeof(word_t) * 8 - 16 }; // 16/48 for 32/64-bit system
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			long     base  :  Base_width; // base port
			unsigned size  :  6;          // size, bytes = 1 << size
			unsigned magic : 10;          // should be 0x26
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned magic : 10;          // should be 0x26
			unsigned size  : 6;           // size, bytes = 1 << size
			long     base  : Base_width;  // base port
			#endif
		} _iop;
	};

	inline bool     is_io()       const { return _iop.magic == Ioport; }
	inline bool     is_nil()      const { return _raw == Nil; }
	inline bool     is_complete() const { return !_mem.base  &&  _mem.size == 1; }
	inline addr_t   addr()        const { return (addr_t)_mem.base << 10; }
	inline long     base()        const { return _mem.base; }
	inline word_t   size()        const { return 1ul << _mem.size;  }
	inline unsigned log2sz()      const { return _mem.size;       }
	inline unsigned access()      const { return _mem.rwx;        }
	inline word_t   end()         const { return addr() + size(); }
	inline word_t   raw()         const { return _raw;            }

	inline word_t   io_port()     const { return _iop.base; }
	inline word_t   io_size()     const { return 1ul << _iop.size;  }

	inline void     base(long v)        { _mem.base = v;        }
	inline void     addr(addr_t v)      { _mem.base = v >> 10;  }
	inline void     log2sz(unsigned v)  { _mem.size = v;        }
	inline void     access(unsigned v)  { _mem.rwx  = v;        }

	void operator = (word_t v)      { _raw = v; }

	inline void set_nil()       { _raw = Nil;      }
	inline void set_complete()  { _mem.base = 0;  _mem.size = 1; }

	static inline L4_fpage_t create(word_t adr, word_t sz_bytes, unsigned acc)
	{
		L4_fpage_t fpage;
		fpage.set(adr, sz_bytes, acc);
		return fpage;
	}

	static inline L4_fpage_t create_io(word_t port, word_t sz_ports)
	{
		L4_fpage_t fpage;
		fpage.set_io(port, sz_ports);
		return fpage;
	}

	static inline L4_fpage_t create(word_t raw)
	{
		L4_fpage_t fpage;
		fpage._raw = raw;
		return fpage;
	}

	static inline L4_fpage_t create_nil()
	{
		L4_fpage_t fpage;
		fpage.set_nil();
		return fpage;
	}

	static inline L4_fpage_t create_complete()
	{
		L4_fpage_t fpage;
		fpage.set_complete();
		return fpage;
	}

	// sz_bytes = 1, 2, 4, 8, 16, 32, ...
	inline void set(word_t adr, word_t sz_bytes, unsigned acc)
	{
		if (sz_bytes < Size_min  ||  !is_aligned(adr, sz_bytes))
		{
			_raw = Nil;
		}
		else
		{
			_mem.base = adr >> 10;
			_mem.size = 0;
			_mem.rwx = acc & Acc_rwx;

			// find single bit in sz_bytes
			word_t bytes = sz_bytes;
			while (!(bytes & 0x1))
			{
				bytes >>= 1;
				_mem.size += 1;
			}

			// check
			if (size() != sz_bytes)
				_raw = Nil;
		}
	}

	// sz_ports = 1, 2, 4, 8, 16, 32, ...
	inline void set_io(word_t port, word_t sz_ports)
	{
		if (!sz_ports  ||  port > 0xffff  ||  (port+sz_ports) > 0xffff  ||  !is_aligned(port, sz_ports))
		{
			_raw = Nil;
		}
		else
		{
			_iop.base = port;
			_iop.size = 0;
			_iop.magic = Ioport;

			// find single bit in sz_ports
			word_t ports = sz_ports;
			while (!(ports & 0x1))
			{
				ports >>= 1;
				_iop.size += 1;
			}

			// check
			if (io_size() != sz_ports)
				_raw = Nil;
		}
	}

	inline bool is_cross(L4_fpage_t p)
	{
		// TODO:  check cases Nil and Complete
		return !(end() <= p.addr()  ||  addr() >= p.end());
	}

	// Is 'p' inside this?
	inline bool is_inside(L4_fpage_t p)
	{
		// TODO:  check cases Nil and Complete
		return p.addr() >= addr()  &&  p.end() <= end();
	}

	// Is 'p' inside this and acc allows to access?
	inline bool is_acc_fit(unsigned acc)
	{
		return (acc & access()) == acc;
	}

	// Is 'p' inside this and acc allows to access?
	inline bool is_inside_acc(L4_fpage_t p)
	{
		return is_inside(p)  &&  is_acc_fit(p.access());
	}
};

static_assert(sizeof(L4_fpage_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// Typed item.
// According to l4-x2-r6.pdf (sections 5.1)
class L4_typed_item_t
{
public:
	enum
	{
		Map_item        = 4,
		Grant_item      = 5,
		String_item_max = 3  // bits pattern is '0hh', hh may be any
	};

	union
	{
		word_t _raw[2];
		struct
		{
			enum { Reserve_width = sizeof(word_t) * 8 - 4 }; // 28/60 for 32/64-bit systems
			word_t   reserve  : Reserve_width;  //
			unsigned type     :  3;  // type code of item - string, map or grant
			unsigned cont     :  1;  // continue flag
		} _val;
	};

	inline bool is_last()        const { return !_val.cont; }
	inline bool is_map_item()    const { return _val.type == Map_item; }
	inline bool is_grant_item()  const { return _val.type == Grant_item; }
	inline bool is_string_item() const { return _val.type <= String_item_max; }

	inline word_t word0() const { return _raw[0]; }
	inline word_t word1() const { return _raw[1]; }

	inline void set(word_t w0, word_t w1)
	{
		_raw[0] = w0;
		_raw[1] = w1;
	}

	static inline L4_typed_item_t create(word_t w0, word_t w1)
	{
		L4_typed_item_t item;
		item.set(w0, w1);
		return item;
	}
};

static_assert(sizeof(L4_typed_item_t) == 2 * sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// Map item.
// According to l4-x2-r6.pdf (sections 5.2)
class L4_map_item_t
{
public:
	union
	{
		word_t _raw[2];
		struct
		{
			struct
			{
				enum { Snd_base_width = sizeof(word_t) * 8 - 10 }; // 22/54 for 32/64-bit systems
				word_t   snd_base : Snd_base_width; // send base >> 10
				unsigned nulls    : 6;              // should be nulls
				unsigned type     : 3;              // should be 4 for map item and 5 for grant item
				unsigned cont     : 1;              // continue flag
			} s;
			L4_fpage_t fpage;      // send fpage
		} _val;
	};

	inline word_t word0() const { return _raw[0]; }
	inline word_t word1() const { return _raw[1]; }

	inline void set(word_t w0, word_t w1)
	{
		_raw[0] = w0;
		_raw[1] = w1;
	}

	inline void set(L4_fpage_t fpg, bool cont = false)
	{
		_val.fpage = fpg;
		_val.s.snd_base = 0;
		_val.s.nulls = 0;
		_val.s.type = L4_typed_item_t::Map_item;
		_val.s.cont = cont;
	}

	inline void set_grant()
	{
		_val.s.type = L4_typed_item_t::Grant_item;
	}

	static inline L4_map_item_t create(L4_fpage_t fpage, bool cont = false)
	{
		L4_map_item_t item;
		item.set(fpage, cont);
		return item;
	}

	static inline L4_map_item_t create(word_t w0, word_t w1)
	{
		L4_map_item_t item;
		item.set(w0, w1);
		return item;
	}

	inline bool is_map()   const { return _val.s.type == L4_typed_item_t::Map_item; }
	inline bool is_grant() const { return _val.s.type == L4_typed_item_t::Grant_item; }
	inline bool is_last()  const { return !_val.s.cont; }
	inline L4_fpage_t& fpage() { return _val.fpage; }
	inline addr_t snd_base()   { return _val.s.snd_base; }
};

static_assert(sizeof(L4_map_item_t) == 2 * sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// Grant item.
// According to l4-x2-r6.pdf (sections 5.3)
class L4_grant_item_t : public L4_map_item_t  // the same as Map Item
{
public:
	inline void set(L4_fpage_t fpg)
	{
		L4_map_item_t::set(fpg);
		_val.s.type = L4_typed_item_t::Grant_item;
	}
};

static_assert(sizeof(L4_grant_item_t) == 2 * sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// String item.
// According to l4-x2-r6.pdf (sections 5.4)
class L4_string_item_t
{
public:
	enum
	{
		Length_width = sizeof(word_t) * 8 - 10, // 22/54 for 32/64-bit systems
		Max_length = 1ull << Length_width       // 4 MB / 0x4000 GB for 32/64-bit systems
	};

	union
	{
		word_t _raw[2];
		struct
		{
			struct
			{
				word_t  length      : Length_width;  // string length
				unsigned nulls      : 7;  // should be 0
				unsigned cache_hint : 2;  // cacheability hint, processor dependend, use 00
				unsigned cont       : 1;  // continue flag
			} s;
			word_t ptr;                   // pointer to buffer
		} _simple;
		struct
		{
			struct
			{
				word_t   length         : Length_width;  // substring length
				unsigned compound_cont  : 1;  // commpound string continue flag
				unsigned last           : 5;  // index of last substring in compound string, equal (size - 1)
				unsigned nulls          : 1;  // should be 0
				unsigned cache_hint     : 2;  // cacheability hint, processor dependend, use 00
				unsigned cont           : 1;  // item continue flag
			} s;
			word_t ptrs;                      // pointers to buffers
		} _compound;
		// NOTE:  simple string is the a special case of compound string
	};

	inline word_t word0() const { return _raw[0]; }
	inline word_t word1() const { return _raw[1]; }

	inline unsigned  substring_number()      const { return _compound.s.last + 1;      }
	inline bool      is_string_continue()    const { return _compound.s.compound_cont; }
	inline unsigned  length()                const { return _compound.s.length;        }
	inline uintptr_t pointer(unsigned i)     const { return (&_compound.ptrs)[i];      }
	inline uintptr_t pointer()               const { return pointer(0);                }
	inline bool      is_last()               const { return !_simple.s.cont;           }

	void inline cont(bool c) { _compound.s.cont = c; }

	inline void pointer(unsigned i, word_t p) { (&_compound.ptrs)[i] = p; }

	void inline simple(word_t ptr, unsigned len, bool c = false)
	{
		_simple.ptr          = (word_t) ptr;
		_simple.s.length     = len;
		_simple.s.nulls      = 0;
		_simple.s.cache_hint = 0;
		_simple.s.cont       = c;
	}

	inline void set(word_t w0, word_t w1)
	{
		_raw[0] = w0;
		_raw[1] = w1;
	}

	inline bool is_string_item() const
	{
		return L4_typed_item_t::create(_raw[0], _raw[1]).is_string_item();
	}

	static inline L4_string_item_t create(word_t w0, word_t w1)
	{
		L4_string_item_t item;
		item.set(w0, w1);
		return item;
	}

	static inline L4_string_item_t create_simple(word_t ptr, unsigned len, bool c = false)
	{
		L4_string_item_t item;
		item.simple(ptr, len, c);
		return item;
	}
};

static_assert(sizeof(L4_string_item_t) == 2 * sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// Acceptor.
// According to l4-x2-r6.pdf (sections 5.5)
class L4_acceptor_t
{
public:
	union
	{
		word_t _raw;
		L4_fpage_t _fpage;          // fpage without access flags
		struct
		{
			enum { Reserve_width = sizeof(word_t) * 8 - 1 };  // 31/63 for 32/64-bit systems
			#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			word_t   reserve : Reserve_width;
			unsigned string  :  1;  // stringItems are accepted iff string == 1
			#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned string  :  1;  // stringItems are accepted iff string == 1
			word_t   reserve : Reserve_width;
			#endif
		} _flags;
	};

	L4_acceptor_t(word_t v = 0) : _raw(v) {}
	L4_acceptor_t(L4_fpage_t v, bool str) { set(v, str); }

	inline word_t raw()              const  { return _raw;           }
	inline L4_fpage_t fpage()        const  { return _fpage;         }
	inline bool string_accepted()    const  { return _flags.string;  }
	inline void fpage(L4_fpage_t v)         { _raw = v.raw() & ~7;   }
	inline void string_accepted(bool str)   { _flags.string = str;   }
	inline void set(L4_fpage_t v, bool str) { _raw = v.raw() & ~7;  _flags.string = str; }

	static inline L4_acceptor_t create(L4_fpage_t fp, bool str = false)
	{
		L4_acceptor_t a;
		a.set(fp, str);
		return a;
	}
};

static_assert(sizeof(L4_acceptor_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// MsgTag.
// According to l4-x2-r6.pdf (sections 5.1 and 5.6).
class L4_msgtag_t
{
public:
	// labels
	enum
	{
		// Protocols
		Thread_start     =  0,  // thread start proto
		Interrupt        = -1,  // interrupt proto
		Pagefault        = -2,  // pagefault proto
		Io_pagefault     = -3,  // IO pagefault proto
		System_exception = -4,  // for all architectures
		Arch_exception   = -5,  // architecture specific exceptions
		Force_exception  = -6,  // user raised force exception
		Mmu_exception    = -7,  // pagefault exception
		Sigma0           = -8,  // sigma0 proto
	};

	#define WRM_ext 2
	union
	{
		word_t _raw;
		struct
		{
			enum { Label_width = sizeof(word_t) * 8 - 16 -WRM_ext };  // 16/48 for 32/64-bit systems
			long     label          : Label_width; // msg label for IPC msg
			unsigned error          : 1; // result of IPC operation
			unsigned cross_proc     : 1; // received IPC came from another processor
			unsigned redirected     : 1; // reserved IPC was redirected, see TCR.intended_receiver
			unsigned propagated     : 1; // propagation, see TCR virt_sender/actual_sender
			unsigned typed_words    : 6;
			unsigned untyped_words  : 6 +WRM_ext;
		} _ipc;
		struct
		{
			enum { Label_width = sizeof(word_t) * 8 - 20 -WRM_ext };  // 12/44 for 32/64-bit systems
			long     label          : Label_width; // msg label for PROTO msg
			unsigned null           : 1; // 0
			unsigned rwx            : 3; // pagefault access
			unsigned nulls          : 4; // result of IPC operation
			unsigned typed_words    : 6;
			unsigned untyped_words  : 6 +WRM_ext;
		} _proto_pf; // memory pfault or io pfault
		struct
		{
			enum { Label_width = sizeof(word_t) * 8 - 20 -WRM_ext };  // 12/44 for 32/64-bit systems
			long     label          : Label_width; // msg label for PROTO msg
			unsigned nulls          : 8;
			unsigned typed_words    : 6;
			unsigned untyped_words  : 6 +WRM_ext;
		} _proto;
	};

	L4_msgtag_t(word_t v=0) : _raw(v) {}

	// API for ipc msg
	inline word_t   raw()                   const { return _raw;       }
	inline long     ipc_label()             const { return _ipc.label; }
	inline bool     ipc_is_failed()         const { return _ipc.error; }
	inline void     ipc_label(long l)             { _ipc.label = l;    }
	inline void     ipc_set_ok()                  { _ipc.error = 0;    }
	inline void     ipc_set_failed()              { _ipc.error = 1;    }
	inline void     ipc_set_failed(bool err)      { _ipc.error = err;  }

	void set_ipc(long label, unsigned untyped, unsigned typed)
	{
		_raw = 0;
		_ipc.label         = label;
		_ipc.typed_words   = typed;
		_ipc.untyped_words = untyped;
	}

	// API for proto msg
	inline long     proto_label()           const { return _proto.label; }
	inline void     proto_label(long v)           { _proto.label = v;    }
	inline void     proto_nulls()                 { _proto.nulls = 0;    }

	void set_proto(int label, unsigned untyped, unsigned typed)
	{
		_proto.label         = label;
		_proto.nulls         = 0;
		_proto.typed_words   = typed;
		_proto.untyped_words = untyped;
	}

	// API for pagefault proto msg
	inline unsigned pfault_access()         const { return _proto_pf.rwx; }
	inline void     pfault_access(unsigned v)     { _proto_pf.rwx = v;    }

	void set_proto_pf(int label, unsigned access, unsigned untyped, unsigned typed)
	{
		_proto_pf.label         = label;
		_proto_pf.null          = 0;
		_proto_pf.rwx           = access;
		_proto_pf.nulls         = 0;
		_proto_pf.typed_words   = typed;
		_proto_pf.untyped_words = untyped;
	}

	// API for any msg
	inline unsigned typed()                 const { return _ipc.typed_words;   }
	inline unsigned untyped()               const { return _ipc.untyped_words; }
	inline bool     propagated()            const { return _ipc.propagated;    }

	inline void     typed(unsigned v)             { _ipc.typed_words   = v;    }
	inline void     untyped(unsigned v)           { _ipc.untyped_words = v;    }
	inline void     propagated(bool v)            { _ipc.propagated    = v;    }

	inline bool is_thread_start() const
	{
		return ipc_label()==Thread_start  &&  typed()==0  &&  untyped()==3;
	}

	inline bool is_pf_request() const
	{
		return proto_label()==Pagefault  &&  typed()==2  &&  untyped()==0;
	}

	inline bool is_pf_reply() const
	{
		return ipc_label()==0  &&  typed()==2  &&  untyped()==0;
	}

	inline bool is_io_pf_request() const
	{
		return proto_label()==Io_pagefault  &&  typed()==2  &&  untyped()==0;
	}

	inline bool is_io_pf_reply() const
	{
		return ipc_label()==0  &&  typed()==2  &&  untyped()==0;
	}

	inline bool is_exc_request() const
	{
		return (proto_label()==System_exception || proto_label()==Arch_exception ||
		        proto_label()==Force_exception  || proto_label()==Mmu_exception) &&
		        typed()==0  /* &&  untyped()==(1+132)*/ ;
	}

	inline bool is_exc_reply() const
	{
		return ipc_label()==0  &&  typed()==0  &&  untyped()==(1+132);
	}
};

static_assert(sizeof(L4_msgtag_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// IpcErrorCode.
// According to l4-x2-r6.pdf (section 5.6)
class L4_ipcerr_t
{
public:
	enum
	{
		Snd = 0,
		Rcv = 1
	};

	union
	{
		word_t _raw;
		struct
		{
			enum { Offset_width = sizeof(word_t) * 8 - 4 };  // 28/60 for 32/64-bit systems
			unsigned long offset    : Offset_width; // byte's number that successfully through string items
			unsigned error          : 3; // error code
			unsigned phase          : 1; // error phase:  0 send-phase, 1 receive-phase
		} _val;
	};

	L4_ipcerr_t(word_t v) : _raw(v) {}
	L4_ipcerr_t(unsigned p, unsigned e, unsigned o = 0)
	{
		_val.phase  = p;
		_val.error  = e;
		_val.offset = o;
	}

	inline word_t raw()           const { return _raw;              }
	inline word_t transferred()   const { return _val.offset;       }
	inline word_t error()         const { return _val.error;        }
	inline bool   is_snd_error()  const { return _val.phase == Snd; }
	inline bool   is_rcv_error()  const { return _val.phase == Rcv; }

	inline void snd_error(word_t e) { _val.error = e;  _val.phase = Snd; }
	inline void rcv_error(word_t e) { _val.error = e;  _val.phase = Rcv; }
};

enum L4_ipc_phase_t
{
	L4_snd_phase = L4_ipcerr_t::Snd,
	L4_rcv_phase = L4_ipcerr_t::Rcv
};

static_assert(sizeof(L4_ipcerr_t) == sizeof(word_t));

//--------------------------------------------------------------------------------------------------
// User-level thread control block.
// According to l4-x2-r6.pdf (sections 1.1 and 7.7)
class L4_utcb_t
{
public:

	enum { Mr_words = 256 };  // orig sz=64, wrm increase up to 256 words

	// Thread Control Registers (TCRs)
	word_t _global_id;                        //  0  ro  my global ID
	word_t _pager;                            //  1  rw
	word_t _exc_handler;                      //  2  rw
	word_t _user_defined_handle;              //  3  rw
	word_t _xfer_timeouts;                    //  4  rw
	word_t _error;                            //  5  ro  ErrorCode
	word_t _receiver;                         //  6  ro  IntendedReceiver
	word_t _sender;                           //  7  rw  VirtualSender / ActualSender
	word_t _proc_no;                          //  8  ro  ProcessorNo
	uint8_t _preempt_flags;                   //  9  rw  byte 1
	uint8_t _cop_flags;                       //  9  wo  byte 2
	uint8_t _reserve [ sizeof(word_t) - 2 ];  //  9      alignment

	// User-level Thread Local Storage (UTLS)
	// word 0 - thread name
	// word 1 - errno
	word_t tls [21];                          // 10..30  alignment

	// Buffer Registers (BRs)
	word_t br [33];                           // 31..63

	// Message Registers (MRs)
	word_t mr [Mr_words];                     // 64..(64 + Mr_words - 1)

	// FIXME:  no access to regs via pointer, check it !!!

	inline L4_thrid_t     global_id()           const { return (L4_thrid_t) _global_id; }
	inline L4_thrid_t     local_id()            const { return (word_t) this;           }
	inline L4_msgtag_t    msgtag()              const { return (L4_msgtag_t) mr[0];     }
	inline L4_acceptor_t  acceptor()            const { return (L4_acceptor_t) br[0];   }
	inline word_t         error()               const { return _error;                  }
	inline L4_ipcerr_t    ipc_error_code()      const { return (L4_ipcerr_t) _error;    }
	inline L4_thrid_t     sender()              const { return (L4_thrid_t) _sender;    }
	inline L4_thrid_t     exception_handler()   const { return _exc_handler;            }
	inline word_t         user_defined_handle() const { return _user_defined_handle;    }

	inline void global_id(L4_thrid_t v)         { _global_id   = v.raw(); }
	inline void msgtag(L4_msgtag_t v)           { mr[0]        = v.raw(); }
	inline void error(int v)                    { _error       = v;       }
	inline void ipc_error_code(L4_ipcerr_t v)   { _error       = v.raw(); }
	inline void sender(L4_thrid_t v)            { _sender      = v.raw(); }
	inline void acceptor(L4_acceptor_t v)       { br[0]        = v.raw(); }
	inline void exception_handler(L4_thrid_t v) { _exc_handler = v.raw(); }
	inline void user_defined_handle(word_t v)   { _user_defined_handle = v; }
};

#endif // __cplusplus

#endif // L4_TYPES_H
