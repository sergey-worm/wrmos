//##################################################################################################
//
//  Stack entry frame. Should be according with crt0.S.
//
//##################################################################################################

#ifndef ENTRY_FRAME
#define ENTRY_FRAME

#include "l4types.h"
#include "printk.h"

class Syscall_frame_t
{
public:
	// as in arm/crt0.S

	// stored by trap handler
	word_t edi;
	word_t esi;
	word_t ebp;
	word_t ebx;
	word_t edx;
	word_t ecx;
	word_t eax;

	// stored by hw
	word_t err;   // exception error, for SW traps and IRQ set 0, see crt0.S
	word_t eip;
	word_t cs;
	word_t eflags;
	word_t esp;   // only for crossing rings (user to kernel)
	word_t ss;    // only for crossing rings (user to kernel)

	inline bool is_krn_entry() const { return !(cs & 0x3); }
	inline word_t ip() const { return eip; }

	void dump(bool need_print_ustack = false)
	{
		printk("  eax:     0x%lx\n", eax);
		printk("  ecx:     0x%lx\n", ecx);
		printk("  edx:     0x%lx\n", edx);
		printk("  ebx:     0x%lx\n", ebx);
		printk("  ebp:     0x%lx\n", ebp);
		printk("  esi:     0x%lx\n", esi);
		printk("  edi:     0x%lx\n", edi);

		printk("  err:     0x%lx\n", err);
		printk("  eip:     0x%lx\n", eip);
		printk("  cs:      0x%lx\n", cs);
		printk("  eflags:  0x%lx\n", eflags);

		if (!is_krn_entry())
		{
			word_t* sp = (word_t*)esp;
			if (need_print_ustack)
				printk("  esp:     0x%lx : %08lx %08lx %08lx\n", esp, sp[0], sp[1], sp[2]);
			else
				printk("  esp:     0x%lx\n", esp);
			printk("  ss:      0x%lx\n", ss);
		}
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
	Syscall_frame_t     syscall_frame;

	void dump(bool need_print_ustack = false)
	{
		syscall_frame.dump(need_print_ustack);
	}

	inline bool is_krn_entry() { return syscall_frame.is_krn_entry(); }
	inline word_t error()      { return syscall_frame.err; }

	inline unsigned scall_number() { return *(word_t*)(syscall_frame.esp); } // on user stack

	inline void scall_kip_base_addr(word_t v)   { syscall_frame.eax = v; }
	inline void scall_kip_api_version(word_t v) { syscall_frame.ecx = v; }
	inline void scall_kip_api_flags(word_t v)   { syscall_frame.edx = v; }
	inline void scall_kip_kernel_id(word_t v)   { syscall_frame.esi = v; }

	inline word_t scall_exreg_dest()    const { return syscall_frame.eax; }
	inline word_t scall_exreg_control() const { return syscall_frame.ecx; }
	inline word_t scall_exreg_sp()      const { return syscall_frame.edx; }
	inline word_t scall_exreg_ip()      const { return syscall_frame.esi; }
	inline word_t scall_exreg_flags()   const { return syscall_frame.edi; }
	inline word_t scall_exreg_uhandle() const { return syscall_frame.ebx; }
	inline word_t scall_exreg_pager()   const { return syscall_frame.ebp; }
	inline void   scall_exreg_result(word_t v)  { syscall_frame.eax = v; }
	inline void   scall_exreg_control(word_t v) { syscall_frame.ecx = v; }
	inline void   scall_exreg_sp(word_t v)      { syscall_frame.edx = v; }
	inline void   scall_exreg_ip(word_t v)      { syscall_frame.esi = v; }
	inline void   scall_exreg_flags(word_t v)   { syscall_frame.edi = v; }
	inline void   scall_exreg_uhandle(word_t v) { syscall_frame.ebx = v; }
	inline void   scall_exreg_pager(word_t v)   { syscall_frame.ebp = v; }

	inline word_t scall_thctl_dest()          const { return syscall_frame.eax; }
	inline word_t scall_thctl_space()         const { return syscall_frame.esi; }
	inline word_t scall_thctl_scheduler()     const { return syscall_frame.edx; }
	inline word_t scall_thctl_pager()         const { return syscall_frame.ecx; }
	inline word_t scall_thctl_utcb_location() const { return syscall_frame.edi; }
	inline void   scall_thctl_result(word_t v) { syscall_frame.eax = v; }

	inline void   scall_sclock(L4_clock_t v) { syscall_frame.eax = v;  syscall_frame.edx = v >> 32; }

	inline word_t scall_thsw_dest() const { return syscall_frame.eax; }

	inline word_t scall_sched_dest()      const { return syscall_frame.eax; }
	inline word_t scall_sched_time_ctl()  const { return syscall_frame.edx; }
	inline word_t scall_sched_proc_ctl()  const { return syscall_frame.esi; }
	inline word_t scall_sched_prio()      const { return syscall_frame.ecx; }
	inline word_t scall_sched_preem_ctl() const { return syscall_frame.edi; }
	inline void   scall_sched_result(word_t v)   { syscall_frame.eax = v; }
	inline void   scall_sched_time_ctl(word_t v) { syscall_frame.edx = v; }

	inline word_t scall_ipc_to()        const { return syscall_frame.eax; }
	inline word_t scall_ipc_from_spec() const { return syscall_frame.edx; }
	inline word_t scall_ipc_timeouts()  const { return syscall_frame.ecx; }
	inline void   scall_ipc_from(word_t v) { syscall_frame.eax = v; }

	// TODO:  lipc
	// TODO:  unmap

	inline word_t scall_sctl_space_spec()  const { return syscall_frame.eax; }
	inline word_t scall_sctl_control()     const { return syscall_frame.ecx; }
	inline word_t scall_sctl_kip_area()    const { return syscall_frame.edx; }
	inline word_t scall_sctl_utcb_area()   const { return syscall_frame.esi; }
	inline word_t scall_sctl_redirector()  const { return syscall_frame.edi; }
	inline void   scall_sctl_result(word_t v)  { syscall_frame.eax = v; }
	inline void   scall_sctl_control(word_t v) { syscall_frame.ecx = v; }

	inline word_t scall_pctl_proc_no()  const { return syscall_frame.eax; }
	inline word_t scall_pctl_int_freq() const { return syscall_frame.ecx; }
	inline word_t scall_pctl_ext_freq() const { return syscall_frame.edx; }
	inline word_t scall_pctl_voltage()  const { return syscall_frame.esi; }
	inline void   scall_pctl_result(word_t v)  { syscall_frame.eax = v; }

	inline word_t scall_mctl_control()  const { return syscall_frame.eax; }
	inline word_t scall_mctl_attr0()    const { return syscall_frame.ecx; }
	inline word_t scall_mctl_attr1()    const { return syscall_frame.edx; }
	inline word_t scall_mctl_attr2()    const { return syscall_frame.ebx; }
	inline word_t scall_mctl_attr3()    const { return syscall_frame.ebp; }
	inline void   scall_mctl_result(word_t v)  { syscall_frame.eax = v; }

	inline word_t scall_kdb_opcode() const { return syscall_frame.eax; }
	inline word_t scall_kdb_param()  const { return syscall_frame.ecx; }
	inline word_t scall_kdb_data()   const { return syscall_frame.edx; }
	inline word_t scall_kdb_size()   const { return syscall_frame.esi; }
	inline void   scall_kdb_result(word_t v) { syscall_frame.eax = v; }

	inline void   enable_fpu()  { /* TODO */ }
	inline void   disable_fpu() { /* TODO */ }
};

enum
{
	Syscall_frame_sz = sizeof(Syscall_frame_t),
	Entry_frame_sz   = Syscall_frame_sz
};

#endif // ENTRY_FRAME
