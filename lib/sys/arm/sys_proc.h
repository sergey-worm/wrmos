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
	static inline word_t sp()   { word_t r; asm volatile ("mov %0, sp"   : "=r"(r)); return r; }
	static inline word_t usp()  { volatile word_t r=0; asm volatile ("stmia %0, {sp}^" :: "r"(&r)); return r; }
	static inline word_t ulr()  { volatile word_t r=0; asm volatile ("stmia %0, {lr}^" :: "r"(&r)); return r; }

	static inline void cpsr(word_t r) { (void)r; asm volatile("msr cpsr, %0" :: "r"(r)); }
	static inline void spsr(word_t r) { (void)r; asm volatile("msr spsr, %0" :: "r"(r)); }
	static inline void sp(word_t r)   { (void)r; asm volatile("mov sp, %0"   :: "r"(r)); }

	static inline Psr_t cpsr_s() { return (Psr_t) cpsr(); }

	static inline word_t mpidr() { word_t r; asm volatile ("mrc p15, 0, %0, c0, c0, 5": "=r"(r)); return r; }
	static inline unsigned cpuid() { return  mpidr() & 0x3; }

	static inline void rmb()        { asm volatile ("dsb" ::: "memory"); }
	static inline void wmb()        { asm volatile ("dsb" ::: "memory"); }
	static inline void mb()         { asm volatile ("dsb" ::: "memory"); }
	static inline void wait_event() { asm volatile ("wfe" ::: "memory"); }
	static inline void send_event() { asm volatile ("sev" ::: "memory"); }

	static inline void set_new_stack_area(word_t addr, size_t sz)
	{
		sp(addr + sz);
	}

	static inline void jump(long addr) __attribute__((noreturn))
	{
		asm volatile ("mov pc, %0" :: "r"(addr));
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
	static inline void store_phys_word(word_t v, addr_t pa) { (void)v; (void)pa; } // unsupported
	static inline word_t load_phys_word(addr_t pa) { (void)pa; return -1; }        // unsupported



	static inline word_t mfcpsr() { word_t r; asm volatile ("mrs %0, cpsr" : "=r"(r)); return r; }
	static inline void   mtcpsr(word_t v) { asm volatile ("msr cpsr, %0" :: "r"(v)); }

	static inline word_t actlr()  { word_t r; asm volatile ("mrc p15, 0, %0, c1, c0, 1" : "=r"(r)); return r; }
	static inline void   actlr(word_t v) { asm volatile ("mcr p15, 0, %0, c1, c0, 1" :: "r"(v)); }

	// CP15 operations
	#define mtcp(rn, v) asm volatile ("mcr " rn "\n" :: "r"(v));
	#define mfcp(rn) ({uint32_t rval; asm volatile ("mrc " rn "\n" : "=r" (rval)); rval; })

	#define Cp15_cache_size_sel          "p15, 2, %0,  c0,  c0, 0"
	#define Cp15_cache_size_id           "p15, 1, %0,  c0,  c0, 0"
	#define Cp15_clean_inval_dc_line_sw  "p15, 0, %0,  c7, c14, 2"
	#define Cp15_inval_dc_line_sw        "p15, 0, %0,  c7,  c6, 2"
	#define Cp15_inval_ic_pou            "p15, 0, %0,  c7,  c5, 0"

	enum
	{
		//L2cc_baseaddr               = 0xf8f02000,  //
		L2cc_baseaddr                 = 0xfc002000,  //
		L2cc_offset_cache_sync        = 0x730,       // cache sync
		L2cc_offset_dummy_cache_sync  = 0x740,       // dummy Register for cache sync
		L2cc_offset_cache_inv_cln_way = 0x7fc,       // cache invalidate and clean by way
		L2cc_offset_debug_ctrl        = 0xf40,       // debug control register
		L2cc_offset_id                = 0x000,       //
		L2cc_offset_type              = 0x004,       //
		L2cc_offset_cntrl             = 0x100,       //
		L2cc_offset_aux_cntrl         = 0x104,       //
		L2cc_offset_tag_ram_cntrl     = 0x108,       //
		L2cc_offset_data_ram_cntrl    = 0x10c,       //
		L2cc_offset_cache_invld_way   = 0x77c,       // cache invalid by way
		L2cc_offset_ier               = 0x214,       // interrupt mask
		L2cc_offset_ipr               = 0x218,       // masked interrupt status
		L2cc_offset_isr               = 0x21c,       // raw interrupt status
		L2cc_offset_iar               = 0x220,       // interrupt clear

		// enable all prefetching, cache replacement policy, parity enable,
		// event monitor bus enable and way size (64 KB)
		L2cc_aux_reg_default_mask     = 0x72360000,
		L2cc_aux_reg_zero_mask        = 0xfff1ffff,
		L2cc_tag_ram_default_mask     = 0x00000111,  // latency for tag ram
		L2cc_data_ram_default_mask    = 0x00000121,  // latency for data ram

		Do_inval = 1,
		Do_flush = 2,
	};

	static void out32(addr_t a, word_t v) { *(volatile word_t*)a = v; }
	static word_t in32(addr_t a)          { return *(volatile word_t*)a; }

	static void l1dcache_cmd(int cmd)
	{
		// select cache level 0 and D cache in CSSR
		mtcp(Cp15_cache_size_sel, 0);
		register word_t csid = mfcp(Cp15_cache_size_id);

		//  determine cache parameters
		word_t cache_sz = (((csid >> 13) & 0x1ff) + 1) * 128;
		word_t num_ways = ((csid & 0x3ff) >> 3) + 1;
		word_t line_sz  = (csid & 0x7) + 4;
		word_t num_set  = cache_sz / num_ways / (1 << line_sz);

		word_t way = 0;
		word_t set = 0;

		// invalidate all the cachelines
		for (unsigned way_index=0; way_index<num_ways; way_index++)
		{
			for (unsigned set_index=0; set_index<num_set; set_index++)
			{
				register word_t c7 = way | set;
				if (cmd == Do_flush)
					asm volatile ("mcr " Cp15_clean_inval_dc_line_sw :: "r" (c7));  // flush by set/way
				else
					asm volatile ("mcr " Cp15_inval_dc_line_sw :: "r" (c7));   // invalidate by set/way
				set += (1 << line_sz);
			}
			set = 0;
			way += 0x40000000;
		}
		mb();  // wait for L1 flush to complete
	}

	static inline void l2_write_dbg_ctrl(word_t v)
	{
		#if 1 //defined(PL310_ERRATA_588369) || defined(PL310_ERRATA_727915)
		out32(L2cc_baseaddr + L2cc_offset_debug_ctrl, v);
		#else
		(void)v;
		#endif
	}

	static inline void l2_cache_sync()
	{
		#if 1 //defined(PL310_ERRATA_753970)
		out32(L2cc_baseaddr + L2cc_offset_dummy_cache_sync, 0x0);
		#else
		out32(L2cc_baseaddr + L2cc_offset_cache_sync, 0x0);
		#endif
	}

	static void l2cache_enable()
	{
		/*DBG*/
			volatile unsigned long* scu = (unsigned long*)0xfc000000;
			printf("scu:  ctrl:  0x%08lx\n", *(scu+0));
			printf("scu:  conf:  0x%08lx\n", *(scu+1));
			printf("scu:  powr:  0x%08lx\n", *(scu+2));
			printf("scu:  inv:   0x%08lx\n", *(scu+3));
			printf("scu:  strt:  0x%08lx\n", *(scu+4));
			printf("scu:  end:   0x%08lx\n", *(scu+5));
			printf("scu:  acc:   0x%08lx\n", *(scu+6));
			printf("scu:  nacc:  0x%08lx\n", *(scu+7));
			printf("scu:  ctrl:  0x%08lx\n", *(scu+0));
			printf("scu:  conf:  0x%08lx\n", *(scu+1));
			printf("actlr=0x%lx.\n", Proc::actlr());
			volatile unsigned long* l2cc = (unsigned long*)0xfc002000;
			printf("l2cc:  ctrl:  0x%08lx\n", *(l2cc + 0x100/4));
			printf("l2cc:  auxc:  0x%08lx\n", *(l2cc + 0x104/4));

			Proc::actlr(Proc::actlr() | (1<<6));  // set SMP bit
		/*~DBG*/

		register word_t reg = in32(L2cc_baseaddr + L2cc_offset_cntrl);
		if (reg & 0x1)
			return;  // already enabled

		// set up the way size and latencies
		reg = in32(L2cc_baseaddr + L2cc_offset_aux_cntrl);
		reg &= L2cc_aux_reg_zero_mask;
		reg |= L2cc_aux_reg_default_mask;
		out32(L2cc_baseaddr + L2cc_offset_aux_cntrl, reg);
		out32(L2cc_baseaddr + L2cc_offset_tag_ram_cntrl, L2cc_tag_ram_default_mask);
		out32(L2cc_baseaddr + L2cc_offset_data_ram_cntrl, L2cc_data_ram_default_mask);

		// clear the pending interrupts
		reg = in32(L2cc_baseaddr + L2cc_offset_isr);
		out32(L2cc_baseaddr + L2cc_offset_iar, reg);

		l2cache_inval();

		// enable the L2CC
		reg = in32(L2cc_baseaddr + L2cc_offset_cntrl);
		out32(L2cc_baseaddr + L2cc_offset_cntrl, reg | 0x1);

		l2_cache_sync();

		mb();  // synchronize the processor
	}

	static void l2cache_flush()
	{
		// disable write-back and line fills
		l2_write_dbg_ctrl(0x3);

		out32(L2cc_baseaddr + L2cc_offset_cache_inv_cln_way, 0xffff);

		word_t res = 0;
		do
		{
			res = in32(L2cc_baseaddr + L2cc_offset_cache_inv_cln_way) & 0xffff;
		}
		while (res);

		l2_cache_sync();

		// enable write-back and line fills
		l2_write_dbg_ctrl(0x0);

		mb();  // wait for L1 invalidate to complete
	}

	static void l2cache_inval(void)
	{
		/*
		u32 stack_start,stack_end,stack_size;
		stack_end = (u32)&_stack_end;
		stack_start = (u32)&__undef_stack;
		stack_size=stack_start-stack_end;

		// flush stack memory to save return address
		Xil_DCacheFlushRange(stack_end, stack_size);
		*/

		// invalidate the caches
		out32(L2cc_baseaddr + L2cc_offset_cache_invld_way, 0xffff);
		word_t res = in32(L2cc_baseaddr + L2cc_offset_cache_invld_way) & 0xffff;
		while (res)
		{
			res = in32(L2cc_baseaddr + L2cc_offset_cache_invld_way) & 0xffff;
		}

		// wait for the invalidate to complete
		l2_cache_sync();

		mb();  // synchronize the processor
	}


	static void dcache_flush()
	{
		l1dcache_cmd(Do_flush);
		//l2cache_flush();  // TODO
	}

	static void dcache_inval()
	{
		l1dcache_cmd(Do_inval);
		//l2cache_inval();  // TODO
	}

	static void l1icache_inval()
	{
		mtcp(Cp15_cache_size_sel, 1);   //
		mtcp(Cp15_inval_ic_pou, 0);     // invalidate the instruction cache
		mb();                           // wait for L1 invalidate to complete
	}

	static void icache_inval()
	{
		//l2cache_inval();  // TODO
		l1icache_inval();
	}
};

#endif // SYS_PROC_H
