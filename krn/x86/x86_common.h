//##################################################################################################
//
//  Common arch code for x86 and x86_64.
//
//##################################################################################################

#ifndef COMMON_H
#define COMMON_H

#include "krn-config.h"
#include "printk.h"
#include "panic.h"
#include <string.h>

//--------------------------------------------------------------------------------------------------
//  TSS
//--------------------------------------------------------------------------------------------------

#ifdef Cfg_arch_x86
// struct describing a Task State Segment
struct Tss_t
{
	uint32_t prev_tss;
	uint32_t esp0;       // will load in kernel entry
	uint32_t ss0;        // will load in kernel entry
	uint32_t esp1;
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;         // will load in kernel entry
	uint32_t cs;         // will load in kernel entry
	uint32_t ss;         // will load in kernel entry
	uint32_t ds;         // will load in kernel entry
	uint32_t fs;         // will load in kernel entry
	uint32_t gs;         // will load in kernel entry
	uint32_t ldt;
	uint16_t trap;
	uint16_t iomap_base;

	inline void set_ksp(uint32_t sp) { esp0 = sp; }
	inline uint32_t get_ksp() { return esp0; }

	inline void init()
	{
		// init TSS
		memset(this, 0, sizeof(Tss_t));
		ss0  = 0x10;  // kernel stack segment
		esp0 = 0;     // kernel stack pointer will be set after

		// Here we set the cs, ss, ds, es, fs and gs entries in the TSS. These specify what
		// segments should be loaded when the processor switches to kernel mode. Therefore
		// they are just our normal kernel code/data segments - 0x08 and 0x10 respectively,
		// but with the last two bits set, making 0x0b and 0x13. The setting of these bits
		// sets the RPL (requested privilege level) to 3, meaning that this TSS can be used
		// to switch to kernel mode from ring 3.
		cs = 0x0b;
		ss = ds = es = fs = 0x13;
		gs = 0x3b;

		// Load the index of our TSS structure - The index is
		// 0x28, as it is the 5th selector and each is 8 bytes
		// long, but we set the bottom two bits (making 0x2b)
		// so that it has an RPL of 3, not zero.
		// Load 0x2b into the task state register.
		//asm volatile ("push %ax;  mov $0x2b, %ax;  ltr %ax;  pop %ax"); // FIXME:  why RPL=3 ?
		asm volatile ("push %ax;  mov $0x2b, %ax;  ltr %ax;  pop %ax");
	}
} __attribute__((packed));

#else // Cfg_arch_x86_64

struct Tss_t
{
	uint32_t	reserved1;
	uint64_t	rsp0;
	uint64_t	rsp1;
	uint64_t	rsp3;
	uint32_t	reserved2;
	uint32_t	reserved3;
	uint64_t	ist[7];
	uint32_t	reserved4;
	uint32_t	reserved5;
	uint32_t	iobase;

	inline void set_ksp(uint64_t sp) { rsp0 = sp; }
	inline uint64_t get_ksp() const  { return rsp0; }

	inline void init()
	{
		//assert(false && "IMPL ME");

		// Load the index of our TSS structure - The index is
		// 0x28, as it is the 5th selector and each is 8 bytes
		// long, but we set the bottom two bits (making 0x2b)
		// so that it has an RPL of 3, not zero.
		// Load 0x2b into the task state register.
		asm volatile ("push %ax;  mov $0x28, %ax;  ltr %ax;  pop %ax");
	}
} __attribute__((packed));
#endif

enum
{
	Priv_kernel,
	Priv_user,

	Opsz_32bit,
	Opsz_64bit,

	Type_code,
	Type_data,
	Type_system,

	Limit_bytes,
	Limit_4kbloks,
};

//--------------------------------------------------------------------------------------------------
//  GDT
//--------------------------------------------------------------------------------------------------

struct Glob_desc_t
{
	uint16_t _limit_low16;  //
	uint16_t _base_low16;   //
	uint8_t  _base_mid8;    //

	union
	{
		// differents part for data, code and system sygment
		struct
		{
			unsigned a               : 1;  // accessed
			unsigned w               : 1;  // writable
			unsigned e               : 1;  // expansion direction
			unsigned is_code         : 1;  // '0' for data
		} __attribute__((packed)) _data;
		struct
		{
			unsigned a               : 1;  // accessed
			unsigned r               : 1;  // readable
			unsigned c               : 1;  // conforming
			unsigned is_code         : 1;  // '1' for code
		} __attribute__((packed)) _code;
		struct
		{
			unsigned type            : 4;  //
		} __attribute__((packed)) _system;

