//##################################################################################################
//
//  IPC processing.
//
//##################################################################################################

#include "syscalls.h"
#include "threads.h"
#include "kuart.h"
#include "l4_syscalls.h"


void process_pfault(Thread_t* fault_thr, word_t fault_addr, word_t fault_access, word_t fault_inst);

// use if processor support phys copying
inline void store_phys_byte(uint8_t val, addr_t pa)
{
	addr_t aligned_pa = align_down(pa, sizeof(word_t));
	word_t word = Proc::load_phys_word(aligned_pa);
	uint8_t* ptr = (uint8_t*) &word;
	ptr[pa - aligned_pa] = val;
	Proc::store_phys_word(word, aligned_pa);
}

// use if processor support phys copying
inline uint8_t load_phys_byte(addr_t pa)
{
	addr_t aligned_pa = align_down(pa, sizeof(word_t));
	word_t word = Proc::load_phys_word(aligned_pa);
	uint8_t* ptr = (uint8_t*) &word;
	return ptr[pa - aligned_pa];
}

// src, dst and len may be not aligned by sizeof(word_t)
static void memcpy_phys_dst(addr_t src, addr_t dst_pa, size_t len)
{
	printk("%s:  src=0x%lx, dst_pa=0x%lx, len=0x%zx.\n", __func__, src, dst_pa, len);
	size_t wordsz = sizeof(word_t);

	if (Proc::is_phys_copy_supported())
	{
		if (is_aligned(src, wordsz) && is_aligned(dst_pa, wordsz) && is_aligned(len, wordsz))
		{
			for (unsigned i=0; i<len; i+=sizeof(word_t))
				Proc::store_phys_word(*(word_t*)(src+i), dst_pa + i);
		}
		else
		{
			// TODO:  may to optimize fore some cases of alignment
			for (unsigned i=0; i<len; i++)
				store_phys_byte(*(uint8_t*)(src+i), dst_pa + i);
		}
	}
	else
	{
		// alloc kspace, map dst_pa to it, copy, unmap, free kspace
		size_t sz = round_pg_up(dst_pa + len) - round_pg_down(dst_pa);
		addr_t kva = Aspace::alloc_kspace(sz);
		Sched_t::current()->task()->map(kva, round_pg_down(dst_pa), sz, Acc_kdata, Cachable);
		memcpy((void*)(kva + get_pg_offset(dst_pa)), (const void*)src, len);
		Sched_t::current()->task()->unmap(kva, sz);
		Aspace::free_kspace(kva, sz);
	}
}

// src, dst and len may be not aligned by sizeof(word_t)
static void memcpy_phys_src(addr_t src_pa, addr_t dst, size_t len)
{
	printk("%s:  src_pa=0x%lx, dst=0x%lx, len=0x%zx.\n", __func__, src_pa, dst, len);
	size_t wordsz = sizeof(word_t);
	if (Proc::is_phys_copy_supported())
	{
		if (is_aligned(src_pa, wordsz) && is_aligned(dst, wordsz) && is_aligned(len, wordsz))
		{
			for (unsigned i=0; i<len; i+=sizeof(word_t))
				*(word_t*)(dst+i) = Proc::load_phys_word(src_pa + i);
		}
		else
		{
			// TODO:  may to optimize fore some cases of alignment
			for (unsigned i=0; i<len; i++)
				*(uint8_t*)(dst+i) = load_phys_byte(src_pa + i);
		}
	}
	else
	{
		// make like in memcpy_phys_dst(), but use Acc_krodata
		panic("%s: IMPLME", __func__);
	}
}

