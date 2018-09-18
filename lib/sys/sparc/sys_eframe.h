//##################################################################################################
//
//  Stack entry frame. Should be according with crt0.S.
//
//##################################################################################################

#ifndef ENTRY_FRAME
#define ENTRY_FRAME

#include "l4_types.h"

enum
{
	Proc_status_frame_sz = 16,       // stack space to store user PSR reg, to allow modify it
	Stack_frame_sz       = 96,       // stack space for store one window and 32 bytes for compiler use
	Regwin_frame_sz      = 64,       // stack space for store one window
	Syscall_frame_sz     = 32,
	Global_frame_sz      = 32,
	Float_frame_sz       = 128 + 8,
	Entry_frame_sz       = Proc_status_frame_sz + Syscall_frame_sz + Global_frame_sz +
	                       1*Regwin_frame_sz,

	Entry_type_syscall   = 1,
	Entry_type_pfault    = 2,
	Entry_type_kpfault   = 3,
	Entry_type_irq       = 4,
	Entry_type_exc       = 5
};

// data struct to navigate on the stack values
class Regwin_frame_t
{
public:
	// as in sparc/crt0.S
	union
	{
		struct
		{
			word_t l0;
			word_t l1;
			word_t l2;
			word_t l3;
			word_t l4;
			word_t l5;
			word_t l6;
			word_t l7;
			word_t i0;
			word_t i1;
			word_t i2;
			word_t i3;
			word_t i4;
			word_t i5;
			word_t i6;
			word_t i7;
		};
		struct
		{
			word_t l[8];
			word_t i[8];
		};
	};
};

// data struct to navigate on the stack values
class Syscall_frame_t
{
public:
	// as in sparc/crt0.S
	union
	{
		struct
		{
			word_t i0;
			word_t i1;
			word_t i2;
			word_t i3;
			word_t i4;
			word_t i5;
			word_t i6;
			word_t i7;
		};
		struct
		{
			word_t i[8];
		};
	};
};

// data struct to navigate on the stack values
class Global_frame_t
{
public:
	// as in sparc/crt0.S
	union
	{
		struct
		{
			word_t y;
			word_t g1;
			word_t g2;
			word_t g3;
			word_t g4;
			word_t g5;
			word_t g6;
			word_t g7;
		};
		struct
		{
			word_t g[8];
		};
	};
};

class Float_frame_t
{
public:
	float f[32];
	word_t fsr;
	word_t _;    // alignment
};

// data struct to navigate on the stack values
class Proc_status_frame_t
{
public:
	// as in sparc/crt0.S
	word_t psr;  // can be modifyed
	word_t wim;  // read only
	word_t pc;   // read only
	word_t npc;  // read unly
};

// data struct to navigate on the stack values
// exception entry frame
class Entry_frame_t
{
	enum { Regwin_frames = 8 - 1 };

public:
	// as in sparc/crt0.S
	Regwin_frame_t      regwin_frame[Regwin_frames];
	Global_frame_t      global_frame;
	Syscall_frame_t     syscall_frame;
	Proc_status_frame_t proc_status_frame;

	static inline unsigned wim2invwin(word_t wim)
	{
		for (unsigned i=0; i<8; ++i)
			if ((unsigned)(1 << i) == wim)
				return i;
		return (unsigned)-1;
	}

	unsigned dirty_wins()
	{
		unsigned inv_win = wim2invwin(proc_status_frame.wim);
		unsigned trap_win = proc_status_frame.psr & 0x1f;  // trap window pointer
		unsigned dirty_wins = (trap_win == inv_win)  ?  7  :  (inv_win - trap_win - 1) % 8;
		return dirty_wins;
	}

