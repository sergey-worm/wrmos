//##################################################################################################
//
//  Cadence UART PS low-level driver.
//
//##################################################################################################

#ifndef CADENCE_UART_H
#define CADENCE_UART_H

#include <stdint.h>

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
	// control register bits
	Ctrl_stop_tx_break  = 1 <<  8,  // rw  stop TX break
	Ctrl_start_tx_break = 1 <<  7,  // rw  start TX break
	Ctrl_restart_rx_tc  = 1 <<  6,  // rw  restart RX timeout counter
	Ctrl_tx_dis         = 1 <<  5,  // rw  TX disable
	Ctrl_tx_en          = 1 <<  4,  // rw  TX enable
	Ctrl_rx_dis         = 1 <<  3,  // rw  RX disable
	Ctrl_rx_en          = 1 <<  2,  // rw  RX enable
	Ctrl_tx_reset       = 1 <<  1,  // rw  reset TX
	Ctrl_rx_reset       = 1 <<  0,  // rw  reset RX

	// status register bits
	Stat_tx_fifo_nfull  = 1 << 14,  // ro  TX FIFO nearly full
	Stat_tx_fifo_trig   = 1 << 13,  // ro  TX FIFO trigger
	Stat_rx_delay_trig  = 1 << 12,  // ro  RX flow delay trigger
	Stat_tx_active      = 1 << 11,  // ro  TX state machine active
	Stat_rx_active      = 1 << 10,  // ro  RX state machine active
	Stat_tx_full        = 1 <<  4,  // ro  TX FIFO full
	Stat_tx_empty       = 1 <<  3,  // ro  TX FIFO empty
	Stat_rx_full        = 1 <<  2,  // ro  RX FIFO full
	Stat_rx_empty       = 1 <<  1,  // ro  RX FIFO empty
	Stat_rx_fifo_trig   = 1 <<  0,  // ro  RX FIFO trigger

	// interrupt en/dis/mask/status register bits
	Int_tx_overflow     = 1 << 12,  // wr  TX FIFO overflow
	Int_tx_fifo_nfull   = 1 << 11,  // wr  TX FIFO nearly full
	Int_tx_fifo_trig    = 1 << 10,  // wr  TX FIFO trigger
	Int_dms             = 1 <<  9,  // wr  delta modem status indicator
	Int_rx_tout_err     = 1 <<  8,  // wr  RX timeout error
	Int_rx_parity_err   = 1 <<  7,  // wr  RX parity error
	Int_rx_frame_err    = 1 <<  6,  // wr  RX frame error
	Int_rx_overflow_err = 1 <<  5,  // wr  RX overflow error
	Int_tx_full         = 1 <<  4,  // wr  TX FIFO full
	Int_tx_empty        = 1 <<  3,  // wr  TX FIFO empty
	Int_rx_full         = 1 <<  2,  // wr  RX FIFO full
	Int_rx_empty        = 1 <<  1,  // wr  RX FIFO empty
	Int_rx_fifo_trig    = 1 <<  0,  // wr  RX FIFO trigger
	Int_all_mask        = 0x1fff,   // 
};

typedef struct
{
	uint32_t control;     //     UART control register
	uint32_t mode;        //     UART mode register
	uint32_t int_en;      //     interrupt enable register
	uint32_t int_dis;     //     interrupt disable register
	uint32_t int_mask;    // rd  interrupt mask register
	uint32_t int_status;  //     channel interrupt status register
	uint32_t brate_gen;   //     baud rate generator register
	uint32_t rx_tmout;    //     RX timeout register
	uint32_t rx_ftl;      //     RX FIFO trigger level register
	uint32_t modem_ctrl;  //     modem control register
	uint32_t modem_stat;  //     modem status register
	uint32_t status;      // rd  channel status register
	uint32_t fifo;        //     TX and RX fifo
	uint32_t brate_div;   //     baud rate divider register
	uint32_t flow_delay;  //     flow control delay register
	uint32_t tx_ftl;      //     TX FIFO trigger level register
} Cadence_uart_regs_t;

inline void uart_init(unsigned base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	(void) regs;
	(void) baudrate;
	(void) sys_clock_hz;

	// disable and reset
	regs->control = Ctrl_tx_dis | Ctrl_rx_dis | Ctrl_tx_reset | Ctrl_rx_reset;

	// wait while reset is doing
	while (regs->control & Ctrl_tx_reset  ||  regs->control & Ctrl_rx_reset);

	// setup and enable
	regs->rx_ftl = 1;                         // to get Int_rx_fifo_trig every 1 charecter
	regs->control = Ctrl_tx_en | Ctrl_rx_en;  //
}

// return 0/1 - is tx irq was enabled
inline int uart_tx_irq(unsigned long base_addr, int enable)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	int was_enab = !!(regs->int_mask & Int_tx_empty);
	if (enable)
		regs->int_en = Int_tx_empty;
	else
		regs->int_dis = Int_tx_empty;
	return was_enab;
}

// return 0/1 - is rx irq was enabled
inline int uart_rx_irq(unsigned long base_addr, int enable)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	int was_enab = !!(regs->int_mask & Int_rx_fifo_trig);
	if (enable)
		regs->int_en = Int_rx_fifo_trig;
	else
		regs->int_dis = Int_rx_fifo_trig;
	return was_enab;
}

// return tx/rx ready and tx/rx irq-pending flags
inline int uart_status(unsigned long base_addr)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	(void) regs;
	uint32_t status = regs->status;
	int res = 0;
	res |= !(status & Stat_rx_empty) ? Uart_status_rx_ready : 0;
	res |= !(status & Stat_tx_full)  ? Uart_status_tx_ready : 0;
	uint32_t istatus = regs->int_status;
	uint32_t mask = regs->int_mask;
	res |= (mask & istatus & Int_rx_fifo_trig) ? Uart_status_rx_irq : 0;
	res |= (mask & istatus & Int_tx_empty)     ? Uart_status_tx_irq : 0;
	return res;
}

// clear interrupt status flags, if need re-check status - return 1, else - 0
inline int uart_clear_irq(unsigned long base_addr)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	regs->int_status = Int_all_mask;
	return !!(regs->int_mask & regs->int_status & (Int_rx_fifo_trig | Int_tx_empty));
}

// if send operation is complete - return 1, else - 0
inline int uart_is_tx_done(unsigned long base_addr)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	return !!(regs->status & Stat_tx_empty);
}

// return error code (<0), or written bytes number (0 or 1)
inline int uart_putc(unsigned long base_addr, int c)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	if (regs->status & Stat_tx_full)
		return 0;
	regs->fifo = c;
	return 1;
}

// need only for working with uart_put_buf(),
// value 1 is mean driver will put to FIFO char-by-char,
// it is good for this driver
inline unsigned uart_fifo_size(unsigned long base_addr)
{
	(void) base_addr;
	return 1;
}

// return error code (<0) or written bytes number
inline int uart_put_buf(unsigned long base_addr, const char* buf, unsigned sz)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned written = 0;
	if (!(regs->status & Stat_tx_full)  &&  (written < sz))
		regs->fifo = buf[written++];
	return written;
}

// get one byte from UART input
inline int uart_getc(unsigned base_addr)
{
	volatile Cadence_uart_regs_t* regs = (Cadence_uart_regs_t*)base_addr;
	if (regs->status & Stat_rx_empty)
		return 0;
	return regs->fifo;
}

#endif // CADENCE_UART_H
