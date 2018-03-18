//##################################################################################################
//
//  PL011 low-level driver.
//
//##################################################################################################

#ifndef UART_H
#define UART_H

#include <stdint.h>

enum
{
	Uart_status_tx_empty = 1 << 1,
	Uart_status_rx_full  = 1 << 2,

	// interrupt bits
	Irq_overrun_err = 1 << 10,  // overrun error
	Irq_break_err   = 1 <<  9,  // break error
	Irq_parity_err  = 1 <<  8,  // parity error
	Irq_framing_err = 1 <<  7,  // framing error
	Irq_rx_timeout  = 1 <<  6,  // receive timeout
	Irq_tx          = 1 <<  5,  // transmit
	Irq_rx          = 1 <<  4,  // receive
	Irq_modem_dsr   = 1 <<  3,  // modem DSR
	Irq_modem_dcd   = 1 <<  2,  // modem DCD
	Irq_modem_cts   = 1 <<  1,  // modem CTS
	Irq_modem_ri    = 1 <<  0,  // modem RI

	// flags bit
	Flag_ri         = 1 <<  8,  // ring indicator
	Flag_tx_empty   = 1 <<  7,  // transmit FIFO empty
	Flag_rx_full    = 1 <<  6,  // receive  FIFO full
	Flag_tx_full    = 1 <<  5,  // transmit FIFO full
	Flag_rx_empty   = 1 <<  4,  // receive  FIFO empty
	Flag_busy       = 1 <<  3,  // UART busy
	Flag_dcd        = 1 <<  2,  // data carrier detect
	Flag_dsr        = 1 <<  1,  // data set ready
	Flag_cts        = 1 <<  0,  // clear to send
};


#define SPACE_SIZE(addr1, addr2) (((addr2) - (addr1)) / sizeof(uint32_t))

typedef struct
{
	uint32_t dr;                               // 0x00  data register
	uint32_t rsrecr;                           // 0x04  rx status register / error clear register
	uint32_t space1 [SPACE_SIZE(0x08, 0x18)];  //
	uint32_t fr;                               // 0x18  flag register
	uint32_t space2 [SPACE_SIZE(0x1c, 0x20)];  //
	uint32_t ilpr;                             // 0x20  IrDA low-power counter register
	uint32_t ibrd;                             // 0x24  integer baud rate register
	uint32_t fbrd;                             // 0x28  fractional baud rate register
	uint32_t lcrh;                             // 0x2c  line control register
	uint32_t cr;                               // 0x30  control register
	uint32_t ifls;                             // 0x34  interrupt FIFO level select register
	uint32_t imsc;                             // 0x38  interrupt mask set / clear register
	uint32_t ris;                              // 0x3c  raw interrupt status register
	uint32_t mis;                              // 0x40  masked interrupt status register
	uint32_t icr;                              // 0x44  interrupt clear register
	uint32_t dmacr;                            // 0x48  DMA control register
	uint32_t space3[SPACE_SIZE(0x4c, 0x80)];   //
	uint32_t itcr;                             // 0x80
	uint32_t itip;                             // 0x84
	uint32_t itop;                             // 0x88
	uint32_t tdr;                              // 0x8c
} Pl011_regs_t;

inline void uart_init(unsigned base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;

	regs->cr = 0;       // disable UART
	regs->icr = 0x7ff;  // clear pending interrupts

	// Set integer & fractional part of baud rate.
	// Divider = PL011_CLOCK/(16 * Baud)
	// Fraction part register = (Fractional part * 64) + 0.5
	// PL011_CLOCK = 3000000; Baud = 115200.
	(void) baudrate;
	(void) sys_clock_hz;

	// divider = 3000000/(16 * 115200) = 1.627 = ~1
	// fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40
	regs->ibrd = 1;  //mmio_write(base_addr + PL011_IBRD, 1);
	regs->fbrd = 40; //mmio_write(base_addr + PL011_FBRD, 40);

	// enable FIFO & 8 bit data transmissio (1 stop bit, no parity)
	regs->lcrh = (1<<4) | (1<<5) | (1<<6);

	// mask all interrupts
	regs->imsc = 0x7ff; // (1<<1) | (1<<4) | (1<<5) | (1<<6) | (1<<7) | (1<<8) | (1<<9) | (1<<10);

	// enable UART, receive & transfer part of UART
	regs->cr = (1<<0) | (1<<8) | (1<<9); // en, txen, rxen
}

// return 0/1 - is tx irq was enabled
inline int uart_tx_irq(unsigned long base_addr, int enable)
{
	//TODO
	(void)base_addr;
	(void)enable;
	return 0;
}

// return 0/1 - is rx irq was enabled
inline int uart_rx_irq(unsigned long base_addr, int enable)
{
	//TODO
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	int mask = regs->imsc;
	if (enable)
		regs->imsc = mask & ~Irq_rx;
	else
		regs->imsc = mask | Irq_rx;
	return (mask & Irq_rx) ? 1 : 0;
}

inline unsigned uart_fifo_size(unsigned long base_addr)
{
	// TODO
	(void)base_addr;
	return 1;  // only holding register are available, sz = 1
}

// return Uart_status_tx_empty or Uart_status_rx_full
inline int uart_status(unsigned long base_addr)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	unsigned long status = regs->ris;
	int res = 0;
	res |= (status & Irq_rx) ? Uart_status_rx_full : 0;
	res |= (status & Irq_tx) ? Uart_status_tx_empty : 0;
	return res;
}

/*
inline void uart_ack(unsigned long base_addr)
{
 volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
 regs->icr = 0x7ff;  // clear pending interrupts
}
~*/

// return error code (<0), or written bytes number (0 or 1)
inline int uart_putc(unsigned long base_addr, int c)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	if (regs->fr & (Flag_tx_full))  // is uart ready to transmit ?
		return 0;
	regs->dr = c;
	return 1;
}

// return error code (<0) or written bytes number
inline int uart_put_buf(unsigned long base_addr, const char* buf, unsigned sz)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned written = 0;
	if (!(regs->fr & Flag_tx_full)  &&  written < sz) // is uart ready to transmit ?
		regs->dr = buf[written++];
	return written;
}

// get one byte from UART input
inline int uart_getc(unsigned base_addr)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	if (regs->fr & Flag_rx_empty)  // is data ready ?
		return 0;
	return regs->dr;
}

inline int uart_get_buf(unsigned long base_addr, char* buf, unsigned sz)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned read = 0;
	while (!(regs->fr & Flag_rx_empty)  &&  read < sz)  // is data ready ?
	{
		buf[read++] = regs->dr;
	}
	return read;
}

#endif // UART_H
