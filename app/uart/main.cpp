//##################################################################################################
//
//  uart - userspace UART driver.
//
//##################################################################################################

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "sys_utils.h"
#include "l4_api.h"
#include "wrmos.h"
#include "cbuf.h"

#define UART_WITH_VIDEO
#include "uart.h"

// one-directional stream
struct Stream_t
{
	Cbuf_t    cbuf;               // circular buffer
	Wrm_sem_t sem;                // 'irq received' semaphore
};

// driver data
struct Driver_t
{
	Stream_t  tx;                 //
	Stream_t  rx;                 //
	Wrm_mtx_t write_to_uart_mtx;  //
	addr_t    ioaddr;             //
};
static Driver_t driver;

int wait_attach_msg(const char* thread_name, L4_thrid_t* client)
{
	// register thread by name
	word_t key0 = 0;
	word_t key1 = 0;
	int rc = wrm_nthread_register(thread_name, &key0, &key1);
	if (rc)
	{
		wrm_loge("attach:  wrm_nthread_register(%s) - rc=%u.\n", thread_name, rc);
		assert(false);
	}
	wrm_logi("attach:  thread '%s' is registered, key:  %lx/%lx.\n", thread_name, key0, key1);

	L4_utcb_t* utcb = l4_utcb();

	// wait attach msg loop
	L4_thrid_t from = L4_thrid_t::Nil;
	while (1)
	{
		wrm_logi("attach:  wait attach msg.\n");
		int rc = l4_receive(L4_thrid_t::Any, L4_time_t::Never, &from);
		if (rc)
		{
			wrm_loge("attach:  l4_receive() - rc=%u.\n", rc);
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
		rc = l4_send(from, L4_time_t::Zero);
		if (rc)
			wrm_loge("attach:  l4_send(rep) - rc=%d.\n", rc);

		if (!ecode && !rc)
			break;  // attached
	}

	*client = from;
	wrm_logi("attach:  attached to %u.\n", from.number());
	return 0;
}

// read from tx.cbuf and write to uart device
// this func may be called from hw-thread and tx-thread
void write_to_uart()
{
	//wrm_logd("--:  %s().\n", __func__);

	uart_tx_irq(driver.ioaddr, 0); // disable tx irq
	unsigned written = 0;

	// NOTE:  The best way is write to UART FIFO char-by-char through uart_putc()
	//        while fifo-is-not-full. But TopUART doesn't allow detect state
	//        fifo-is-not-full. Therefore, we use uart_put_buf() and write FIFO_SZ
	//        bytes. For most UARTs driver return FIFO_SZ=1 --> we will work
	//        as uart_putc() while fifo-is-not-full. For TopUART FIFO_SZ=64, every
	//        writing we put to UART FIFO_SZ bytes.

	#if 0 // char-by-char

	// write to uart char-by-char
	wrm_mtx_lock(&driver.write_to_uart_mtx);
	while (1)
	{
		static char txchar = 0;
		static int is_there_unsent = 0;

		// read charecter from tx-buffer if need
		if (!is_there_unsent)
		{
			unsigned read = driver.tx.cbuf.read(&txchar, 1);
			if (!read)
				break;
			is_there_unsent = 1;
		}

		// write charecter to uart
		int rc = uart_putc(driver.ioaddr, txchar);
		if (rc < 0)
		{
			wrm_loge("--:  uart_put_buf() - rc=%d.\n", rc);
			break;  // uart tx empty
		}
		if (!rc)
		{
			wrm_logd("--:  uart_tx_full, enable tx irq.\n");
			uart_tx_irq(driver.ioaddr, 1);
			break;
		}
		is_there_unsent = 0;
		written++;
	}
	wrm_mtx_unlock(&driver.write_to_uart_mtx);

	#else // put buf

	unsigned fifo_sz = 0;
	char buf[128];
	fifo_sz = uart_fifo_size(driver.ioaddr);
	assert(fifo_sz);
	if (fifo_sz > sizeof(buf))
	{
		wrm_logw("--:  fifo_sz/%u > bufsz/%zu, use %zu.\n", fifo_sz, sizeof(buf), sizeof(buf));
		fifo_sz = sizeof(buf);
	}

	// write to uart fifo-by-fifo
	wrm_mtx_lock(&driver.write_to_uart_mtx);
	while (1)
	{
		if (!(uart_status(driver.ioaddr) & Uart_status_tx_ready)  &&  !driver.tx.cbuf.empty())
		{
			//wrm_logd("--:  uart_tx_not_ready, data exist -> enable tx irq.\n");
			uart_tx_irq(driver.ioaddr, 1);
			break;
		}

		// read charecters from tx-buffer if need
		unsigned read = driver.tx.cbuf.read(buf, fifo_sz);
		//wrm_logd("--:  read from cbuf %d bytes.\n", read);

		if (!read)
			break; // no data anymore

		// write charecter to uart
		int rc = uart_put_buf(driver.ioaddr, buf, read);
		if (rc < 0)
		{
			wrm_loge("--:  uart_put_buf() - rc=%d.\n", rc);
			break;
		}
		written += rc;
		if (read != (unsigned) rc)
		{
			wrm_loge("--:  uart_put_buf() - wr=%u, lost %u bytes.\n", read, read - rc);
			break;
		}
	}
	wrm_mtx_unlock(&driver.write_to_uart_mtx);

	#endif

	//wrm_logd("--:  written=%u.\n", written);
}

size_t put_to_txbuf(const char* buf, unsigned sz)
{
	// put line by line and add '\r'
	unsigned written = 0;
	while (written < sz)
	{
		const char* pos = buf + written;
		const char* end = strchr(pos, '\n');
		unsigned l = end ? (end + 1 - pos) : (sz - written); // length for line or tail

		// write all line
		unsigned wr = driver.tx.cbuf.write(pos, l);
		written += wr;

		if (wr != l)
		{
			wrm_loge("tx:  buffer full, write=%u, written=%u, lost=%u.\n", l, wr, l - wr);
			break;  // buf full
		}

		if (end)
		{
			wr = driver.tx.cbuf.write("\r", 1);
			if (wr != 1)
			{
				wrm_loge("tx:  buffer full, write=1, written=%u.\n", wr);
				break;  // buf full
			}
		}
	}
	return written;
}

int tx_thread(int unused)
{
	wrm_logi("tx:  hello:  %s.\n", __func__);
	wrm_logi("tx:  myid=%u.\n", l4_utcb()->global_id().number());

	// wait for attach message
	L4_thrid_t client = L4_thrid_t::Nil;
	int rc = wait_attach_msg("uart-tx-stream", &client);
	if (rc)
	{
		wrm_loge("tx:  wait_attach_msg() - rc=%d.\n", rc);
		assert(false);
	}
	wrm_logi("tx:  attached to client=%u\n", client.number());

	// prepare for requests
	L4_utcb_t* utcb = l4_utcb();
	static char tx_buf[0x1000];
	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true); // allow strings
	L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)tx_buf, sizeof(tx_buf)-1);
	utcb->br[0] = acceptor.raw();
	utcb->br[1] = bitem.word0();
	utcb->br[2] = bitem.word1();

	// wait request loop
	while (1)
	{
		//wrm_logi("tx:  wait request.\n");
		L4_thrid_t from = L4_thrid_t::Nil;
		int rc = l4_receive(client, L4_time_t::Never, &from);
		if (rc)
		{
			wrm_loge("tx:  l4_receive() - rc=%u.\n", rc);
			assert(false);
		}

		L4_msgtag_t tag = utcb->msgtag();
		word_t mr[L4_utcb_t::Mr_words];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));
		from = tag.propagated() ? utcb->sender() : from;

		//wrm_logi("tx:  received:  from=%u, tag=0x%x, u=%u, t=%u, mr[1]=0x%x, mr[2]=0x%x.\n",
		//	from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1], mr[2]);

		L4_typed_item_t  item  = L4_typed_item_t::create(mr[1], mr[2]);
		L4_string_item_t sitem = L4_string_item_t::create(mr[1], mr[2]);
		assert(tag.untyped() == 0);
		assert(tag.typed() == 2);
		assert(item.is_string_item());

		//wrm_logi("tx:  request is got:  sz=%u.\n", sitem.length());
		//wrm_logi("tx:  request is got:  buf=0x%x, sz=%u, txbf=0x%p:  %.*s.\n",
		//	sitem.pointer(), sitem.length(), tx_buf, sitem.length(), (char*)sitem.pointer());

		((char*)sitem.pointer())[sitem.length()] = '\0'; // add terminator
		unsigned written = put_to_txbuf((char*)sitem.pointer(), sitem.length());

		if (written != sitem.length())
		{
			wrm_loge("tx:  buffer full, write=%u, written=%u, lost=%u.\n",
				sitem.length(), written, sitem.length() - written);
		}

		//wrm_logd("tx:  written=%u to cbuf.\n", written);

		if (written)
		{
			write_to_uart();
		}

		// send reply to client
		tag.propagated(false);
		tag.ipc_label(0);
		tag.untyped(2);
		tag.typed(0);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = rc;           // ecode
		utcb->mr[2] = written;      // bytes written
		rc = l4_send(from, L4_time_t::Zero);
		if (rc)
		{
			wrm_loge("l4_send(rep) - rc=%u.\n", rc);
			l4_kdb("tx:  l4_send(cli) is failed");
		}
	}
	return 0;
}

