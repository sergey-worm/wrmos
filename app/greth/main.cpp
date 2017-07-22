//##################################################################################################
//
//  greth - userspace GRETH driver.
//
//##################################################################################################

#include "l4api.h"
#include "wrmos.h"
#include "greth.h"
#include "sys-utils.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>


// One directional stream
struct Stream_t
{
	unsigned queue_sz;           // queue size [1..128], set from config
	unsigned char** msgs;        // array of ptrs to char bufs
	unsigned used_msgs;          // number of msgs put in queue
	Wrm_sem_t sem;                // 'hw ready' semaphore
};

// driver data
struct Driver_t
{
	Greth_dev_t greth;
	unsigned diff_pa_va;         // diff between phys addr and virt addr in allocated contig mem
	Stream_t tx;
	Stream_t rx;
};
static Driver_t driver;


static void dprint_hw(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	//wrm_logw("hw:  ");
	//vprintf(fmt, args);
	va_end(args);
}

int initialize_greth_device(addr_t iomem, addr_t mem_va, paddr_t mem_pa,  size_t mem_sz)
{
	// FIXME:  set from config
	driver.tx.queue_sz = 4;
	driver.rx.queue_sz = 4;

	// FIXME:  set from config
	uint64_t mac = 0x223344556677;


	int rc = wrm_sem_init(&driver.tx.sem, Wrm_sem_binary, 0);
	if (rc)
	{
		wrm_loge("failed to init tx sem, rc=%d.", rc);
		return -1;
	}

	rc = wrm_sem_init(&driver.rx.sem, Wrm_sem_binary, 0);
	if (rc)
	{
		wrm_loge("failed to init rx sem, rc=%d.", rc);
		return -2;
	}

	// PREPARE BUFFER MAP

	// allocate descr tables and messages from 'RAM_drv_malloc'
	// [ txdtb | rxdtb | txmsgs_ptr | rxmsgs_ptr | txmsgs_buf | rxmsgs_buf ]

	unsigned msgsz = round_up(Greth_buf_max, 4);        // msg bufs should be alligned to 4 bytes
	unsigned required_sz = 0;                           //

	unsigned offset_txdtb = required_sz;                // 1 kB for tx descr table
	required_sz += 1024;                                //

	unsigned offset_rxdtb = required_sz;                // 1 kB for rx descr table
	required_sz += 1024;                                //

	unsigned offset_txmsgs_ptr = required_sz;           // tx msg ptrs virt
	required_sz += sizeof(addr_t) * driver.tx.queue_sz; //

	unsigned offset_rxmsgs_ptr = required_sz;           // rx msg ptrs virt
	required_sz += sizeof(addr_t) * driver.rx.queue_sz; //

	unsigned offset_txmsgs_buf = required_sz;           // space for tx msgs
	required_sz += msgsz * driver.tx.queue_sz;          //

	unsigned offset_rxmsgs_buf = required_sz;           // space for rx msgs
	required_sz += msgsz * driver.rx.queue_sz;          //

	wrm_logi("%s() - requird_sz=0x%x, mem_sz=0x%x.\n", __func__, required_sz, mem_sz);

	if (required_sz > mem_sz)
	{
		wrm_loge("requird_sz=0x%x, but mem_sz=0x%x.\n", required_sz, mem_sz);
		return -3;
	}

	required_sz = round_up(required_sz, Cfg_page_sz);   // round to page size

	// SET MAP TO DRIVER

	// set all pointers
	Greth_descr_t* txdtb = (Greth_descr_t*)(mem_va + offset_txdtb);       // tx descr table, should be aligned to 1kB
	Greth_descr_t* rxdtb = (Greth_descr_t*)(mem_va + offset_rxdtb);       // rx descr table, should be aligned to 1kB
	Greth_descr_t* txdtb_phys = (Greth_descr_t*)(mem_pa + offset_txdtb);  // txdtb phys addr
	Greth_descr_t* rxdtb_phys = (Greth_descr_t*)(mem_pa + offset_rxdtb);  // rxdtb phys addr
	driver.tx.msgs = (unsigned char**)(mem_va + offset_txmsgs_ptr);
	driver.rx.msgs = (unsigned char**)(mem_va + offset_rxmsgs_ptr);

	for (unsigned i=0; i<driver.tx.queue_sz; ++i)
		driver.tx.msgs[i] = (unsigned char*)(mem_va + offset_txmsgs_buf + i * msgsz);

	for (unsigned i=0; i<driver.rx.queue_sz; ++i)
		driver.rx.msgs[i] = (unsigned char*)(mem_va + offset_rxmsgs_buf + i * msgsz);

	driver.diff_pa_va = mem_pa - mem_va;

	// initialize greth struct
	Greth_dev_t* greth = &driver.greth;
	greth->regs        = (Greth_regs_t*)iomem;
	greth->tx_dtb      = txdtb;
	greth->rx_dtb      = rxdtb;
	greth->tx_dtb_phys = txdtb_phys;
	greth->rx_dtb_phys = rxdtb_phys;
	greth->tx_dtb_sz   = driver.tx.queue_sz;
	greth->rx_dtb_sz   = driver.rx.queue_sz;
	greth->mac         = mac;
	greth->dprint      = dprint_hw;

	// real device initialization
	rc = greth_init(greth);
	return rc;
}