		// comon part
		struct
		{
			unsigned _                : 4;  // differents
			unsigned _is_code_or_data : 1;  // '0' for system
			unsigned _dpl             : 2;  //
			unsigned _present         : 1;  //
			unsigned _limit_hi4       : 4;  //
			unsigned _avl             : 1;  //
			unsigned _lmode           : 1;  // '0' for system
			unsigned _opsz            : 1;  // '0' for system
			unsigned _g               : 1;  // granularity, 0 - limit in bytes, 1 - limit in 4 kB bloks
		} __attribute__((packed));

	};
	uint8_t _base_hi8;

	void set(addr_t base, uint32_t limit, int gran, int type, int priv, int opsz)
	{
		// comon fields for all selector types
		_limit_low16     = limit & 0xffff;
		_base_low16      = base & 0xffff;
		_base_mid8       = (base >> 16) & 0xff;
		_base_hi8        = base >> 24;
		_limit_hi4       = limit >> 16;
		_dpl             = priv == Priv_kernel ? 0 : 3;
		_present         = 1;
		_g               = gran == Limit_4kbloks ? 1 : 0;
		_avl             = 0; // avail to sys prog (?)
		_is_code_or_data = type == Type_system ? 0 : 1;

		if (type == Type_code)
		{
			_code.a       = 0; // accesed
			_code.r       = 1; // readable
			_code.c       = 0; // conforming
			_code.is_code = 1; //
			_opsz         = opsz == Opsz_32bit ? 1 : 0;
			_lmode        = opsz == Opsz_64bit ? 1 : 0;
		}
		else if (type == Type_data)
		{
			_data.a       = 0; // accesed
			_data.w       = 1; // writable
			_data.e       = 0; // expansion direction
			_code.is_code = 0; //
			_opsz         = opsz == Opsz_32bit ? 1 : 0;
			_lmode        = opsz == Opsz_64bit ? 1 : 0;
		}
		else if (type == Type_system)
		{
			_system.type  = 9; // 32/64-bit TSS (available)
			_opsz         = 0;
			_lmode        = 0;

			#ifdef Cfg_arch_x86_64
			// in 64-bit mode the TSS descriptor is expanded to 16 byte
			((uint32_t*)this)[2] = base >> 32;
			((uint32_t*)this)[3] = 0;
			#endif
		}
	}
} __attribute__((packed));

static_assert(sizeof(Glob_desc_t) == 8);

struct Gdtr_t
{
	uint16_t limit;     // table size in bytes minus 1
	uintptr_t base;     // base address
} __attribute__((packed));

static_assert(sizeof(Gdtr_t) == sizeof(word_t) + 2);

Glob_desc_t gdt[8] __attribute__((aligned(8)));

Tss_t tss;

void set_ksp(uintptr_t ksp)
{
	tss.set_ksp(ksp);
}

uintptr_t get_ksp()
{
	return tss.get_ksp();
}

#ifdef Cfg_arch_x86
inline void reload_cs_0x08()
{
	asm volatile ("ljmp $0x8, $1f;  1:");
}
#else // 64
inline void reload_cs_0x08()
{
	asm volatile ("push $0x08;  push $1f;  lretq;  1:");
}
#endif

void set_gdt()
{
	memset(gdt, 0, sizeof(gdt));

	if (sizeof(long) == 4)
	{
		// 0x08:  code, kernel, all address space
		gdt[1].set(0x0, 0xfffff, Limit_4kbloks, Type_code, Priv_kernel, Opsz_32bit);

		// 0x10:  date, kernel, all address space
		gdt[2].set(0x0, 0xfffff, Limit_4kbloks, Type_data, Priv_kernel, Opsz_32bit);

		// 0x18:  code, user, all address space
		gdt[3].set(0x0, 0xfffff, Limit_4kbloks, Type_code, Priv_user, Opsz_32bit);

		// 0x20:  date, user, all address space
		gdt[4].set(0x0, 0xfffff, Limit_4kbloks, Type_data, Priv_user, Opsz_32bit);

		// 0x28:  task state segment
		gdt[5].set((long)&tss, sizeof(tss), Limit_bytes, Type_system, Priv_kernel, Opsz_32bit);

		// skip next array item to be compatibility with x86_64

		// 0x38:  selector to access at 0xff000000 as gs:0, need to get UTCB address
		gdt[7].set(0xff000000, sizeof(long), Limit_bytes, Type_data, Priv_user, Opsz_32bit);
	}
	else if (sizeof(long) == 8)
	{
		// 0x08:  code, kernel, all address space
		gdt[1].set(0x0, 0x0, Limit_bytes, Type_code, Priv_kernel, Opsz_64bit);

		// 0x10:  date, kernel, all address space
		gdt[2].set(0x0, 0x0, Limit_bytes, Type_data, Priv_kernel, Opsz_64bit);

		// 0x18:  code, user, all address space
		gdt[3].set(0x0, 0x0, Limit_bytes, Type_code, Priv_user, Opsz_64bit);

		// 0x20:  date, user, all address space
		gdt[4].set(0x0, 0x0, Limit_bytes, Type_data, Priv_user, Opsz_64bit);

		// 0x28:  task state segment
		// NOTE:  TSS selector has 16-byte size
		gdt[5].set((long)&tss, sizeof(tss), Limit_bytes, Type_system, Priv_kernel, Opsz_64bit);

		// skip next array item cause TSS selector has 16-byte size

		// 0x38:  selector to access at 0xff000000 as gs:0, need to get UTCB address
		gdt[7].set(0xff000000, sizeof(long), Limit_bytes, Type_data, Priv_user, Opsz_64bit);
	}
	else
		panic("unknown bitness\n\r");


	Gdtr_t gdtr;
	gdtr.limit = sizeof(gdt) - 1;
	gdtr.base  = (uintptr_t) gdt;

	asm volatile ("lgdt %0" :: "m"(gdtr));
	reload_cs_0x08();
	asm volatile ("mov %0, %%gs" :: "r"(0x38));

	printk("Hello new GDT.\n");
}

