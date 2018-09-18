//##################################################################################################
//
//  Thread - to create and manage by thread.
//
//##################################################################################################

#include "wrm_thr.h"
#include "wrm_mpool.h"
#include "wrm_log.h"
#include "wrm_labels.h"
#include "l4_api.h"
#include "l4_thrid.h"
#include "sys_stack.h"
#include "wlibc_panic.h"
#include "wlibc_assert.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


// thread entry point
// there are 3 params on top of the stack:  flags, entry, arg
// use assm code to avoid function epilogue influens on SP
static void thread_func()
{
#if defined (Cfg_arch_sparc)
	asm volatile
	(
		"ld [%sp + 0], %o0   \n"
		"ld [%sp + 4], %o1   \n"
		"ld [%sp + 8], %o2   \n"
		"add %sp, 12, %sp    \n"
		"add %sp, -96, %sp   \n"
		"b thread_startup    \n"
		" nop                \n"
	);
#elif defined (Cfg_arch_arm)
	asm volatile
	(
		"pop {r0, r1, r2}    \n"
		"b thread_startup    \n"
	);
#elif defined (Cfg_arch_x86)
	asm volatile
	(
		"call thread_startup \n"
	);
#elif defined (Cfg_arch_x86_64)
	asm volatile
	(
		"pop %rdi            \n"
		"pop %rsi            \n"
		"pop %rdx            \n"
		"call thread_startup \n"
	);
#else
#error Unknown arch.
#endif
}

// app data - initialized on app startup, but locate here to possible link shared library
word_t tls_s   = 0;
word_t tls_e   = 0;
word_t tdata_s = 0;
word_t tdata_e = 0;
word_t tbss_s  = 0;
word_t tbss_e  = 0;

extern "C" void wrm_thr_init_tls()
{
	size_t tls_sz   = tls_e   - tls_s;
	size_t tdata_sz = tdata_e - tdata_s;
	size_t tbss_sz  = tbss_e  - tbss_s;
	if (!tls_sz)
		return;
	char* tls_area = (char*) malloc(tls_sz);
	if (!tls_area)
		panic("wrm_mpool_alloc(%zu) - failed.\n", tls_sz);
	memcpy(tls_area + tdata_s - tls_s, (void*)tdata_s, tdata_sz);
	memset(tls_area + tbss_s - tls_s, 0, tbss_sz);
	#if defined(Cfg_arch_arm) or defined(Cfg_arch_x86) or defined(Cfg_arch_x86_64)
	panic("TLS for ARM/x86/x86-64 is not supported yet.");
	#endif
	Proc::tls((word_t)tls_area + tls_sz);
}

// common threads entry - pop from the stack thread_addr and thread_arg and run thread(arg)
extern "C" void thread_startup(long flags, L4_thread_func_t entry, long arg)
{
	wrm_thr_init_tls();

	// enable fpu if need
	if (flags)
	{
		int rc = l4_exreg_flags(l4_utcb()->local_id(), (word_t*)&flags, NULL);
		if (rc)
			panic("l4_exreg_flags(fpu) - rc=%u.\n", rc);
	}

	// run target thread function
	long rc = entry(arg);

	L4_utcb_t* utcb = l4_utcb();
	L4_thrid_t thrid = utcb->global_id();
	//wrm_logw("wrm:  thread=%u terminated with rc=%ld.\n", thrid.number(), rc);

	// notify alpha about thread termination
	int term_state;
	L4_fpage_t utcb_fp;
	addr_t stack;
	size_t stack_sz;
	wrm_thr_delete(thrid, &rc, &term_state, &utcb_fp, &stack, &stack_sz);
	l4_kdb("thread terminated but is executing");
}