// note:  assume that pa point to allocated contiguous memory
static inline void* msg_vaddr(void* pa)
{
	return (void*)((unsigned)pa - driver.diff_pa_va);
}

// note:  assume that va point to allocated contiguous memory
static inline void* msg_paddr(void* va)
{
	return (void*)((unsigned)va + driver.diff_pa_va);
}


// helper macro
#define break_if(cond, oper)           \
	if (1)                             \
	{                                  \
		if (cond)                      \
		{                              \
			(oper); /* do operation */ \
			break;                     \
		}                              \
	};


int write_to_hw(const unsigned char* msg, size_t len, size_t* written)
{
	*written = 0;

	if (len > Greth_buf_max)
		return 1;  // too big

	// NOTE:
	//   attempt 1 - try to write to hw, if hw busy - wait tx.sem;
	//   attempt 2 - tx.sem received, try to write, hw may be busy cause tx.sem was set by old irq;
	//   attempt 3 - tx.sem received, try to write, if hw busy - something going wrong, error
	enum { Tx_attempts = 2 }; // XXX:  used 2 for test, set up 3 in future

	int attempts = Tx_attempts;  // how much attempts allowed for tx
	int rc = 0;                  // result code
	int place = 0;               // place for trace error
	int done = 0;                // done flag

	while (attempts--)
	{
		// if exist free txmsg --> just write
		if (driver.tx.used_msgs < driver.tx.queue_sz)
		{
			void* vbuf = driver.tx.msgs[driver.tx.used_msgs];  // tx buf virt addr
			void* pbuf = msg_paddr(vbuf);                      // tx buf phys addr
			driver.tx.used_msgs++;

			// prepare new msg
			*written = len;
			memcpy(vbuf, msg, len);

			// put msg to greth
			rc = greth_transmit(&driver.greth, pbuf, len);
			break_if(rc, place = 1);

			break_if(1, done = 1); // all done
		}

		// reuse transmitted buffer
		unsigned char* pbuf = 0;       // tx buffer phys addr
		Greth_tx_status_t status;      // tx status
		int rc = greth_txstatus(&driver.greth, (void**)&pbuf, &status);
		break_if(rc, place = 2);

		// if buffer becomes free --> write
		if (status == Greth_tx_status_ok  ||  status == Greth_tx_status_err)
		{
			// free received message
			rc = greth_txfree(&driver.greth);
			break_if(rc, place = 3);

			// prepare new msg
			void* vbuf = msg_vaddr(pbuf);
			break_if(!vbuf, place = 4);

			*written = len;
			memcpy(vbuf, msg, len);

			// put msg to greth
			rc = greth_transmit(&driver.greth, pbuf, len);
			break_if(rc, place = 5);

			break_if(1, done = 1); // all done
		}

		break_if(status == Greth_tx_status_idle, place = 6); // something going wrong

		// no free tx buffers, wait

		break_if(!attempts, place = 6);    // don't wait irq

		//wrm_logi("tx:  wait sem.\n");
		rc = wrm_sem_wait(&driver.tx.sem);  // wait free tx buffer
		break_if(rc, place = 7);
	}

	if (!done) // something going wrong
	{
		wrm_loge("TX failed:  attempts=%u, place=%d, rc=%d.\n", attempts, place, rc);
		return 100;
	}
	return rc;
}