int rx_thread(int unused)
{
	wrm_logi("rx:  hello:  %s.\n", __func__);
	wrm_logi("rx:  myid=%u.\n", l4_utcb()->global_id().number());

	// wait for attach message
	L4_thrid_t client = L4_thrid_t::Nil;
	int rc = wait_attach_msg("uart-rx-stream", &client);
	if (rc)
	{
		wrm_loge("rx:  wait_attach_msg() - rc=%u.\n", rc);
		assert(false);
	}
	wrm_logi("rx:  attached to client=%u\n", client.number());

	// prepare for requests
	L4_utcb_t* utcb = l4_utcb();
	static char rx_buf[0x1000];
	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), false);
	utcb->br[0] = acceptor.raw();

	// wait request loop
	while (1)
	{
		//wrm_logi("rx:  wait request.\n");
		L4_thrid_t from = L4_thrid_t::Nil;
		int rc = l4_receive(client, L4_time_t::Never, &from);
		L4_msgtag_t tag = utcb->msgtag();
		if (rc)
		{
			wrm_loge("rx:  l4_receive() - rc=%u.\n", rc);
			assert(false);
		}

		word_t mr[256];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));
		from = tag.propagated() ? utcb->sender() : from;

		//wrm_logi("rx:  received ipc:  from=%u, tag=0x%lx, u=%u, t=%u, mr[1]=0x%lx, mr[2]=0x%lx.\n",
		//	from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1], mr[2]);

		assert(tag.untyped() == 1);
		assert(tag.typed() == 0);

		size_t cli_bfsz = mr[1];
		size_t read = 0;

		while (1)
		{
			read = driver.rx.cbuf.read(rx_buf, min(sizeof(rx_buf), cli_bfsz));
			if (read)
				break;
			rc = wrm_sem_wait(&driver.rx.sem);  // wait rx operation
			if (rc)
			{
				wrm_loge("wrm_sem_wait(rx) - rc=%d.\n", rc);
				rc = -1;
				break;
			}
		}

		//wrm_logi("rx:  read=%u.\n", read);

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
		rc = l4_send(from, L4_time_t::Zero);
		if (rc)
		{
			wrm_loge("l4_send(rep) - rc=%u.\n", rc);
			l4_kdb("rx:  l4_send(cli) is failed");
		}
	}
	return 0;
}

