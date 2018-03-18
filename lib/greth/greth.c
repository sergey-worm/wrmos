//##################################################################################################
//
//  Low-level GRETH driver.
//
//##################################################################################################

#include "greth.h"
#include <stdint.h>

#ifdef DEBUG
#  define print(...)  if (dev->dprint) dev->dprint(__VA_ARGS__)
#else
#  define print(...) { (void)dev; }
#endif

#define dcache_inval(a,s)  if (dev->dcache_inval) dev->dcache_inval((unsigned)(a),(s))
#define dcache_flush(a,s)  if (dev->dcache_flush) dev->dcache_flush((unsigned)(a),(s))

typedef struct Greth_regs
{
	uint32_t ctrl;        // 0x00 Control register
	uint32_t status;      // 0x04 Status/Interrupt-source register
	uint32_t macmsb;      // 0x08 MAC Address MSB
	uint32_t maclsb;      // 0x0c MAC Address LSB
	uint32_t mdio;        // 0x10 MDIO Control/Status
	uint32_t txdptr;      // 0x14 Transmit descriptor pointer
	uint32_t rxdptr;      // 0x18 Receiver descriptor pointer
	uint32_t edclip;      // 0x1c EDCL IP
	uint32_t htmsb;       // 0x20 Hash table msb
	uint32_t htlsb;       // 0x24 Hash table lsb
	uint32_t edclmacmsb;  // 0x28 EDCL MAC address MSB
	uint32_t edclmaclsb;  // 0x2c EDCL MAC address LSB
	// 0x10000 - 0x107FC Transmit RAM buffer debug access
	// 0x20000 - 0x207FC Receiver RAM buffer debug access
	// 0x30000 - 0x3FFFC EDCL buffer debug access
} Greth_regs_t;

// registers bits
enum
{
	Ctrl_ea            = 1 << 31,  // EDCL available
	Ctrl_ebs_shift     = 28,       // EDCL buffer size 0->1kB, 1->2kB, ... , 6->64kB
	Ctrl_ebs_mask      = 0x7,      //
	Ctrl_ma            = 1 << 26,  // MDIO interrupts available, RO
	Ctrl_mc            = 1 << 25,  // multicast available, RO
	Ctrl_ed            = 1 << 14,  // EDCL disable, 1->off, 0->on
	Ctrl_rd            = 1 << 13,  // RAM debug enable
	Ctrl_eddd          = 1 << 12,  // disable the EDCL speed/duplex detection FSM
	Ctrl_me            = 1 << 11,  // multicast enable
	Ctrl_phyie         = 1 << 10,  // PHY status change interrupt enable
	Ctrl_sp            = 1 <<  7,  // speed, 0->10 Mbit, 1->100 Mbit, used for rmii=1
	Ctrl_rs            = 1 <<  6,  // reset
	Ctrl_pm            = 1 <<  5,  // promiscuous mode, receive all packets with any dest addr
	Ctrl_fd            = 1 <<  4,  // 1 - full duplex, 0 - halfduplex
	Ctrl_rxie          = 1 <<  3,  // RX interrupt enable
	Ctrl_txie          = 1 <<  2,  // TX interrupt enable
	Ctrl_rxe           = 1 <<  1,  // RX enable, set after set new rx-descriptor
	Ctrl_txe           = 1 <<  0,  // TX enable, set after set new tx-descriptor

	Status_phy         = 1 <<  8,  // PHY status changes
	Status_inva        = 1 <<  7,  // invalid address, to cleare write 1
	Status_sml         = 1 <<  6,  // packet too small, to cleare write 1
	Status_txahberr    = 1 <<  5,  // AHB error in TX DMA engine, to cleare write 1
	Status_rxahberr    = 1 <<  4,  // AHB error in RX DMA engine, to cleare write 1
	Status_txok        = 1 <<  3,  // packet TX without error, to cleare write 1
	Status_rxok        = 1 <<  2,  // packet RX without error, to cleare write 1
	Status_txerr       = 1 <<  1,  // packet TX with error, to cleare write 1
	Status_rxerr       = 1 <<  0,  // packet RX with error, to cleare write 1