int read_from_hw(unsigned char* msg, size_t len, size_t* read)
{
	*read = 0;

	if (len < Greth_buf_max)
		return 1;  // too small

	// NOTE:
	//   attempt 1 - try to read fromhw, if no data - wait rx.sem;
	//   attempt 2 - rx.sem received, try to read, may be no data cause rx.sem was set by old irq;
	//   attempt 3 - rx.sem received, try to read, if no data - something going wrong, error
	enum { Rx_attempts = 3 };

	int attempts = Rx_attempts;     // how much attempts allowed for rx
	int rc = 0;                     // result code
	int place = 0;                  // place for trace error
	int done = 0;                   // done flag

	while (attempts--)
	{
		unsigned char* pbuf = 0;    // rx buffer phys addr
		unsigned len = 0;           // rx len
		Greth_rx_status_t status;   // rx status

		int rc = greth_rxstatus(&driver.greth, (void**)&pbuf, &len, &status);
		break_if(rc, place = 1);

		if (status == Greth_rx_status_ok)
		{
			// rx success
			void* vbuf = msg_vaddr(pbuf);
			break_if(!vbuf, place = 2);

			*read = len;
			memcpy(msg, vbuf, len);
		}

		// if buffer becomes free --> set buffer again, and break
		if (status == Greth_rx_status_ok  ||  status == Greth_rx_status_err)
		{
			// free received message
			rc = greth_rxfree(&driver.greth);
			break_if(rc, place = 3);

			// set received buffer again
			rc = greth_receive(&driver.greth, pbuf, Greth_buf_max);
			break_if(rc, place = 4);
		}

		break_if(status == Greth_rx_status_idle, place = 5); // something going wrong
		break_if(status == Greth_rx_status_ok, done = 1);    // all done

		// no received msgs, wait

		break_if(!attempts, place = 6);    // don't wait

		//wrm_logi("rx:  wait sem.\n");
		rc = wrm_sem_wait(&driver.rx.sem);  // wait data
		break_if(rc, place = 7);
	}

	if (!done) // something going wrong
	{
		wrm_loge("RX failed:  attempts=%u, place=%d, rc=%d.\n", attempts, place, rc);
		return 100;
	}
	return rc;
}

int wait_attach_msg(const char* thread_name, L4_thrid_t* client)
{
	// register thread by name
	word_t key0 = 0;
	word_t key1 = 0;
	int rc = wrm_nthread_register(thread_name, &key0, &key1);
	if (rc)
	{
		wrm_loge("tx:  wrm_nthread_register(%s) failed, rc=%u.\n", thread_name, rc);
		assert(false);
	}
	wrm_logi("tx:  thread '%s' is registered, key:  0x%x 0x%x.\n", thread_name, key0, key1);

	L4_utcb_t* utcb = l4_utcb();

	// wait attach msg loop
	L4_thrid_t from = L4_thrid_t::Nil;
	while (1)
	{
		wrm_logi("attach:  wait attach msg.\n");
		int rc = l4_receive(L4_thrid_t::Any, L4_time_t::Never, from);
		if (rc)
		{
			wrm_loge("attach:  l4_receive() failed, rc=%u.\n", rc);
			continue;
		}

		word_t ecode = 0;
		L4_msgtag_t tag = utcb->msgtag();
		word_t k0 = utcb->mr[1];
		word_t k1 = utcb->mr[2];

		if (tag.untyped() != 2  ||  tag.typed() != 0)
		{
			wrm_loge("attach:  wrong msg format.\n");
			ecode = 1;
		}

		if (!ecode  &&  (k0 != key0  ||  k1 != key1))
		{
			wrm_loge("attach:  wrong key.\n");
			ecode = 2;
		}

		// send reply
		tag.untyped(1);
		tag.typed(0);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = ecode;
		rc = l4_send(from, L4_time_t::Zero/*Never*/);
		if (rc)
			wrm_loge("attach:  l4_send(rep) failed, rc=%d.\n", rc);

		if (!ecode && !rc)
			break;  // attached
	}

	*client = from;
	wrm_logi("attach:  attached to 0x%x/%u.\n", from.raw(), from.number());
	return 0;
}

