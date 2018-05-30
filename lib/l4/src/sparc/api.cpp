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
	//asm volatile ("ta 0x91; nop");
	asm volatile ("ta 0x91");
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

	register word_t    g1 asm ("%g1") = L4_syscall_kernel_interface;
	(void)g1;
	do_syscall();
	register L4_kip_t* o0 asm ("%o0");

	app_data.kip = o0;
	return app_data.kip;
}

// get pointer to UTCB
L4_utcb_t* l4_utcb()
{
	#if 0
	register L4_utcb_t* utcb asm ("%g7");    // UTCB address always is in %g7
	return utcb;
	#else
	return *(L4_utcb_t**)0xff000000;    // UTCB address can be loaded from 0xff000000
	#endif
}

// get my local ID, equal to UTCB address
L4_utcb_t* l4_local_id()
{
	return l4_utcb(); //app_data.utcb;
}

// do IPC syscall
int l4_ipc(L4_thrid_t to, L4_thrid_t from_spec, L4_timeouts_t timeouts, L4_thrid_t& from) // obsolete
{
	return l4_ipc(to, from_spec, timeouts, &from);
}
int l4_ipc(L4_thrid_t to, L4_thrid_t from_spec, L4_timeouts_t timeouts, L4_thrid_t* from)
{
	register word_t        g1 asm ("%g1") = L4_syscall_ipc;
	register L4_thrid_t    o0 asm ("%o0") = to;
	register L4_thrid_t    o1 asm ("%o1") = from_spec;
	register L4_timeouts_t o2 asm ("%o2") = timeouts;
	(void)g1; (void)o0; (void)o1; (void)o2;
	do_syscall();
	register word_t rcv_from asm ("%o0");
	if (from)
		*from = rcv_from;
	return l4_utcb()->msgtag().ipc_is_failed() ? l4_utcb()->ipc_error_code().error() : 0;
}

