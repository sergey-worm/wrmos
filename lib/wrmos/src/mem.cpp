//##################################################################################################
//
//  Memory - API to get named memory, named memory is configured in project config.
//
//##################################################################################################

#include "wrm_mem.h"
#include "wrm_log.h"
#include "wrm_labels.h"
#include "l4_api.h"
#include "l4_thrid.h"
#include <string.h>
#include <assert.h>

enum
{
	Mem_name_len_max = 32
};

// send map request to alpha
// if 'size' exist and != -1  -->  use sz='size', else sz=1page
// if 'addr'==-1  -->  allocate free vaddr, else use vaddr='addr'
extern "C" int wrm_mem_get_named(const char* mem_name, addr_t* addr, size_t* size, paddr_t* paddr,
                              unsigned* access, unsigned* cached, unsigned* contig)
{
	unsigned mem_name_len = strlen(mem_name) + 1;
	if (mem_name_len > Mem_name_len_max)
		return -1;

	static addr_t _iospace = 0x71000000; // FIXME:  how to allocate vspace ?

	//wrm_logd("%s:  inc:  addr=0x%x, sz=0x%x.\n", __func__, *addr, size ? *size : -1);

	// if 'size' exist and != -1  -->  use sz='size', else sz=1page
	size_t vspace_sz = (size && *size!=(size_t)-1) ? *size : Cfg_page_sz;
	vspace_sz = align_pg_up(vspace_sz);

	// if 'addr'==-1  -->  allocate free vaddr, else use vaddr='addr'
	addr_t vspace_addr = 0;
	if (*addr == (addr_t)-1)
	{
		vspace_addr = _iospace;
		_iospace += vspace_sz;
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

	unsigned words = round_up(mem_name_len, sizeof(word_t)) / sizeof(word_t);
	L4_msgtag_t tag;
	tag.set_ipc(Wrm_ipc_get_named_mem, words, 0);
	utcb->mr[0] = tag.raw();

	word_t name [Mem_name_len_max / sizeof(word_t)];
	memcpy(name, mem_name, mem_name_len);
	for (unsigned i=0; i<words; ++i)
		utcb->mr[1+i] = name[i];

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = l4_thrid_roottask();
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from); // send and receive
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return -2;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];  // err:  error code
	word_t phys0 = utcb->mr[1];  // ok:   phys address MSW
	word_t phys1 = utcb->mr[2];  // ok:   phys address LSW
	word_t cach  = utcb->mr[3];  // ok:   cachable attribute
	word_t cont  = utcb->mr[4];  // ok:   contiguous attribute
	L4_map_item_t mitem = L4_map_item_t::create(utcb->mr[5], utcb->mr[6]);

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_get_named_mem   ||       // bad label
		!((tag.untyped() == 1  &&  tag.typed() == 0)  ||    // err
		  (tag.untyped() == 4  &&  tag.typed() == 2   &&    // ok
		   mitem.is_map()      &&  !mitem.fpage().is_nil())))
	{
		wrm_loge("%s:  Wrm_ipc_get_named_mem:  alph=%d, lbl=%d, t=%u, u=%u, is_map=%d, is_nil=%d.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_get_named_mem, tag.typed(), tag.untyped(),
			mitem.is_map(), mitem.fpage().is_nil());
		return -3;
	}

	if (tag.untyped() == 1) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_get_named_mem - ecode=%lu.\n", __func__, ecode);
		if (ecode == 1)   // no app
			return -4;
		if (ecode == 2)   // no device
			return -5;
		if (ecode == 3)   // no permission
			return -6;
		return -7;        // unknown err code
	}

	*addr = mitem.fpage().addr();
	*size = mitem.fpage().size();

	if (paddr)
		*paddr = ((paddr_t)phys0 << 32) | phys1;

	if (access)
		*access = mitem.fpage().access();

	if (cached)
		*cached = cach;

	if (contig)
		*contig = cont;

	return 0;
}

// send map request to alpha
// 'size' - page-aligned and log2-valid size of requested memory
// 'addr' - 'size'-aligned addr
extern "C" int wrm_mem_get_usual(addr_t addr, size_t size)
{
	//wrm_logd("%s:  inc:  addr=0x%lx, sz=0x%zx.\n", __func__, addr, size);

	if (!size || !is_pg_aligned(size) || !get_log2sz(size))
	{
		wrm_loge("%s:  size=0x%zx is wrong, get_log2sz=%u.\n", __func__, size, get_log2sz(size));
		return -1;
	}

	if (!addr || !is_aligned(addr, size))
	{
		wrm_loge("%s:  addr=0x%lx is not size=0x%zx aligned.\n", __func__, addr, size);
		return -2;
	}

	L4_fpage_t vspace_fpage = L4_fpage_t::create(addr, size, Acc_nil);
	assert(!vspace_fpage.is_nil());

	L4_utcb_t* utcb = l4_utcb();
	utcb->acceptor(L4_acceptor_t(vspace_fpage, false));

	L4_msgtag_t tag;
	tag.set_ipc(Wrm_ipc_get_usual_mem, 2, 0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = addr;
	utcb->mr[2] = size;

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = l4_thrid_roottask();
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from); // send and receive
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return -3;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];  // err:  error code
	L4_map_item_t mitem = L4_map_item_t::create(utcb->mr[1], utcb->mr[2]);

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_get_usual_mem   ||       // bad label
		!((tag.untyped() == 1  &&  tag.typed() == 0)  ||    // err
		  (tag.untyped() == 0  &&  tag.typed() == 2   &&    // ok
		   mitem.is_map()      &&  !mitem.fpage().is_nil())))
	{
		wrm_loge("%s:  Wrm_ipc_get_usual_mem:  alph=%d, lbl=%d, t=%u, u=%u, is_map=%d, is_nil=%d.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_get_usual_mem, tag.typed(), tag.untyped(),
			mitem.is_map(), mitem.fpage().is_nil());
		return -4;
	}

	if (tag.untyped() == 1) // error reply
	{
		if (ecode == 2)  // heap not enough
			return -5;

		wrm_loge("%s:  Wrm_ipc_get_usual_mem - ecode=%lu.\n", __func__, ecode);
		return -6 - ecode;
	}

	return 0;
}