	typedef int (*print_func_t)(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

	void dump(print_func_t outfunc, bool need_print_usr_mem = false)
	{
		unsigned inv_win = wim2invwin(proc_status_frame.wim);
		unsigned trap_win = proc_status_frame.psr & 0x1f;  // trap window pointer
		unsigned dirty_wins = (trap_win == inv_win)  ?  7  :  (inv_win - trap_win - 1) % 8;

		outfunc(" entry_frame=0x%p\n", this);
		outfunc("          ");
		for (int w=dirty_wins; w>0; --w)
			outfunc("win%u       ", (trap_win + w) % 8);
		outfunc("win%u/trap\n", trap_win);

		// print inc-regs for all dirty regwins and trap window
		for (unsigned l=0; l<8; ++l)
		{
			outfunc(" %-4s %u:  ", !l ? "inc" : "", l);
			for (unsigned w=0; w<dirty_wins; ++w)
				outfunc("%08lx   ", regwin_frame[w].i[l]);
			outfunc("%08lx\n", syscall_frame.i[l]);
		}
		outfunc("\n");

		// print loc-regs for all dirty regwins
		for (unsigned l=0; l<8; ++l)
		{
			outfunc(" %-4s %u:  ", !l ? "loc" : "", l);
			for (unsigned w=0; w<dirty_wins; ++w)
				outfunc("%08lx   ", regwin_frame[w].l[l]);
			outfunc("\n");
		}
		outfunc("\n");

		// print glob-regs
		for (unsigned l=0; l<8; ++l)
		{
			outfunc(" %-4s %u:  %08lx\n", !l ? "glob" : "", l, !l ? 0 : global_frame.g[l]);
		}
		outfunc("\n");

		// print regs:  psr, wim, pc, npc, y
		outfunc(" psr:  %08lx   trap_win=%u\n", proc_status_frame.psr, trap_win);
		outfunc(" wim:  %08lx   inv_win=%u, dirty_wins=%u\n", proc_status_frame.wim, inv_win, dirty_wins);
		outfunc("   y:  %08lx\n", global_frame.y);
		if (need_print_usr_mem)
		{
			outfunc("  pc:  %08lx   opcode:  %08lx\n", proc_status_frame.pc, *(word_t*)proc_status_frame.pc);
			outfunc(" npc:  %08lx   opcode:  %08lx\n", proc_status_frame.npc, *(word_t*)proc_status_frame.npc);
		}
		else
		{
			outfunc("  pc:  %08lx   opcode:  ???\n", proc_status_frame.pc);
			outfunc(" npc:  %08lx   opcode:  ???\n", proc_status_frame.npc);
		}

		/*  DELME
		extern unsigned long cur_ksp;
		outfunc("\n");
		outfunc(" cur_ksp:              0x%lx.\n", cur_ksp);
		outfunc(" entry_frame_start:    0x%lx.\n", (word_t)this);
		outfunc(" entry_frame_sz:       0x%lx.\n", sizeof(*this));
		*/
	}

	void flush_regwins(bool need_change_wim = false)
	{
		unsigned inv_win = wim2invwin(proc_status_frame.wim);
		unsigned trap_win = proc_status_frame.psr & 0x1f; // trap window pointer
		unsigned dirty_wins = (trap_win == inv_win)  ?  7  :  (inv_win - trap_win - 1) % 8;

		for (unsigned w=0; w<dirty_wins; ++w)
		{
			unsigned long sp = ((w+1) == dirty_wins) ? syscall_frame.i[6] : regwin_frame[w+1].i[6];
			for (unsigned i=0; i<8; ++i)
				*(long*)(sp + i*4) = regwin_frame[w].l[i];
			for (unsigned i=0; i<8; ++i)
				*(long*)(sp + 0x20 + i*4)= regwin_frame[w].i[i];
		}
		if (1  ||  (need_change_wim  &&  dirty_wins > 1))
		{
			proc_status_frame.wim = 1 << ((trap_win + 2) % 8); // leave one dirty win
			for (unsigned i=0; i<8; ++i)
				regwin_frame[0].i[i] = regwin_frame[dirty_wins - 1].i[i];
			for (unsigned i=0; i<8; ++i)
				regwin_frame[0].l[i] = regwin_frame[dirty_wins - 1].l[i];
		}
	}

	inline unsigned scall_number() { return global_frame.g1; }

	inline void scall_kip_base_addr(word_t v)   { syscall_frame.i0 = v; }
	inline void scall_kip_api_version(word_t v) { syscall_frame.i1 = v; }
	inline void scall_kip_api_flags(word_t v)   { syscall_frame.i2 = v; }
	inline void scall_kip_kernel_id(word_t v)   { syscall_frame.i3 = v; }

	inline word_t scall_exreg_dest()      const { return syscall_frame.i0; }
	inline word_t scall_exreg_control()   const { return syscall_frame.i1; }
	inline word_t scall_exreg_sp()        const { return syscall_frame.i2; }
	inline word_t scall_exreg_ip()        const { return syscall_frame.i3; }
	inline word_t scall_exreg_ip2()       const { return global_frame.g5;  }
	inline word_t scall_exreg_pager()     const { return syscall_frame.i4; }
	inline word_t scall_exreg_uhandle()   const { return syscall_frame.i5; }
	inline word_t scall_exreg_flags()     const { return global_frame.g4;  }
	inline void   scall_exreg_result(word_t v)    { syscall_frame.i0 = v; }
	inline void   scall_exreg_control(word_t v)   { syscall_frame.i1 = v; }
	inline void   scall_exreg_sp(word_t v)        { syscall_frame.i2 = v; }
	inline void   scall_exreg_ip(word_t v)        { syscall_frame.i3 = v; }
	inline void   scall_exreg_ip2(word_t v)       { global_frame.g5  = v; }
	inline void   scall_exreg_pager(word_t v)     { syscall_frame.i4 = v; }
	inline void   scall_exreg_uhandle(word_t v)   { syscall_frame.i5 = v; }
	inline void   scall_exreg_flags(word_t v)     { global_frame.g4  = v; }

	inline word_t scall_thctl_dest()          const { return syscall_frame.i0; }
	inline word_t scall_thctl_space()         const { return syscall_frame.i1; }
	inline word_t scall_thctl_scheduler()     const { return syscall_frame.i2; }
	inline word_t scall_thctl_pager()         const { return syscall_frame.i3; }
	inline word_t scall_thctl_utcb_location() const { return syscall_frame.i4; }
	inline void   scall_thctl_result(word_t v) { syscall_frame.i0 = v; }

	inline void   scall_sclock(L4_clock_t v) { syscall_frame.i0 = v;  syscall_frame.i1 = v >> 32; }

	inline word_t scall_thsw_dest() const { return syscall_frame.i0; }

	inline word_t scall_sched_dest()      const { return syscall_frame.i0; }
	inline word_t scall_sched_time_ctl()  const { return syscall_frame.i1; }
	inline word_t scall_sched_proc_ctl()  const { return syscall_frame.i2; }
	inline word_t scall_sched_prio()      const { return syscall_frame.i3; }
	inline word_t scall_sched_preem_ctl() const { return syscall_frame.i4; }
	inline void   scall_sched_result(word_t v)   { syscall_frame.i0 = v; }
	inline void   scall_sched_time_ctl(word_t v) { syscall_frame.i1 = v; }

	inline word_t scall_ipc_to()        const { return syscall_frame.i0; }
	inline word_t scall_ipc_from_spec() const { return syscall_frame.i1; }
	inline word_t scall_ipc_timeouts()  const { return syscall_frame.i2; }
	inline void   scall_ipc_from(word_t v) { syscall_frame.i0 = v; }

	// TODO:  lipc

	inline word_t scall_unmap_control()  const { return syscall_frame.i0; }

	inline word_t scall_sctl_space_spec()  const { return syscall_frame.i0; }
	inline word_t scall_sctl_control()     const { return syscall_frame.i1; }
	inline word_t scall_sctl_kip_area()    const { return syscall_frame.i2; }
	inline word_t scall_sctl_utcb_area()   const { return syscall_frame.i3; }
	inline word_t scall_sctl_redirector()  const { return syscall_frame.i4; }
	inline void   scall_sctl_result(word_t v)  { syscall_frame.i0 = v; }
	inline void   scall_sctl_control(word_t v) { syscall_frame.i1 = v; }

	inline word_t scall_pctl_proc_no()  const { return syscall_frame.i0; }
	inline word_t scall_pctl_int_freq() const { return syscall_frame.i1; }
	inline word_t scall_pctl_ext_freq() const { return syscall_frame.i2; }
	inline word_t scall_pctl_voltage()  const { return syscall_frame.i3; }
	inline void   scall_pctl_result(word_t v)  { syscall_frame.i0 = v; }

	inline word_t scall_mctl_control()  const { return syscall_frame.i0; }
	inline word_t scall_mctl_attr0()    const { return syscall_frame.i1; }
	inline word_t scall_mctl_attr1()    const { return syscall_frame.i2; }
	inline word_t scall_mctl_attr2()    const { return syscall_frame.i3; }
	inline word_t scall_mctl_attr3()    const { return syscall_frame.i4; }
	inline void   scall_mctl_result(word_t v)  { syscall_frame.i0 = v; }

	inline word_t scall_kdb_opcode() const { return syscall_frame.i0; }
	inline word_t scall_kdb_param()  const { return syscall_frame.i1; }
	inline word_t scall_kdb_data()   const { return syscall_frame.i2; }
	inline word_t scall_kdb_size()   const { return syscall_frame.i3; }
	inline void   scall_kdb_result(word_t v) { syscall_frame.i0 = v; }

	inline void   enable_fpu()        { proc_status_frame.psr |=   1 << 12;  }  // set psr.ef
	inline void   disable_fpu()       { proc_status_frame.psr &= ~(1 << 12); }  // clear psr.ef
	int           is_fpu_enabled()    { return proc_status_frame.psr & (1 << 12); }
	inline void   set_carry_bit()     { proc_status_frame.psr |=   1 << 20;  }  // set psr.icc.carry
	inline void   clear_carry_bit()   { proc_status_frame.psr &= ~(1 << 20); }  // clear psr.icc.carry

	inline word_t entry_pc() const    { return proc_status_frame.pc;         }  // get entry pc
	inline void   entry_pc(word_t v)  { proc_status_frame.pc = v;            }  // set entry pc
	inline word_t entry_pc2() const   { return proc_status_frame.npc;        }  // get entry npc
	inline void   entry_pc2(word_t v) { proc_status_frame.npc = v;           }  // set entry npc

	inline word_t exit_pc(int entry_type) const
	{
		switch (entry_type)
		{
			case Entry_type_syscall:  return proc_status_frame.npc;
			case Entry_type_pfault:   return proc_status_frame.pc;
			case Entry_type_kpfault:  return 0;  // invalid
			case Entry_type_irq:      return proc_status_frame.pc;
			case Entry_type_exc:      return 0;  // unknown, will defined in exc-reply
		}
		return 0;
	}

	inline word_t exit_pc2(int entry_type) const
	{
		switch (entry_type)
		{
			case Entry_type_syscall:  return proc_status_frame.npc + 4;
			case Entry_type_pfault:   return proc_status_frame.npc;
			case Entry_type_kpfault:  return 0;  // invalid
			case Entry_type_irq:      return proc_status_frame.npc;
			case Entry_type_exc:      return 0;  // unknown, will defined in exc-reply
		}
		return 0;
	}

	inline void exit_pc(int entry_type, word_t v1, word_t v2)
	{
		switch (entry_type)
		{
			case Entry_type_syscall:
				// skip entry inst:
				//   exit_pc  = entry_npc
				//   exit_npc = entry_npc + 4
				if (v1+4 != v2)
					proc_status_frame.pc = proc_status_frame.npc = 0;  // panic
				proc_status_frame.npc = v2 - 4;
				break;

			case Entry_type_pfault:
			case Entry_type_irq:
			case Entry_type_exc:
				// reexecute entry inst:
				//   exit_pc  = entry_pc
				//   exit_npc = entry_npc
				proc_status_frame.pc  = v1;
				proc_status_frame.npc = v2;
				break;

			default:
				proc_status_frame.pc = 0;
		}
	}
};

#endif  // ENTRY_FRAME
