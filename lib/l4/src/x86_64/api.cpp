//##################################################################################################
//
//  API to L4 microkernel.
//
//##################################################################################################

#include "l4_api.h"
#include "l4_syscalls.h"
#include "sys_utils.h"

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

	register word_t rax asm ("rax") = 0x10;
	register word_t rcx asm ("rcx") = 0x11;
	register word_t rdx asm ("rdx") = 0x22;
	register word_t rsi asm ("rsi") = 0x33;
	register word_t rdi asm ("rdi") = 0x44;
	register word_t rbx asm ("rbx") = 0x55;
	register word_t r8  asm ("r8")  = 0x8;
	register word_t r9  asm ("r9")  = 0x9;
	register word_t r10 asm ("r10") = 0x10;
	register word_t r11 asm ("r11") = 0x11;
	register word_t r12 asm ("r12") = 0x12;
	register word_t r13 asm ("r13") = 0x13;
	register word_t r14 asm ("r14") = 0x14;
	register word_t r15 asm ("r15") = 0x15;
	asm volatile ("mov %0, %%rbp" :: "n"(0x66));
	(void)rax; (void)rcx; (void)rdx; (void)rsi; (void)rdi; (void)rbx;
	(void)r8; (void)r9; (void)r10; (void)r11; (void)r12; (void)r13; (void)r14; (void)r15;
	asm volatile ("push %0" :: "n"(L4_syscall_kernel_interface));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	app_data.kip = (L4_kip_t*) rax;
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
	register L4_thrid_t    rax asm ("rax") = to;
	register L4_thrid_t    rdx asm ("rdx") = from_spec;
	register L4_timeouts_t rcx asm ("rcx") = timeouts;
	(void)rax; (void)rdx; (void)rcx;
	asm volatile ("push %0" :: "n"(L4_syscall_ipc));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	register word_t rcv_from asm ("rax");
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
/*
// receive phase only
int l4_receive(L4_thrid_t from_spec, L4_time_t timeout, L4_thrid_t& from) // obsolete
{
	L4_timeouts_t timeouts(L4_time_t::Never, timeout);
	return l4_ipc(L4_thrid_t::Nil, from_spec, timeouts, from);
}
*/
int l4_receive(L4_thrid_t from_spec, L4_time_t timeout, L4_thrid_t* from)
{
	L4_timeouts_t timeouts(L4_time_t::Never, timeout);
	return l4_ipc(L4_thrid_t::Nil, from_spec, timeouts, from);
}

// do ThreadControl syscall
int l4_thread_control(L4_thrid_t dest, L4_thrid_t space, L4_thrid_t sched, L4_thrid_t pager, addr_t utcb)
{
	register L4_thrid_t rax asm ("rax") = dest;
	register L4_thrid_t rsi asm ("rsi") = space;
	register L4_thrid_t rdx asm ("rdx") = sched;
	register L4_thrid_t rcx asm ("rcx") = pager;
	register addr_t     rdi asm ("rdi") = utcb;
	(void)rax; (void)rsi; (void)rdx; (void)rcx; (void)rdi;
	asm volatile ("push %0" :: "n"(L4_syscall_thread_control));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	register word_t result asm ("rax");
	return result==1 ? 0 : l4_utcb()->error();
}

// do SpaceControl syscall
int l4_space_control(L4_thrid_t space, word_t control, L4_fpage_t kip, L4_fpage_t utcbs, L4_thrid_t redirector)
{
	register L4_thrid_t rax asm ("rax") = space;
	register word_t     rcx asm ("rcx") = control;
	register L4_fpage_t rdx asm ("rdx") = kip;
	register L4_fpage_t rsi asm ("rsi") = utcbs;
	register L4_thrid_t rdi asm ("rdi") = redirector;
	(void)rax; (void)rcx; (void)rdx; (void)rsi; (void)rdi;
	asm volatile ("push %0" :: "n"(L4_syscall_space_control));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	register word_t result asm ("rax");
	return result==1 ? 0 : l4_utcb()->error();
}

// do MemoryControl syscall
// NOTE:  fpages shoud be in MRs
int l4_memory_control(word_t control, word_t attr0, word_t attr1, word_t attr2, word_t attr3)
{
	register word_t rdx asm ("rdx") = control;
	register word_t rcx asm ("rcx") = attr0;
	register word_t rsi asm ("rsi") = attr1;
	register word_t r08 asm ("r8")  = attr2;
	register word_t r11 asm ("r11") = attr3;
	(void)rdx; (void)rcx; (void)rsi; (void)r08;(void)r11;
	asm volatile ("push %0" :: "n"(L4_syscall_memory_control));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	register word_t result asm ("rdx");
	return result==1 ? 0 : l4_utcb()->error();
}