void read_from_uart()
{
	char buf[64];
	unsigned read = 0;
	while (1)
	{
		// read data from uart
		unsigned rd = 0;
		char ch = 0;
		while (rd < sizeof(buf)  &&  (ch = uart_getc(driver.ioaddr)))
			buf[rd++] = ch;

		//wrm_logd("hw:  read from uart %d bytes.\n", rc);
		//wrm_logd("hw:  rx(%u):  '%.*s' (rx=%u, tx=%u all=%u)\n",
		//	rc, rc, buf, cnt_rx, cnt_tx, cnt_all);

		read += rd;
		if (!rd)
			break;

		// write data to rx-buffer
		unsigned written = driver.rx.cbuf.write(buf, rd);
		if (written != rd)
		{
			wrm_loge("hw:  rx_buf full, lost %u bytes.\n", rd - written);
			break;
		}

		//wrm_logi("post rx sem.\n");
		int rc = wrm_sem_post(&driver.rx.sem);
		if (rc)
			wrm_loge("hw:  wrm_sem_post(rx.sem) - rc=%d.\n", rc);
	}

	if (!read)
		wrm_loge("hw:  no data for read in uart.\n");
}

void irq_thread(const char* uart_dev_name)
{
	// rename main thread
	memcpy(&l4_utcb()->tls[0], "u-hw", 4);

	// attach to IRQ
	unsigned intno = -1;
	int rc = wrm_dev_attach_int(uart_dev_name, &intno);
	wrm_logi("attach_int:  dev=%s, irq=%u.\n", uart_dev_name, intno);
	if (rc)
	{
		wrm_loge("wrm_dev_attach_int() - rc=%d.\n", rc);
		return;
	}

	uart_rx_irq(driver.ioaddr, 1);

	unsigned icnt_rx = 0;
	unsigned icnt_tx = 0;
	unsigned icnt_all = 0;
	unsigned icnt_nil = 0;

	// wait interrupt loop
	while (1)
	{
		// wait interrupt
		//wrm_logd("hw:  wait interrupt ... (irq: rx/tx/all/nil=%u/%u/%u/%u).\n",
		//	icnt_rx, icnt_tx, icnt_all, icnt_nil);

		rc = wrm_dev_wait_int(intno, Uart_need_ack_irq_before_reenable);
		assert(!rc);
		icnt_all++;

		unsigned loop_cnt = 0;
		while (1)
		{
			int status = uart_status(driver.ioaddr);

			int tx_ready = !!(status & Uart_status_tx_ready);
			int rx_ready = !!(status & Uart_status_rx_ready);
			int tx_irq   = !!(status & Uart_status_tx_irq);
			int rx_irq   = !!(status & Uart_status_rx_irq);

			//wrm_logd("hw:  status:  loop=%u:  ready_rx/tx=%d/%d, irq_rx/tx=%d/%d.\n",
			//	loop_cnt, rx_ready, tx_ready, rx_irq, tx_irq);

			if (!rx_irq && !tx_irq)
			{
				// we got real irq but UART hasn't interrupt inside int_status register
				//wrm_loge("hw:  no irq:  loop=%u:  ready_rx/tx=%d/%d, irq_rx/tx=%d/%d.\n",
				//	loop_cnt, rx_ready, tx_ready, rx_irq, tx_irq);
				icnt_nil++;
				break;
			}

			if (rx_irq)
				icnt_rx++;

			if (tx_irq)
				icnt_tx++;

			if (rx_ready)
				read_from_uart();

			if (tx_ready)
				write_to_uart();

			loop_cnt++;
			int need_check_status = uart_clear_irq(driver.ioaddr);
			if (!need_check_status)
				break;
		}
	}
}

