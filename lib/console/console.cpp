//##################################################################################################
//
//  console - system console implementation;
//
//##################################################################################################

#include "console.h"
#include "console_opcodes.h"
#include "wlibc_cb.h"
#include "wrmos.h"
#include "l4_api.h"
#include <assert.h>

//--------------------------------------------------------------------------------------------------
// app local data
//--------------------------------------------------------------------------------------------------

struct Console_data_t
{
	struct Server_t
	{
		word_t key0;
		word_t key1;
		L4_thrid_t id;
	};
	Server_t srv;
};
static Console_data_t _console;

//--------------------------------------------------------------------------------------------------
// console protocol API
//--------------------------------------------------------------------------------------------------

extern "C" int w4console_init()
{
	//wrm_logd("conlib:  init.\n");

	// get thread ID by name
	const char* name = "console-server";
	int rc = wrm_nthread_get_id(name, &_console.srv.id, &_console.srv.key0, &_console.srv.key1);
	if (rc)
	{
		wrm_loge("conlib:  init:  wrm_nthread_get_id(%s) - rc=%u.\n", name, rc);
		return -1;
	}
	//wrm_logd("conlib:  init:  attached to console:  id=%u, key=%x/%x.\n",
	//	_console.srv.id.number(), _console.srv.key0, _console.srv.key1);

	return 0;
}

extern "C" int w4console_open()
{
	//wrm_logd("conlib:  open.\n");

	if (_console.srv.id.is_nil())
	{
		//wrm_loge("console:  open:  not inited.\n");
		return -1;
	}

	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(4);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = _console.srv.key0;
	utcb->mr[2] = _console.srv.key1;
	utcb->mr[3] = Console_open;       // cmd id
	utcb->mr[4] = utcb->tls[0];       // name
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t timeout = L4_time_t::create_rel(3*1000*1000);  // 3 sec
	int rc = l4_ipc(_console.srv.id, _console.srv.id, L4_timeouts_t(timeout, timeout), &from);
	if (rc)
	{
		//wrm_logi("conlib:  open:  l4_ipc(srv) - rc=%u.\n", rc);
		return -2;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0))
	{
		//wrm_loge("conlib:  open:  l4_ipc(srv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -3;
	}

	if (ecode)
	{
		//wrm_loge("conlib:  open:  l4_ipc(srv) received ecode=%u.\n", ecode);
		return -4;
	}

	//wrm_logd("conlib:  open, ok.\n");

	return 0;
}

extern "C" int w4console_close()
{
	//wrm_logd("conlib:  close.\n");

	if (_console.srv.id.is_nil())
	{
		//wrm_loge("console:  close:  not inited.\n");
		return -1;
	}

	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(3);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = _console.srv.key0;
	utcb->mr[2] = _console.srv.key1;
	utcb->mr[3] = Console_close;
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t timeout = L4_time_t::create_rel(3*1000*1000);  // 3 sec
	int rc = l4_ipc(_console.srv.id, _console.srv.id, L4_timeouts_t(timeout, timeout), &from);
	if (rc)
	{
		//wrm_logi("conlib:  close:  l4_ipc(srv) - rc=%u.\n", rc);
		return -2;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0))
	{
		//wrm_loge("conlib:  close:  l4_ipc(srv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -3;
	}

	if (ecode)
	{
		//wrm_loge("conlib:  close:  l4_ipc(srv) received ecode=%u.\n", ecode);
		return -4;
	}

	//wrm_logd("conlib:  close, ok.\n");

	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = NULL;
	cb->out_string = l4_kdb_putsn;
	cb->in_char    = NULL;
	cb->in_string  = NULL;

	return 0;
}