// 'src' or 'dst' may be another aspace
// 'src', 'dst' and 'len' may be not aligned by sizeof(word_t)
static void copy_thread_buf(addr_t src, addr_t dst, size_t len, const Thread_t* snd, const Thread_t* rcv)
{
	printk("%s:  src=0x%lx, dst=0x%lx, len=0x%zx.\n", __func__, src, dst, len);

	if (!len)
		return;

	// common aspace
	if (snd->task() == rcv->task())
	{
		assert(snd->task() == Sched_t::current()->task());
		memcpy((void*)dst, (void*)src, len);
	}
	// dst is another aspace
	else if (snd->task() == Sched_t::current()->task())
	{
		if (round_pg_down(dst) != round_pg_down(dst + len - 1))
			panic("TODO:  remote dst aspace is page crossed - process it.\n");

		paddr_t dst_pa = rcv->task()->walk(round_pg_down(dst), round_pg_up(len) /*, TODO: acc*/);
		printk("%s:  dst_pa=0x%x.\n", __func__, (int)dst_pa);
		assert(dst_pa);
		dst_pa += get_pg_offset(dst);

		memcpy_phys_dst(src, dst_pa, len);
	}
	// src is another aspace
	else
	{
		assert(rcv->task() == Sched_t::current()->task());
		if (round_pg_down(src) != round_pg_down(src + len - 1))
			panic("TODO:  remote src aspace is page crossed - process it.\n");

		paddr_t src_pa = snd->task()->walk(round_pg_down(src), round_pg_up(len) /*, TODO: acc*/);
		printk("%s:  src_pa=0x%x.\n", __func__, (int)src_pa);
		assert(src_pa);
		src_pa += get_pg_offset(src);

		memcpy_phys_src(src_pa, dst, len);
	}
}

