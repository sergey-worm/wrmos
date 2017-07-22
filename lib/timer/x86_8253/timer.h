//##################################################################################################
//
//  PIT 8253 - low level driver for 8253 Programmable Interval Timer.
//
//##################################################################################################

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "processor.h"

#ifdef Cfg_debug
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

enum
{
	Sys_clock_hz = 1193182,
	Usec_per_sec = 1000 * 1000,

	Timer_io_ch0  = 0x40,  // channel 0 data port (read/write), used fo OS ticks
	Timer_io_ch1  = 0x41,  // channel 1 data port (read/write), used for DMA
	Timer_io_ch2  = 0x42,  // channel 2 data port (read/write), used for PC speaker
	Timer_io_ctrl = 0x43,  // Mode/Command register (write only, a read is ignored)

	Ctrl_offset_counter  = 6,
	Ctrl_offset_rw       = 4,
	Ctrl_offset_mode     = 1,

	Ctrl_cntr0           = 0,
	Ctrl_cntr1           = 1,
	Ctrl_cntr2           = 2,

	Ctrl_rw_latch        = 0,
	Ctrl_rw_lsb          = 1,
	Ctrl_rw_msb          = 2,
	Ctrl_rw_lsb_then_msb = 3,

	Ctrl_mode_interrupt  = 0,
	Ctrl_mode_one_short  = 1,
	Ctrl_mode_rate_gen   = 2,
	Ctrl_mode_square_gen = 3,
	Ctrl_mode_sw_strobe  = 4,
	Ctrl_mode_hw_strobe  = 5
};

inline uint8_t _timer_ctrl(uint8_t counter, uint8_t read_write, uint8_t mode)
{
	return (counter << Ctrl_offset_counter) | (read_write << Ctrl_offset_rw) | (mode << Ctrl_offset_mode);
}

inline uint16_t _timer_value()
{
	Proc::outb(Timer_io_ctrl, _timer_ctrl(Ctrl_cntr0, Ctrl_rw_latch, 0));
	uint8_t lsb = Proc::inb(Timer_io_ch0);
	uint8_t msb = Proc::inb(Timer_io_ch0);
	return ((uint16_t)msb << 8) | lsb;
}

inline void _timer_value(uint16_t val)
{
	Proc::outb(Timer_io_ctrl, _timer_ctrl(Ctrl_cntr0, Ctrl_rw_lsb_then_msb, Ctrl_mode_rate_gen));
	Proc::outb(Timer_io_ch0, val & 0xff);
	Proc::outb(Timer_io_ch0, val >> 8);
}

typedef void (*Timer_print_t)(const char* format, ...);

inline void timer_init(uintptr_t base_addr, Timer_print_t dprint)
{
	// nothing
	(void) base_addr;
	(void) dprint;
}

inline int timer_set(uintptr_t base_addr, unsigned sysclock_hz, unsigned period_usec)
{
	(void) base_addr;

	if (sysclock_hz != Sys_clock_hz)
		return 1;

	uint64_t val = (uint64_t)period_usec * Sys_clock_hz / Usec_per_sec;
	if (val > 0xffff)
		return 2;  // too big timer value

	_timer_value(val);
	return 0;
}

inline void timer_start(uintptr_t base_addr)
{
	(void) base_addr;
	// nothing
}

inline void timer_stop(uintptr_t base_addr)
{
	(void) base_addr;
	// nothing
}

inline uint64_t timer_usec_from_raw_value(uintptr_t base_addr, unsigned sysclock_hz, unsigned reload_usec, unsigned value_reg)
{
	(void) base_addr;
	(void) sysclock_hz;

	uint64_t val = value_reg;
	uint64_t reload_val = (uint64_t)reload_usec * Sys_clock_hz / Usec_per_sec;
	return (reload_val - val) * Usec_per_sec / Sys_clock_hz;
}

inline uint64_t timer_value_usec(uintptr_t base_addr, unsigned sysclock_hz, unsigned reload_usec)
{
	return timer_usec_from_raw_value(base_addr, sysclock_hz, reload_usec, _timer_value());
}


inline void timer_irq_ack(uintptr_t base_addr)
{
	(void) base_addr;
	// nothing
}

inline void timer_dump(uintptr_t base_addr, Timer_print_t dprint)
{
	(void) base_addr;
	(void) dprint;
	print("Timer:   PIT 8253, freq=%u Hz.\n", Sys_clock_hz);
}

#endif // TIMER_H