extern "C" int w4console_write(const void* buf, unsigned sz)
{
	//wrm_logd("conlib:  write:  bufsz=%u.\n", sz);

	if (_console.srv.id.is_nil())
	{
		//wrm_loge("conlib:  write:  not inited.\n");
		return -1;
	}

	L4_string_item_t sitem;
	sitem.simple((word_t)buf, sz);
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(3);
	tag.typed(2);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = _console.srv.key0;
	utcb->mr[2] = _console.srv.key1;
	utcb->mr[3] = Console_write;
	utcb->mr[4] = sitem.word0();
	utcb->mr[5] = sitem.word1();
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t timeout = L4_time_t::create_rel(3*1000*1000);  // 3 sec
	int rc = l4_ipc(_console.srv.id, _console.srv.id, L4_timeouts_t(timeout, timeout), &from);
	if (rc)
	{
		//wrm_loge("conlib:  write:  l4_ipc(srv) - rc=%d/%s.\n", rc, l4_ipcerr2str(rc));
		if (rc != L4_ipc_overflow)
			return -2;
		else
			return utcb->ipc_error_code().transferred();  // don't check incomming msg, just return
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	word_t sent  = utcb->mr[2];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0)  &&   // err
	   (!(tag.untyped() == 2  &&  tag.typed() == 0)))     // ok
	{
		//wrm_loge("conlib:  write:  l4_ipc(srv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -3;
	}

	if (ecode)
	{
		//wrm_loge("conlib:  write:  l4_ipc(srv) received ecode=%u.\n", ecode);
		return -4;
	}

	//wrm_logd("conlib:  write:  bufsz=%u, sent=%u.\n", sz, sent);

	return sent;
}

extern "C" int w4console_read(void* buf, unsigned sz)
{
	//wrm_logd("conlib:  read:  bufsz=%u.\n", sz);

	if (_console.srv.id.is_nil())
	{
		//wrm_loge("conlib:  read:  not inited.\n");
		return -1;
	}

	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true);  // allow strings
	L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)buf, sz);
	L4_utcb_t* utcb = l4_utcb();
	utcb->br[0] = acceptor.raw();
	utcb->br[1] = bitem.word0();
	utcb->br[2] = bitem.word1();

	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(5);
	tag.typed(0);
	utcb->mr[0]  = tag.raw();
	utcb->mr[1]  = _console.srv.key0;
	utcb->mr[2]  = _console.srv.key1;
	utcb->mr[3]  = Console_read;
	utcb->mr[4]  = sz;
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t timeout = L4_time_t::Never;
	int rc = l4_ipc(_console.srv.id, _console.srv.id, L4_timeouts_t(timeout, timeout), &from);
	unsigned received = 0;
	if (rc == 4) // MsgOverflow
	{
		// some data was been transfered, but incoming buffer to small
		received = utcb->ipc_error_code().transferred();
		rc = 0;
	}
	else if (rc)
	{
		//wrm_loge("conlib:  read:  l4_ipc(srv) - rc=%u.\n", rc);
		return -2;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	L4_string_item_t sitem;
	sitem.set(utcb->mr[2], utcb->mr[3]);

	if (!(tag.untyped() == 1  &&  tag.typed() == 0)  &&                             // err
	    !(tag.untyped() == 1  &&  tag.typed() == 2   &&  sitem.is_string_item()))   // ok
	{
		//wrm_loge("conlib:  read:  l4_ipc(srv) received wrong msg format:  u=%u, t=%u, str=%d.\n",
		//	tag.untyped(), tag.typed(), sitem.is_string_item());
		return -3;
	}

	if (ecode)
	{
		//wrm_loge("conlib:  read:  l4_ipc(srv) received ecode=%u.\n", ecode);
		return -4;
	}

	assert(sitem.pointer() == (word_t)buf);

	if (sitem.pointer() != (word_t)buf)
	{
		//wrm_loge("conlib:  read:  sitem.pointer() != buf, something going wrong.\n");
		return -5;
	}

	//if (received)
	//	wrm_logw("conlib:  read:  receive=%u, but msgsz=%u.\n", received, sitem.length());

	//wrm_logd("conlib:  read:  exit, len=%u.\n", received ? received : sitem.length());

	return received ? received : sitem.length();
}

static void out_string(const char* str, size_t len)
{
	w4console_write(str, len);
}

static int in_char()
{
	char ch;
	int rc = w4console_read(&ch, 1);
	return rc<0 ? 0 : ch;
}

static int in_string(char* buf, size_t len)
{
	return w4console_read(buf, len);
}

extern "C" void w4console_set_wlibc_cb()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = NULL;
	cb->out_string = out_string;
	cb->in_char    = in_char; // TODO:  set NULL
	cb->in_string  = in_string;
}