	Mdio_data_shift    = 16,       // data read during a read operation, and data that is transmitted is taken from
	Mdio_data_mask     = 0xffff,   //
	Mdio_phyaddr_shift = 11,       // PHY address
	Mdio_phyaddr_mask  = 0x1f,     //
	Mdio_regaddr_shift = 6,        // register address
	Mdio_regaddr_mask  = 0x1f,     //
	Mdio_nvalid        = 1 <<  4,  // not valid, after operation (busy=0) if 0 -> rx correct data
	Mdio_busy          = 1 <<  3,  // busy, 1 -> operation in progress, 0 -> operation is finished
	Mdio_linkfail      = 1 <<  2,  // link fail, after operation (busy=0) if 1 -> mngment link fail
	Mdio_rd            = 1 <<  1,  // read op, start read operation on the mngmnt iface, data is stored in the data field
	Mmio_wr            = 0 <<  0,  // write op, start a write operation on the mngmnt iface, data is taken from the Data field

	Txdptr_addr_shift  = 10,       // base addr of TX descriptor table
	Txdptr_addr_mask   = 0x3fffff, //
	Txdptr_ptr_shift   = 3,        // pointer to individual descriptor, auto incremented by MAC
	Txdptr_ptr_mask    = 0x7f,     //

	Rxdptr_addr_shift  = 10,       // base addr of RX descriptor table
	Rxdptr_addr_mask   = 0x3fffff, //
	Rxdptr_ptr_shift   = 3,        // pointer to individual descriptor, auto incremented by MAC
	Rxdptr_ptr_mask    = 0x7f,     //
};

typedef struct __attribute__ ((__packed__))
{
	unsigned reserved_1 : 16; // not used
	unsigned al         :  1; // status bit - attempt limit error, to mach collisions
	unsigned ue         :  1; // status bit - underrun error, tx incorrect, FIFO underrun
	unsigned ie         :  1; // control bit - interrupt enable, generate IRQ after sending
	unsigned wr         :  1; // control bit - wrap, set descr_ptr to 0 after send, else +=8
	unsigned en         :  1; // control bit - enable, set after all descriptor fields
	unsigned len        : 11; // length of descriptor buffer
	unsigned addr       : 32; // pointer to start of descriptor buffer, 4 byte alignment
} Tx_desc_t;

typedef struct __attribute__ ((__packed__))
{
	unsigned reserved_1 :  5; // not used
	unsigned mc         :  1; // status bit - multicast, received mcast packet
	unsigned reserved_2 :  7; // not used
	unsigned le         :  1; // status bit - length error, wrong length/type field, pkt accepted
	unsigned oe         :  1; // status bit - overrun error, rx incorrect, FIFO overrun
	unsigned ce         :  1; // status bit - CRC error
	unsigned ft         :  1; // status bit - frame too long
	unsigned ae         :  1; // status bit - alignment error
	unsigned ie         :  1; // control bit - interrupt enable, generate IRQ after receiving
	unsigned wr         :  1; // control bit - wrap, set descr_ptr to 0 after rx, else +=8
	unsigned en         :  1; // control bit - enable, set after all descriptor fields
	unsigned len        : 11; // length of received buffer
	unsigned addr       : 32; // pointer to start of descriptor buffer, 4 byte alignment
} Rx_desc_t;

