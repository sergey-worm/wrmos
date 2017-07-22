//##################################################################################################
//
//  Device API - driver API to get access to MMIO device.
//
//##################################################################################################

#include "wrm_dev.h"
#include "wrm_log.h"
#include "wrm_labels.h"
#include "l4api.h"
#include <string.h>

#include "assert.h"

enum
{
	Dev_name_len_max = 32
};

/*
// make valid fpage
static L4_fpage_t make_fpage(addr_t addr, size_t size)
{
	// if size exist and != -1 --> use sz=1page, else use sz=size
	size_t vspace_sz = (size && *size!=(unsigned)-1) ? *size : Cfg_page_sz;

	// if addr==-1 --> allocate free vaddr, else use vaddr=addr
	addr_t vspace_addr = 0;
	if (*addr == (unsigned)-1)
	{
		vspace_addr = iospace;
		iospace += vspace_sz;
	}
	else
	{
		vspace_addr = round_pg_down(*addr);
	}
}
*/

// send map request to alpha
// if size exist and != -1  -->  use sz=1page, else use sz=size
// if addr==-1  -->  allocate free vaddr, else use vaddr=addr
extern "C" int wrm_dev_map_io(const char* dev_name, addr_t* addr, size_t* size)
{
	unsigned dev_name_len = strlen(dev_name) + 1;
	if (dev_name_len > Dev_name_len_max)
		return -1;

	static addr_t iospace = 0x70000000; // FIXME:  how to allocate vspace ?

	//wrm_logd("%s:  inc:  addr=0x%x, sz=0x%x.\n", __func__, *addr, size ? *size : -1);

	// if size exist and != -1 --> use sz=1page, else use sz=size
	size_t vspace_sz = (size && *size!=(unsigned)-1) ? *size : Cfg_page_sz;
	vspace_sz = align_pg_up(vspace_sz);

	// if addr==-1 --> allocate free vaddr, else use vaddr=addr
	addr_t vspace_addr = 0;
	if (*addr == (unsigned)-1)
	{
		vspace_addr = iospace;
		iospace += vspace_sz;
	}
	else
	{
		vspace_addr = round_pg_down(*addr);
	}

	//wrm_logd("%s:  use:  addr=0x%x, sz=0x%x.\n", __func__, vspace_addr, vspace_sz);

	L4_fpage_t vspace_fpage = L4_fpage_t::create(vspace_addr, vspace_sz, Acc_nil);
	assert(!vspace_fpage.is_nil());

	L4_utcb_t* utcb = l4_utcb();
	utcb->acceptor(L4_acceptor_t(vspace_fpage, false));

	unsigned words = round_up(dev_name_len, sizeof(word_t)) / sizeof(word_t);
	utcb->msgtag().ipc_label(Wrm_ipc_map_io);
	utcb->msgtag().untyped(words);
	utcb->msgtag().typed(0);

	word_t name [Dev_name_len_max / sizeof(word_t)];
	memcpy(name, dev_name, dev_name_len);
	for (unsigned i=0; i<words; ++i)
		utcb->mr[1+i] = name[i];

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	//TODO: make api for get alpha ID
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1);
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), from); // send and receive
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return -2;
	}

	L4_msgtag_t tag = utcb->msgtag();
	word_t ecode  = utcb->mr[1];  // err:  error code
	word_t offset = utcb->mr[1];  // ok:   device addr offset in fpage
	word_t sz     = utcb->mr[2];  // ok:   device space size
	L4_map_item_t mitem = L4_map_item_t::create(utcb->mr[3], utcb->mr[4]);

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_map_io  ||                   // bad label
		!((tag.untyped() == 1  &&  tag.typed() == 0)  ||    // err
		  (tag.untyped() == 2  &&  tag.typed() == 2   &&    // ok
		   mitem.is_map()      &&  !mitem.fpage().is_nil())))
	{
		wrm_loge("%s:  Wrm_ipc_map_io wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u, is_map=%d, is_nil=%d.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_map_io, tag.typed(), tag.untyped(),
			mitem.is_map(), mitem.fpage().is_nil());
		return -3;
	}

	if (tag.untyped() == 1) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_map_io failed, ecode=%u.\n", __func__, ecode);
		if (ecode == 1)   // no app
			return -4;
		if (ecode == 2)   // no device
			return -5;
		if (ecode == 3)   // no permission
			return -6;
		return -7;        // unknown err code
	}

	*addr = mitem.fpage().addr() + offset;

	if (size)
		*size = sz;

	return 0;
}

// send interrupt attach/detach request to alpha
static int send_attach_detach_request(const char* dev_name, unsigned* intno, unsigned action)
{
	unsigned dev_name_len = strlen(dev_name) + 1;
	if (dev_name_len > Dev_name_len_max)
		return -1;

	L4_utcb_t* utcb = l4_utcb();

	unsigned words = round_up(dev_name_len, sizeof(word_t)) / sizeof(word_t);
	utcb->msgtag().ipc_label(action);
	utcb->msgtag().untyped(words);
	utcb->msgtag().typed(0);
	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_nil(), false));

	word_t name [Dev_name_len_max / sizeof(word_t)];
	memcpy(name, dev_name, dev_name_len);
	for (unsigned i=0; i<words; ++i)
		utcb->mr[1+i] = name[i];

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1); //TODO: make api for get alpha ID
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), from); // send and receive
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return -2;
	}

	L4_msgtag_t tag = utcb->msgtag();
	word_t irq = utcb->mr[1];

	if (alpha != from                        ||    // bad sender
		(unsigned)tag.ipc_label() != action  ||    // bad label
		tag.untyped() != 1                   ||
		tag.typed()   != 0 )
	{
		wrm_loge("%s:  Wrm_ipc_attach_int wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_map_io, tag.typed(), tag.untyped());
		return -3;
	}

	if ((int)irq < 0)     // error reply
	{
		int ecode = (int)irq;
		wrm_loge("%s:  Wrm_ipc_map_io failed, ecode=%d.\n", __func__, ecode);
		if (ecode == 1)   // no app
			return -4;
		if (ecode == 2)   // no device
			return -5;
		if (ecode == 3)   // no permission
			return -6;
		return -7;      // unknown err code
	}

	*intno = irq;

	return 0;
}

extern "C" int wrm_dev_attach_int(const char* dev_name, unsigned* intno)
{
	return send_attach_detach_request(dev_name, intno, Wrm_ipc_attach_int);
}

extern "C" int wrm_dev_detach_int(const char* dev_name)
{
	unsigned unused;
	return send_attach_detach_request(dev_name, &unused, Wrm_ipc_detach_int);
}

// send re-enable msg and wait new interrupt
extern "C" int wrm_dev_wait_int(unsigned intno, unsigned flags)
{
	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.untyped(1);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = flags;
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_thrid_t int_id = L4_thrid_t::create_global(intno, 1);
	return l4_ipc(int_id, int_id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
}