// parse all items, process it and copy to dest
static int do_normal_ipc(Thread_t* snd, Thread_t* rcv, bool propagated, bool use_local_id)
{
	printk("do_ipc:  snd=%s (%s) --> rcv=%s (%s).\n",
		snd->name(), snd->state_str(), rcv->name(), rcv->state_str());

	assert(snd != rcv);

	//assert(snd->state() == Thread_t::Send_ipc);    // ?
	//assert(rcv->state() == Thread_t::Receive_ipc); // ?

	L4_utcb_t* sutcb = snd->utcb();
	L4_utcb_t* rutcb = rcv->utcb();
	L4_msgtag_t tag = sutcb->msgtag();

	//printk("++ %s() - msg:  label=%d/%d, u=%u, t=%d.\n", __func__,
	//	tag.ipc_label(), tag.proto_label(), tag.untyped(), tag.typed());

	// copy msg_tag and from_id to dest
	rutcb->mr[0] = tag.raw();
	word_t real_sender = use_local_id ? snd->localid().raw() : snd->globid().raw();
	if (propagated)
	{
		rcv->entry_frame()->scall_ipc_from(sutcb->sender().raw());
		rutcb->sender(real_sender);  // actual sender
	}
	else
	{
		rcv->entry_frame()->scall_ipc_from(real_sender);
	}

	// untyped words - just copy to dest
	for (int i=0; i<tag.untyped(); ++i)
		rutcb->mr[i+1] = sutcb->mr[i+1];

	// for sitem processing
	unsigned next_buf = 1;  // buffer may by simple or compound string
	size_t bytes_copied = 0;

	// typed words - parse and process
	if (tag.typed())
	{
		unsigned first = tag.untyped() + 1;
		unsigned last  = tag.untyped() + tag.typed();
		for (unsigned i=first; i<=last; )
		{
			L4_typed_item_t* item = (L4_typed_item_t*) &sutcb->mr[i];
			if (i == last)    // last msg reg but typed item have size >= 2
			{
				printk("ERROR:  wrong msg:  untyped=%u, typed=%u, first=%u, last=%u, cur=%u.\n",
					tag.untyped(), tag.typed(), first, last, i);
				assert(false && "Parse error:  not enough regs for typed items.");
				return 1;
			}

			// map/grant items - do mapping and copy item to dest
			if (item->is_map_item() || item->is_grant_item())
			{
				L4_map_item_t* mitem = (L4_map_item_t*) item;
				L4_fpage_t snd_fpage = mitem->fpage();

				/**/printk("map:  map/grant item, i=%u,  mr%d=0x%lx, mr%d=0x%lx, addr=0x%lx, sz=0x%lx, acc=%d.\n",
				/**/	i, i, sutcb->mr[i], i+1, sutcb->mr[i+1],
				/**/	snd_fpage.addr(), snd_fpage.size(), snd_fpage.access());

				assert(!snd_fpage.is_complete());

				addr_t src_va = 0;
				size_t src_sz = 0;
				addr_t dst_va = 0;
				size_t dst_sz = 0;
				int    dst_acc = 0;

				if (!snd_fpage.is_nil())
				{
					int cached = snd->task()->cached(snd_fpage);  // return <0 if alien fpage
					if (cached < 0)
					{
						printk("map:  attempt to map/grant memory that is not mapped to current aspace.\n");
						printk("map:  send pfault to sender.pager.\n");

						// send pfault request to snd->pager
						process_pfault(snd, snd_fpage.addr(), snd_fpage.access(), snd_fpage.addr());

						cached = snd->task()->cached(snd_fpage);  // try again
						if (cached < 0)
						{
							snd->task()->dump();
							panic("map:  attempt to map/grant alien fpage, IMPLEMENT ME.");
							// TODO:  set error code and return
						}
					}

					/*
					// FIXME:  docs says that snd_fpage may have any access, but new access = owner.acc & snd_fpage.acc
					if (!snd->task()->is_inside_acc(snd_fpage))
					{
						snd->task()->dump();
						panic("attempt to map/grant alien fpage, IMPLEMENT ME.");
						// TODO:  set error code and return
					}
					*/

					paddr_t pa = snd->task()->walk(snd_fpage.addr(), snd_fpage.size() /*, TODO: acc*/);
					assert(pa);

					L4_fpage_t acceptor = rutcb->acceptor().fpage();
					if (acceptor.is_nil()) // dst denies any map/grant operations
					{
						panic("Rcv.acceptor is nil page, IMPLEMENT ME.");
					}
					else if (acceptor.is_complete()) // dst accept in whole aspace
					{
						//printk("++ %s() - rcv.acceptor is complete fpage.\n", __func__);

						#if 0
						dst_va = rcv->task()->find_free_uspace(snd_fpage.size(), snd_fpage.size());
						assert(dst_va);
						assert(is_aligned(dst_va, snd_fpage.size()));
						printk("%s() - dst_va=0x%x.\n", __func__, dst_va);
						#else  // do not allow the kernel alloc free uspace
						dst_va = snd_fpage.addr();
						#endif

						src_va = snd_fpage.addr();
						src_sz = snd_fpage.size();
						dst_sz = src_sz;
						dst_acc = snd_fpage.access(); // TODO:  & snd_acc_permition
					}
					else // use acceptor.rcv_window
					{
						if (acceptor.size() != snd_fpage.size())
							panic("Rcv.acceptor.sz != snd_fpage.sz, snd=[%#lx - 0x%#lx), "
							      "acceptor=[%#lx - 0x%#lx), IMPLEMENT ME.",
								snd_fpage.addr(), snd_fpage.end(), acceptor.addr(), acceptor.end());

						src_va = snd_fpage.addr();
						src_sz = snd_fpage.size();
						dst_va = acceptor.addr();
						dst_sz = acceptor.size();
						dst_acc = snd_fpage.access(); // TODO:  & snd_acc_permition
					}

					rcv->task()->map(dst_va, pa, dst_sz, Aspace::uacc2acc(dst_acc), cached);

					if (mitem->is_grant())
						rcv->task()->unmap(src_va, src_sz);
				}

				// copy item to rcv msg
				L4_fpage_t rcv_fpage = L4_fpage_t::create(dst_va, dst_sz, dst_acc);
				if (mitem->is_grant())
				{
					L4_grant_item_t* rcv_item = (L4_grant_item_t*) &rutcb->mr[i];
					rcv_item->set(rcv_fpage);
				}
				else
				{
					L4_map_item_t* rcv_item = (L4_map_item_t*) &rutcb->mr[i];
					rcv_item->set(rcv_fpage);
				}
				//printk("++ %s() - mr%d=0x%x, mr%d=0x%x.\n\n", __func__, i, rutcb->mr[i], i+1, rutcb->mr[i+1]);
				i += sizeof(L4_map_item_t);
			}
			// string items - copy buffers to dest
			else if (item->is_string_item())
			{
				//printk("++ %s() - string item, i=%u.\n", __func__, i);

				if (!rutcb->acceptor().string_accepted())
					panic("Receiver does not accept string items. What need to do?"); // return error and bytes_copied = 0

				if (next_buf == -1)
					panic("No more buffers to copy string item. What need to do?");   // return error and bytes_copied

				L4_string_item_t* sitem = (L4_string_item_t*) item;

				L4_string_item_t bitem = L4_string_item_t::create(rutcb->br[next_buf], rutcb->br[next_buf+1]);

				if (bitem.is_last())
					next_buf = -1;  // no more buffers
				else
					next_buf += 1 + bitem.substring_number();

				// copy string items (simple or compound) one by one
				rutcb->mr[i] = sitem->word0();
				unsigned num = sitem->substring_number();
				for (unsigned j=0; j<num; ++j)
				{
					//printk("++ %s() - substring, i=%u, j=%u.\n", __func__, i, j);
					if (i+j > last)
					{
						printk("ERROR:  sitem point out of msg:  untyped=%u, typed=%u, sitem_reg=%u, sstr_reg=%u.\n",
							tag.untyped(), tag.typed(), i, i+j);
						assert(false);
						return 2;
					}

					// copy str_item data to buf_item buffers

					if (sitem->substring_number() > 1)
						panic("TODO:  support compound strings.\n");

					if (bitem.substring_number() > 1)
						panic("TODO:  support compound buffers.\n");

					unsigned len = min(sitem->length(), bitem.length());
					copy_thread_buf(sitem->pointer(j), bitem.pointer(0), len, snd, rcv);

					rutcb->mr[i+1+j] = (word_t)bitem.pointer(0);
					bytes_copied += len;

					if (len < sitem->length())
					{
						printk("WRN:  strlen=%u, buflen=%u;  TODO:  return err to sndr and rcvr and offset = bytes_copied.\n",
							sitem->length(), bitem.length());

						// sender anf receiver will get MsgOverflow error
						sutcb->ipc_error_code(L4_ipcerr_t(L4_snd_phase, L4_ipc_overflow, bytes_copied));
						rutcb->ipc_error_code(L4_ipcerr_t(L4_rcv_phase, L4_ipc_overflow, bytes_copied));
						tag.ipc_set_failed();
						sutcb->mr[0] = tag.raw();
						rutcb->mr[0] = tag.raw();
						return 0;
					}
				}
				i += num + 1;  // add sitem size

				if (!item->is_last()  &&  i > last)
				{
					printk("ERROR:  wrong msg:  untyped=%u, typed=%u, first=%u, last=%u, cur=%u.\n",
						tag.untyped(), tag.typed(), first, last, i);
					assert(false && "Parse error:  regs is over but sitem is not last.");
					return 3;
				}

				if (item->is_last()  &&  i != last + 1)
				{
					printk("ERROR:  wrong msg:  untyped=%u, typed=%u, first=%u, last=%u, cur=%u.\n",
						tag.untyped(), tag.typed(), first, last, i);
					assert(false && "Parse error:  last sitem, but is not last reg.");
					return 4;
				}
			}
			else
			{
				assert(false && "Parse error:  unknown typed item.");
				return 5;
			}
		}
	}
	return 0;
}

