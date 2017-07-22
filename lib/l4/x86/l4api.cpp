//##################################################################################################
//
//  l4api.c - API to L4 microkernel.
//
//##################################################################################################

#include "l4api.h"
#include "sys-utils.h"
#include "l4syscalls.h"

struct App_data_t
{
	L4_kip_t* kip;  // also it is used as init flag
};

static App_data_t app_data;

__attribute__((always_inline)) inline void do_syscall()
{
	asm volatile ("int $0x80");
}

// get pointer to Kernel Interface Page
L4_kip_t* l4_kip()
{
	// It uses syscall KernelInterface. KIP address will remain constant
	// through the lifetime of that address space - store KIP address
	// in a static variable for further use.

	if (app_data.kip)
		return app_data.kip;

	// do syscall

	register int eax asm ("eax") = 0x10;
	register int ecx asm ("ecx") = 0x11;
	register int edx asm ("edx") = 0x22;
	register int esi asm ("esi") = 0x33;
	register int edi asm ("edi") = 0x44;
	register int ebx asm ("ebx") = 0x55;
	asm volatile ("mov %0, %%ebp" :: "n"(0x66));
	(void)eax; (void)ecx; (void)edx; (void)esi; (void)edi; (void)ebx;
	asm volatile ("push %0" :: "n"(L4_syscall_kernel_interface));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number
	app_data.kip = (L4_kip_t*) eax;
	return app_data.kip;
}

// get pointer to UTCB
L4_utcb_t* l4_utcb()
{
	L4_utcb_t* v = 0;
	asm volatile ("mov %%gs:0, %0" : "=r"(v));  // UTCB address can be loaded from %gs:0
	return (L4_utcb_t*) v;
}

// get my local ID, equal to UTCB address
L4_utcb_t* l4_local_id()
{
	return l4_utcb();  //app_data.utcb;
}

// do IPC syscall
int l4_ipc(L4_thrid_t to, L4_thrid_t from_spec, L4_timeouts_t timeouts, L4_thrid_t& from) // obsolete
{
	return l4_ipc(to, from_spec, timeouts, &from);
}
int l4_ipc(L4_thrid_t to, L4_thrid_t from_spec, L4_timeouts_t timeouts, L4_thrid_t* from)
{
	register L4_thrid_t    eax asm ("eax") = to;
	register L4_thrid_t    edx asm ("edx") = from_spec;
	register L4_timeouts_t ecx asm ("ecx") = timeouts;
	(void)eax; (void)edx; (void)ecx;
	asm volatile ("pushl %0" :: "n"(L4_syscall_ipc));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number
	register word_t rcv_from asm ("eax");
	if (from)
		*from = rcv_from;
	return l4_utcb()->msgtag().ipc_is_failed() ? l4_utcb()->ipc_error_code().error() : 0;
}

// send phase only
int l4_send(L4_thrid_t to, L4_time_t timeout)
{
	L4_timeouts_t timeouts(timeout, L4_time_t::Never);
	return l4_ipc(to, L4_thrid_t::Nil, timeouts);
}

// receive phase only
int l4_receive(L4_thrid_t from_spec, L4_time_t timeout, L4_thrid_t& from) // obsolete
{
	L4_timeouts_t timeouts(L4_time_t::Never, timeout);
	return l4_ipc(L4_thrid_t::Nil, from_spec, timeouts, from);
}
int l4_receive(L4_thrid_t from_spec, L4_time_t timeout, L4_thrid_t* from)
{
	L4_timeouts_t timeouts(L4_time_t::Never, timeout);
	return l4_ipc(L4_thrid_t::Nil, from_spec, timeouts, from);
}

// do ThreadControl syscall
int l4_thread_control(L4_thrid_t dest, L4_thrid_t space, L4_thrid_t sched, L4_thrid_t pager, addr_t utcb)
{
	register L4_thrid_t eax asm ("eax") = dest;
	register L4_thrid_t esi asm ("esi") = space;
	register L4_thrid_t edx asm ("edx") = sched;
	register L4_thrid_t ecx asm ("ecx") = pager;
	register addr_t     edi asm ("edi") = utcb;
	(void)eax; (void)esi; (void)edx; (void)ecx; (void)edi;
	asm volatile ("pushl %0" :: "n"(L4_syscall_thread_control));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number
	register word_t result asm ("eax");
	return result==1 ? 0 : l4_utcb()->error();
}

// do SpaceControl syscall
int l4_space_control(L4_thrid_t space, word_t control, L4_fpage_t kip, L4_fpage_t utcbs, L4_thrid_t redirector)
{
	register L4_thrid_t eax asm ("eax") = space;
	register word_t     ecx asm ("ecx") = control;
	register L4_fpage_t edx asm ("edx") = kip;
	register L4_fpage_t esi asm ("esi") = utcbs;
	register L4_thrid_t edi asm ("edi") = redirector;
	(void)eax; (void)ecx; (void)edx; (void)esi; (void)edi;
	asm volatile ("push %0" :: "n"(L4_syscall_space_control));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number
	register word_t result asm ("eax");
	return result==1 ? 0 : l4_utcb()->error();
}

