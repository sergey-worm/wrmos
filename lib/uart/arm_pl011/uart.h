//##################################################################################################
//
//  PL011 low-level driver.
//
//##################################################################################################

#ifndef UART_H
#define UART_H

#include <stdint.h>

#define SPACE_SIZE(addr1, addr2) (((addr2) - (addr1)) / sizeof(uint32_t))

typedef struct
{
	uint32_t dr;     // 0x00
	uint32_t rsrecr; // 0x04
	uint32_t space1 [SPACE_SIZE(0x08, 0x18)];
	uint32_t fr;     // 0x18
	uint32_t space2 [SPACE_SIZE(0x1c, 0x20)];
	uint32_t ilpr;   // 0x20
	uint32_t ibrd;   // 0x24
	uint32_t fbrd;   // 0x28
	uint32_t lcrh;   // 0x2c
	uint32_t cr;     // 0x30
	uint32_t ifls;   // 0x34
	uint32_t imsc;   // 0x38
	uint32_t ris;    // 0x3c
	uint32_t mis;    // 0x40
	uint32_t icr;    // 0x44
	uint32_t dmacr;  // 0x48
	uint32_t space3[SPACE_SIZE(0x4c, 0x80)];
	uint32_t itcr;   // 0x80
	uint32_t itip;   // 0x84
	uint32_t itop;   // 0x88
	uint32_t tdr;    // 0x8c
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
	regs->imsc = (1<<1) | (1<<4) | (1<<5) | (1<<6) | (1<<7) | (1<<8) | (1<<9) | (1<<10);

	// enable UART, receive & transfer part of UART
	regs->cr = (1<<0) | (1<<8) | (1<<9);
}

inline void uart_putc(unsigned base_addr, int ch)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	while (regs->fr & (1<<5));  // wait for UART to become ready to transmit
	regs->dr = ch;
}

// get one byte from UART input
inline int uart_getc(unsigned base_addr)
{
	volatile Pl011_regs_t* regs = (Pl011_regs_t*)base_addr;
	//while (regs->fr & (1<<4));  // wait for UART to have recieved something
	return regs->dr;
}

#endif // UART_H
