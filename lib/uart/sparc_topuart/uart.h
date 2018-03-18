//##################################################################################################
//
//  TOPCON UART low-level driver.
//
//##################################################################################################

#ifndef TOP_UART_H
#define TOP_UART_H

enum
{
	Uart_status_tx_empty = 1 << 1,
	Uart_status_rx_full  = 1 << 2
};

enum
{
	REG_RBR           = 0x0,  // R  - receive buffer register
	REG_THR           = 0x0,  // W  - transmit holding register
	REG_IER           = 0x1,  // W  - interrupt enable register
	REG_IIR           = 0x2,  // R  - interrupt identification register
	REG_FCR           = 0x2,  // W  - FIFO control register
	REG_LCR           = 0x3,  // W  - line control register
	REG_MCR           = 0x4,  // W  - modem control register
	REG_LSR           = 0x5,  // R  - line status register
	REG_MSR           = 0x6,  // R  - modem status register
	REG_SPR           = 0x7,  // RW - Scratchpad Register

	// Baud Rate Generator Divisor Registers – selected when LCR[7]=1
	REG_DLL           = 0x0,  // W  - LSB of Divisor Latch
	REG_DLM           = 0x1,  // W  - MSB of Divisor Latch

	// Enhanced Register Set - selected when LCR=0xBF
	REG_EFR           = 0x2,   // RW - enhanced feature register
	REG_XON1          = 0x4,   // RW - Xon-1 word
	REG_XON2          = 0x5,   // RW - Xon-2 word
	REG_XOFF1         = 0x6,   // RW - Xoff-1 word
	REG_XOFF2         = 0x7,   // RW - Xoff-2 word

	REGS_COUNT        = 0x8,   //

	LCR_DLAB          = 0x80,  // Divisor Latch Access Bit
	LCR_SB            = 0x40,  // Set Break
	LCR_SP            = 0x20,  // Stick Parity
	LCR_EPS           = 0x10,  // Even Parity Select
	LCR_PEN           = 0x08,  // Parity Enabled
	LCR_STB           = 0x04,  // Number of Stop bits
	LCR_WL8BIT        = 0x03,  // Word Length 8 bits
	LCR_WL7BIT        = 0x02,  // Word Length 7 bits
	LCR_WL6BIT        = 0x01,  // Word Length 6 bits
	LCR_WL5BIT        = 0x00,  // Word Length 5 bits

	LSR_FIFOERR       = 0x80,  // RX Data Error in FIFO
	LSR_TEMT          = 0x40,  // Transmitter Empty
	LSR_THRE          = 0x20,  // TX Holding Register Empty
	LSR_BI            = 0x10,  // Break Interrupt
	LSR_FE            = 0x08,  // Framing Error
	LSR_PE            = 0x04,  // Parity Error
	LSR_OE            = 0x02,  // Overrun Error
	LSR_DR            = 0x01,  // Data Ready

	Uart_ier_rx       = 0x01,  // RX Buffer Register Interrupt
	Uart_ier_tx       = 0x02,  // TX Holding Register Empty Interrupt
	Uart_ier_rls      = 0x04,  // RX Line Status Interrupt
	Uart_ier_msr      = 0x08,  // Modem Status Interrupt
	Uart_ier_sleep    = 0x10,  // Power-Down Mode
	Uart_ier_xoff     = 0x20,  // XOFF Interrupt
	Uart_ier_rts      = 0x40,  // RTS Interrupt
	Uart_ier_cts      = 0x80,  // CTS Interrupt

	Iir_code_mask     = 0x3f,  // 6 bits
	Iir_code_nopend   =    1,  // 000001 - no interrupt pending
	Iir_code_rxline   =    6,  // 000110 - rx line status
	Iir_code_rxready  =    4,  // 000100 - rx data received
	Iir_code_rxto     =   12,  // 001100 - rx fifo charecter timeout
	Iir_code_txready  =    2,  // 000010 - tx holding register (or tx fifo) empty
	Iir_code_msc      =    0,  // 000000 - modem status changed
	Iir_code_sfc      =   16,  // 010000 - software flow control
	Iir_code_hfc      =   32,  // 100000 - hardware flow control

