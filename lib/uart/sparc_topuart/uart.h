//##################################################################################################
//
//  Topcon UART low-level driver.
//
//##################################################################################################

#ifndef TOP_UART_H
#define TOP_UART_H

enum
{
	Uart_status_tx_ready = 1 << 1,
	Uart_status_rx_ready = 1 << 2,
	Uart_status_tx_irq   = 1 << 3,
	Uart_status_rx_irq   = 1 << 4,

	Uart_need_ack_irq_before_reenable = 1
};

enum
{
	// registers
	REG_RBR           = 0x0,  // ro  receive buffer register
	REG_THR           = 0x0,  // wo  transmit holding register
	REG_IER           = 0x1,  // wo  interrupt enable register
	REG_IIR           = 0x2,  // ro  interrupt identification register
	REG_FCR           = 0x2,  // wo  FIFO control register
	REG_LCR           = 0x3,  // wo  line control register
	REG_MCR           = 0x4,  // wo  modem control register
	REG_LSR           = 0x5,  // ro  line status register
	REG_MSR           = 0x6,  // ro  modem status register
	REG_SPR           = 0x7,  // rw  scratchpad Register

	// baud rate generator divisor registers – selected when LCR[7]=1
	REG_DLL           = 0x0,  // w   LSB of Divisor Latch
	REG_DLM           = 0x1,  // w   MSB of Divisor Latch

	// enhanced register set - selected when LCR=0xBF
	REG_EFR           = 0x2,   // RW - enhanced feature register
	REG_XON1          = 0x4,   // RW - Xon-1 word
	REG_XON2          = 0x5,   // RW - Xon-2 word
	REG_XOFF1         = 0x6,   // RW - Xoff-1 word
	REG_XOFF2         = 0x7,   // RW - Xoff-2 word

	REGS_COUNT        = 0x8,   //

	// line control register bits
	Lcr_dlab          = 0x80,  // divisor latch access bit
	Lcr_sb            = 0x40,  // set break
	Lcr_sp            = 0x20,  // stick parity
	Lcr_eps           = 0x10,  // even parity select
	Lcr_pen           = 0x08,  // parity enabled
	Lcr_stb           = 0x04,  // number of stop bits
	Lcr_wl8bit        = 0x03,  // word Length 8 bits
	Lcr_wl7bit        = 0x02,  // word Length 7 bits
	Lcr_wl6bit        = 0x01,  // word Length 6 bits
	Lcr_wl5bit        = 0x00,  // Word Length 5 bits

	// line status register bits
	Lsr_rx_fifo_err   = 1 << 7,  // RX FIFO data error
	Lsr_tx_empty      = 1 << 6,  // TX empty
	Lsr_tx_hold_empty = 1 << 5,  // TX holding register empty
	Lsr_bi            = 1 << 4,  // break interrupt
	Lsr_fe            = 1 << 3,  // framing error
	Lsr_pe            = 1 << 2,  // parity error
	Lsr_oe            = 1 << 1,  // overrun error
	Lsr_data_ready    = 1 << 0,  // data ready

	// interrupt enable register bits
	Ier_cts           = 1 << 7,  // CTS interrupt
	Ier_rts           = 1 << 6,  // RTS interrupt
	Ier_xoff          = 1 << 5,  // XOFF interrupt
	Ier_sleep         = 1 << 4,  // power-down mode
	Ier_msr           = 1 << 3,  // modem status interrupt
	Ier_rls           = 1 << 2,  // RX line status interrupt
	Ier_tx            = 1 << 1,  // TX holding register empty interrupt
	Ier_rx            = 1 << 0,  // RX buffer register interrupt

	// interrupt identification register codes
	Iir_code_mask     = 0x3f,    // 6 bits
	Iir_code_nopend   =    1,    // 000001 - no interrupt pending
	Iir_code_rxline   =    6,    // 000110 - rx line status
	Iir_code_rxready  =    4,    // 000100 - rx data received
	Iir_code_rxto     =   12,    // 001100 - rx fifo charecter timeout
	Iir_code_txready  =    2,    // 000010 - tx holding register (or tx FIFr) empty
	Iir_code_msc      =    0,    // 000000 - modem status changed
	Iir_code_sfc      =   16,    // 010000 - software flow control
	Iir_code_hfc      =   32,    // 100000 - hardware flow control