void do_pfault_ipc(const Thread_t* snd, Thread_t* rcv, word_t fault_addr, word_t fault_access, word_t fault_inst)
{
	printk("do_pf_ipc:  snd=%s (%s) --> rcv=%s (%s).\n",
		snd->name(), snd->state_str(), rcv->name(), rcv->state_str());

	assert(snd != rcv);

	//L4_utcb_t* sutcb = snd->utcb();
	L4_utcb_t* rutcb = rcv->utcb();

	L4_msgtag_t tag;
	tag.proto_label(L4_msgtag_t::Pagefault);
	tag.propagated(0);
	tag.pfault_access(fault_access);
	tag.untyped(2);
	tag.typed(0);
	rutcb->mr[0] = tag.raw();
	rutcb->mr[1] = fault_addr;
	rutcb->mr[2] = fault_inst;
	rcv->entry_frame()->scall_ipc_from(snd->globid().raw());
}

void do_exc_ipc(const Thread_t* snd, Thread_t* rcv, int exc_type, word_t pfault_addr_and_acc)
{
	printk("do_exc_ipc:  snd=%s (%s) --> rcv=%s (%s).\n",
		snd->name(), snd->state_str(), rcv->name(), rcv->state_str());

	assert(snd != rcv);

	L4_utcb_t* rutcb = rcv->utcb();

	L4_msgtag_t tag;
	tag.proto_label(exc_type);
	tag.propagated(0);
	tag.untyped(1 + sizeof(Entry_frame_t)/sizeof(word_t));
	tag.typed(0);
	rutcb->mr[0] = tag.raw();
	rutcb->mr[1] = pfault_addr_and_acc;
	memcpy(&rutcb->mr[2], snd->entry_frame(), sizeof(Entry_frame_t));
	rcv->entry_frame()->scall_ipc_from(snd->globid().raw());
}

