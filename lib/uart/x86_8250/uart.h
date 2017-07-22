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
	DLH  = 1     // Divisor latch High
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

inline void uart_putc(uintptr_t base_addr, int ch)
{
	uint16_t port = base_addr & 0xfff;       // lower 3 bytes
	while (!(uart_inb(port + LSR) & 0x20));  // wait tx empty
	uart_outb(port + TXR, ch);

	#ifdef UART_WITH_VIDEO
	video_putc(ch);
	#endif
}

// get one byte from UART input
inline int uart_getc(uintptr_t base_addr)
{
	(void) base_addr;
	return 0;
}

#endif // UART_H
