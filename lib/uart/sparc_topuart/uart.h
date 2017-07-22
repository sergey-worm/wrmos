//##################################################################################################
//
//  TOPCON UART low-level driver.
//
//##################################################################################################

#ifndef TOP_UART_H
#define TOP_UART_H

enum
{
	REG_RBR     = 0x0,  // R  - receive buffer register
	REG_THR     = 0x0,  // W  - transmit holding register
	REG_IER     = 0x1,  // W  - interrupt enable register
	REG_ISR     = 0x2,  // R  - interrupt status register
	REG_FCR     = 0x2,  // W  - FIFO control register
	REG_LCR     = 0x3,  // W  - line control register
	REG_MCR     = 0x4,  // W  - modem control register
	REG_LSR     = 0x5,  // R  - line status register
	REG_MSR     = 0x6,  // R  - modem status register
	REG_SPR     = 0x7,  // RW - Scratchpad Register

	// Baud Rate Generator Divisor Registers – selected when LCR[7]=1
	REG_DLL     = 0x0,  // W  - LSB of Divisor Latch
	REG_DLM     = 0x1,  // W  - MSB of Divisor Latch

	// Enhanced Register Set - selected when LCR=0xBF
	REG_EFR     = 0x2,  // RW - enhanced feature register
	REG_XON1    = 0x4,  // RW - Xon-1 word
	REG_XON2    = 0x5,  // RW - Xon-2 word
	REG_XOFF1   = 0x6,  // RW - Xoff-1 word
	REG_XOFF2   = 0x7,  // RW - Xoff-2 word

	REGS_COUNT  = 0x8,  //

	LCR_DLAB    = 0x80, // Divisor Latch Access Bit
	LCR_SB      = 0x40, // Set Break
	LCR_SP      = 0x20, // Stick Parity
	LCR_EPS     = 0x10, // Even Parity Select
	LCR_PEN     = 0x08, // Parity Enabled
	LCR_STB     = 0x04, // Number of Stop bits
	LCR_WL8BIT  = 0x03, // Word Length 8 bits
	LCR_WL7BIT  = 0x02, // Word Length 7 bits
	LCR_WL6BIT  = 0x01, // Word Length 6 bits
	LCR_WL5BIT  = 0x00, // Word Length 5 bits

	LSR_FIFOERR = 0x80, // RX Data Error in FIFO
	LSR_TEMT    = 0x40, // Transmitter Empty
	LSR_THRE    = 0x20, // TX Holding Register Empty
	LSR_BI      = 0x10, // Break Interrupt
	LSR_FE      = 0x08, // Framing Error
	LSR_PE      = 0x04, // Parity Error
	LSR_OE      = 0x02, // Overrun Error
	LSR_DR      = 0x01, // Data Ready
};

// Topuart registers
typedef struct
{
	unsigned reg[REGS_COUNT];
} Topuart_regs_t;

// in table more exect values
static unsigned short baud2divisor(unsigned baud, unsigned sys_clock_hz)
{
	if (sys_clock_hz == 125*1000*1000)
	{
		switch (baud)
		{
			case 300:    return 26042;
			case 600:    return 13021;
			case 1200:   return 6510;
			case 2400:   return 3255;
			case 4800:   return 1628;
			case 9600:   return 814;
			case 19200:  return 407;
			case 38400:  return 203;
			case 57600:  return 136;
			case 115200: return 68;
			case 230400: return 34;
			case 460800: return 17;
		}
	}
	return sys_clock_hz / (16 * baud);
}

inline void uart_init(unsigned base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	unsigned short divisor = baud2divisor(baudrate, sys_clock_hz);
	unsigned char lcr = LCR_WL8BIT;           // LCR value
	regs->reg[REG_LCR] = lcr | LCR_DLAB;      // select Divisor Latch Registers
	regs->reg[REG_DLL] = divisor & 0xff;
	regs->reg[REG_DLM] = (divisor >> 8) & 0xff;
	regs->reg[REG_LCR] = lcr;
}

inline void uart_putc(unsigned base_addr, int ch)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	while (!((regs->reg[REG_LSR] & LSR_TEMT) == LSR_TEMT));
	regs->reg[REG_THR] = ch & 0xff;
	while ((regs->reg[REG_LSR] & LSR_TEMT) != LSR_TEMT);
}

// get one byte from UART input
inline int uart_getc(unsigned base_addr)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	return regs->reg[REG_RBR];
}

#endif // TOP_UART_H
