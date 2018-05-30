//##################################################################################################
//
//  National Semiconductor 8250 UART low-level driver.
//
//##################################################################################################

#ifndef NS_8250_UART_H
#define NS_8250_UART_H

#include <stdint.h>

#ifdef UART_WITH_VIDEO
#  include "video.h"
#endif

enum
{
	Uart_status_tx_ready = 1 << 1,
	Uart_status_rx_ready = 1 << 2,
	Uart_status_tx_irq   = 1 << 3,
	Uart_status_rx_irq   = 1 << 4,

	Uart_need_ack_irq_before_reenable = 0 // for x86 don't need ack irq before re-enable
};

enum
{
	// registers
	TXR = 0,                          // Transmit register (WRITE)
	RXR = 0,                          // Receive register  (READ)
	IER = 1,                          // Interrupt Enable
	IIR = 2,                          // Interrupt ID
	FCR = 2,                          // FIFO control
	LCR = 3,                          // Line control
	MCR = 4,                          // Modem control
	LSR = 5,                          // Line Status
	MSR = 6,                          // Modem Status
	DLL = 0,                          // Divisor Latch Low
	DLH = 1,                          // Divisor latch High

	// line control register bits
	Lcr_dlab = 0x80,


	// line status register bits
	Lsr_rx_fifo_err        = 1 << 7,  // error in received FIFO
	Lsr_rx_empty           = 1 << 6,  // empty data holding register
	Lsr_tx_empty           = 1 << 5,  // empty transmitter holding register
	Lsr_break_int          = 1 << 4,  // break interrupt
	Lsr_framing_err        = 1 << 3,  // framing error
	Lsr_parity_err         = 1 << 2,  // parity error
	Lsr_overrun_err        = 1 << 1,  // overrun error
	Lsr_data_ready         = 1 << 0,  // data ready

	// interrupt enable register bits
	Ier_low_pow_mode       = 1 << 5,  // enable low power mode (16750)
	Ier_sleep_mode         = 1 << 4,  // enable low power mode (16750)
	Ier_modem_status_irq   = 1 << 3,  // enable modem status interrupt
	Ier_rx_line_status_irq = 1 << 2,  // enable receiver line status interrupt
	Ier_tx_empty_irq       = 1 << 1,  // enable transmitter holding register interrupt
	Ier_rx_data_irq        = 1 << 0,  // enable receiver data available interrupt
};

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

inline void uart_init(unsigned long base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	(void) sys_clock_hz;                    // don't need
	uint16_t port = base_addr & 0xfff;      // lower 3 bytes

	uart_outb(port + LCR, 0x3);             // 8n1
	uart_outb(port + IER, 0);               // no interrupt
	uart_outb(port + FCR, 0);               // no fifo
	uart_outb(port + MCR, 0x3);             // DTR + RTS

	unsigned divisor = 115200 / baudrate;
	unsigned char c = uart_inb(port + LCR);
	uart_outb(port + LCR, c | Lcr_dlab);
	uart_outb(port + DLL, divisor & 0xff);
	uart_outb(port + DLH, (divisor >> 8) & 0xff);
	uart_outb(port + LCR, c & ~Lcr_dlab);

	#ifdef UART_WITH_VIDEO
	video_init(base_addr & 0xfffffffffffff000);
	#endif
}

// return 0/1 - is tx irq was enabled
inline int uart_tx_irq(unsigned long base_addr, int enable)
{
	uint16_t port = base_addr & 0xfff;  // lower 3 bytes
	int ier = uart_inb(port + IER);
	if (enable)
		uart_outb(port + IER, ier | Ier_tx_empty_irq);
	else
		uart_outb(port + IER, ier & ~Ier_tx_empty_irq);
	return !!(ier & Ier_tx_empty_irq);
}

// return 0/1 - is rx irq was enabled
inline int uart_rx_irq(unsigned long base_addr, int enable)
{
	uint16_t port = base_addr & 0xfff;  // lower 3 bytes
	int ier = uart_inb(port + IER);
	if (enable)
		uart_outb(port + IER, ier | Ier_rx_data_irq);
	else
		uart_outb(port + IER, ier & ~Ier_rx_data_irq);
	return !!(ier & Ier_rx_data_irq);
}

// return tx/rx ready and tx/rx irq-pending flags
inline int uart_status(unsigned long base_addr)
{
	uint16_t port = base_addr & 0xfff;  // lower 3 bytes
	int res = 0;
	unsigned long status = uart_inb(port + LSR);
	res |= (status & Lsr_data_ready) ? Uart_status_rx_ready : 0;
	res |= (status & Lsr_tx_empty)   ? Uart_status_tx_ready : 0;
	res |= (status & Lsr_data_ready) ? Uart_status_rx_irq   : 0;
	res |= (status & Lsr_tx_empty)   ? Uart_status_tx_irq   : 0;
	return res;
}

// clear interrupt status flags, if need re-check status - return 1, else - 0
inline int uart_clear_irq(unsigned long base_addr)
{
	(void) base_addr; // nothing
	return 0;
}

// if send operation is complete - return 1, else - 0
inline int uart_is_tx_done(unsigned long base_addr)
{
	uint16_t port = base_addr & 0xfff;  // lower 3 bytes
	return !!(uart_inb(port + LSR) & Lsr_tx_empty);
}

inline int uart_putc(unsigned long base_addr, int ch)
{
	uint16_t port = base_addr & 0xfff;  // lower 3 bytes
	#ifdef UART_WITH_VIDEO
	video_putc(base_addr & 0xfffffffffffff000, ch);
	#endif
	if (!(uart_inb(port + LSR) & Lsr_tx_empty))
		return 0;
	uart_outb(port + TXR, ch);
	return 1;
}

// need only for working with uart_put_buf(),
// value 1 is mean driver will put to FIFO char-by-char
inline unsigned uart_fifo_size(unsigned long base_addr)
{
	(void) base_addr;
	return 1;
}

// return error code (<0) or written bytes number
inline int uart_put_buf(unsigned long base_addr, const char* buf, unsigned sz)
{
	uint16_t port = base_addr & 0xfff;  // lower 3 bytes
	if (!sz)
		return 0;
	unsigned written = 0;
	while ((uart_inb(port + LSR) & Lsr_tx_empty)  &&  written < sz)
	{
		#ifdef UART_WITH_VIDEO
		video_putc(base_addr & 0xfffffffffffff000, buf[written]);
		#endif
		uart_outb(port + TXR, buf[written++]);
	}
	return written;
}

// get one byte from UART input
inline int uart_getc(unsigned long base_addr)
{
	uint16_t port = base_addr & 0xfff;  // lower 3 bytes
	if (uart_inb(port + LSR) & Lsr_data_ready)
		return uart_inb(port + RXR);
	return 0;
}

#endif // NS_8250_UART_H