// initialize but not enable
Greth_err_t greth_init(Greth_dev_t* dev)
{
	print("init greth:\n");
	print("    regs:   0x%x\n", dev->regs);
	print("    mac:    0x%x\n", dev->mac);
	print("    txdtb:  0x%x / 0x%x, sz=%d\n", dev->tx_dtb, dev->tx_dtb_phys, dev->tx_dtb_sz);
	print("    rxdtb:  0x%x / 0x%x, sz=%d\n", dev->rx_dtb, dev->rx_dtb_phys, dev->rx_dtb_sz);
	print("\n");

	// check table pointers for NULL
	if (!dev->tx_dtb || !dev->tx_dtb_phys ||
	    !dev->rx_dtb || !dev->rx_dtb_phys)
		return Greth_err_no_buf;

	// check alignment of rx/tx descriptor tables to 1k byte boundary
	if ((unsigned)dev->tx_dtb & (1024-1) || (unsigned)dev->tx_dtb_phys & (1024-1) ||
	    (unsigned)dev->rx_dtb & (1024-1) || (unsigned)dev->rx_dtb_phys & (1024-1))
		return Greth_err_not_aligned;

	// check table size
	if (!dev->tx_dtb_sz || dev->tx_dtb_sz > 128 ||
	    !dev->rx_dtb_sz || dev->rx_dtb_sz > 128)
		return Greth_err_wrong_size;

	volatile Greth_regs_t* regs = (void*) dev->regs;  // volatile!

	regs->ctrl = Ctrl_rs; // reset

	regs->macmsb = dev->mac >> 32;
	regs->maclsb = dev->mac;

	regs->txdptr = (unsigned) dev->tx_dtb_phys;
	regs->rxdptr = (unsigned) dev->rx_dtb_phys;

	// clean up descriptor tables
	for (int i=0; i<dev->tx_dtb_sz; ++i)
		dev->tx_dtb[i] = 0ll;

	for (int i=0; i<dev->rx_dtb_sz; ++i)
		dev->rx_dtb[i] = 0ll;

	// auxiliary tx fields
	dev->tx_dtb_start = dev->tx_dtb;
	dev->tx_dtb_end   = dev->tx_dtb + dev->tx_dtb_sz;
	dev->tx_dptr_wr   = dev->tx_dtb;
	dev->tx_dptr_rd   = dev->tx_dtb;

	// auxiliary rx fields
	dev->rx_dtb_start = dev->rx_dtb;
	dev->rx_dtb_end   = dev->rx_dtb + dev->rx_dtb_sz;
	dev->rx_dptr_wr   = dev->rx_dtb;
	dev->rx_dptr_rd   = dev->rx_dtb;

	// clear all status bits
	regs->status |= Status_inva | Status_sml | Status_txahberr | Status_rxahberr |
	                Status_txok | Status_rxok | Status_txerr | Status_rxerr;

	regs->ctrl = Ctrl_sp | Ctrl_fd | Ctrl_rxie | Ctrl_txie;  // 100Mb, full duplex, rx/tx int enable

	// TODO:  set up MDIO

	return Greth_ok;
}

/* examples
inline static void cache_inval(volatile void* ptr)
{
	// descriptor memory is cached, do cache miss (for leon3 ASI=1)
	asm volatile ("lda [%[addr]] 0x1, %%g0" : : [addr] "r" (ptr));
}

// XXX: Does it work? Check it!
inline static void cache_flush(volatile void* ptr)
{
	// descriptor memory is cached, do cache flush (for SPARCv8 ASI=0x10)
	asm volatile ("lda [%[addr]] 0x10, %%g0" : : [addr] "r" (ptr));
}
*/

inline static void dump_tx_desc(const Greth_dev_t* dev, const char* lable, volatile Tx_desc_t* d)
{
	#ifdef DEBUG
	print("%s  desc=0x%x, al=%d, ue=%d, ie=%d, wr=%d, en=%d, len=%d, addr=0x%x\n",
		lable, d, d->al, d->ue, d->ie, d->wr, d->en, d->len, d->addr);
	#else
	(void)dev;
	(void)lable;
	(void)d;
	#endif
}

inline static void dump_rx_desc(const Greth_dev_t* dev, const char* lable, volatile Rx_desc_t* d)
{
	#ifdef DEBUG
	print("%s  desc=0x%x, mc=%d, le=%d, oe=%d, ce=%d, ft=%d, ae=%d, ie=%d, wr=%d, en=%d, len=%d, addr=0x%x\n",
		lable, d, d->mc, d->le, d->oe, d->ce, d->ft, d->ae, d->ie, d->wr, d->en, d->len, d->addr);
	#else
	(void)dev;
	(void)lable;
	(void)d;
	#endif
}