// send pf-msg from fault_thr to its pager
void process_pfault(Thread_t* fault_thr, word_t fault_addr, word_t fault_access, word_t fault_inst)
{
	printk("pfault:  addr=0x%lx, access=%ld, inst=0x%lx.\n", fault_addr, fault_access, fault_inst);

	Thread_t* pgr = fault_thr->pager();
	if (!pgr)
		panic("pagefault:  no pager.");

	if (pgr->state() == Thread_t::Receive_ipc)
	{
		// send pf-msg
 		do_pfault_ipc(fault_thr, pgr, fault_addr, fault_access, fault_inst);
		pgr->state(Thread_t::Ready);
		fault_thr->pf_save(fault_addr, fault_access, fault_inst);
		fault_thr->ipc_from_spec(pgr->globid());
		fault_thr->state(Thread_t::Receive_pfault);
		Sched_t::switch_to(pgr);
	}
	else
	{
		// wait
		fault_thr->pf_save(fault_addr, fault_access, fault_inst);
		fault_thr->ipc_to(pgr->globid());
		fault_thr->state(Thread_t::Send_pfault);
		Sched_t::switch_to_next();
	}
}

// send pf-msg from fault_thr to its exc-handler
void process_exception(Thread_t* fault_thr, int exc_type, word_t pfault_addr_and_acc)
{
	printk("exc:  inst=0x%lx.\n", fault_thr->entry_frame()->entry_pc());

	L4_thrid_t exhid = fault_thr->utcb()->exception_handler();
	Thread_t* exh = Threads_t::find(exhid);
	if (!exh || !exh->is_active())
	{
		if (!exh)
			force_printk("exc:  ERROR:  exc-handler does not exist, exhid=%u.\n", exhid.number());
		else
			force_printk("exc:  ERROR:  exc-handler is not active, exhid=%u, state=%u.\n", exhid.number(), exh->state());

		force_printk("exc:  use default exc-handler - roottask.\n");
		exhid = thrid_roottask();
		exh = Threads_t::find(exhid);
		if (!exh)
			panic("exc:  failed to find roottask thread");
		//panic("exc:  no exception handler.");
	}

	if (exh->state() == Thread_t::Receive_ipc)
	{
		// send exception msg
		do_exc_ipc(fault_thr, exh, exc_type, pfault_addr_and_acc);
		exh->state(Thread_t::Ready);
		fault_thr->ipc_from_spec(exh->globid());
		fault_thr->state(Thread_t::Receive_exception);
		Sched_t::switch_to(exh);
	}
	else
	{
		// wait
		fault_thr->exc_save(exc_type, pfault_addr_and_acc);
		fault_thr->ipc_to(exh->globid());
		fault_thr->state(Thread_t::Send_exception);
		Sched_t::switch_to_next();
	}
}

