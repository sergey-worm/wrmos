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
	Cfg_uart_use_grmon_u_key  = 0,               // set 1 if use grmon with -u param
};

enum
{
	REG_DATA   = 0x00,
	REG_STATUS = 0x04,
	REG_CTRL   = 0x08,
	REG_SCALER = 0x0C,

	DATA_MASK  = 0x0F,

	STATUS_DR  = 0x001,   // data ready
	STATUS_TS  = 0x002,   // transmit shift empty
	STATUS_TE  = 0x004,   // transmit fifo empty
	STATUS_BR  = 0x008,   // BREAK received
	STATUS_OV  = 0x010,   // overrun
	STATUS_PE  = 0x020,   // parity error
	STATUS_FE  = 0x040,   // framing error
	STATUS_TH  = 0x080,   // transmit fifo half-full
	STATUS_RH  = 0x100,   // recv fifo half-full
	STATUS_TF  = 0x200,   // transmit fifo full
	STATUS_RF  = 0x400,   // recv fifo full

	STATUS_TCNT_MASK  = 0x3F,
	STATUS_TCNT_SHIFT = 20,
	STATUS_RCNT_MASK  = 0x3F,
	STATUS_RCNT_SHIFT = 26,

	CTRL_RE    = 0x001,
	CTRL_TE    = 0x002,
	CTRL_RI    = 0x004,
	CTRL_TI    = 0x008,
	CTRL_PS    = 0x010,
	CTRL_PE    = 0x020,
	CTRL_FL    = 0x040,
	CTRL_LB    = 0x080,
	CTRL_EC    = 0x100,
	CTRL_TF    = 0x200,
	CTRL_RF    = 0x400,

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

inline void uart_init(unsigned base_addr, unsigned baudrate, unsigned sys_clock_hz)
{
	//  volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	if (!Cfg_uart_use_grmon_u_key)
	{
		// GRIP says:  scaler_value = bus_clock / (8 * baudrate) - 1
		mem_wr(base_addr + REG_SCALER, sys_clock_hz / (baudrate * 8) - 1);
		mem_wr(base_addr + REG_CTRL, (CTRL_RE | CTRL_TE));
		mem_wr(base_addr + REG_STATUS, 0x0);
	}
}

inline void uart_putc(unsigned base_addr, int c)
{
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	while (mem_rd(base_addr + REG_STATUS) & STATUS_TF);
	mem_wr(base_addr + REG_DATA, c);
}

inline int uart_getc(unsigned base_addr)
{
	// volatile Apbuart_regs_t* regs = (Apbuart_regs_t*)base_addr;
	return mem_rd(base_addr + REG_DATA);
}

#endif // APB_UART_H
