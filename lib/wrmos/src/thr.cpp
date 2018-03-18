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
#include "sys_stack.h"
#include "wlibc_panic.h"
#include "wlibc_assert.h"


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

// common threads entry - pop from the stack thread_addr and thread_arg and run thread(arg)
extern "C" void thread_startup(long flags, thread_entry_t entry, long arg)
{
	// enable fpu if need
	if (flags)
	{
		int rc = wrm_thread_flags(l4_utcb()->local_id(), flags);
		if (rc)
			panic("wrm_thread_flags(fpu) - rc=%u.\n", rc);
	}

	int rc = entry(arg);

	wrm_logd("wrm:  thread terminated with rc=%d.\n", rc);
	l4_kdb("Thread terminated");
}

extern "C" int wrm_thread_flags(L4_thrid_t id, word_t flags)
{
	// set flags via Exchange Registers
	int rc = l4_exreg(&id, L4_exreg_ctl_f, 0 /*sp*/, 0 /*ip*/, &flags);
	if (rc)
		wrm_loge("%s:  l4_exreg() - rc=%d.\n", __func__, rc);
	return rc;
}

// send thread-create request to alpha
extern "C" int wrm_thread_create2(L4_fpage_t utcb_location, thread_entry_t entry, int arg, addr_t stack,
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
	tag.ipc_label(Wrm_ipc_create_thread);
	tag.propagated(0);
	tag.untyped(5);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = utcb_location.raw();
	utcb->mr[2] = prio;
	utcb->mr[3] = space.raw();
	utcb->mr[4] = sched.raw();
	utcb->mr[5] = pager.raw();

	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1); //TODO: make api for get alpha ID
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
		tag.ipc_label() != Wrm_ipc_create_thread  ||        // bad label
		!(tag.untyped() == 2  &&  tag.typed() == 0))        // ecode and thrid
	{
		wrm_loge("%s:  Wrm_ipc_create_thread wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_map_io, tag.typed(), tag.untyped());
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
	tag = 0;
	tag.ipc_label(L4_msgtag_t::Thread_start);
	tag.propagated(0);
	tag.untyped(3);
	tag.typed(0);
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

// send task-create request to alpha
extern "C" int wrm_task_create(L4_fpage_t utcbs_area, thread_entry_t entry, int arg, addr_t stack,
                               size_t stack_sz, unsigned prio, const char* short_name, word_t flags,
                               L4_thrid_t pager, L4_fpage_t kip_area, L4_thrid_t* id)
{
	L4_utcb_t* utcb = l4_utcb();

	// put thread's entry point and thread's argument on the stack
	addr_t sp = stack + stack_sz;
	Stack::push(&sp, arg);
	Stack::push(&sp, (word_t)entry);
	Stack::push(&sp, flags);

	L4_msgtag_t tag;
	tag.ipc_label(Wrm_ipc_create_task);
	tag.propagated(0);
	tag.untyped(4);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = utcbs_area.raw();
	utcb->mr[2] = prio;
	utcb->mr[3] = pager.raw();
	utcb->mr[4] = kip_area.raw();

	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1); //TODO: make api for get alpha ID
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
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_map_io, tag.typed(), tag.untyped());
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
	tag = 0;
	tag.ipc_label(L4_msgtag_t::Thread_start);
	tag.propagated(0);
	tag.untyped(3);
	tag.typed(0);
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
