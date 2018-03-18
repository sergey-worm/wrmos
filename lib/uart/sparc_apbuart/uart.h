//##################################################################################################
//
//  APB UART low-level driver.
//
//##################################################################################################

#ifndef APB_UART_H
#define APB_UART_H

#include <stdint.h>

enum
{
	Uart_status_tx_empty = 1 << 1,
	Uart_status_rx_full  = 1 << 2
};

enum
{
	REG_DATA   = 0x00,
	REG_STATUS = 0x04,
	REG_CTRL   = 0x08,
	REG_SCALER = 0x0C,

	DATA_MASK  = 0x0F,

	STATUS_DR  = 1 <<  0,  // data ready
	STATUS_TS  = 1 <<  1,  // transmit shift empty
	STATUS_TE  = 1 <<  2,  // transmit fifo empty
	STATUS_BR  = 1 <<  3,  // BREAK received
	STATUS_OV  = 1 <<  4,  // overrun
	STATUS_PE  = 1 <<  5,  // parity error
	STATUS_FE  = 1 <<  6,  // framing error
	STATUS_TH  = 1 <<  7,  // transmit fifo half-full
	STATUS_RH  = 1 <<  8,  // recv fifo half-full
	STATUS_TF  = 1 <<  9,  // transmit fifo full
	STATUS_RF  = 1 << 10,  // recv fifo full

	STATUS_TCNT_MASK  = 0x3F,
	STATUS_TCNT_SHIFT = 20,
	STATUS_RCNT_MASK  = 0x3F,
	STATUS_RCNT_SHIFT = 26,

	CTRL_RE    = 1 <<  0,
	CTRL_TE    = 1 <<  1,
	CTRL_RI    = 1 <<  2,
	CTRL_TI    = 1 <<  3,
	CTRL_PS    = 1 <<  4,
	CTRL_PE    = 1 <<  5,
	CTRL_FL    = 1 <<  6,
	CTRL_LB    = 1 <<  7,
	CTRL_EC    = 1 <<  8,
	CTRL_TF    = 1 <<  9,
	CTRL_RF    = 1 << 10,
	CTRL_FA    = 1 << 31,  // fifo available

	SCALER_MASK = 0xFFF,
};

// TODO:  do Device_apbuart_t { ... }

inline unsigned long mem_rd(unsigned long addr)
{
	unsigned val;
	asm volatile ("ld  [ %[addr] ], %[val]" : [val]"=r"(val) : [addr]"r"(addr));
	return val;
}

inline void mem_wr(unsigned long addr, unsigned long val)
{
	asm volatile ("st  %[val], [ %[addr] ]" : : [val]"r"(val), [addr]"r"(addr));
}

inline int uart_init(unsigned base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	//  volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	if (control & CTRL_FA)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		mem_wr(base_addr + REG_SCALER, sys_clock_hz / (baudrate * 8) - 1);
		mem_wr(base_addr + REG_CTRL, CTRL_RE | CTRL_TE);
		mem_wr(base_addr + REG_STATUS, 0x0);
		return 0;
	}
}

// return 0/1 - is tx irq was enabled
inline int uart_tx_irq(unsigned long base_addr, int enable)
{
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	unsigned long new_val = 0;
	if (control & CTRL_FA)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		new_val = enable ? (control | CTRL_TI) : (control & ~CTRL_TI);
	}
	mem_wr(base_addr + REG_CTRL, new_val);
	return (control | CTRL_TI) ? 1 : 0;
}

// return 0/1 - is rx irq was enabled
inline int uart_rx_irq(unsigned long base_addr, int enable)
{
	//  volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	unsigned long new_val = 0;
	if (control & CTRL_FA)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		new_val = enable ? (control | CTRL_RI) : (control & ~CTRL_RI);
	}
	mem_wr(base_addr + REG_CTRL, new_val);
	return (control | CTRL_RI) ? 1 : 0;
}

inline unsigned uart_fifo_size(unsigned long base_addr)
{
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	if (control & CTRL_FA)
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
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	int res = 0;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	if (control & CTRL_FA)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		unsigned long status = mem_rd(base_addr + REG_STATUS);
		res |= (status & STATUS_DR) ? Uart_status_rx_full : 0;
		res |= (status & STATUS_TS) ? Uart_status_tx_empty : 0;
	}
	return res;
}

// return error code (<0), or written bytes number (0 or 1)
inline int uart_putc(unsigned long base_addr, int c)
{
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	if (control & CTRL_FA)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		if (mem_rd(base_addr + REG_STATUS) & STATUS_TS) // is TX shift register empty ?
		{
			mem_wr(base_addr + REG_DATA, c);
			return 1;
		}
		return 0;
	}
}

// return error code (<0) or written bytes number
inline int uart_put_buf(unsigned long base_addr, const char* buf, unsigned sz)
{
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	if (control & CTRL_FA)
	{
		return -1; // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		unsigned written = 0;
		while (mem_rd(base_addr + REG_STATUS) & STATUS_TS  &&  written < sz) // is TX shift register empty ?
		{
			mem_wr(base_addr + REG_DATA, buf[written++]);
		}
		return written;
	}
}

inline int uart_getc(unsigned long base_addr)
{
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	if (control & CTRL_FA)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		if (mem_rd(base_addr + REG_STATUS) & STATUS_DR)  // is data ready ?
			return mem_rd(base_addr + REG_DATA);
		return 0;
	}
}

inline int uart_get_buf(unsigned long base_addr, char* buf, unsigned sz)
{
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	if (!sz)
		return 0;
	unsigned long control = mem_rd(base_addr + REG_CTRL);
	if (control & CTRL_FA)
	{
		return -1;  // TODO:  fifo available but this driver doesn't use it
	}
	else
	{
		unsigned read = 0;
		while (mem_rd(base_addr + REG_STATUS) & STATUS_DR  &&  read < sz)  // is data ready ?
		{
			buf[read++] = mem_rd(base_addr + REG_DATA);
		}
		return read;
	}
}

#endif // APB_UART_H
