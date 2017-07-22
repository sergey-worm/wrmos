//##################################################################################################
//
//  Stack entry frame. Should be according with crt0.S.
//
//##################################################################################################

#ifndef ENTRY_FRAME
#define ENTRY_FRAME

#include "l4types.h"
#include "printk.h"
#include <stdio.h>

enum
{
	Proc_status_frame_sz =  8,       // stack space to store user PSR reg, to allow modify it
	Stack_frame_sz       = 96,       // stack space for store one window and 32 bytes for compiler use
	Regwin_frame_sz      = 64,       // stack space for store one window
	Syscall_frame_sz     = 32,
	Global_frame_sz      = 32,
	Float_frame_sz       = 128 + 8,
	Entry_frame_sz       = Proc_status_frame_sz + Syscall_frame_sz + Global_frame_sz +
	                       Float_frame_sz + 1*Regwin_frame_sz
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
			uint32_t l0;
			uint32_t l1;
			uint32_t l2;
			uint32_t l3;
			uint32_t l4;
			uint32_t l5;
			uint32_t l6;
			uint32_t l7;
			uint32_t i0;
			uint32_t i1;
			uint32_t i2;
			uint32_t i3;
			uint32_t i4;
			uint32_t i5;
			uint32_t i6;
			uint32_t i7;
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
			uint32_t i0;
			uint32_t i1;
			uint32_t i2;
			uint32_t i3;
			uint32_t i4;
			uint32_t i5;
			uint32_t i6;
			uint32_t i7;
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
			uint32_t y;
			uint32_t g1;
			uint32_t g2;
			uint32_t g3;
			uint32_t g4;
			uint32_t g5;
			uint32_t g6;
			uint32_t g7;
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
	word_t psr;
	word_t _;    // alignment
};

// data struct to navigate on the stack values
// exception entry frame
class Entry_frame_t
{
public:
	// as in sparc/crt0.S
	Regwin_frame_t      regwin_frame[1]; // only first user regwin
	Float_frame_t       float_frame;
	Global_frame_t      global_frame;
	Syscall_frame_t     syscall_frame;
	Proc_status_frame_t proc_status_frame;

	void dump(bool need_print_ustack = false)
	{
		(void) need_print_ustack;

		//syscall_frame.dump();
		//proc_status_frame.dump();

		const Regwin_frame_t&  rframe = regwin_frame[0];
		const Global_frame_t&  gframe = global_frame;
		const Syscall_frame_t& sframe = syscall_frame;

		printf("     in         loc        global\n");
		for (unsigned i=0; i<8; ++i)
			printf(" %d:  %08x   %08x   %08x\n", i, rframe.i[i], rframe.l[i], !i ? 0 : gframe.g[i]);

		printf("\n");
		printf("     syscall args\n");
		for (unsigned i=0; i<8; ++i)
			printf(" %d:  %08x\n", i, sframe.i[i]);
	}

	inline unsigned scall_number() { return global_frame.g1; }

	inline void scall_kip_base_addr(word_t v)   { syscall_frame.i0 = v; }
	inline void scall_kip_api_version(word_t v) { syscall_frame.i1 = v; }
	inline void scall_kip_api_flags(word_t v)   { syscall_frame.i2 = v; }
	inline void scall_kip_kernel_id(word_t v)   { syscall_frame.i3 = v; }

	inline word_t scall_exreg_dest()    const { return syscall_frame.i0; }
	inline word_t scall_exreg_control() const { return syscall_frame.i1; }
	inline word_t scall_exreg_sp()      const { return syscall_frame.i2; }
	inline word_t scall_exreg_ip()      const { return syscall_frame.i3; }
	inline word_t scall_exreg_pager()   const { return syscall_frame.i4; }
	inline word_t scall_exreg_uhandle() const { return syscall_frame.i5; }
	inline word_t scall_exreg_flags()   const { return global_frame.g4; }
	inline void   scall_exreg_result(word_t v)  { syscall_frame.i0 = v; }
	inline void   scall_exreg_control(word_t v) { syscall_frame.i1 = v; }
	inline void   scall_exreg_sp(word_t v)      { syscall_frame.i2 = v; }
	inline void   scall_exreg_ip(word_t v)      { syscall_frame.i3 = v; }
	inline void   scall_exreg_pager(word_t v)   { syscall_frame.i4 = v; }
	inline void   scall_exreg_uhandle(word_t v) { syscall_frame.i5 = v; }
	inline void   scall_exreg_flags(word_t v)   { global_frame.g4  = v; }

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
	// TODO:  unmap

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

	inline void   enable_fpu()  { proc_status_frame.psr |= 1 << 12; }     // set psr.ef
	inline void   disable_fpu() { proc_status_frame.psr &= ~(1 << 12); }  // clear psr.ef
};

#endif  // ENTRY_FRAME