void greth_dump(const Greth_dev_t* dev)
{
	#ifdef DEBUG
	print("regs:  0x%x\n", dev->regs);

	volatile Greth_regs_t* regs = (void*) dev->regs;  // volatile!

	print("    ctrl:       0x%x\n", regs->ctrl);
	print("    status:     0x%x\n", regs->status);
	print("    macmsb:     0x%x\n", regs->macmsb);
	print("    maclsb:     0x%x\n", regs->maclsb);
	print("    mdio:       0x%x\n", regs->mdio);
	print("    txdptr:     0x%x\n", regs->txdptr);
	print("    rxdptr:     0x%x\n", regs->rxdptr);
	print("    edclip:     0x%x\n", regs->edclip);
	print("    htmsb:      0x%x\n", regs->htmsb);
	print("    htlsb:      0x%x\n", regs->htlsb);
	print("    edclmacmsb: 0x%x\n", regs->edclmacmsb);
	print("    edclmaclsb: 0x%x\n", regs->edclmaclsb);

	print("tx_dtb:  ##  al ue ie wr en len addr\n");
	for (unsigned i=0; i<dev->tx_dtb_sz; ++i)
	{
		volatile Tx_desc_t* txd = (void*)&dev->tx_dtb_start[i];
		dcache_inval(txd, sizeof(*txd));  // desc is cached, invalidate
		print("          %u   %u  %u  %u  %u  %u   %u 0x%x\n",
			i, txd->al, txd->ue, txd->ie, txd->wr, txd->en, txd->len, txd->addr);
	}

	print("rx_dtb:  ##  mc le oe ce ft ae ie wr en len addr\n");
	for (unsigned i=0; i<dev->rx_dtb_sz; ++i)
	{
		volatile Rx_desc_t* rxd = (void*)&dev->rx_dtb_start[i];
		dcache_inval(rxd, sizeof(*rxd)); // desc is cached, invalidate
		print("          %u   %u  %u  %u  %u  %u  %u  %u  %u  %u   %u 0x%x\n",
			i, rxd->mc, rxd->le, rxd->oe, rxd->ce, rxd->ft, rxd->ae, rxd->ie, rxd->wr, rxd->en, rxd->len, rxd->addr);
	}
	print("\n");
	#else
	(void)dev;
	#endif
}

//##################################################################################################
//
//  TRANSMITING
//
//  Each descriptor have 'addr' field. This field don't change by HW. Use this field to
//  determinate descriptor state:  addr=0 --> IDLE.
//
//  Descriptor table uses by this driver as FIFO. Where are 2 pointers - RD and WR.
//
//  TX descriptor may be in one of het next states:
//   - IDLE - "!rxd->en && !rxd->addr", not enabled, no addr;
//   - TX   - "rxd->en",                enabled for transmitting;
//   - WAIT - "!rxd->en && rxd->addr",  not enabled, addr exist.
//
//  There are 3 functions for RX menegement:
//   - greth_transmit (dev, buf, sz)            - IDLE -> TX,   WR++, start transmitting
//   - greth_txstatus (dev, &buf, &sz, &status) - WAIT -> WAIT,       transmitted, wait for free
//   - greth_txfree (dev)                       - WAIT -> IDLE, RD++, free waited descr
//
//##################################################################################################

// set up next IDLE descriptor and enable transmiter
// incr WR ptr
Greth_err_t greth_transmit(Greth_dev_t* dev, const void* buf_phys, unsigned size)
{
	if (!buf_phys)
		return Greth_err_no_buf;

	if ((unsigned)buf_phys & 3)
		return Greth_err_not_aligned; // should be aligned to 4 byte

	// NOTE:  if len < Greth_buf_min -- HW adds (60 - len) null bytes
	//if (size < Greth_buf_min)
	//	return Greth_err_too_small;

	if (size > Greth_buf_max)
		return Greth_err_too_big;

	if (!dev)
		return Greth_err_wrong_param;

	Tx_desc_t* txd = (void*) dev->tx_dptr_wr;
	dcache_inval(txd, sizeof(*txd));  // desc is cached, invalidate

	dump_tx_desc(dev, "tx:  ", txd);

	if (txd->en || txd->addr)         // is not IDLE ?
		return Greth_err_busy;

	*(unsigned*)(void*)txd = 0;       // clean up word 0
	txd->addr = (unsigned) buf_phys;
	txd->len = size;
	txd->ie = 1;

	if (txd == (void*)(dev->tx_dtb_end - 1))
		txd->wr = 1;                  // wrap, set descr ptr to 0 after

	txd->en = 1;

	dcache_flush(txd, sizeof(*txd));  // desc is cached, flush

	// set new wr ptr
	dev->tx_dptr_wr = (void*)(txd+1) >= (void*)dev->tx_dtb_end ? dev->tx_dtb_start : (void*)(txd+1);

	dump_tx_desc(dev, "tx:  ", txd);

	// tx enable
	volatile Greth_regs_t* regs = (void*) dev->regs;  // volatile!
	regs->ctrl |= Ctrl_txe;

	return Greth_ok;
}