// do MemoryControl syscall
// NOTE:  fpages shoud be in MRs
int l4_memory_control(word_t control, word_t attr0, word_t attr1, word_t attr2, word_t attr3)
{
/*
	register word_t     r0 asm ("r0") = control;
	register word_t     r1 asm ("r1") = attr0;
	register word_t     r2 asm ("r2") = attr1;
	register word_t     r3 asm ("r3") = attr2;
	register word_t     r4 asm ("r4") = attr3;
	register word_t     r7 asm ("r7") = L4_syscall_memory_control;
	(void)r0; (void)r1; (void)r2; (void)r3; (void)r4; (void)r7;
	do_syscall();
	register word_t result asm ("r0");
	return result==1 ? 0 : l4_utcb()->error();
*/
	l4_kdb("IMPME:  l4_memory_control().\n", 1);
	return 123;
}

// do SystemClock syscall
L4_clock_t l4_system_clock()
{
	asm volatile ("push %0" :: "n"(L4_syscall_system_clock));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number
	register word_t clock_lo asm ("eax");
	register word_t clock_hi asm ("edx");
	return ((L4_clock_t)clock_hi << 32) | clock_lo;
}

// do ThreadSwitch syscall
void l4_thread_switch(L4_thrid_t dest)
{
/*
	register L4_thrid_t r0 asm ("r0") = dest;
	register word_t     r7 asm ("r7") = L4_syscall_thread_switch;
	(void)r0; (void)r7;
	do_syscall();
*/
	l4_kdb("IMPME:  l4_thread_switch().\n", 1);
}

int l4_schedule(L4_thrid_t dest, word_t time_ctl, word_t proc_ctl, word_t prio,
                word_t preempt_ctl, word_t* state, word_t* ret_time_ctl)
{
	register L4_thrid_t eax asm ("eax") = dest;
	register word_t     edx asm ("edx") = time_ctl;
	register word_t     esi asm ("esi") = proc_ctl;
	register word_t     ecx asm ("ecx") = prio;
	register word_t     edi asm ("edi") = preempt_ctl;
	(void)eax; (void)edx; (void)esi; (void)ecx; (void)edi;
	asm volatile ("push %0" :: "n"(L4_syscall_schedule));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number
	register word_t result       asm ("eax");
	register word_t cur_time_ctl asm ("edx");
	if (state)
		*state = result;
	if (ret_time_ctl)
		*ret_time_ctl = cur_time_ctl;
	return result==0 ? l4_utcb()->error() : 0;
}

int l4_exreg(L4_thrid_t* dest, word_t ctrl, word_t* sp, word_t* ip,
             word_t* flags, L4_thrid_t* pager, word_t* usr_def_handle)
{
// FIXME:  it works incorrect now
	register word_t     eax asm ("eax") = dest->raw();
	register word_t     ecx asm ("ecx") = ctrl;
	register word_t     edx asm ("edx") = sp ? *sp : 0;
	register word_t     esi asm ("esi") = ip ? *ip : 0;
	register word_t     edi asm ("edi") = flags ? *flags: 0;
	register word_t     ebx asm ("ebx") = usr_def_handle ? *usr_def_handle : 0;
	asm volatile ("mov %0, %%ebp" :: "m"(pager ? pager->raw() : 0));
	(void)eax; (void)ecx; (void)edx; (void)esi; (void)edi; (void)ebx;
	asm volatile ("push %0" :: "n"(L4_syscall_exchange_registers));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number
	L4_thrid_t result(eax);
	*dest = result;
	if (sp)
		*sp = edx;
	if (ip)
		*ip = esi;
	if (flags)
		*flags = edi;
	if (pager)
		//*pager = r6;
		asm volatile ("mov %%ebp, %0" : "=m"(pager));
	if (usr_def_handle)
		*usr_def_handle = ebx;
	return result.is_nil() ? l4_utcb()->error() : 0;
}

// run KDB
int l4_kdb(int opcode, int param, void* data, unsigned sz)
{
	register int    eax asm ("eax") = opcode;
	register int    ecx asm ("ecx") = param;
	register void*  edx asm ("edx") = data;
	register word_t esi asm ("esi") = sz;

	(void)eax; (void)ecx; (void)edx; (void)esi;

	asm volatile ("push %0" :: "n"(L4_syscall_kdb));
	do_syscall();
	asm volatile ("add $4, %esp"); // leave syscall number

	register int res asm ("eax");
	return res;
}

// run KDB
void l4_kdb(const char* str, bool error_entry)
{
	l4_kdb(L4_kdb_console, error_entry, (void*)str, 0);
}

extern "C" void break_execution(const char* str) { l4_kdb(str); }

// XXX replace to l4io.cpp or l4console.cpp

// slow output string via IPC to kernel
void l4_out_string(const char* str, size_t len)
{
	L4_utcb_t* utcb = l4_utcb();

	// prepare msgtag
	utcb->msgtag().ipc_label(0x201); // Log // TODO:  use enums
	utcb->msgtag().untyped(0);
	utcb->msgtag().typed(2);

	// put string
	L4_string_item_t* sitem = (L4_string_item_t*) &utcb->mr[1];
	sitem->simple((word_t)str, len);

	// send msg to kernel
	L4_time_t never(L4_time_t::Never);
	L4_timeouts_t timeouts(never, never);
	l4_ipc(L4_thrid_t::Any, L4_thrid_t::Nil, timeouts);
}
