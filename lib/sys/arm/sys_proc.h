//##################################################################################################
//
//  Helpers for processor usage.
//
//##################################################################################################

#ifndef SYS_PROC_H
#define SYS_PROC_H

#include "sys_types.h"
#include <stdio.h>

class Proc
{
public:

	enum
	{
		Usr = 0x10,
		Fiq = 0x11,
		Irq = 0x12,
		Svc = 0x13,
		Mon = 0x16,  // Security Extensions only
		Abt = 0x17,
		Hyp = 0x1a,  // Virtualization Extensions only
		Und = 0x1b,
		Sys = 0x1f,  // ARMv4 and above
	};

	struct Psr_t
	{
		union
		{
			word_t raw;
			struct
			{
				#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

				// flags
				unsigned n : 1;
				unsigned z : 1;
				unsigned c : 1;
				unsigned v : 1;
				unsigned _some_flag_fields : 4;       // TODO
				// status
				unsigned _some_status_fields : 8;     // TODO
				// extention
				unsigned _some_extention_fields : 8;  // TODO
				// control
				unsigned i : 1;
				unsigned f : 1;
				unsigned t : 1;
				unsigned mode : 5;

				#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

				// control
				unsigned mode : 5;
				unsigned t : 1;
				unsigned f : 1;
				unsigned i : 1;
				// extention
				unsigned _some_extention_fields : 8;  // TODO
				// status
				unsigned _some_status_fields : 8;     // TODO
				// flags
				unsigned _some_flag_fields : 4;       // TODO
				unsigned n : 1;
				unsigned z : 1;
				unsigned c : 1;
				unsigned v : 1;

				#endif
			};
		};
		Psr_t(word_t r) { raw = r; }

		const char* str_mode(int m)
		{
			switch (m)
			{
				case Usr:  return "usr";
				case Fiq:  return "fiq";
				case Irq:  return "irq";
				case Svc:  return "svc";
				case Mon:  return "mon";
				case Abt:  return "abt";
				case Hyp:  return "hyp";
				case Und:  return "und";
				case Sys:  return "sys";
			}
			return "???";
		}

		const char* str()
		{
			static char s[32];
			snprintf(s, sizeof(s), "%c%c%c%c%c%c%c:%s", n?'N':'n', z?'Z':'z', c?'C':'c', v?'V':'v',
				i?'I':'i', f?'F':'f', t?'T':'t', str_mode(mode));
			return s;
		}
	};

	static inline word_t cpsr() { word_t r; asm volatile ("mrs %0, cpsr" : "=r"(r)); return r; }
	static inline word_t spsr() { word_t r; asm volatile ("mrs %0, spsr" : "=r"(r)); return r; }
	static inline word_t sp()   { word_t r; asm volatile ("mov %0, sp" : "=r"(r));  return r; }
	static inline word_t usp()  { volatile word_t r=0; asm volatile ("stmia %0, {sp}^" :: "r"(&r)); return r; }
	static inline word_t ulr()  { volatile word_t r=0; asm volatile ("stmia %0, {lr}^" :: "r"(&r)); return r; }

	static inline void cpsr(word_t r) { (void)r; asm volatile("msr cpsr, %0" :: "r"(r)); }
	static inline void spsr(word_t r) { (void)r; asm volatile("msr spsr, %0" :: "r"(r)); }
	static inline void sp(word_t r)   { (void)r; asm volatile("mov sp, %0" : : "r"(r));  }

	static inline Psr_t cpsr_s() { return (Psr_t) cpsr(); }

	static inline void set_new_stack_area(word_t addr, size_t sz)
	{
		sp(addr + sz);
	}

	static inline void jump(long addr)
	{
		asm volatile ("mov pc, %0" :: "r"(addr));
	}

	static inline void enable_irq()
	{
		Psr_t psr = Proc::cpsr_s();
		psr.i = 0;
		psr.f = 0;
		cpsr(psr.raw);
	}

	static inline void disable_irq()
	{
		Psr_t psr = Proc::cpsr_s();
		psr.i = 1;
		psr.f = 1;
		cpsr(psr.raw);
	}

	static inline void halt()
	{
		while (1);
	}

	static inline int is_phys_copy_supported() { return 0; }
	static inline void store_phys_word(word_t val, addr_t pa) { (void)val; (void)pa; }
	static inline word_t load_phys_word(addr_t pa) { (void)pa; return -1; }
};

#endif // SYS_PROC_H
