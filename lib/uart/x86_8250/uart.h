//##################################################################################################
//
//  8250 UART low-level driver.
//
//##################################################################################################

#ifndef UART_H
#define UART_H

#include <stdint.h>

#ifdef UART_WITH_VIDEO
#  include "video.h"
#endif

__attribute__((always_inline)) inline void uart_outb(uint16_t port, uint8_t v)
{
	asm volatile ("outb %0, %1" :: "a"(v), "dN"(port));
}

__attribute__((always_inline)) inline uint8_t uart_inb(uint16_t port)
{
	uint8_t v;
	asm volatile ("inb %1, %0" : "=a"(v) : "dN"(port));
	return v;
}

enum
{
	Uart_status_tx_empty = 1 << 1,
	Uart_status_rx_full  = 1 << 2
};

enum
{
	DLAB = 0x80,
	TXR  = 0,    // Transmit register (WRITE)
	RXR  = 0,    // Receive register  (READ)
	IER  = 1,    // Interrupt Enable
	IIR  = 2,    // Interrupt ID
	FCR  = 2,    // FIFO control
	LCR  = 3,    // Line control
	MCR  = 4,    // Modem control
	LSR  = 5,    // Line Status
	MSR  = 6,    // Modem Status
	DLL  = 0,    // Divisor Latch Low
	DLH  = 1,    // Divisor latch High

	Lsr_rx_fifo_err = 1 << 7,  // error in received FIFO
	Lsr_rx_empty    = 1 << 6,  // empty data holding register
	Lsr_tx_empty    = 1 << 5,  // empty transmitter holding register
	Lsr_break_int   = 1 << 4,  // break interrupt
	Lsr_framing_err = 1 << 3,  // framing error
	Lsr_parity_err  = 1 << 2,  // parity error
	Lsr_overrun_err = 1 << 1,  // overrun error
	Lsr_data_ready  = 1 << 0   // data ready
};

inline void uart_init(uintptr_t base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	(void) sys_clock_hz;               // don't need
	uint16_t port = base_addr & 0xfff; // lower 3 bytes

	uart_outb(port + LCR, 0x3);        // 8n1
	uart_outb(port + IER, 0);          // no interrupt
	uart_outb(port + FCR, 0);          // no fifo
	uart_outb(port + MCR, 0x3);        // DTR + RTS

	unsigned divisor = 115200 / baudrate;
	unsigned char c = uart_inb(port + LCR);
	uart_outb(port + LCR, c | DLAB);
	uart_outb(port + DLL, divisor & 0xff);
	uart_outb(port + DLH, (divisor >> 8) & 0xff);
	uart_outb(port + LCR, c & ~DLAB);

	#ifdef UART_WITH_VIDEO
	video_init(base_addr & 0xfffffffffffff000);
	#endif
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
	(void)base_addr;
	(void)enable;
	return 0;
}

inline unsigned uart_fifo_size(unsigned long base_addr)
{
	(void) base_addr;
	if (0) // TODO
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		// only holding register are available, sz = 1
		return 1;
	}
}

// return Uart_status_tx_empty or Uart_status_rx_full
inline int uart_status(unsigned long base_addr)
{
	uint16_t port = base_addr & 0xfff;               // lower 3 bytes
	int res = 0;
	if (0)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		unsigned long status = uart_inb(port + LSR);
		res |= (status & Lsr_data_ready) ? Uart_status_rx_full : 0;
		res |= (status & Lsr_tx_empty)   ? Uart_status_tx_empty : 0;
	}
	return res;
}

inline int uart_putc(uintptr_t base_addr, int ch)
{
	uint16_t port = base_addr & 0xfff;               // lower 3 bytes

	#ifdef UART_WITH_VIDEO
	video_putc(ch);
	#endif

	if (uart_inb(port + LSR) & Lsr_tx_empty)  // is tx ready
	{
		uart_outb(port + TXR, ch);
		return 1;
	}
	return 0;
}

// return error code (<0) or written bytes number
inline int uart_put_buf(unsigned long base_addr, const char* buf, unsigned sz)
{
	uint16_t port = base_addr & 0xfff;               // lower 3 bytes
	if (!sz)
		return 0;
	if (0)
	{
		return -1; // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		unsigned written = 0;
		while (uart_inb(port + LSR) & Lsr_tx_empty  &&  written < sz) // is TX shift register empty ?
			uart_outb(port + TXR, buf[written++]);
		return written;
	}
}

// get one byte from UART input
inline int uart_getc(uintptr_t base_addr)
{
	uint16_t port = base_addr & 0xfff;               // lower 3 bytes
	if (uart_inb(port + LSR) & Lsr_data_ready)
		return uart_inb(port + RXR);
	return 0;
}

inline int uart_get_buf(unsigned long base_addr, char* buf, unsigned sz)
{
	uint16_t port = base_addr & 0xfff;               // lower 3 bytes
	if (!sz)
		return 0;
	if (0)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		unsigned read = 0;
		while (uart_inb(port + LSR) & Lsr_data_ready  &&  read < sz)  // is data ready ?
			buf[read++] = uart_inb(port + RXR);
		return read;
	}
}

#endif // UART_H