// get RD descriptor status
Greth_err_t greth_txstatus(Greth_dev_t* dev, void** buf_phys, Greth_tx_status_t* status)
{
	if (!dev || !buf_phys || !status)
		return Greth_err_wrong_param;

	volatile Tx_desc_t* txd = (void*) dev->tx_dptr_rd;
	dcache_inval(txd, sizeof(*txd)); // desc is cached, invalidate

	dump_tx_desc(dev, "txst:", txd);

	*buf_phys = (void*) txd->addr;

	if (txd->en)
	{
		*status = Greth_tx_status_txing;
	}
	else
	{
		if (txd->addr)
		{
			if (txd->ue || txd->al)
				*status = Greth_tx_status_err;
			else
				*status = Greth_tx_status_ok;
		}
		else
		{
			*status = Greth_tx_status_idle;
		}
	}
	return Greth_ok;
}

// mark RD descriptor as free
// incr RD ptr
Greth_err_t greth_txfree(Greth_dev_t* dev)
{
	if (!dev)
		return Greth_err_wrong_param;

	volatile Tx_desc_t* txd = (void*) dev->tx_dptr_rd;
	dcache_inval(txd, sizeof(*txd));  // desc is cached, invalidate

	dump_tx_desc(dev, "txfr:", txd);

	if (txd->en || !txd->addr)        // is not WAIT ?
		return Greth_err_failed;

	txd->addr = 0;
	dcache_flush(txd, sizeof(*txd));  // desc is cached, flush

	dump_tx_desc(dev, "txfr:", txd);

	dev->tx_dptr_rd = (void*)(txd+1) >= (void*)dev->tx_dtb_end ? dev->tx_dtb_start : (void*)(txd+1);
	return Greth_ok;
}

//##################################################################################################
//
//  RECEIVING
//
//  Each descriptor have 'len' field. This field is set by HW. Use this field to
//  determinate descriptor state:  len=0 --> IDLE.
//
//  Descriptor table uses by this driver as FIFO. Where are 2 pointers - RD and WR.
//
//  RX descriptor may be in one of het next states:
//   - IDLE - "!rxd->en && !rxd->len", not enabled and doesn't have data;
//   - RX   - "rxd->en",               enabled for receiving;
//   - WAIT - "!rxd->en && rxd->len",  not enabled and have data for reading.
//
//  There are 3 functions for RX menegement:
//   - greth_receive (dev, buf, sz)           - IDLE -> RX,   WR++, start receiving;
//   - greth_rxread (dev, &buf, &sz, &status) - WAIT -> WAIT,       received, wait for reading;
//   - greth_rxfree (dev)                     - WAIT -> IDLE, RD++, free waited descr.
//
//##################################################################################################

// set up next IDLE descriptor and enable receiver
// incr WR ptr
Greth_err_t greth_receive(Greth_dev_t* dev, void* buf_phys, unsigned size)
{
	if (!buf_phys)
		return Greth_err_no_buf;

	if ((unsigned)buf_phys & 3)
		return Greth_err_not_aligned; // should be aligned to 4 byte

	if (size < Greth_buf_max)
		return Greth_err_too_small;

	if (!dev)
		return Greth_err_failed;

	Rx_desc_t* rxd = (void*) dev->rx_dptr_wr;
	dcache_inval(rxd, sizeof(*rxd));  // desc is cached, invalidate

	dump_rx_desc(dev, "rx:  ", rxd);

	if (rxd->en || rxd->len)          // is not IDLE ?
		return Greth_err_busy;

	*(unsigned*)(void*)rxd = 0;       // clean up word 0
	rxd->addr = (unsigned) buf_phys;
	rxd->ie = 1;

	if (rxd == (void*)(dev->rx_dtb_end - 1))
		rxd->wr = 1;                  // wrap, set descr ptr to 0 after

	rxd->en = 1;

	dcache_flush(rxd, sizeof(*rxd));  // desc is cached, flush

	// set new wr ptr
	dev->rx_dptr_wr = (void*)(rxd+1) >= (void*)dev->rx_dtb_end ? dev->rx_dtb_start : (void*)(rxd+1);

	dump_rx_desc(dev, "rx:  ", rxd);

	// rx enable
	volatile Greth_regs_t* regs = (void*) dev->regs;  // volatile!
	regs->ctrl |= Ctrl_rxe;

	return Greth_ok;
}

