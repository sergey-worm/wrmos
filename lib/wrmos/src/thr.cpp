//##################################################################################################
//
//  Thread - to create and manage by thread.
//
//##################################################################################################

#include "wrm_thr.h"
#include "wrm_mpool.h"
#include "wrm_log.h"
#include "wrm_labels.h"
#include "l4api.h"
#include "stack.h"

#include "l4syscalls.h" //?

#include "assert.h"

// FIXME:  replace me
enum { Stack_frame_sz = 96 };

// common threads entry - pop from the stack thread_addr and thread_arg and run thread(arg)
static void thread_func()
{
	#if defined (Cfg_arch_sparc)
	addr_t sp = Proc::fp();
	#elif defined (Cfg_arch_arm)
	addr_t sp = Proc::sp();
	#elif defined (Cfg_arch_x86) || defined (Cfg_arch_x86_64)
	addr_t sp = Proc::sp();
	#else
	# error unsupportes arch
	#endif

	thread_entry_t entry = (thread_entry_t) Stack::pop(&sp);
	int arg = Stack::pop(&sp);

	int rc = entry(arg);

	wrm_logd("wrm:  thread terminated with rc=%d.\n", rc);
	l4_kdb("Thread terminated");
}

extern "C" int wrm_thread_flags(L4_thrid_t id, word_t flags)
{
	// set flags via Exchange Registers
	int rc = l4_exreg(&id, L4_exreg_ctl_f, 0 /*sp*/, 0 /*ip*/, &flags);
	if (rc)
		wrm_loge("%s:  l4_exreg() - failed, rc=%u.\n", __func__, rc);
	return rc;
}

// send thread-createmap request to alpha
extern "C" int wrm_thread_create(addr_t utcb_location, thread_entry_t entry, int arg, addr_t stack,
                                size_t stack_sz, unsigned prio, const char* short_name, word_t flags,
                                L4_thrid_t* id)
{
	L4_utcb_t* utcb = l4_utcb();

	// put thread's entry point and thread's argument on the stack
	addr_t sp = stack + stack_sz - Stack_frame_sz + 2*sizeof(word_t);
	Stack::push(&sp, arg);
	Stack::push(&sp, (word_t)entry);

	L4_msgtag_t tag;
	tag.ipc_label(Wrm_ipc_create_thread);
	tag.untyped(6);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = utcb_location;
	utcb->mr[2] = (word_t) thread_func; // entry  // DELME
	utcb->mr[3] = stack;                          // DELME
	utcb->mr[4] = stack_sz;                       // DELME
	utcb->mr[5] = prio;
	utcb->mr[6] = *(word_t*)short_name;           // DELME
	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1); //TODO: make api for get alpha ID
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), from); // send and receive
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return 1;
	}

	tag = utcb->msgtag();
	word_t     ecode = utcb->mr[1];  // error code
	L4_thrid_t thrid = utcb->mr[2];  // new thread id

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_create_thread  ||            // bad label
		!(tag.untyped() == 2  &&  tag.typed() == 0))        // ecode and thrid
	{
		wrm_loge("%s:  Wrm_ipc_create_thread wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_map_io, tag.typed(), tag.untyped());
		return 2;
	}

	if (ecode) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_create_thread failed, ecode=%u.\n", __func__, ecode);
		if (ecode == 1)   // no app
			return 3;
		if (ecode == 2)   // no device
			return 4;
		if (ecode == 3)   // no permission
			return 5;
		return 6;         // unknown err code
	}

	// set flags
	rc = wrm_thread_flags(thrid, flags);
	if (rc)
	{
		wrm_loge("%s:  wrm_thread_flags() - failed, rc=%d.\n", __func__, rc);
		return 7;
	}

	// start local thread by sending Thread_start msg
	tag = 0;
	tag.ipc_label(L4_msgtag_t::Thread_start);
	tag.untyped(3);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = (word_t)thread_func;
	utcb->mr[2] = sp; //stack + stack_sz - Stack_frame_sz;
	utcb->mr[3] = *(word_t*)short_name;
	rc = l4_send(thrid, L4_time_t::Zero);
	if (rc)
	{
		wrm_logd("%s:  l4_send(start_msg) - failed, rc=%u.\n", __func__, rc);
		return 8;
	}

	*id = thrid;

	return 0;
}

// privileged, only Alpha can call this func
extern "C" int wrm_thread_create_priv(L4_thrid_t id, L4_thrid_t space, addr_t utcb_location, addr_t entry/*DELME*/,
                                     addr_t stack/*DELME*/, size_t stack_sz/*DELME*/, unsigned prio, word_t short_name/*DELME*/)
{
	L4_utcb_t* utcb = l4_utcb();
	L4_thrid_t alpha = utcb->global_id();
	assert(alpha == L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1));

	//wrm_logd("%s:  alpha=0x%x/%d, dst=0x%x/%d, ep=0x%x.\n", __func__,
	//	alpha.raw(), alpha.number(), id.raw(), id.number(), entry);

	// create thread via ThreadControl

	L4_thrid_t sched = alpha;
	L4_thrid_t pager = alpha;

	int rc = l4_thread_control(id, space, sched, pager, utcb_location);
	if (rc)
	{
		wrm_loge("%s:  l4_thread_control() - failed, rc=%u.\n", __func__, rc);
		return 1;
	}

	// set thread params via Schedule, it should be done by thread scheduler
	//  XXX:  it may be outside Alpha, or not

	rc = l4_schedule(id, -1, -1, prio, -1);
	if (rc)
	{
		wrm_loge("%s:  l4_schedule() - failed, rc=%u.\n", rc);
		return 2;
	}

	/*
	// start local thread by sending Thread_start msg  XXX:  it may be outside Alpha

	L4_msgtag_t tag;
	tag.ipc_label(L4_msgtag_t::Thread_start);
	tag.untyped(3);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = entry;
	utcb->mr[2] = stack + stack_sz - Stack_frame_sz;
	utcb->mr[3] = short_name;

	rc = l4_send(id, L4_time_t::Never);
	if (rc)
	{
		wrm_logd("%s:  l4_send(start_msg) - failed, rc=%u.\n", __func__, rc);
		return 3;
	}
	*/

	return 0;
}
