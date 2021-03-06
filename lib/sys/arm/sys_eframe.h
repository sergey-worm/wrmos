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
	Proc_status_frame_sz =    4,   // stack space to store user PSR reg, to allow modify it
	Syscall_frame_sz     = 14*4,
	Entry_frame_sz       = Proc_status_frame_sz + Syscall_frame_sz,

	Entry_type_syscall   = 1,
	Entry_type_pfault    = 2,
	Entry_type_kpfault   = 3,
	Entry_type_irq       = 4,
	Entry_type_exc       = 5
};

class Syscall_frame_t
{
public:
	// as in arm/crt0.S
	union
	{
		struct
		{
			word_t r0;
			word_t r1;
			word_t r2;
			word_t r3;
			word_t r4;
			word_t r5;
			word_t r6;
			word_t r7;
			word_t r8;
			word_t r9;
			word_t r10;
			word_t r11;
			word_t r12;
			word_t r14;
		};
		struct
		{
			word_t r[13];
			word_t lr;
		};
	};
	void dump()
	{
		//for (unsigned i=0; i<13; ++i)
		//	printk("r%02u = 0x%08x\n", i, r[i]);
		//printk("lr  = 0x%08x\n", lr);
	}
};

// data struct to navigate on the stack values
class Proc_status_frame_t
{
public:
	// as in sparc/crt0.S
	word_t psr;

	void dump()
	{
		//printk("spsr = 0x%08x\n", psr);
	}
};

class Float_frame_t
{
public:
	// TODO
	float f[32];
	word_t fsr;
	word_t _;    // alignment
};

// data struct to navigate on the stack values
// exception entry frame
class Entry_frame_t
{
public:
	// as in arm/crt0.S
	Proc_status_frame_t proc_status_frame;
	Syscall_frame_t     syscall_frame;