int main(int argc, const char* argv[])
{
	wrm_logi("hello.\n");
	wrm_logi("argc=%d, argv=0x%p.\n", argc, argv);

	for (int i=0; i<argc; i++)
		wrm_logi("arg[%d] = %s.\n", i, argv[i]);

	wrm_logi("myid=%u.\n", l4_utcb()->global_id().number());

	// map IO
	const char* uart_dev_name = argc>=2 ? argv[1] : "";
	addr_t ioaddr = -1;
	size_t iosize = -1;
	int rc = wrm_dev_map_io(uart_dev_name, &ioaddr, &iosize);
	if (rc)
	{
		wrm_loge("wrm_dev_map_io() - rc=%d.\n", rc);
		return -1;
	}
	wrm_logi("map_io:  addr=0x%lx, sz=0x%zx.\n", ioaddr, iosize);
	driver.ioaddr = ioaddr;

	static char rx_buf[0x1000];
	static char tx_buf[0x2000];
	driver.rx.cbuf.init(rx_buf, sizeof(rx_buf));
	driver.tx.cbuf.init(tx_buf, sizeof(tx_buf));

	rc = wrm_sem_init(&driver.rx.sem, Wrm_sem_binary, 0);
	if (rc)
	{
		wrm_loge("wrm_sem_init(rx.sem) - rc=%d.", rc);
		return -2;
	}

	rc = wrm_mtx_init(&driver.write_to_uart_mtx);
	if (rc)
	{
		wrm_loge("wrm_mtx_init(write_to_uart_mtx) - rc=%d.", rc);
		return -3;
	}

	// I do not known sys_freq.
	// Do not init, I hope uart was initialized by kernel.
	// uart_init(ioaddr, 115200, 40*1000*1000/*?*/);

	// create tx thread
	L4_fpage_t stack_fp = wrm_mpool_alloc(Cfg_page_sz);
	L4_fpage_t utcb_fp = wrm_mpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	L4_thrid_t txid = L4_thrid_t::Nil;
	rc = wrm_thread_create(utcb_fp, tx_thread, 0, stack_fp.addr(), stack_fp.size(), 255,
	                      "u-tx", Wrm_thr_flag_no, &txid);
	wrm_logi("create_thread:  rc=%d, id=%u.\n", rc, txid.number());
	if (rc)
	{
		wrm_loge("wrm_thread_create(tx) - rc=%d.", rc);
		return -4;
	}

	// create rx thread
	stack_fp = wrm_mpool_alloc(Cfg_page_sz);
	utcb_fp = wrm_mpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	L4_thrid_t rxid = L4_thrid_t::Nil;
	rc = wrm_thread_create(utcb_fp, rx_thread, 0, stack_fp.addr(), stack_fp.size(), 255,
	                      "u-rx", Wrm_thr_flag_no, &rxid);
	wrm_logi("create_thread:  rc=%d, id=%u.\n", rc, rxid.number());
	if (rc)
	{
		wrm_loge("wrm_thread_create(rx) - rc=%d.\n", rc);
		return -5;
	}

	irq_thread(uart_dev_name);
	wrm_loge("return from app uart - something going wrong.\n");

	return 0;
}