void do_ipc(Thread_t& cur, Entry_frame_t& eframe)
{
	L4_thrid_t    to        = eframe.scall_ipc_to();
	L4_thrid_t    from_spec = eframe.scall_ipc_from_spec();
	L4_timeouts_t timeouts  = eframe.scall_ipc_timeouts();
	L4_utcb_t*    utcb      = cur.uutcb();
	L4_msgtag_t   tag       = utcb->msgtag();

	printk("ipc entry:  snd=%d, rcv=%d, p=%d, u=%d, t=%d.\n", to.number(),
		from_spec.is_any() ? -1 : from_spec.number(), tag.propagated(), tag.untyped(), tag.typed());

	Thread_t* snd_partner = 0;
	Thread_t* rcv_partner = 0;

	tag.ipc_set_ok();
	utcb->mr[0] = tag.raw();

	// send phase
	if (!to.is_nil())
	{
		// interrupt re-enable message
		if (thrid_is_int(to)  &&  tag.untyped() == 1)  // re-enable msg
		{
			word_t flags = utcb->mr[1];
			unsigned irq = to.number();
			Int_thread_t* ithr = Threads_t::int_thread(irq);
			if (!ithr  ||  cur.globid() != ithr->handler())
			{
				printk("ipc:  ERROR:  wrong irq or sender is not handler of interrupt thread.\n");
				utcb->ipc_error_code(L4_ipcerr_t(L4_snd_phase, L4_ipc_no_partner));
				tag.ipc_set_failed();
				utcb->mr[0] = tag.raw();
				return;
			}

			Intc::eoi(irq);

			printk("ipc:  re-enable interrupt %u.\n", irq);
			if (flags & 0x1)  // flags:  is need clear befor unmask ?
				Intc::clear(irq);

			if (Intc::is_pending(irq))
			{
				// already pending
				Intc::clear(irq);
				Threads_t::int_thread(irq)->pending(true);
			}
			else
			{
				// wait
				Intc::unmask(irq);
			}
		}
		else
		{
			Thread_t* dst = Threads_t::find(to);
			if (!dst || !dst->is_active())
			{
				if (!dst)
					force_printk("ipc:  ERROR:  dest does not exist, dest_thr_id=%u.\n", to.number());
				else
					force_printk("ipc:  ERROR:  dest is not active, dest_thr_id=%u, state=%u.\n", to.number(), dst->state());

				utcb->ipc_error_code(L4_ipcerr_t(L4_snd_phase, L4_ipc_no_partner));
				tag.ipc_set_failed();
				utcb->mr[0] = tag.raw();
				return;
			}

			// thread start
			if (dst->state() == Thread_t::Active  &&  tag.is_thread_start())
			{
				// wrm extention:  set thread name
				if (utcb->mr[3])
				{
					dst->name((char*)&utcb->mr[3], sizeof(word_t));
					dst->task()->name((char*)&utcb->mr[3], sizeof(word_t));
				}
				dst->start(utcb->mr[1], utcb->mr[2]);
				snd_partner = dst;
			}
			else if (tag.is_pf_request())
			{
				panic("User should not send pagefault msgs, IMPLEMENT ME.");
				// TODO:  set error code and return
			}
			// TODO:  check that msg from pager
			else if (dst->state() == Thread_t::Receive_pfault  &&  tag.is_pf_reply())
			{
				printk("pf_reply:  dst:  name=%s, state=%s.\n", dst->name(), dst->state_str());

				// check that typed item is map or grant
				assert(((const L4_typed_item_t*)&utcb->mr[1])->is_map_item() ||
			    	   ((const L4_typed_item_t*)&utcb->mr[1])->is_grant_item());

				// one map/grant item in msg
				L4_map_item_t* item = (L4_map_item_t*) &utcb->mr[1];
				L4_fpage_t fpage = item->fpage();

				printk("pf_reply:  cur:  name=%s, state=%s, task=0x%p.\n", cur.name(), cur.state_str(), cur.task());
				printk("pf_reply:  pg:   addr=0x%lx, sz=0x%lx, acc=%d.\n", fpage.addr(), fpage.size(), fpage.access());

				// wrm extention:  if fpage is complete - raise exception
				if (fpage.is_complete())
				{
					printk("pf_reply:  pager was unable to process pfault request - raise exception.\n");
					process_exception(dst, L4_msgtag_t::Mmu_exception, dst->pf_addr() | dst->pf_access());
				}
				// if fpage is not nil - do mapping and resume dst thread
				else if (!fpage.is_nil())
				{
					if (!cur.task()->is_inside_acc(fpage))
					{
						printk("pf_reply:  attempt to map/grant memory that is not mapped to current aspace.\n");

						// send pfault request to current->pager
						process_pfault(&cur, fpage.addr(), fpage.access(), fpage.addr());

						if (!cur.task()->is_inside_acc(fpage))
						{
							cur.task()->dump();
							panic("pf:  attempt to map/grant alien fpage, IMPLEMENT ME.");
							// TODO:  set error code and return
						}
					}

					paddr_t pa = cur.task()->walk(fpage.addr(), fpage.size());
					//if (!pa || (cur.id()==1 && pa != fpage.addr()))
					//	cur.task()->dump();
					assert(pa);
					assert(cur.id() != 1/*sigma0*/  ||  pa == fpage.addr());
					addr_t va = round_down(dst->pf_addr(), Cfg_page_sz);

					dst->task()->map(va, pa, fpage.size(), Aspace::uacc2acc(fpage.access()), Cachable);

					if (item->is_grant())
						dst->task()->unmap(fpage.addr(), fpage.size());

					dst->state(Thread_t::Ready);
					snd_partner = dst;
					assert(dst->prio_heir().is_nil());
				}
				// fpage is nil - skip mapping and just resume dst thread
				else
				{
					assert(fpage.is_nil());
					dst->state(Thread_t::Ready);
					snd_partner = dst;
					assert(dst->prio_heir().is_nil());
				}
			}
			// TODO:  check that msg from exc-handler
			else if (dst->state() == Thread_t::Receive_exception  &&  tag.is_exc_reply())
			{
				printk("exc_reply:  dst:  name=%s, state=%s.\n", dst->name(), dst->state_str());
				//dst->entry_frame()->entry_pc(utcb->mr[1]);
				memcpy(dst->entry_frame(), &utcb->mr[2], sizeof(Entry_frame_t));
				dst->state(Thread_t::Ready);
				snd_partner = dst;
				assert(dst->prio_heir().is_nil());
			}
			// wrm extention - trigger software irq
			else if (tag.ipc_label() == 0x202)
			{
				printk("ipc:  trigger sw irq for dst=%u.\n", dst->globid().number());
				assert(&cur != dst);
				if (dst->state() == Thread_t::Receive_ipc  &&  dst->ipc_from_spec() == dst->globid())
				{
					dst->entry_frame()->scall_ipc_from(dst->globid().raw());
					// TODO:  may to set ipc_label
					dst->state(Thread_t::Ready);
					snd_partner = dst;
				}
				else
				{
					dst->signal_pending(true);
				}
			}
			else // normal ipc message
			{
				Thread_t* sender = &cur;

				bool propagated = false;
				if (tag.propagated())
				{
					printk("ipc:  propagate:  to=%u, virt_sender=%u.\n", to.number(), utcb->sender().number());

					Thread_t* virt_sender = Threads_t::find(utcb->sender());

					// check popagate permittion conditions
					if (virt_sender &&
					    (cur.task() == virt_sender->task()  ||
					     cur.task() == dst->task()))
					{
						printk("ipc:  propagate:  permit.\n");
						sender = virt_sender;
						propagated = true;
					}
					else
					{
						printk("ipc:  propagate:  don't permit.\n");
						assert(0 && "implme: return error?");
					}
				}

				// receiver is ready for ipc
				bool use_local_id = false;
				if (dst->state() == Thread_t::Receive_ipc  &&  dst->is_good_sender(sender, &use_local_id))
				{
					int rc = do_normal_ipc(&cur, dst, propagated, use_local_id);
					if (!rc)
					{
						if (dst->state() != Thread_t::Ready) // dst may become Ready inside do_normal_ipc()
							dst->state(Thread_t::Ready);
						snd_partner = dst;
					}
					else
					{
						// TODO:  set error code:    dest?   cur?
						panic("do_normal_ipc() failed - TODO:  set error code.");
					}
				}
				// receiver is not ready for ipc, zero timeout
				else if (timeouts.snd().is_zero())
				{
					utcb->ipc_error_code(L4_ipcerr_t(L4_snd_phase, L4_ipc_timeout));
					tag.ipc_set_failed();
					utcb->mr[0] = tag.raw();
					return;
				}
				// receiver is not ready for ipc, wait
				else
				{
					// inherit prio if need - increase dst.prio up to current max prio
					if (dst != &cur  &&  dst->prio() < cur.prio_max())
					{
						dst->inherit_prio_add(cur.globid(), cur.prio_max());
						cur.prio_heir(dst->globid());
					}

					// wait
					cur.save_snd_phase(timeouts.snd(), to);
					Sched_t::switch_to_next();
				}
			}
		}
	}

	// receive phase
	if (!from_spec.is_nil()  &&  !utcb->msgtag().ipc_is_failed())
	{
		// extention - receive software irq
		if (from_spec == cur.globid()  &&  tag.ipc_label() == 0x202) // NOTE:  label for rcv!
		{
			printk("ipc:  wait sw irq.\n");
			if (cur.signal_pending())
			{
				cur.entry_frame()->scall_ipc_from(cur.globid().raw());
				cur.signal_pending(false);
			}
			else if (timeouts.rcv().is_zero())
			{
				utcb->ipc_error_code(L4_ipcerr_t(L4_rcv_phase, L4_ipc_timeout));
				tag.ipc_set_failed();
				utcb->mr[0] = tag.raw();
			}
			else
			{
				// wait
				cur.save_rcv_phase(timeouts.rcv(), from_spec);
				Sched_t::switch_to_next();
				snd_partner = 0;
			}
		}
		// l4, not extention
		else
		{
			// check existing of pending interrupt thread
			if (thrid_is_int(from_spec))
			{
				Int_thread_t* isnd = Threads_t::find_int_sender(cur, from_spec);
				if (isnd)
				{
					// already pending
					tag.proto_label(L4_msgtag_t::Interrupt);
					tag.proto_nulls();
					tag.untyped(0);
					tag.typed(0);
					utcb->mr[0] = tag.raw();
					eframe.scall_ipc_from(isnd->globid().raw());
					isnd->pending(false);
				}
				else
				{
					// wait
					cur.save_rcv_phase(timeouts.rcv(), from_spec);
					Sched_t::switch_to_next();
					snd_partner = 0;
				}
			}
			// check regular threads
			else
			{
				// TODO:  check:  if from!=any && !=any_local -- from should exists
				bool propagated = false;
				bool use_local_id = false;
				Thread_t* snd = Threads_t::find_sender(cur, from_spec, &propagated, &use_local_id);

				if (snd)
				{
					// sender was found
					if (snd->state() == Thread_t::Send_pfault)
					{
						// receive pagefault msg
						do_pfault_ipc(snd, &cur, snd->pf_addr(), snd->pf_access(), snd->pf_inst());
						snd->state(Thread_t::Receive_pfault);
						rcv_partner = snd;
					}
					else if (snd->state() == Thread_t::Send_exception)
					{
						// receive exception msg
						do_exc_ipc(snd, &cur, snd->exc_type(), snd->exc_pf());
						snd->state(Thread_t::Receive_exception);
						rcv_partner = snd;
					}
					else if (snd->state() == Thread_t::Send_ipc)
					{
						// receive normal ipc msg
						int rc = do_normal_ipc(snd, &cur, propagated, use_local_id);
						if (!rc)
						{
							if (snd->state() != Thread_t::Ready) // snd may become Ready inside do_normal_ipc()
								snd->state(Thread_t::Ready);
							rcv_partner = snd;
						}
						else
						{
							// TODO:  set error code:    dest?   cur?
							panic("do_normal_ipc() failed.");
						}
					}
					else
					{
						assert(false && "Wrong sender state.");
					}
				}
				else if (timeouts.rcv().is_zero())
				{
					// sender not found and zero timeout
					printk("ipc:  rcv:  ERROR:  no sender and timeout=0.\n");
					utcb->ipc_error_code(L4_ipcerr_t(L4_rcv_phase, L4_ipc_timeout));
					tag.ipc_set_failed();
					utcb->mr[0] = tag.raw();
				}
				else
				{
					// sender not found --> save state and switch to next thread

					// inherit prio if need - increase sender.prio up to current max prio
					if (from_spec.is_single()  &&  from_spec != cur.globid())
					{
						Thread_t* sender = Threads_t::find(from_spec);
						assert(sender);
						if (sender->prio() < cur.prio_max())
						{
							printk("ipc:  rcv:  inh prio:  myid=%u, myprio=%u, sndid=%u, sndprio=%u.\n",
								cur.globid().number(), cur.prio_max(), sender->globid().number(), sender->prio());

							sender->inherit_prio_add(cur.globid(), cur.prio_max());
							cur.prio_heir(sender->globid());
						}
					}

					// wait
					cur.save_rcv_phase(timeouts.rcv(), from_spec);
					Sched_t::switch_to_next();
					snd_partner = 0;
				}
			}
		}
	}

	assert(cur.state() == Thread_t::Ready);
	assert(cur.prio() == cur.prio_max());

	// switch to snd or rcv partner if need
	if (snd_partner || rcv_partner)
	{
		unsigned snd_partner_prio = snd_partner ? snd_partner->prio_max() : 0;
		unsigned rcv_partner_prio = rcv_partner ? rcv_partner->prio_max() : 0;
		if (max(snd_partner_prio, rcv_partner_prio)  >  cur.prio_max())
		{
			if (snd_partner_prio >= rcv_partner_prio)
				Sched_t::switch_to(snd_partner);
			else
				Sched_t::switch_to(rcv_partner);
		}
	}
}
