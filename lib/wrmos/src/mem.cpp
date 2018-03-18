//##################################################################################################
//
//  Memory - API to get named memory, named memory is configured in project config.
//
//##################################################################################################

#include "wrm_mem.h"
#include "wrm_log.h"
#include "wrm_labels.h"
#include "l4_api.h"
#include <string.h>
#include <assert.h>

enum
{
	Mem_name_len_max = 32
};

// send map request to alpha
// if size exist and != -1  -->  use sz=1page, else use sz=size
// if addr==-1  -->  allocate free vaddr, else use vaddr=addr
extern "C" int wrm_memory_get(const char* mem_name, addr_t* addr, size_t* size, paddr_t* paddr,
                              unsigned* access, unsigned* cached, unsigned* contig)
{
	unsigned mem_name_len = strlen(mem_name) + 1;
	if (mem_name_len > Mem_name_len_max)
		return -1;

#if 1

	static addr_t iospace = 0x71000000; // FIXME:  how to allocate vspace ?

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

//

	L4_utcb_t* utcb = l4_utcb();
	utcb->acceptor(L4_acceptor_t(vspace_fpage, false));

	unsigned words = round_up(mem_name_len, sizeof(word_t)) / sizeof(word_t);
	L4_msgtag_t tag;
	tag.ipc_label(Wrm_ipc_get_named_mem);
	tag.propagated(false);
	tag.untyped(words);
	tag.typed(0);
	utcb->mr[0] = tag.raw();

#endif

	word_t name [Mem_name_len_max / sizeof(word_t)];
	memcpy(name, mem_name, mem_name_len);
	for (unsigned i=0; i<words; ++i)
		utcb->mr[1+i] = name[i];

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1); //TODO: make api for get alpha ID
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
		tag.ipc_label() != Wrm_ipc_get_named_mem   ||           // bad label
		!((tag.untyped() == 1  &&  tag.typed() == 0)  ||    // err
		  (tag.untyped() == 4  &&  tag.typed() == 2   &&    // ok
		   mitem.is_map()      &&  !mitem.fpage().is_nil())))
	{
		wrm_loge("%s:  Wrm_ipc_map_io wrong reply format:  alph=%d, lbl=%d, t=%u, u=%u, is_map=%d, is_nil=%d.\n",
			__func__, alpha==from, tag.ipc_label()==Wrm_ipc_map_io, tag.typed(), tag.untyped(),
			mitem.is_map(), mitem.fpage().is_nil());
		return -3;
	}

	if (tag.untyped() == 1) // error reply
	{
		wrm_loge("%s:  Wrm_ipc_map_io failed, ecode=%lu.\n", __func__, ecode);
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

/*
// send map request to alpha
extern "C" int wrm_memory_get(const char* mem_name, addr_t* addr, size_t* size, paddr_t* paddr,
                              unsigned* access, unsigned* cached, unsigned* contig)
{
	unsigned mem_name_len = strlen(mem_name) + 1;
	if (mem_name_len > Mem_name_len_max)
		return -1;

	L4_utcb_t* utcb = l4_utcb();

	unsigned words = round_up(mem_name_len, sizeof(word_t)) / sizeof(word_t);
	utcb->msgtag().ipc_label(Wrm_ipc_get_named_mem);
	utcb->msgtag().untyped(words);
	utcb->msgtag().typed(0);
	utcb->acceptor(L4_acceptor_t(L4_fpage_t::create(0x71000000, 4 * Cfg_page_sz, Acc_nil), false));// FIXME: how to allocate vspace ?

	word_t name [Mem_name_len_max / sizeof(word_t)];
	memcpy(name, mem_name, mem_name_len);
	for (unsigned i=0; i<words; ++i)
		utcb->mr[1+i] = name[i];

	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	const L4_thrid_t alpha = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1, 1); //TODO: make api for get alpha ID
	int rc = l4_ipc(alpha, alpha, L4_timeouts_t(never, never), &from); // send and receive
	if (rc)
	{
		wrm_loge("%s:  l4_ipc(alpha) failed, rc=%u.\n", __func__, rc);
		return -2;
	}

	L4_msgtag_t tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];  // err:  error code
	word_t phys0 = utcb->mr[1];  // ok:   phys address MSW
	word_t phys1 = utcb->mr[2];  // ok:   phys address LSW
	word_t cach  = utcb->mr[3];  // ok:   cachable attribute
	word_t cont  = utcb->mr[4];  // ok:   contiguous attribute
	L4_map_item_t mitem = L4_map_item_t::create(utcb->mr[5], utcb->mr[6]);

	if (alpha != from  ||                                   // bad sender
		tag.ipc_label() != Wrm_ipc_get_named_mem   ||           // bad label
		!((tag.untyped() == 1  &&  tag.typed() == 0)  ||    // err
		  (tag.untyped() == 4  &&  tag.typed() == 2   &&    // ok
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
*/