//--------------------------------------------------------------------------------------------------
//  IDT
//--------------------------------------------------------------------------------------------------

#ifdef Cfg_arch_x86

struct Int_gate_t
{
	enum
	{
		Type_task_32 = 0x5,
		Type_int_16  = 0x6,
		Type_trap_16 = 0x7,
		Type_int_32  = 0xe,
		Type_trap_32 = 0xf
	};
	uint16_t offset_lo16;      // offset bits 0..15
	uint16_t selector;         // code segment selector in GDT or LDT
	uint8_t  nulls;            // should be '0'
	struct
	{
		unsigned type    : 4;  //
		unsigned s       : 1;  // should be '0' for interrupt gate
		unsigned dpl     : 2;  // priv level
		unsigned present : 1;  //
	} __attribute__((packed));
	uint16_t offset_hi16;      // offset bits 16..31

	inline void set(uintptr_t handler, unsigned priv)
	{
		offset_lo16 = handler & 0xffff;
		offset_hi16 = handler >> 16;
		selector    = 0x8;
		nulls       = 0;
		type        = Type_int_32;
		s           = 0;
		dpl         = priv == Priv_kernel ? 0 : 3;
		present     = 1;
	}
};

static_assert(sizeof(Int_gate_t) == 8);

#else // 64

struct Int_gate_t
{
	enum
	{
		Type = 0xe
	};
	uint16_t offset_lo16;      // offset bits 0..15
	uint16_t selector;         // code segment selector in GDT or LDT
	struct
	{
		unsigned ist     : 3;  // ndex in interrupt stack table for tss.ist[]
		unsigned null1   : 1;  // should be '0' for interrupt gate
		unsigned null2   : 1;  // should be '0' for interrupt gate
		unsigned null3   : 3;  // should be '0' for interrupt gate
		unsigned type    : 4;  //
		unsigned s       : 1;  // should be '0' for interrupt gate
		unsigned dpl     : 2;  // priv level
		unsigned present : 1;  //
	} __attribute__((packed));
	uint16_t offset_mi16;      // offset bits 16..31
	uint32_t offset_hi32;      // offset bits 32..63
	uint32_t null4;            // should be '0'

	void set(uintptr_t handler, unsigned priv)
	{
		offset_lo16 = handler & 0xffff;
		offset_mi16 = (handler >> 16) & 0xffff;
		offset_hi32 = handler >> 32;
		selector    = 0x8;
		null1       = 0;
		null2       = 0;
		null3       = 0;
		type        = Type;
		s           = 0;
		dpl         = priv == Priv_kernel ? 0 : 3;
		present     = 1;
		null4       = 0;
	}
};

static_assert(sizeof(Int_gate_t) == 16);

#endif

Int_gate_t idt[256];

struct Idtr_t
{
	uint16_t limit;     // table size minus 1
	uintptr_t base;     // base address
} __attribute__((packed));

static_assert(sizeof(Idtr_t) == sizeof(word_t) + 2);

void set_idt()
{
	memset(idt, 0, sizeof(idt));

	extern int gate_table;
	uintptr_t* gate_table_arr = (uintptr_t*)&gate_table;

	for (int i=0; i<0xff; ++i)
	{
		idt[i].set(gate_table_arr[i], i==0x80 ? Priv_user : Priv_kernel); // allow user to use sw irq 0x80
	}

	Idtr_t idtr;
	idtr.limit = sizeof(idt) - 1;
	idtr.base  = (uintptr_t) idt;

	asm volatile ("lidt %0" :: "m"(idtr));
}

#endif // COMMON_H
