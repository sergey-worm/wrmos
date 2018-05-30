//##################################################################################################
//
//  sigma0 - root pager.
//
//##################################################################################################

#include "l4_api.h"
#include "l4_thrid.h"
#include "wrmos.h"
#include "list.h"
#include "sys_utils.h"
#include "wlibc_cb.h"
#include "wlibc_panic.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

extern "C" { void test_snprintf(); }

uint8_t roottask_utcb [Cfg_page_sz] __attribute__((aligned(Cfg_page_sz))); // FIXME

// TODO:  replace to Wrm API
struct Mem_region_t
{
	addr_t _addr;
	size_t _size;
	word_t _acc;

	Mem_region_t(addr_t adr, size_t sz, word_t ac) : _addr(adr), _size(sz), _acc(ac) {}

	void set(addr_t adr, size_t sz, word_t ac)
	{
		_addr = adr;
		_size = sz;
		_acc  = ac;
	}

	addr_t addr() { return _addr; }
	addr_t size() { return _size; }
	acc_t  acc()  { return _acc;  }
	addr_t end()  { return _addr + _size; }

	bool is_inside(L4_fpage_t p)
	{
		return p.addr() >= addr()  &&  p.end() <= end();
	}

	inline bool is_acc_fit(acc_t acc)
	{
		return (acc & _acc) == acc;
	}

	inline bool is_inside_acc(L4_fpage_t p)
	{
		return is_inside(p)  &&  is_acc_fit(p.access());
	}
};

static size_t conv_mem_size = 0;

static void init_io()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = NULL;
	cb->out_string = l4_kdb_putsn;
}

