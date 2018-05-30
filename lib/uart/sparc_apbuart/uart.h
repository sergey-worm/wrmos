//##################################################################################################
//
//  Gaisler APB UART low-level driver.
//
//##################################################################################################

#ifndef APB_UART_H
#define APB_UART_H

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
	Status_data_ready     = 1 <<  0,  // data ready
	Status_tx_shift_empty = 1 <<  1,  // transmit shift empty
	Status_tx_fifo_empty  = 1 <<  2,  // transmit fifo empty
	Status_break_received = 1 <<  3,  // BREAK received
	Status_overrun        = 1 <<  4,  // overrun
	Status_parity_err     = 1 <<  5,  // parity error
	Status_framing_err    = 1 <<  6,  // framing error
	Status_tx_fifo_hfull  = 1 <<  7,  // transmit fifo half-full
	Status_rx_fifo_hfull  = 1 <<  8,  // recv fifo half-full
	Status_tx_fifo_full   = 1 <<  9,  // transmit fifo full
	Status_rx_fifo_full   = 1 << 10,  // recv fifo full

	Status_tcnt_mask      = 0x3F,     //
	Status_tcnt_shift     = 20,       //
	Status_rcnt_mask      = 0x3F,     //
	Status_rcnt_shift     = 26,       //

	Ctrl_rx_enable        = 1 <<  0,  //
	Ctrl_tx_enable        = 1 <<  1,  //
	Ctrl_rx_irq_enable    = 1 <<  2,  //
	Ctrl_tx_irq_enable    = 1 <<  3,  //
	Ctrl_ps               = 1 <<  4,  //
	Ctrl_pe               = 1 <<  5,  //
	Ctrl_fl               = 1 <<  6,  //
	Ctrl_lb               = 1 <<  7,  //
	Ctrl_ec               = 1 <<  8,  //
	Ctrl_tf               = 1 <<  9,  //
	Ctrl_rf               = 1 << 10,  //
	Ctrl_fifo_avail       = 1 << 31,  // fifo available

	Scaler_mask           = 0xFFF
};

typedef struct
{
	uint32_t data;
	uint32_t status;
	uint32_t control;
	uint32_t scaller;

} Apb_uart_regs_t;

inline int uart_init(unsigned base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	regs->control = 0;
	if (regs->control & Ctrl_fifo_avail)
		return -1; // TODO:  test with fifo, code below for disabled fifo should be correct
	regs->scaller = sys_clock_hz / (baudrate * 8) - 1;
	regs->status = 0;
	regs->control = Ctrl_rx_enable | Ctrl_tx_enable;
	return 0;
}

// return 0/1 - is tx irq was enabled
inline int uart_tx_irq(unsigned long base_addr, int enable)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	uint32_t control = regs->control;
	if (control & Ctrl_fifo_avail)
		return -1; // TODO:  test with fifo, code below for disabled fifo should be correct
	if (enable)
		regs->control = control | Ctrl_tx_irq_enable;
	else
		regs->control = control & ~Ctrl_tx_irq_enable;
	return !!(control & Ctrl_tx_irq_enable);
}

// return 0/1 - is rx irq was enabled
inline int uart_rx_irq(unsigned long base_addr, int enable)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	uint32_t control = regs->control;
	if (control & Ctrl_fifo_avail)
		return -1; // TODO:  test with fifo, code below for disabled fifo should be correct
	if (enable)
		regs->control = control | Ctrl_rx_irq_enable;
	else
		regs->control = control & ~Ctrl_rx_irq_enable;
	return !!(control & Ctrl_rx_irq_enable);
}

// return tx/rx ready and tx/rx irq-pending flags
inline int uart_status(unsigned long base_addr)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	int res = 0;
	if (regs->control & Ctrl_fifo_avail)
		return -1; // TODO:  test Apbuart with fifo, code for disabled fifo should be correct
	unsigned long status = regs->status;
	res |= (status & Status_data_ready)     ? Uart_status_rx_ready : 0;
	res |= (status & Status_tx_shift_empty) ? Uart_status_tx_ready : 0;
	res |= (status & Status_data_ready)     ? Uart_status_rx_irq   : 0;
	res |= (status & Status_tx_shift_empty) ? Uart_status_tx_irq   : 0;
	return res;
}

// clear interrupt status flags, if need re-check status - return 1, else - 0
inline int uart_clear_irq(unsigned long base_addr)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	(void) regs; // nothing
	return 0;
}

// if send operation is complete - return 1, else - 0
inline int uart_is_tx_done(unsigned long base_addr)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	if (regs->control & Ctrl_fifo_avail)
		return -1; // TODO:  test Apbuart with fifo, code for disabled fifo should be correct
	return !!(regs->status & (Status_tx_shift_empty | Status_tx_fifo_empty));
}

// return error code (<0), or written bytes number (0 or 1)
inline int uart_putc(unsigned long base_addr, int c)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	if (regs->control & Ctrl_fifo_avail)
		return -1; // TODO:  test Apbuart with fifo, code for disabled fifo should be correct
	if (!(regs->status & Status_tx_shift_empty))
		return 0;
	regs->data = c;
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
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	if (regs->control & Ctrl_fifo_avail)
		return -1; // TODO:  test Apbuart with fifo, code for disabled fifo should be correct
	if (!sz)
		return 0;
	unsigned written = 0;
	while ((regs->status & Status_tx_shift_empty)  &&  written < sz) // is TX shift register empty ?
		regs->data = buf[written++];
	return written;
}

inline int uart_getc(unsigned long base_addr)
{
	volatile Apb_uart_regs_t* regs = (Apb_uart_regs_t*)base_addr;
	if (regs->control & Ctrl_fifo_avail)
		return -1; // TODO:  test with fifo, code below for disabled fifo should be correct
	if (regs->status & Status_data_ready)
		return regs->data;
	return 0;
}

#endif // APB_UART_H
