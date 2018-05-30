//##################################################################################################
//
//  Named thread - allow to register threads by name.
//
//##################################################################################################

#include "wrm_nthr.h"
#include "wrm_mpool.h"
#include "wrm_log.h"
#include "wrm_labels.h"
#include "l4_api.h"
#include "l4_thrid.h"
#include <assert.h>
#include <string.h>

enum
{
	Name_len_max = 32
};

// send thread-register request to alpha
extern "C" int wrm_nthread_register(const char* name, word_t* key0, word_t* key1)
{
	unsigned name_len = strlen(name) + 1;
	if (name_len > Name_len_max)
		return 1;

	L4_utcb_t* utcb = l4_utcb();

	unsigned words = round_up(name_len, sizeof(word_t)) / sizeof(word_t);
	L4_msgtag_t tag;
	tag.ipc_label(Wrm_ipc_register_thread);
	tag.propagated(false);
	tag.untyped(words);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	word_t n [Name_len_max / sizeof(word_t)];
	memcpy(n, name, name_len);
	for (unsigned i=0; i<words; ++i)
		utcb->mr[1+i] = n[i];

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = l4_thrid_roottask();
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from);
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return 2;
	}

	assert(from == alpha);

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];  // error code
	word_t k0    = utcb->mr[2];  // key word 0
	word_t k1    = utcb->mr[3];  // key word 1

	if (tag.ipc_label() != Wrm_ipc_register_thread        ||  // bad label
		!(tag.untyped() == 3  &&  tag.typed() == 0))      // bad msg format
	{
		wrm_loge("%s:  Wrm_ipc_register_thread wrong reply format:  lbl_ok=%d, t=%u, u=%u.\n",
			__func__, tag.ipc_label()==Wrm_ipc_register_thread, tag.typed(), tag.untyped());
		return 3;
	}

	if (ecode) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_register_thread failed, ecode=%lu.\n", __func__, ecode);
		if (ecode == 1)  return 4;  // no app
		if (ecode == 2)  return 5;  // no device
		if (ecode == 3)  return 6;  // no permission
		return 7;                   // unknown err code
	}

	*key0 = k0;
	*key1 = k1;
	return 0;
}

// send get-thread-id request to alpha
extern "C" int wrm_nthread_get_id(const char* name, L4_thrid_t* id, word_t* key0, word_t* key1)
{
	unsigned name_len = strlen(name) + 1;
	if (name_len > Name_len_max)
		return 1;

	L4_utcb_t* utcb = l4_utcb();

	unsigned words = round_up(name_len, sizeof(word_t)) / sizeof(word_t);
	L4_msgtag_t tag;
	tag.ipc_label(Wrm_ipc_get_thread_id);
	tag.propagated(false);
	tag.untyped(words);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	word_t n [Name_len_max / sizeof(word_t)];
	memcpy(n, name, name_len);
	for (unsigned i=0; i<words; ++i)
		utcb->mr[1+i] = n[i];

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = l4_thrid_roottask();
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from);
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return 2;
	}

	assert(from == alpha);

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];  // error code
	word_t thrid = utcb->mr[2];  // thread ID
	word_t k0    = utcb->mr[3];  // key word 0
	word_t k1    = utcb->mr[4];  // key word 1

	if (tag.ipc_label() != Wrm_ipc_get_thread_id      ||  // bad label
		!(tag.untyped() == 4  &&  tag.typed() == 0))  // bad msg format
	{
		wrm_loge("%s:  Wrm_ipc_get_thread_id wrong reply format:  lbl_ok=%d, t=%u, u=%u.\n",
			__func__, tag.ipc_label()==Wrm_ipc_get_thread_id, tag.typed(), tag.untyped());
		return 3;
	}

	if (ecode) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_get_thread_id failed, ecode=%lu.\n", __func__, ecode);
		if (ecode == 1)  return 4;  // no app
		if (ecode == 2)  return 5;  // no device
		if (ecode == 3)  return 6;  // no permission
		return 7;                   // unknown err code
	}

	id->set(thrid);
	*key0 = k0;
	*key1 = k1;
	return 0;
}