	typedef int (*print_func_t)(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

	void dump(print_func_t outfunc, bool need_print_usr_mem = false)
	{
		(void) need_print_usr_mem;
		(void) outfunc;
		syscall_frame.dump();
		proc_status_frame.dump();
	}

	inline unsigned scall_number() { return syscall_frame.r7; }

	inline void scall_kip_base_addr(word_t v)   { syscall_frame.r0 = v; }
	inline void scall_kip_api_version(word_t v) { syscall_frame.r1 = v; }
	inline void scall_kip_api_flags(word_t v)   { syscall_frame.r2 = v; }
	inline void scall_kip_kernel_id(word_t v)   { syscall_frame.r3 = v; }

	inline word_t scall_exreg_dest()    const { return syscall_frame.r0; }
	inline word_t scall_exreg_control() const { return syscall_frame.r1; }
	inline word_t scall_exreg_sp()      const { return syscall_frame.r2; }
	inline word_t scall_exreg_ip()      const { return syscall_frame.r3; }
	inline word_t scall_exreg_ip2()     const { return 0;                }
	inline word_t scall_exreg_flags()   const { return syscall_frame.r4; }
	inline word_t scall_exreg_uhandle() const { return syscall_frame.r5; }
	inline word_t scall_exreg_pager()   const { return syscall_frame.r6; }
	inline void   scall_exreg_result(word_t v)  { syscall_frame.r0 = v; }
	inline void   scall_exreg_control(word_t v) { syscall_frame.r1 = v; }
	inline void   scall_exreg_sp(word_t v)      { syscall_frame.r2 = v; }
	inline void   scall_exreg_ip(word_t v)      { syscall_frame.r3 = v; }
	inline void   scall_exreg_ip2(word_t v)     { (void)v;              }
	inline void   scall_exreg_flags(word_t v)   { syscall_frame.r4 = v; }
	inline void   scall_exreg_uhandle(word_t v) { syscall_frame.r5 = v; }
	inline void   scall_exreg_pager(word_t v)   { syscall_frame.r6 = v; }

	inline word_t scall_thctl_dest()          const { return syscall_frame.r0; }
	inline word_t scall_thctl_space()         const { return syscall_frame.r1; }
	inline word_t scall_thctl_scheduler()     const { return syscall_frame.r2; }
	inline word_t scall_thctl_pager()         const { return syscall_frame.r3; }
	inline word_t scall_thctl_utcb_location() const { return syscall_frame.r4; }
	inline void   scall_thctl_result(word_t v) { syscall_frame.r0 = v; }

	inline void   scall_sclock(L4_clock_t v) { syscall_frame.r0 = v;  syscall_frame.r1 = v >> 32; }

	inline word_t scall_thsw_dest() const { return syscall_frame.r0; }

	inline word_t scall_sched_dest()      const { return syscall_frame.r0; }
	inline word_t scall_sched_time_ctl()  const { return syscall_frame.r1; }
	inline word_t scall_sched_proc_ctl()  const { return syscall_frame.r2; }
	inline word_t scall_sched_prio()      const { return syscall_frame.r3; }
	inline word_t scall_sched_preem_ctl() const { return syscall_frame.r4; }
	inline void   scall_sched_result(word_t v)   { syscall_frame.r0 = v; }
	inline void   scall_sched_time_ctl(word_t v) { syscall_frame.r1 = v; }

	inline word_t scall_ipc_to()        const { return syscall_frame.r0; }
	inline word_t scall_ipc_from_spec() const { return syscall_frame.r1; }
	inline word_t scall_ipc_timeouts()  const { return syscall_frame.r2; }
	inline void   scall_ipc_from(word_t v) { syscall_frame.r0 = v; }

	// TODO:  lipc

	inline word_t scall_unmap_control()  const { return syscall_frame.r0; }

	inline word_t scall_sctl_space_spec()  const { return syscall_frame.r0; }
	inline word_t scall_sctl_control()     const { return syscall_frame.r1; }
	inline word_t scall_sctl_kip_area()    const { return syscall_frame.r2; }
	inline word_t scall_sctl_utcb_area()   const { return syscall_frame.r3; }
	inline word_t scall_sctl_redirector()  const { return syscall_frame.r4; }
	inline void   scall_sctl_result(word_t v)  { syscall_frame.r0 = v; }
	inline void   scall_sctl_control(word_t v) { syscall_frame.r1 = v; }

	inline word_t scall_pctl_proc_no()  const { return syscall_frame.r0; }
	inline word_t scall_pctl_int_freq() const { return syscall_frame.r1; }
	inline word_t scall_pctl_ext_freq() const { return syscall_frame.r2; }
	inline word_t scall_pctl_voltage()  const { return syscall_frame.r3; }
	inline void   scall_pctl_result(word_t v)  { syscall_frame.r0 = v; }

	inline word_t scall_mctl_control()  const { return syscall_frame.r0; }
	inline word_t scall_mctl_attr0()    const { return syscall_frame.r1; }
	inline word_t scall_mctl_attr1()    const { return syscall_frame.r2; }
	inline word_t scall_mctl_attr2()    const { return syscall_frame.r3; }
	inline word_t scall_mctl_attr3()    const { return syscall_frame.r4; }
	inline void   scall_mctl_result(word_t v)  { syscall_frame.r0 = v; }

	inline word_t scall_kdb_opcode() const { return syscall_frame.r0; }
	inline word_t scall_kdb_param()  const { return syscall_frame.r1; }
	inline word_t scall_kdb_data()   const { return syscall_frame.r2; }
	inline word_t scall_kdb_size()   const { return syscall_frame.r3; }
	inline void   scall_kdb_result(word_t v) { syscall_frame.r0 = v; }

	inline void   enable_fpu()        { /* TODO */ }
	inline void   disable_fpu()       { /* TODO */ }
	int           is_fpu_enabled()    { return 0; /* TODO */ }
	inline void   set_carry_bit()     { /* TODO */ }
	inline void   clear_carry_bit()   { /* TODO */ }

	inline word_t entry_pc() const    { return syscall_frame.lr - 4;                       }  // get entry pc
	inline void   entry_pc(word_t v)  { (void)v;  /* proc_status_frame.pc = v;     TODO */ }  // set entry pc
	inline word_t entry_pc2() const   { return 0; }  // pc2 not need for arm
	inline void   entry_pc2(word_t v) { (void)v;  }  // pc2 not need for arm

	inline word_t exit_pc(int entry_type) const
	{
		(void)entry_type;
		/*
		switch (entry_type)
		{
			case Entry_type_syscall:  return proc_status_frame.npc;
			case Entry_type_pfault:   return proc_status_frame.pc;
			case Entry_type_kpfault:  return 0;  // invalid
			case Entry_type_irq:      return proc_status_frame.pc;
			case Entry_type_exc:      return 0;  // unknown, will defined in exc-reply
		}
		*/
		return 0;
	}

	// pc2 not need for arm
	inline word_t exit_pc2(int entry_type) const
	{
		(void)entry_type;
		return 0;
	}

	inline void exit_pc(int entry_type, word_t v1, word_t v2)
	{
		(void)entry_type; (void)v1; (void)v2;
		/*
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
		*/
	}
};

#endif // ENTRY_FRAME