// send thread-create request to alpha
extern "C" int wrm_thr_create(L4_fpage_t utcb_location, L4_thread_func_t entry, long arg, addr_t stack,
                              size_t stack_sz, unsigned prio, const char* short_name, word_t flags,
                              L4_thrid_t* id, L4_thrid_t space, L4_thrid_t sched, L4_thrid_t pager)
{
	L4_utcb_t* utcb = l4_utcb();

	// put thread's entry point and thread's argument on the stack
	addr_t sp = stack + stack_sz;
	Stack::push(&sp, arg);
	Stack::push(&sp, (word_t)entry);
	Stack::push(&sp, flags);

	L4_msgtag_t tag;
	tag.set_ipc(Wrm_ipc_create_thread, 7, 0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = utcb_location.raw();
	utcb->mr[2] = prio;
	utcb->mr[3] = space.raw();
	utcb->mr[4] = sched.raw();
	utcb->mr[5] = pager.raw();
	utcb->mr[6] = stack;
	utcb->mr[7] = stack_sz;
	// TODO:  utcb->mr[8] = TLS

	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = l4_thrid_roottask();
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from);
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) - rc=%d.\n", __func__, rc);
		return 1;
	}

	tag = utcb->msgtag();
	word_t     ecode = utcb->mr[1];  // error code
	L4_thrid_t thrid = utcb->mr[2];  // new thread id

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_create_thread  ||        // bad label
		!(tag.untyped() == 2  &&  tag.typed() == 0))        // ecode and thrid
	{
		wrm_loge("%s:  Wrm_ipc_create_thread wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_create_thread, tag.typed(), tag.untyped());
		return 2;
	}

	if (ecode) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_create_thread failed, ecode=%lu.\n", __func__, ecode);
		if (ecode == 1)   // no app
			return 3;
		if (ecode == 2)   // no device
			return 4;
		if (ecode == 3)   // no permission
			return 5;
		return 6;         // unknown err code
	}

	// start local thread by sending Thread_start msg
	tag.set_ipc(L4_msgtag_t::Thread_start, 3, 0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = (word_t)thread_func;
	utcb->mr[2] = sp;
	utcb->mr[3] = *(word_t*)short_name;
	rc = l4_send(thrid, L4_time_t::Zero);
	if (rc)
	{
		wrm_logd("%s:  l4_send(start_msg) - rc=%d.\n", __func__, rc);
		return 8;
	}
	*id = thrid;
	return 0;
}

extern "C" int wrm_thr_delete(L4_thrid_t id, long* term_code, int* state, L4_fpage_t* utcb_fp,
                              addr_t* stack, size_t* stack_sz)
{
	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.set_ipc(Wrm_ipc_delete_thread, 2, 0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = id.raw();
	utcb->mr[2] = *term_code;
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = l4_thrid_roottask();
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from);
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) - rc=%d.\n", __func__, rc);
		return 1;
	}

	// FIXME
	// FIXME:  free(tls)
	// FIXME

	tag = utcb->msgtag();
	word_t     ecode = utcb->mr[1];
	L4_thrid_t thrid = utcb->mr[2];
	*utcb_fp   = utcb->mr[3];
	*stack     = utcb->mr[4];
	*stack_sz  = utcb->mr[5];
	*term_code = utcb->mr[6];
	*state     = utcb->mr[7];

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_delete_thread  ||        // bad label
		thrid != id  ||                                     // bad thrid
		tag.untyped() != 7  ||  tag.typed() != 0)           // bad length
	{
		wrm_loge("%s:  Wrm_ipc_delete_thread wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_delete_thread, tag.typed(), tag.untyped());
		return 2;
	}
	return ecode;
}

// send task-create request to alpha
extern "C" int wrm_tsk_create(L4_fpage_t utcb_location, L4_thread_func_t entry, long arg, addr_t stack,
                              size_t stack_sz, unsigned prio, const char* short_name, word_t flags,
                              L4_thrid_t pager, L4_fpage_t kip_area, L4_fpage_t utcbs_area, L4_thrid_t* id)
{
	L4_utcb_t* utcb = l4_utcb();

	// put thread's entry point and thread's argument on the stack
	addr_t sp = stack + stack_sz;
	Stack::push(&sp, arg);
	Stack::push(&sp, (word_t)entry);
	Stack::push(&sp, flags);

	L4_msgtag_t tag;
	tag.set_ipc(Wrm_ipc_create_task, 5, 0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = utcb_location.raw();
	utcb->mr[2] = prio;
	utcb->mr[3] = pager.raw();
	utcb->mr[4] = kip_area.raw();
	utcb->mr[5] = utcbs_area.raw();

	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = l4_thrid_roottask();
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from); // send and receive
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) - rc=%d.\n", __func__, rc);
		return 1;
	}

	tag = utcb->msgtag();
	word_t     ecode = utcb->mr[1];  // error code
	L4_thrid_t thrid = utcb->mr[2];  // new thread id

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_create_task  ||          // bad label
		!(tag.untyped() == 2  &&  tag.typed() == 0))        // ecode and thrid
	{
		wrm_loge("%s:  Wrm_ipc_create_thread wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_create_task, tag.typed(), tag.untyped());
		return 2;
	}

	if (ecode) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_create_task failed, ecode=%lu.\n", __func__, ecode);
		if (ecode == 1)   // no app
			return 3;
		if (ecode == 2)   // no device
			return 4;
		if (ecode == 3)   // no permission
			return 5;
		return 6;         // unknown err code
	}

	// start local thread by sending Thread_start msg
	tag.set_ipc(L4_msgtag_t::Thread_start, 3, 0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = (word_t)thread_func;
	utcb->mr[2] = sp;
	utcb->mr[3] = *(word_t*)short_name;
	rc = l4_send(thrid, L4_time_t::Zero);
	if (rc)
	{
		wrm_logd("%s:  l4_send(start_msg) - rc=%d.\n", __func__, rc);
		return 8;
	}
	*id = thrid;
	return 0;
}