// do SystemClock syscall
L4_clock_t l4_system_clock()
{
	asm volatile ("push %0" :: "n"(L4_syscall_system_clock));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	register word_t clock asm ("rax");
	return clock;
}

// do ThreadSwitch syscall
void l4_thread_switch(L4_thrid_t dest)
{
	l4_kdb("IMPME:  l4_thread_switch()", 1);
}

int l4_schedule(L4_thrid_t dest, word_t time_ctl, word_t proc_ctl, word_t prio,
                word_t preempt_ctl, word_t* state, word_t* ret_time_ctl)
{
	register L4_thrid_t rax asm ("rax") = dest;
	register word_t     rdx asm ("rdx") = time_ctl;
	register word_t     rsi asm ("rsi") = proc_ctl;
	register word_t     rcx asm ("rcx") = prio;
	register word_t     rdi asm ("rdi") = preempt_ctl;
	(void)rax; (void)rdx; (void)rsi; (void)rcx; (void)rdi;
	asm volatile ("push %0" :: "n"(L4_syscall_schedule));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	register word_t result       asm ("rax");
	register word_t cur_time_ctl asm ("rdx");
	if (state)
		*state = result;
	if (ret_time_ctl)
		*ret_time_ctl = cur_time_ctl;
	return result==0 ? l4_utcb()->error() : 0;
}

word_t __attribute__((noinline)) _l4_exreg(word_t dest, word_t ctrl, word_t sp, word_t ip,
             word_t flags, word_t pager, word_t usr_def_handle)
{
	// FIXME:  it works incorrect now
	register word_t     rax asm ("rax") = dest;
	register word_t     rcx asm ("rcx") = ctrl;
	register word_t     rdx asm ("rdx") = sp;
	register word_t     rsi asm ("rsi") = ip;
	register word_t     rdi asm ("rdi") = flags;
	register word_t     rbx asm ("rbx") = usr_def_handle;
	asm volatile ("mov %0, %%rbp" :: "m"(pager));
	(void)rax; (void)rcx; (void)rdx; (void)rsi; (void)rdi; (void)rbx;
	asm volatile ("push %0" :: "n"(L4_syscall_exchange_registers));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	return rax;
}

int l4_exchange_registers(L4_thrid_t* dest, word_t ctrl, word_t* sp, word_t* ip,
                          word_t* flags, L4_thrid_t* pager, word_t* usr_def_handle)
{
#if 1
	word_t res = _l4_exreg(
		dest->raw(),
		ctrl,
		sp ? *sp : 0,
		ip ? *ip : 0,
		flags ? *flags: 0,
		pager ? pager->raw() : 0,
		usr_def_handle ? *usr_def_handle : 0
	);

	L4_thrid_t result(res);
	*dest = result;
	/*
	if (sp)
		*sp = rdx;
	if (ip)
		*ip = rsi;
	if (flags)
		*flags = rdi;
	if (pager)
		asm volatile ("mov %%rbp, %0" : "=m"(pager));
	if (usr_def_handle)
		*usr_def_handle = rbx;
	*/
	return result.is_nil() ? l4_utcb()->error() : 0;

#else

// FIXME:  it works incorrect now
	register word_t     rax asm ("rax") = dest->raw();
	register word_t     rcx asm ("rcx") = ctrl;
	register word_t     rdx asm ("rdx") = sp ? *sp : 0;
	register word_t     rsi asm ("rsi") = ip ? *ip : 0;
	register word_t     rdi asm ("rdi") = flags ? *flags: 0;
	register word_t     rbx asm ("rbx") = usr_def_handle ? *usr_def_handle : 0;
	asm volatile ("mov %0, %%rbp" :: "m"(pager ? pager->raw() : 0));
	(void)rax; (void)rcx; (void)rdx; (void)rsi; (void)rdi; (void)rbx;
	asm volatile ("push %0" :: "n"(L4_syscall_exchange_registers));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number
	L4_thrid_t result(rax);
	*dest = result;
	if (sp)
		*sp = rdx;
	if (ip)
		*ip = rsi;
	if (flags)
		*flags = rdi;
	if (pager)
		asm volatile ("mov %%rbp, %0" : "=m"(pager));
	if (usr_def_handle)
		*usr_def_handle = rbx;
	return result.is_nil() ? l4_utcb()->error() : 0;

#endif
}

// run KDB
int l4_kdb(word_t opcode, word_t param, void* data, size_t sz)
{
	register word_t rax asm ("rax") = opcode;
	register word_t rcx asm ("rcx") = param;
	register void*  rdx asm ("rdx") = data;
	register word_t rsi asm ("rsi") = sz;

	(void)rax; (void)rcx; (void)rdx; (void)rsi;

	asm volatile ("push %0" :: "n"(L4_syscall_kdb));
	do_syscall();
	asm volatile ("add $8, %rsp"); // leave syscall number

	register word_t res asm ("rax");
	return res;
}
