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
public:

	static inline void outb(uint16_t p, uint8_t v)  { asm volatile ("outb %0, %1" :: "a"(v), "dN"(p)); }
	static inline void outw(uint16_t p, uint16_t v) { asm volatile ("outw %0, %1" :: "a"(v), "dN"(p)); }
	static inline void outl(uint16_t p, uint32_t v) { asm volatile ("outl %0, %1" :: "a"(v), "dN"(p)); }
	static inline uint8_t  inb(uint16_t p) { uint8_t v;  asm volatile ("inb %1, %0" : "=a"(v) : "dN"(p)); return v; }
	static inline uint16_t inw(uint16_t p) { uint16_t v; asm volatile ("inw %1, %0" : "=a"(v) : "dN"(p)); return v; }
	static inline uint32_t inl(uint16_t p) { uint32_t v; asm volatile ("inl %1, %0" : "=a"(v) : "dN"(p)); return v; }

	static inline uint64_t msr(uint32_t msr)
	{
		uint32_t lo;
		uint32_t hi;
		asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
		return ((uint64_t)hi << 32) | lo;
	}

	static inline void msr(uint32_t msr, uint64_t val)
	{
		uint32_t lo = val & 0xffffffff;
		uint32_t hi = val >> 32;
		asm volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
	}

	static inline void set_new_stack_area(word_t addr, size_t sz)
	{
		sp(addr + sz);
	}

	static inline void jump(long addr) // TODO: __attribute__((noreturn))
	{
		asm volatile ("jmp *%0" :: "r"(addr));
		// TODO: while (1) {}
	}

	// no inline to don't use stack variables after set_new_stack_area()
	static void set_new_stack_area_and_jump(addr_t stack, size_t sz, addr_t addr)
		__attribute__((noinline,noreturn))
	{
		set_new_stack_area(stack, sz);
		jump(addr);
		while (1) {}
	}

	static inline uint32_t eax() { uint32_t v; asm volatile("mov %%eax, %0" : "=r"(v)); return v; }
	static inline uint32_t ecx() { uint32_t v; asm volatile("mov %%ecx, %0" : "=r"(v)); return v; }
	static inline uint32_t edx() { uint32_t v; asm volatile("mov %%edx, %0" : "=r"(v)); return v; }
	static inline uint32_t ebx() { uint32_t v; asm volatile("mov %%ebx, %0" : "=r"(v)); return v; }
	static inline uint32_t esp() { uint32_t v; asm volatile("mov %%esp, %0" : "=r"(v)); return v; }
	static inline uint32_t ebp() { uint32_t v; asm volatile("mov %%ebp, %0" : "=r"(v)); return v; }
	static inline uint32_t esi() { uint32_t v; asm volatile("mov %%esi, %0" : "=r"(v)); return v; }
	static inline uint32_t edi() { uint32_t v; asm volatile("mov %%edi, %0" : "=r"(v)); return v; }
	static inline uint32_t cs()  { uint32_t v; asm volatile("mov %%cs, %0"  : "=r"(v)); return v; }
	static inline uint32_t ss()  { uint32_t v; asm volatile("mov %%ss, %0"  : "=r"(v)); return v; }
	static inline uint32_t ds()  { uint32_t v; asm volatile("mov %%ds, %0"  : "=r"(v)); return v; }
	static inline uint32_t es()  { uint32_t v; asm volatile("mov %%es, %0"  : "=r"(v)); return v; }
	static inline uint32_t fs()  { uint32_t v; asm volatile("mov %%fs, %0"  : "=r"(v)); return v; }
	static inline uint32_t gs()  { uint32_t v; asm volatile("mov %%gs, %0"  : "=r"(v)); return v; }

	static inline void fs(uint32_t v) {  asm volatile("mov %0, %%fs" :: "r"(v));  }

	static inline uint32_t eflags() { uint32_t v; asm volatile("pushf; pop %0" : "=r"(v)); return v; }
	static inline uint32_t eip()    { uint32_t v; asm volatile("call 1f; 1: pop %0" : "=r"(v)); return v; }

	static inline word_t sp()        { word_t r=0; asm volatile ("mov %%rsp, %0" : "=r"(r));  return r; }
	static inline word_t cr0()       { word_t v; asm volatile("mov %%cr0, %0" : "=r"(v)); return v; }
	static inline word_t cr2()       { word_t v; asm volatile("mov %%cr2, %0" : "=r"(v)); return v; }
	static inline word_t cr3()       { word_t v; asm volatile("mov %%cr3, %0" : "=r"(v)); return v; }
	static inline word_t cr4()       { word_t v; asm volatile("mov %%cr4, %0" : "=r"(v)); return v; }
	static inline void cr0(word_t v) { asm volatile("mov %0, %%cr0" :: "r"(v)); }
	static inline void cr3(word_t v) { asm volatile("mov %0, %%cr3" :: "r"(v)); }
	static inline void cr4(word_t v) { asm volatile("mov %0, %%cr4" :: "r"(v)); }
	static inline void sp(word_t r)  {  asm volatile("mov %0, %%rsp" : : "r"(r));  }

	static inline void enable_irq()  { asm volatile ("sti"); }
	static inline void disable_irq() { asm volatile ("cli"); }
	static inline void halt()        { asm volatile ("hlt"); }

	static inline int is_phys_copy_supported() { return 0; }
	static inline void store_phys_word(word_t val, addr_t pa) { (void)val; (void)pa; }
	static inline word_t load_phys_word(addr_t pa) { (void)pa; return -1; }

	static inline unsigned cpuid() { return 0; }                           // FIXME:  IMPLME

	static inline void rmb()        { asm volatile ("" ::: "memory"); }    // FIXME:  IMPLME
	static inline void wmb()        { asm volatile ("" ::: "memory"); }    // FIXME:  IMPLME
	static inline void wait_event() { asm volatile ("" ::: "memory"); }    // FIXME:  IMPLME
	static inline void send_event() { asm volatile ("" ::: "memory"); }    // FIXME:  IMPLME

	static inline void tls(word_t v) { /*fs(v);*/ (void)v; }
	static inline word_t tls() { return 0/*fs()*/; }

	static inline void dcache_flush()
	{
		// FIXME:  IMPLME
	}

	static inline void dcache_inval()
	{
		// FIXME:  IMPLME
	}

	static inline void icache_inval()
	{
		// FIXME:  IMPLME
	}
};

#endif // SYS_PROC_H