int tx_thread(int unused)
{
	wrm_logi("tx:  hello:  %s.\n", __func__);
	wrm_logi("tx:  my global_id=0x%x/%u.\n", l4_utcb()->global_id().raw(), l4_utcb()->global_id().number());

	// wait for attach message
	L4_thrid_t client = L4_thrid_t::Nil;
	int rc = wait_attach_msg("eth-tx-stream", &client);
	if (rc)
	{
		wrm_loge("tx:  wait_attach_msg() failed, rc=%u.\n", rc);
		assert(false);
	}
	wrm_logi("tx:  attached to client=0x%x/%u\n", client.raw(), client.number());

	// prepare for requests
	L4_utcb_t* utcb = l4_utcb();
	static char tx_buf[0x1000];
	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true); // allow strings
	L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)tx_buf, sizeof(tx_buf));
	utcb->br[0] = acceptor.raw();
	utcb->br[1] = bitem.word0();
	utcb->br[2] = bitem.word1();

	// wait request loop
	while (1)
	{
		//wrm_logi("tx:  wait request.\n");
		L4_thrid_t from = L4_thrid_t::Nil;
		int rc = l4_receive(client, L4_time_t::Never, from);
		if (rc)
		{
			wrm_loge("tx:  l4_receive() failed, rc=%u.\n", rc);
			assert(false);
		}

		L4_msgtag_t tag = utcb->msgtag();
		word_t mr[64];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));
		from = tag.propagated() ? utcb->sender() : from;

		//wrm_logi("tx:  received IPC:  from=0x%x/%u, tag=0x%x, u=%u, t=%u, mr[1]=0x%x, mr[2]=0x%x.\n",
		//	from.raw(), from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1], mr[2]);

		L4_typed_item_t  item  = L4_typed_item_t::create(mr[1], mr[2]);
		L4_string_item_t sitem = L4_string_item_t::create(mr[1], mr[2]);
		assert(tag.untyped() == 0);
		assert(tag.typed() == 2);
		assert(item.is_string_item());

		//wrm_logi("tx:  request is got:  buf=0x%x, sz=%u, txbf=0x%x:", sitem.pointer(), sitem.length(), tx_buf);
		const unsigned char* msg = (const unsigned char*)sitem.pointer();
		/*
		for (unsigned i=0; i<sitem.length(); ++i)
		{
			if (!(i%20))
				printf("\n <- ");
			printf(" %02x", msg[i]);
		}
		printf("\n");
		*/

		size_t written = 0;
		rc = write_to_hw(msg, sitem.length(), &written);

		// send reply to client
		tag.propagated(false);
		tag.ipc_label(0);           // ?
		tag.untyped(2);
		tag.typed(0);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = rc;           // ecode
		utcb->mr[2] = written;      // bytes written
		rc = l4_send(from, L4_time_t::Zero/*Never*/);
		if (rc)
		{
			wrm_logi("l4_send(rep) failed, rc=%u.\n", rc);
			l4_kdb("Sending reply to tcpip is failed");
		}
	}
	return 0;
}

