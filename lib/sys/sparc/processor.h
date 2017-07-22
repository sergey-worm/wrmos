//##################################################################################################
//
//  processor.h - helpers for processor usage.
//
//##################################################################################################

#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "types.h"

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
		Regwins_num = 8
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

	static inline void psr(word_t r)      { asm volatile("mov %0, %%psr; nop; nop; nop" : : "r"(r)); }
	static inline void psr_fast(word_t r) { asm volatile("mov %0, %%psr" : : "r"(r)); }
	static inline void cwp(word_t v)      { psr((psr() & ~Psr_cwp_mask) | (v & Psr_cwp_mask)); }
	static inline void wim(word_t r)      { asm volatile("mov %0, %%wim; nop; nop; nop" : : "r"(r));  }
	static inline void sp(word_t r)       { asm volatile("mov %0, %%sp" : : "r"(r));  }
	static inline void o7(word_t r)       { asm volatile("mov %0, %%o7" : : "r"(r));  }

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

	static inline void set_new_stack_area(word_t addr, size_t sz)
	{
		discard_dirty_regwins();
		sp(addr + sz - Stack_frame_sz);
	}

	/*
	static inline void jump(long addr)
	{
		asm volatile ("b %0;  nop" :: "i"(addr));
	}
	*/

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
};

#endif // PROCESSOR_H
