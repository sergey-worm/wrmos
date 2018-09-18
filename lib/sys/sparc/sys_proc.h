//##################################################################################################
//
//  Helpers for processor usage.
//
//##################################################################################################

#ifndef SYS_PROC_H
#define SYS_PROC_H

#include "sys_types.h"

class Proc
{
	enum
	{
		Psr_cwp_mask = 0x1f, // bits [0..4]
		Psr_int_lvl_shift = 8,
		Stack_frame_sz = 96
	};

public:

	enum
	{
		Regwins_num = 8,

		Asi_flush_dcache  = 0x11,  // flush dcache only
		Asi_flush_mmu_tlb = 0x18,  // flush TLB and I/D cache
		Asi_mmu_reg       = 0x19,  // MMU registers
		Asi_mmu_bypass    = 0x1c   // MMU only: MMU and cache bypass
	};

	static inline word_t psr() { word_t r; asm volatile ("mov %%psr, %0" : "=r"(r)); return r; }
	static inline word_t cwp() { return psr() & Psr_cwp_mask; }
	static inline word_t wim() { word_t r; asm volatile ("mov %%wim, %0" : "=r"(r)); return r; }
	static inline word_t sp()  { word_t r; asm volatile ("mov %%sp,  %0" : "=r"(r)); return r; }
	static inline word_t o7()  { word_t r; asm volatile ("mov %%o7,  %0" : "=r"(r)); return r; }

	static inline word_t i0()  { word_t r; asm volatile ("mov %%i0,  %0" : "=r"(r)); return r; }
	static inline word_t i1()  { word_t r; asm volatile ("mov %%i1,  %0" : "=r"(r)); return r; }
	static inline word_t i2()  { word_t r; asm volatile ("mov %%i2,  %0" : "=r"(r)); return r; }
	static inline word_t i3()  { word_t r; asm volatile ("mov %%i3,  %0" : "=r"(r)); return r; }
	static inline word_t i4()  { word_t r; asm volatile ("mov %%i4,  %0" : "=r"(r)); return r; }
	static inline word_t i5()  { word_t r; asm volatile ("mov %%i5,  %0" : "=r"(r)); return r; }
	static inline word_t i6()  { word_t r; asm volatile ("mov %%fp,  %0" : "=r"(r)); return r; }
	static inline word_t i7()  { word_t r; asm volatile ("mov %%i7,  %0" : "=r"(r)); return r; }
	static inline word_t fp()  { return i6(); }
	static inline word_t g7()  { word_t r; asm volatile ("mov %%g7,  %0" : "=r"(r)); return r; }

	static inline void psr(word_t r)      { asm volatile("mov %0, %%psr; nop; nop; nop" : : "r"(r)); }
	static inline void psr_fast(word_t r) { asm volatile("mov %0, %%psr" : : "r"(r)); }
	static inline void cwp(word_t v)      { psr((psr() & ~Psr_cwp_mask) | (v & Psr_cwp_mask)); }
	static inline void wim(word_t r)      { asm volatile("mov %0, %%wim; nop; nop; nop" : : "r"(r));  }
	static inline void sp(word_t r)       { asm volatile("mov %0, %%sp" : : "r"(r));  }
	static inline void o7(word_t r)       { asm volatile("mov %0, %%o7" : : "r"(r));  }
	static inline void g7(word_t r)       { asm volatile("mov %0, %%g7" : : "r"(r));  }

	static inline void i0(word_t r)       { asm volatile("mov %0, %%i0" : : "r"(r));  }
	static inline void i1(word_t r)       { asm volatile("mov %0, %%i1" : : "r"(r));  }
	static inline void i2(word_t r)       { asm volatile("mov %0, %%i2" : : "r"(r));  }
	static inline void i3(word_t r)       { asm volatile("mov %0, %%i3" : : "r"(r));  }
	static inline void i4(word_t r)       { asm volatile("mov %0, %%i4" : : "r"(r));  }
	static inline void i5(word_t r)       { asm volatile("mov %0, %%i5" : : "r"(r));  }
	static inline void i6(word_t r)       { asm volatile("mov %0, %%fp" : : "r"(r));  }
	static inline void i7(word_t r)       { asm volatile("mov %0, %%i7" : : "r"(r));  }
	static inline void fp(word_t r)       { i6(r);  }

	// flush regwins to the stack
	static inline void flush_regwins()
	{
		// XXX:  I hope that gcc do not use any regs but global here.
		unsigned depth = Regwins_num - 1;

		for (unsigned i = 0; i < depth; i++)
			asm volatile("save");

		for (unsigned i = 0; i < depth; i++)
			asm volatile("restore");
	}

	// set WIM according to CWP+1
	static inline void discard_dirty_regwins()
	{
		unsigned cur_win = Proc::cwp();
		unsigned inv_win = ((cur_win + 1) == Regwins_num)  ?  0  :  cur_win + 1;
		Proc::wim(1 << inv_win);
	}

	static inline word_t asr17() { word_t v; asm volatile("rd %%asr17, %0" : "=r"(v)); return v; }
	#ifdef SMP
	static inline unsigned cpuid() { return asr17() >> 28; }
	#else
	static inline unsigned cpuid() { return 0; }
	#endif

	static inline void rmb()        { asm volatile ("" ::: "memory"); }
	static inline void wmb()        { asm volatile ("" ::: "memory"); }
	static inline void wait_event() { asm volatile ("" ::: "memory"); }
	static inline void send_event() { asm volatile ("" ::: "memory"); }

	static inline void set_new_stack_area(word_t addr, size_t sz)
	{
		discard_dirty_regwins();
		sp(addr + sz - Stack_frame_sz);
	}

	static inline void jump(long addr) __attribute__((noreturn))
	{
		asm volatile ("jmp %0;  nop" :: "r"(addr));
		while (1) {}
	}

	// no inline to don't use stack variables after set_new_stack_area()
	static void set_new_stack_area_and_jump(addr_t stack, size_t sz, addr_t addr)
		__attribute__((noinline,noreturn))
	{
		set_new_stack_area(stack, sz);
		jump(addr);
		while (1) {}
	}


	static inline void enable_irq()
	{
		uint32_t val = Proc::psr();
		val &= ~(0xF << Psr_int_lvl_shift);
		Proc::psr(val);
	}

	static inline void disable_irq()
	{
		word_t val = Proc::psr();
		val |= (0xF << Psr_int_lvl_shift);
		Proc::psr(val);
	}

	static inline void halt()
	{
		asm volatile ("ta 0");
	}

	static inline int is_phys_copy_supported()
	{
		return 1;
	}

	static inline word_t lda(word_t addr, word_t asi)
	{
		word_t val;
		asm volatile ("lda [ %[addr] ] %[asi], %[val]" : [val]"=r"(val) : [addr]"r"(addr), [asi]"i"(asi));
		return val;
	}

	static inline void sta(word_t val, word_t addr, word_t asi)
	{
		asm volatile ("sta %[val], [ %[addr] ] %[asi]" :: [val]"r"(val), [addr]"r"(addr), [asi]"i"(asi));
	}

	static inline void store_phys_word(word_t val, addr_t pa)
	{
		sta(val, pa, Asi_mmu_bypass);
	}

	static inline word_t load_phys_word(addr_t pa)
	{
		return lda(pa, Asi_mmu_bypass);
	}

	static inline void tls(word_t v) { g7(v); }
	static inline word_t tls() { return g7(); }

	static inline void dcache_flush()
	{

	}

	static inline void dcache_inval()
	{

	}

	static inline void icache_inval()
	{

	}
};

#endif // SYS_PROC_H