int rx_thread(int unused)
{
	wrm_logi("rx:  hello:  %s.\n", __func__);
	wrm_logi("rx:  my global_id=0x%x/%u.\n", l4_utcb()->global_id().raw(), l4_utcb()->global_id().number());

	// wait for attach message
	L4_thrid_t client = L4_thrid_t::Nil;
	int rc = wait_attach_msg("eth-rx-stream", &client);
	if (rc)
	{
		wrm_loge("rx:  wait_attach_msg() failed, rc=%u.\n", rc);
		assert(false);
	}
	wrm_logi("rx:  attached to client=0x%x/%u\n", client.raw(), client.number());

	// prepare for requests
	L4_utcb_t* utcb = l4_utcb();
	static unsigned char rx_buf[0x1000];
	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), false);
	utcb->br[0] = acceptor.raw();

	// wait request loop
	while (1)
	{
		//wrm_logi("rx:  wait request.\n");
		L4_thrid_t from = L4_thrid_t::Nil;
		int rc = l4_receive(client, L4_time_t::Never, from);
		L4_msgtag_t tag = utcb->msgtag();
		if (rc)
		{
			wrm_loge("rx:  l4_receive() failed, rc=%u.\n", rc);
			assert(false);
		}

		word_t mr[64];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));
		from = tag.propagated() ? utcb->sender() : from;

		//wrm_logi("rx:  received IPC:  from=0x%x/%u, tag=0x%x, u=%u, t=%u, mr[1]=0x%x, mr[2]=0x%x.\n",
		//	from.raw(), from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1], mr[2]);

		assert(tag.untyped() == 0);
		assert(tag.typed() == 0);

		//wrm_logi("rx:  request is got:  <no params yet>.\n");

		size_t read = 0;
		rc = read_from_hw(rx_buf, sizeof(rx_buf), &read);
		if (rc)
		{
			wrm_loge("read_from_hw() - failed, rc=%d.\n", rc);
			continue;
		}

		/*
		wrm_logi("rx:  received (%d):", read);
		for (unsigned i=0; i<read; ++i)
		{
			if (!(i%20))
				printf("\n -> ");
			printf(" %02x", rx_buf[i]);
		}
		printf("\n");
		*/

		// send reply to client
		L4_string_item_t sitem = L4_string_item_t::create_simple((word_t)rx_buf, read);
		tag.propagated(false);
		tag.ipc_label(0);             // ?
		tag.untyped(1);
		tag.typed(2);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = rc;             // ecode
		utcb->mr[2] = sitem.word0();  //
		utcb->mr[3] = sitem.word1();  //
		rc = l4_send(from, L4_time_t::Zero/*Never*/);
		if (rc)
		{
			wrm_logi("l4_send(rep) failed, rc=%u.\n", rc);
			l4_kdb("Sending reply to tcpip is failed");
		}
	}
	return 0;
}

