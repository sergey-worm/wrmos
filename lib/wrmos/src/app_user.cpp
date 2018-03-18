//##################################################################################################
//
//  App - user API for applications.
//
//##################################################################################################

#include "wrm_app_user.h"
#include "wrm_mpool.h"
#include "wrm_log.h"
#include "wrm_labels.h"
#include "l4_api.h"
#include <assert.h>
#include <string.h>

// send app-threads request to alpha
int wrm_app_threads(L4_thrid_t app_id, unsigned* thrno_begin, unsigned* thrno_end)
{
	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.ipc_label(Wrm_ipc_app_threads);
	tag.propagated(false);
	tag.untyped(1);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = app_id.raw();
	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1); //TODO: make api for get alpha ID
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from);
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return 2;
	}

	assert(from == alpha);

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];  // error code

	if (tag.ipc_label() != Wrm_ipc_app_threads        ||  // bad label
		!(tag.untyped() == 3  &&  tag.typed() == 0))  // bad msg format
	{
		wrm_loge("%s:  Wrm_ipc_register_thread wrong reply format:  lbl_ok=%d, t=%u, u=%u.\n",
			__func__, tag.ipc_label()==Wrm_ipc_app_threads, tag.typed(), tag.untyped());
		return 3;
	}

	if (ecode) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_app_threads failed, ecode=%lu.\n", __func__, ecode);
		if (ecode == 1)  return 4;  // no app
		if (ecode == 2)  return 5;  // no device
		if (ecode == 3)  return 6;  // no permission
		return 7;                   // unknown err code
	}

	*thrno_begin = utcb->mr[2];
	*thrno_end = utcb->mr[3];
	return 0;
}