int main()
{
	init_bss();                        // clear .bss section
	init_io();                         // initialize IO
	call_ctors();                      // call user ctors

	L4_kip_t* kip = l4_kip();

	wrm_logi("hello.\n");

	//test_snprintf();

	// get memory resource list from kip.mem_descr

	typedef list_t <Mem_region_t, 16> mregions_t;
	mregions_t roottask_memory;
	mregions_t conventional_memory;

	const Kip_mem_info_t* minfo = (Kip_mem_info_t*) &kip->memory_info;
	const Mem_desc_t* mdesc = (Mem_desc_t*)(void*)((char*)kip + minfo->mem_desc_ptr);
	for (unsigned i=0; i<minfo->number; ++i)
	{
		addr_t addr = mdesc[i].low << 10;
		size_t sz = ((mdesc[i].heigh + 1) << 10) - addr;
		if (mdesc[i].type == Mem_desc_t::Bootloader)
		{
			int rtsk = 0;
			unsigned acc = Acc_nil;
			switch (mdesc[i].type_exact)
			{
				case Mem_desc_t::Bldr_sigma0_text:
				case Mem_desc_t::Bldr_sigma0_rodata:
				case Mem_desc_t::Bldr_sigma0_data:
					rtsk = 0;
					break;
				case Mem_desc_t::Bldr_roottask_text:    rtsk = 1;  acc = Acc_rx;  break;
				case Mem_desc_t::Bldr_roottask_rodata:  rtsk = 1;  acc = Acc_r;   break;
				case Mem_desc_t::Bldr_roottask_data:    rtsk = 1;  acc = Acc_rw;  break;
				case Mem_desc_t::Bldr_ramfs:            rtsk = 1;  acc = Acc_rx;  break;
				default:
					panic("Unexpected value mem_desc.t=%u.", mdesc[i].type_exact);
			}
			if (rtsk)
			{
				//wrm_logi("rootsk memory:  va=[%x - %x), pa=%x, sz=%x, acc=%s.\n",
				//	addr, addr+sz, addr, sz, acc2str(acc));
				roottask_memory.push_back(Mem_region_t(addr, sz, acc));
			}
		}
		else if (mdesc[i].type == Mem_desc_t::Conventional)
		{
			//wrm_logi("free memory:    va=[%x - %x), pa=%x, sz=%x, acc=%s.\n",
			//	addr, addr+sz, addr, sz, acc2str(Acc_rwx));
			conventional_memory.push_back(Mem_region_t(addr, sz, Acc_rwx));
			conv_mem_size += sz;
		}
	}

	wrm_logi("free memory = %#zx.\n", conv_mem_size);

	// create Roottask thread/aspace via ThreadControl

	L4_thrid_t sgm0  = l4_thrid_sigma0();
	L4_thrid_t rtsk  = l4_thrid_roottask();
	L4_thrid_t space = rtsk; // =dest for aspace creation
	L4_thrid_t sched = sgm0;
	L4_thrid_t pager = L4_thrid_t::Nil;

	int res = l4_thread_control(rtsk, space, sched, pager, (addr_t)roottask_utcb);
	//wrm_logi("l4_thread_control() - res=%u.\n", res);
	if (res)
		panic("l4_thread_control() - failed, res=%u.", res);

	// configure Roottask's aspace via SpaceControl

	word_t control = 0; // FIXME
	L4_thrid_t redirector = L4_thrid_t::Nil;
	L4_fpage_t kip_area = L4_fpage_t::create((addr_t)kip, Cfg_page_sz, L4_fpage_t::Acc_rx);
	L4_fpage_t utcb_area = L4_fpage_t::create((addr_t)roottask_utcb, sizeof(roottask_utcb), L4_fpage_t::Acc_rw);
	assert(!utcb_area.is_nil());

	res = l4_space_control(rtsk, control, kip_area, utcb_area, redirector);
	//wrm_logi("l4_space_control() - res=%u.\n", res);
	if (res)
		panic("l4_space_control() - failed, res=%u.", res);

	// activate Roottask via ThreadControl

	space = rtsk;
	sched = L4_thrid_t::Nil;
	pager = sgm0;

	res = l4_thread_control(rtsk, space, sched, pager, -1);
	//wrm_logi("l4_thread_control() - res=%u.\n", res);
	if (res)
		panic("l4_thread_control() - failed, res=%u.", res);

	// start app by sending Thread_start msg

	L4_msgtag_t tag;
	tag.ipc_label(L4_msgtag_t::Thread_start);
	tag.propagated(false);
	tag.untyped(3);
	tag.typed(0);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0] = tag.raw();;
	utcb->mr[1] = kip->roottask_ip;
	utcb->mr[2] = kip->roottask_sp;
	utcb->mr[3] = *(word_t*)"alph";
	res = l4_send(rtsk, L4_time_t::Never);
	//wrm_logi("l4_send(start_msg) - res=%u.\n", res);
	if (res)
		panic("l4_send(start_msg) - res=%u.\n", res);


	// ipc loop
	while (1)
	{
		// wait pfaults from root, memory requests from kernel/users
		L4_thrid_t from;
		//wrm_logi("wait ipc.\n");
		res = l4_receive(rtsk, L4_time_t::Never, &from);

		// copy msg to use printf below
		tag = utcb->msgtag();
		word_t mr[64];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

		//wrm_logi("received IPC msg, res=%d.\n", res);
		if (res)
			continue;

		word_t addr = mr[1];
		word_t inst = mr[2];

		//wrm_logd("%s:  rx:  tag=0x%lx:  lable=%d/%d, u=%u, t=%u, mr[1]=%lx, mr[2]=%lx.\n", __func__,
		//	tag.raw(), tag.proto_label(), tag.ipc_label(), tag.untyped(), tag.typed(), mr[1], mr[2]);

		// pagefault proto
		if (from == rtsk  &&  tag.proto_label() == L4_msgtag_t::Pagefault)
		{
			assert(tag.untyped() == 2);
			assert(tag.typed() == 0);
			word_t acc  = tag.pfault_access();
			//wrm_logi("pfault from rtsk:  addr=%lx, acc=%s, inst=%lx.\n", addr, acc2str(acc), inst);

			bool found = false;
			L4_fpage_t fault = L4_fpage_t::create(round_down(addr, Cfg_page_sz), Cfg_page_sz, acc);
			for (mregions_t::iter_t it=roottask_memory.begin(); it!=roottask_memory.end(); ++it)
			{
				if (it->is_inside_acc(fault))
				{
					found = true;
					fault.access(acc);
					break;
				}
			}

			if (!found)
			{
				wrm_loge("rtsk pfault for unexpected addr=0x%lx or acc=%s, inst=0x%lx.\n",
					addr, acc2str(acc), inst);
				l4_kdb("Rtsk pfault for unexpected addr or acc");
			}

			// grant/map

			tag.ipc_label(0);
			tag.propagated(false);
			tag.untyped(0);
			tag.typed(2);
			L4_map_item_t item = L4_map_item_t::create(fault);
			utcb->mr[0] = tag.raw();
			utcb->mr[1] = item.word0();
			utcb->mr[2] = item.word1();
			res = l4_send(from, L4_time_t::Never);
			if (res)
			{
				wrm_loge("l4_send(map) failed, res=%u.\n", res);
				l4_kdb("Sending grant/map item is failed");
			}
		}
		// sigma0 proto
		else if (from == rtsk  &&  tag.proto_label() == L4_msgtag_t::Sigma0)
		{
			assert(tag.untyped() == 2);
			assert(tag.typed() == 0);

			L4_fpage_t req_fpage = L4_fpage_t::create(mr[1]);
			//word_t req_attr = mr[2];

			//wrm_logi("memory request from rtsk:  addr=0x%lx/%ld, sz=0x%lx, acc=%s, attr=0x%lx.\n",
			//	req_fpage.addr(), req_fpage.base(), req_fpage.log2sz(), acc2str(req_fpage.access()), req_attr);

			L4_fpage_t map_fpage = L4_fpage_t::create_nil();
			if (req_fpage.base() != -1)
			{
				// IO-map request, map requested fpage 1:1
				map_fpage = req_fpage;
			}
			else
			{
				// find memory 'req_fpage' in conventional_memory

				for (mregions_t::iter_t it=conventional_memory.begin(); it!=conventional_memory.end(); ++it)
				{
					//wrm_logi("*** find:  region=(0x%x, 0x%x, %s)\n", it->addr(), it->size(), acc2str(req_fpage.access()));

					if (it->size() >= req_fpage.size()  &&  it->is_acc_fit(req_fpage.access()))
					{
						addr_t aligned_va = round_up(it->addr(), req_fpage.size());
						size_t diff = aligned_va - it->addr();
						if ((it->size() - diff) >= req_fpage.size())
						{
							conv_mem_size -= req_fpage.size();
							map_fpage.set(aligned_va, req_fpage.size(), req_fpage.access());
							if (map_fpage.is_nil())
								l4_kdb("Wrong fpage");

							// remove region
							if (!diff  &&  it->size() == req_fpage.size())
							{
								// remove all region
								conventional_memory.erase(it);
							}
							else if (!diff)
							{
								// remove part of region at the start
								it->set(it->addr() + req_fpage.size(), it->size() - req_fpage.size(), it->acc());
							}
							else if (it->end() == aligned_va + req_fpage.size())
							{
								// remove part of region at the end
								it->set(it->addr(), it->size() - req_fpage.size(), it->acc());
							}
							else
							{
								// remove part of region inside
								size_t orig_sz = it->size();
								conventional_memory.push_back(Mem_region_t(it->addr(), diff, it->acc()));
								it->set(aligned_va + req_fpage.size(), orig_sz - diff - req_fpage.size(), it->acc());
							}
							//wrm_logi("*** found:   map_fpage:  addr=%lx, sz=%lx, aligned_va=0x%lx.\n",
							//	map_fpage.addr(), map_fpage.size(), aligned_va);
							break;
						}
					}
				}
			}

			//if (!conv_mem_size && req_fpage.size()==Cfg_page_sz)
			//	wrm_logi("free memory = %#x.\n", conv_mem_size);

			// map

			tag.ipc_label(0);
			tag.propagated(false);
			tag.untyped(0);
			tag.typed(2);
			L4_map_item_t mitem = L4_map_item_t::create(map_fpage);
			utcb->mr[0] = tag.raw();
			utcb->mr[1] = mitem.word0();
			utcb->mr[2] = mitem.word1();
			res = l4_send(from, L4_time_t::Never);
			if (res)
			{
				wrm_logi("l4_send(map) failed, res=%u.\n", res);
				l4_kdb("Sending grant/map item is failed");
			}
		}
		else
		{
			wrm_loge("%s:  rx:  tag=0x%lx:  lable=%ld/%ld, u=%u, t=%u, mr[1]=%lx, mr[2]=%lx.\n", __func__,
				tag.raw(), tag.proto_label(), tag.ipc_label(), tag.untyped(), tag.typed(), mr[1], mr[2]);
			l4_kdb("Sigma0 received unexpected msg");
		}
	}

	panic("Sigma0 finished, something going wrong.");
	return 0;
}