int main(int argc, const char* argv[])
{
	wrm_logi("hello.\n");
	wrm_logi("argc=0x%x, argv=0x%x.\n", argc, argv);

	for (int i=0; i<argc; i++)
		wrm_logi("arg[%d] = %s.\n", i, argv[i]);

	wrm_logi("my global_id=0x%x/%u.\n", l4_utcb()->global_id().raw(), l4_utcb()->global_id().number());


	// FIXME:  get app memory from Alpha
	static uint8_t memory [4*Cfg_page_sz] __attribute__((aligned(4*Cfg_page_sz)));
	wrm_mpool_add(L4_fpage_t::create((addr_t)memory, sizeof(memory), Acc_rw));
	// ~FIXME


	// map IO
	addr_t ioaddr = -1;
	size_t iosize = -1;
	int rc = wrm_dev_map_io("greth", &ioaddr, &iosize);
	if (rc)
	{
		wrm_loge("wrm_dev_map_io() failed, rc=%d.\n", rc);
		return -1;
	}
	wrm_logi("map_io:  addr=0x%x, sz=0x%x.\n", ioaddr, iosize);

	// get not cached memory for driver
	addr_t   mem_va = -1;
	paddr_t  mem_pa = -1;
	size_t   mem_sz = 0wrm000;
	unsigned access = -1;
	unsigned cached = -1;
	unsigned contig = -1;
	rc = wrm_memory_get("greth_mem", &mem_va, &mem_sz, &mem_pa, &access, &cached, &contig);
	if (rc)
	{
		wrm_loge("wrm_memory_get() failed, rc=%d.\n", rc);
		return -1;
	}
	wrm_logi("mregion:  %s:  va=0x%x, pa=0x%llx, sz=0x%x, acc=%u, cached=%u, contig=%u.\n",
		"greth_mem", mem_va, mem_pa, mem_sz, access, cached, contig);
	assert((access & Acc_rw) == Acc_rw);
	assert(!cached);
	assert(contig);

	// initialize greth device
	rc = initialize_greth_device(ioaddr, mem_va, mem_pa, mem_sz);
	if (rc)
	{
		wrm_loge("initialize_greth_device() failed, rc=%d.\n", rc);
		return -1;
	}

	// put rx msgs to greth, enable receiver
	for (unsigned i=0; i<driver.rx.queue_sz; ++i)
	{
		int rc = greth_receive(&driver.greth, msg_paddr(driver.rx.msgs[i]), Greth_buf_max);
		if (rc)
		{
			wrm_loge("could not RX enable, rx.queue_sz=%d, msg=%d, rc=%d.\n",
				driver.rx.queue_sz, i, rc);
			return -2;
		}
	}

	// create tx thread
	L4_fpage_t stack_fp = wrm_mpool_alloc(Cfg_page_sz);
	L4_fpage_t utcb_fp = wrm_mpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	L4_thrid_t txid = L4_thrid_t::Nil;
	rc = wrm_thread_create(utcb_fp.addr(), tx_thread, 0, stack_fp.addr(), stack_fp.size(), 255,
	                      "e-tx", Wrm_thr_flag_no, &txid);
	wrm_logi("create_thread:  rc=%d, id=0x%x/%u.\n", rc, txid.raw(), txid.number());
	assert(!rc && "failed to create tx thread");

	// create rx thread
	stack_fp = wrm_mpool_alloc(Cfg_page_sz);
	utcb_fp = wrm_mpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	L4_thrid_t rxid = L4_thrid_t::Nil;
	rc = wrm_thread_create(utcb_fp.addr(), rx_thread, 0, stack_fp.addr(), stack_fp.size(), 255,
	                      "e-rx", Wrm_thr_flag_no, &rxid);
	wrm_logi("create_thread:  rc=%d, id=0x%x/%u.\n", rc, rxid.raw(), rxid.number());
	assert(!rc && "failed to create rx thread");


	// attach to IRQ
	unsigned intno = -1;
	rc = wrm_dev_attach_int("greth", &intno);
	if (rc)
	{
		wrm_loge("wrm_dev_attach_int() failed, rc=%d.\n", rc);
		return -1;
	}
	wrm_logi("attach_int:  dev=%s, irq=%u.\n", "greth", intno);
	assert(intno == 14);


	// wait interrupt loop
	unsigned cnt = 0;
	while (1)
	{
		// wait interrupt
		//wrm_logi("wait interrupt ...\n");
		rc = wrm_dev_wait_int(intno);
		assert(!rc);
		if (!(cnt++ % 100))
			wrm_logi("interrupt received 100 times.\n");


		Greth_irq_status_t irq_status = Greth_irq_status_nil;
		rc = greth_irq_status(&driver.greth, &irq_status);
		assert(!rc);

		if (irq_status & Greth_irq_status_tx)
		{
			//wrm_logi("post tx sem.\n");
			rc = wrm_sem_post(&driver.tx.sem);
			assert(!rc  &&  "failed to post tx sem");
		}

		if (irq_status & Greth_irq_status_rx)
		{
			//wrm_logi("post rx sem.\n");
			rc = wrm_sem_post(&driver.rx.sem);
			assert(!rc  &&  "failed to post rx sem");
		}
	}

	return 0;
}