	Fcr_rx_fifo_lev1  = 1 << 7, // rx fifo trigger level bit 1
	Fcr_rx_fifo_lev0  = 1 << 6, // rx fifo trigger level bit 0
	Fcr_tx_fifo_lev1  = 1 << 5, // tx fifo trigger level bit 1
	Fcr_tx_fifo_lev0  = 1 << 4, // tx fifo trigger level bit 0
	Fcr_dma_mode      = 1 << 3, // set DMA Mode 1
	Fcr_clear_tx_fifo = 1 << 2, // clear tx fifo
	Fcr_clear_rx_fifo = 1 << 1, // clear rx fifo
	Fcr_enable_fifo   = 1 << 0, // enable fifos

	Uart_fifo_sz     = 64
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
	regs->reg[REG_FCR] = Fcr_enable_fifo;
}

// return 0/1 - is tx irq was enabled
inline int uart_tx_irq(unsigned long base_addr, int enable)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	int old = (regs->reg[REG_IER] & Uart_ier_tx) ? 1 : 0;
	if (enable)
		regs->reg[REG_IER] |= Uart_ier_tx;
	else
		regs->reg[REG_IER] &= ~Uart_ier_tx;
	return old;
}

// return 0/1 - is rx irq was enabled
inline int uart_rx_irq(unsigned long base_addr, int enable)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	int old = (regs->reg[REG_IER] & Uart_ier_rx) ? 1 : 0;
	if (enable)
		regs->reg[REG_IER] |= Uart_ier_rx;
	else
		regs->reg[REG_IER] &= ~Uart_ier_rx;
	return old;
}

inline unsigned uart_fifo_size(unsigned long base_addr)
{
	(void) base_addr;
	return Uart_fifo_sz;
}
/*
inline int uart_status_raw(unsigned long base_addr)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	return regs->reg[REG_IIR] & Iir_code_mask;
}
*/
// return Uart_status_tx_empty or Uart_status_rx_full
inline int uart_status(unsigned long base_addr)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	int res = 0;
	int code = regs->reg[REG_IIR] & Iir_code_mask;
	switch (code)
	{
		case Iir_code_rxready:
		case Iir_code_rxto:
			res |= Uart_status_rx_full;
			break;

		case Iir_code_txready:
			res |= Uart_status_tx_empty;
			break;

		case Iir_code_nopend:
			break;

		case Iir_code_rxline:
		case Iir_code_msc:
		case Iir_code_sfc:
		case Iir_code_hfc:
			break;
	}
	return res;
}

// return error code (<0), or written bytes number (0 or 1)
inline int uart_putc(unsigned long base_addr, int c)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	if (!(regs->reg[REG_LSR] & LSR_THRE))
		return 0;
	regs->reg[REG_THR] = c;
	return 1;
}

// return error code (<0) or written bytes number
inline int uart_put_buf(unsigned long base_addr, const char* buf, unsigned sz)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned written = 0;
	//if (regs->reg[REG_LSR] & LSR_THRE)  // is tx ready ?
	//if (uart_status(base_addr) & Uart_status_tx_empty)
	{
		sz = (sz < Uart_fifo_sz) ? sz : (unsigned)Uart_fifo_sz;
		while (written < sz)
			regs->reg[REG_THR] = buf[written++];
	}
	return written;
}

// get one byte from UART input
inline int uart_getc(unsigned base_addr)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	if (regs->reg[REG_LSR] & LSR_DR)  // is data ready ?
		return regs->reg[REG_RBR];
	return 0;
}

// ...
inline int uart_get_buf(unsigned long base_addr, char* buf, unsigned sz)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned read = 0;
	while ((regs->reg[REG_LSR] & LSR_DR)  &&  read < sz)  // is data ready ?
	{
		buf[read++] = regs->reg[REG_RBR];
	}
	return read;
}

#endif // TOP_UART_H