// get RD descriptor status
Greth_err_t greth_rxstatus(Greth_dev_t* dev, void** buf_phys, unsigned* size, Greth_rx_status_t* status)
{
	if (!dev || !buf_phys || !size || !status)
		return Greth_err_wrong_param;

	volatile Rx_desc_t* rxd = (void*) dev->rx_dptr_rd;
	dcache_inval(rxd, sizeof(rxd));  // desc is cached, invalidate

	dump_rx_desc(dev, "rxst:", rxd);

	*buf_phys = (void*) rxd->addr;
	*size = rxd->len;

	if (rxd->en)
	{
		*status = Greth_rx_status_rxing;
	}
	else
	{
		if (rxd->len)
		{
			if (rxd->oe || rxd->ce || rxd->ft || rxd->ae)
				*status = Greth_rx_status_err;
			else
				*status = Greth_rx_status_ok;
		}
		else
		{
			*status = Greth_rx_status_idle;
		}
	}

	return Greth_ok;
}

// mark descriptor as free
// incr RD ptr
Greth_err_t greth_rxfree(Greth_dev_t* dev)
{
	if (!dev)
		return Greth_err_wrong_param;

	volatile Rx_desc_t* rxd = (void*) dev->rx_dptr_rd;
	dcache_inval(rxd, sizeof(*rxd));  // desc is cached, invalidate

	dump_rx_desc(dev, "rxfr:", rxd);

	if (rxd->en || !rxd->len)         // is not WAIT ?
		return Greth_err_failed;

	rxd->len = 0;
	rxd->addr = 0;

	dcache_flush(rxd, sizeof(*rxd));  // desc is cached, flush

	dump_rx_desc(dev, "rxfr:", rxd);

	dev->rx_dptr_rd = (void*)(rxd+1) >= (void*)dev->rx_dtb_end ? dev->rx_dtb_start : (void*)(rxd+1);
	return Greth_ok;
}

Greth_err_t greth_irq_status(Greth_dev_t* dev, Greth_irq_status_t* status)
{
	if (!dev)
		return Greth_err_wrong_param;

	volatile Greth_regs_t* regs = (void*) dev->regs;  // volatile!
	uint32_t status_reg = regs->status;

	// clear all status bits
	regs->status |= Status_inva | Status_sml | Status_txahberr | Status_rxahberr |
	                Status_txok | Status_rxok | Status_txerr | Status_rxerr;

	print("irq:   ctrl=0x%x, status=0x%x: %s%s%s%s%s%s%s%s%s.\n", regs->ctrl, status_reg,
		(status_reg & Status_phy)      ? " phy"      : "",
		(status_reg & Status_inva)     ? " inva"     : "",
		(status_reg & Status_sml)      ? " sml"      : "",
		(status_reg & Status_txahberr) ? " txahberr" : "",
		(status_reg & Status_rxahberr) ? " rxahberr" : "",
		(status_reg & Status_txok)     ? " txok"     : "",
		(status_reg & Status_rxok)     ? " rxok"     : "",
		(status_reg & Status_txerr)    ? " txerr"    : "",
		(status_reg & Status_rxerr)    ? " rxerr"    : "");

	*status = 0;

	if (status_reg & Status_txok  ||  status_reg & Status_txerr)
		*status |= Greth_irq_status_tx;

	if (status_reg & Status_rxok  ||  status_reg & Status_rxerr)
		*status |= Greth_irq_status_rx;

	return Greth_ok;
}