	// FIFO control register bits
	Fcr_rx_fifo_lev1  = 1 << 7,  // RX FIFO trigger level bit 1
	Fcr_rx_fifo_lev0  = 1 << 6,  // RX FIFO trigger level bit 0
	Fcr_tx_fifo_lev1  = 1 << 5,  // TX FIFO trigger level bit 1
	Fcr_tx_fifo_lev0  = 1 << 4,  // TX FIFO trigger level bit 0
	Fcr_dma_mode      = 1 << 3,  // set DMA Mode 1
	Fcr_clear_tx_fifo = 1 << 2,  // clear TX FIFO
	Fcr_clear_rx_fifo = 1 << 1,  // clear RX FIFO
	Fcr_enable_fifo   = 1 << 0,  // enable FIFOs

	Uart_fifo_sz      = 64
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
	regs->reg[REG_LCR] = Lcr_wl8bit | Lcr_dlab;   // select divisor latch registers
	regs->reg[REG_DLL] = divisor & 0xff;
	regs->reg[REG_DLM] = (divisor >> 8) & 0xff;
	regs->reg[REG_LCR] = Lcr_wl8bit;
	regs->reg[REG_FCR] = Fcr_enable_fifo;
}

// return 0/1 - is tx irq was enabled
inline int uart_tx_irq(unsigned long base_addr, int enable)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	int was_enab = !!(regs->reg[REG_IER] & Ier_tx);
	if (enable)
		regs->reg[REG_IER] |= Ier_tx;
	else
		regs->reg[REG_IER] &= ~Ier_tx;
	return was_enab;
}

// return 0/1 - is rx irq was enabled
inline int uart_rx_irq(unsigned long base_addr, int enable)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	int was_enab = !!(regs->reg[REG_IER] & Ier_rx);
	if (enable)
		regs->reg[REG_IER] |= Ier_rx;
	else
		regs->reg[REG_IER] &= ~Ier_rx;
	return was_enab;
}

// return tx/rx ready and tx/rx irq-pending flags
inline int uart_status(unsigned long base_addr)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	int res = 0;
	unsigned status = regs->reg[REG_LSR];
	res |= (status & Lsr_data_ready) ? Uart_status_rx_ready : 0;
	res |= (status & Lsr_tx_hold_empty) ? Uart_status_tx_ready : 0;
	int code = regs->reg[REG_IIR] & Iir_code_mask;
	res |= (code == Iir_code_rxready  ||  code == Iir_code_rxto) ? Uart_status_rx_irq : 0;
	res |= (code == Iir_code_txready) ? Uart_status_tx_irq : 0;
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
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	unsigned flags = Lsr_tx_hold_empty; // | Lsr_tx_empty;
	return (regs->reg[REG_LSR] & flags) == flags;
}

// return error code (<0), or written bytes number (0 or 1)
inline int uart_putc(unsigned long base_addr, int c)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	if (!(regs->reg[REG_LSR] & Lsr_tx_hold_empty))
		return 0;
	regs->reg[REG_THR] = c;
	return 1;
}

// need only for working with uart_put_buf(),
// drive will put to TX-FIFO Uart_fifo_sz bytes every
// call of uart_put_buf
inline unsigned uart_fifo_size(unsigned long base_addr)
{
	(void) base_addr;
	return Uart_fifo_sz;
}

// return error code (<0) or written bytes number
// NOTE:  TopUART doesn't allow to know is-fifo-not-full,
//        we may only detect is-fifo-empty and put Fifo_sz bytes
inline int uart_put_buf(unsigned long base_addr, const char* buf, unsigned sz)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned written = 0;
	if (!(regs->reg[REG_LSR] & Lsr_tx_hold_empty))
		return 0;
	sz = (sz < Uart_fifo_sz) ? sz : (unsigned)Uart_fifo_sz;
	while (written < sz)
		regs->reg[REG_THR] = buf[written++];
	return written;
}

// get one byte from UART input
inline int uart_getc(unsigned base_addr)
{
	volatile Topuart_regs_t* regs = (Topuart_regs_t*)base_addr;
	if (regs->reg[REG_LSR] & Lsr_data_ready)
		return regs->reg[REG_RBR];
	return 0;
}

#endif // TOP_UART_H