// send phase only
int l4_send(L4_thrid_t to, L4_time_t timeout)
{
	L4_timeouts_t timeouts(timeout, L4_time_t::Never);
	L4_thrid_t unused = L4_thrid_t::Nil;
	return l4_ipc(to, L4_thrid_t::Nil, timeouts, unused);
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

void l4_unmap(word_t control)
{
	register word_t g1 asm ("%g1") = L4_syscall_unmap;
	register word_t o0 asm ("%o0") = control;
	(void)g1; (void)o0;
	do_syscall();
}

// do ThreadControl syscall
int l4_thread_control(L4_thrid_t dest, L4_thrid_t space, L4_thrid_t sched, L4_thrid_t pager, addr_t utcb)
{
	register word_t     g1 asm ("%g1") = L4_syscall_thread_control;
	register L4_thrid_t o0 asm ("%o0") = dest;
	register L4_thrid_t o1 asm ("%o1") = space;
	register L4_thrid_t o2 asm ("%o2") = sched;
	register L4_thrid_t o3 asm ("%o3") = pager;
	register addr_t     o4 asm ("%o4") = utcb;
	(void)g1; (void)o0; (void)o1; (void)o2; (void)o3; (void)o4;
	do_syscall();
	register word_t result asm ("%o0");
	return result==1 ? 0 : l4_utcb()->error();
}

// do SpaceControl syscall
int l4_space_control(L4_thrid_t space, word_t control, L4_fpage_t kip, L4_fpage_t utcbs, L4_thrid_t redirector)
{
	register word_t     g1 asm ("%g1") = L4_syscall_space_control;
	register L4_thrid_t o0 asm ("%o0") = space;
	register word_t     o1 asm ("%o1") = control;
	register L4_fpage_t o2 asm ("%o2") = kip;
	register L4_fpage_t o3 asm ("%o3") = utcbs;
	register L4_thrid_t o4 asm ("%o4") = redirector;
	(void)g1; (void)o0; (void)o1; (void)o2; (void)o3; (void)o4;
	do_syscall();
	register word_t result asm ("%o0");
	return result==1 ? 0 : l4_utcb()->error();
}

// do MemoryControl syscall
// NOTE:  fpages shoud be in MRs
int l4_memory_control(word_t control, word_t attr0, word_t attr1, word_t attr2, word_t attr3)
{
	register word_t     g1 asm ("%g1") = L4_syscall_memory_control;
	register word_t     o0 asm ("%o0") = control;
	register word_t     o1 asm ("%o1") = attr0;
	register word_t     o2 asm ("%o2") = attr1;
	register word_t     o3 asm ("%o3") = attr2;
	register word_t     o4 asm ("%o4") = attr3;
	(void)g1; (void)o0; (void)o1; (void)o2; (void)o3; (void)o4;
	do_syscall();
	register word_t result asm ("%o0");
	return result==1 ? 0 : l4_utcb()->error();
}

// do SystemClock syscall
L4_clock_t l4_system_clock()
{
	register word_t g1 asm ("%g1") = L4_syscall_system_clock;
	(void) g1;
	do_syscall();
	register word_t clock_lo asm ("%o0");
	register word_t clock_hi asm ("%o1");
	return (((L4_clock_t)clock_hi) << 32) | clock_lo;
}

// do ThreadSwitch syscall
void l4_thread_switch(L4_thrid_t dest)
{
	register word_t     g1 asm ("%g1") = L4_syscall_thread_switch;
	register L4_thrid_t o0 asm ("%o0") = dest;
	(void)g1; (void)o0;
	do_syscall();
}

int l4_schedule(L4_thrid_t dest, word_t time_ctl, word_t proc_ctl, word_t prio,
                word_t preempt_ctl, word_t* state, word_t* ret_time_ctl)
{
	register word_t     g1 asm ("%g1") = L4_syscall_schedule;
	register L4_thrid_t o0 asm ("%o0") = dest;
	register word_t     o1 asm ("%o1") = time_ctl;
	register word_t     o2 asm ("%o2") = proc_ctl;
	register word_t     o3 asm ("%o3") = prio;
	register word_t     o4 asm ("%o4") = preempt_ctl;
	(void)g1; (void)o0; (void)o1; (void)o2; (void)o3; (void)o4;
	do_syscall();
	register word_t result       asm ("%o0");
	register word_t cur_time_ctl asm ("%o1");
	if (state)
		*state = result;
	if (ret_time_ctl)
		*ret_time_ctl = cur_time_ctl;
	return result==0 ? l4_utcb()->error() : 0;
}

// incomming pointers not 0
__attribute__((noinline)) static word_t do_exreg(word_t dest, word_t ctrl, word_t* sp, word_t* ip,
             word_t* flags, word_t* pager, word_t* udhnd)
{
	register word_t     o0 asm ("%o0") = dest;
	register word_t     o1 asm ("%o1") = ctrl;
	register word_t     o2 asm ("%o2") = *sp;
	register word_t     o3 asm ("%o3") = *ip;
	register word_t     o4 asm ("%o4") = *pager;
	register word_t     o5 asm ("%o5") = *udhnd;
	register word_t     g4 asm ("%g4") = *flags;
	register word_t     g7 asm ("%g7") = *(ip+1);
	register word_t     g1 asm ("%g1") = L4_syscall_exchange_registers;
	(void)g1; (void)o1;
	do_syscall();
	word_t result(o0);
	*sp     = o2;
	*ip     = o3;
	*(ip+1) = g7;
	*flags  = g4;
	*pager  = o4;
	*udhnd  = o5;
	return result;
}

int l4_exchange_registers(L4_thrid_t* dest, word_t ctrl, word_t* sp, word_t* ip,
                          word_t* flags, L4_thrid_t* pager, word_t* usr_def_handle)
{
	register word_t _flags  = flags ? *flags: 0;
	register word_t _sp     = sp ? *sp : 0;
	register word_t _ip[2]  = { ip ? *ip : 0,  ip ? *(ip+1) : 0};
	register word_t _pager  = pager ? pager->raw() : 0;
	register word_t _udhnd  = usr_def_handle ? *usr_def_handle : 0;
	word_t res = do_exreg(dest->raw(), ctrl, &_sp, _ip, &_flags, &_pager, &_udhnd);
	L4_thrid_t result(res);
	*dest = result;
	if (sp)
		*sp = _sp;
	if (ip)
		*ip = _ip[0];
	if (ip)
		*(ip+1) = _ip[1];
	if (flags)
		*flags = _flags;
	if (pager)
		*pager = _pager;
	if (usr_def_handle)
		*usr_def_handle = _udhnd;
	return result.is_nil() ? l4_utcb()->error() : 0;
}

// run KDB
int l4_kdb(word_t opcode, word_t param, void* data, size_t sz)
{
	register int    o0 asm ("%o0") = opcode;
	register int    o1 asm ("%o1") = param;
	register void*  o2 asm ("%o2") = data;
	register word_t o3 asm ("%o3") = sz;
	register int    g1 asm ("%g1") = L4_syscall_kdb;
	(void)g1; (void)o0; (void)o1; (void)o2; (void)o3;
	do_syscall();
	register int res asm ("%o0");
	return res;
}
