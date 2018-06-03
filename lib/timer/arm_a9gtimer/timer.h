//##################################################################################################
//
//  Low-level driver for ARM global timer.
//
//##################################################################################################

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#ifdef DEBUG
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

enum
{
	// control register bits
	Ctrl_prescaler_offset = 8,       // prescaler offset
	Ctrl_prescaler_mask   = 0xff,    // prescaler mask
	Ctrl_auto_inc         = 1 << 3,  // auto-increment enable, per CPU banked
	Ctrl_irq_en           = 1 << 2,  // interrupt enable,      per CPU banked
	Ctrl_comp_en          = 1 << 1,  // comparison enable,     per CPU banked
	Ctrl_tmr_en           = 1 << 0,  // timer enable

	// interrupt statis register bits
	Stat_event            = 1 << 0,  // counter reach comporator -> set 1, write 1 to clear
};

typedef struct
{
	uint32_t counter_lo;             // rw  counter register, lower 32 bits
	uint32_t counter_hi;             // rw  counter register, upper 32 bits
	uint32_t control;                // rw  control register
	uint32_t istatus;                // rw  interrupt status register, per CPU banked
	uint32_t comparator_lo;          // rw  comparator value register, lower 32 bits
	uint32_t comparator_hi;          // rw  comparator value register, upper 32 bits
	uint32_t auto_inc;               // rw  auto-incriment register
} Timer_regs_t;

typedef void (*Timer_print_t)(const char* format, ...) __attribute__((format(printf, 1, 2)));

inline void timer_init(unsigned long base_addr, Timer_print_t dprint)
{
	(void) dprint;
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->control = 0;
}

inline int timer_set(unsigned long base_addr, unsigned sysclock_hz, unsigned period_usec)
{
	sysclock_hz /= 2;  // PERIPHCLK
	enum { Usec_per_sec = 1000*1000 };
	uint64_t value = (uint64_t)sysclock_hz * period_usec / Usec_per_sec;
	if (value > 0xffffffff)
		return 1;  // too big period, this driver implementation support only 32-bit counter

	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->control       = 0;           // disable timer
	regs->counter_lo    = 0;           //
	regs->counter_hi    = 0;           //
	regs->istatus       = Stat_event;  // write to clear
	regs->comparator_lo = value;       // note:  first interval will be more by 1 than period
	regs->comparator_hi = 0;           //
	regs->auto_inc      = value;       // don't need -1, it is period

	return 0;
}

inline void timer_start(unsigned long base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->control = Ctrl_irq_en | Ctrl_comp_en | Ctrl_tmr_en | Ctrl_auto_inc;
}

inline void timer_stop(unsigned long base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->control = 0;
}

inline uint64_t _timer_comparator(unsigned long base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	uint32_t lo = regs->comparator_lo;
	uint32_t hi = regs->comparator_hi;
	if (lo != regs->comparator_lo)
	{
		// lo was updated, re-read
		lo = regs->comparator_lo;
		hi = regs->comparator_hi;
	}
	return ((uint64_t)hi << 32) | lo;
}

inline uint64_t timer_usec_from_raw_value(unsigned long base_addr, unsigned sysclock_hz,
                                          unsigned reload_usec, uint64_t value_reg)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	(void) base_addr;
	(void) reload_usec;
	sysclock_hz /= 2;  // PERIPHCLK
	enum { Usec_per_sec = 1000*1000 };
#if 0
	uint64_t diff = _timer_comparator(base_addr) - value_reg;
	uint64_t remain_usec = diff * Usec_per_sec / sysclock_hz;
	if (remain_usec > reload_usec)
		remain_usec -= reload_usec;
	return reload_usec - remain_usec;
#else
	uint32_t reload_reg = regs->auto_inc;
	uint64_t mod = value_reg % reload_reg;
	uint64_t mod_usec = mod * Usec_per_sec / sysclock_hz;
	return mod_usec;
#endif
}

inline uint64_t timer_raw_value(unsigned long base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	uint32_t hi = regs->counter_hi;
	uint32_t lo = regs->counter_lo;
	if (hi != regs->counter_hi)
	{
		// hi was updated, re-read
		hi = regs->counter_hi;
		lo = regs->counter_lo;
	}
	return ((uint64_t)hi << 32) | lo;
}

// return timer value MOD reload_usec
inline uint64_t timer_value_usec(unsigned long base_addr, unsigned sysclock_hz, unsigned reload_usec)
{
	return timer_usec_from_raw_value(base_addr, sysclock_hz, reload_usec, timer_raw_value(base_addr));
}

inline void timer_irq_ack(unsigned long base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->istatus = Stat_event;  // write 1 to clear
}

inline void timer_dump(unsigned long base_addr, Timer_print_t dprint)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	(void) regs;

	print("Global timer:\n");
	print("  counter_lo:      0x%08x\n", regs->counter_lo);
	print("  counter_hi:      0x%08x\n", regs->counter_hi);
	print("  control:         0x%08x\n", regs->control);
	print("  istatus:         0x%08x\n", regs->istatus);
	print("  comparator_lo:   0x%08x\n", regs->comparator_lo);
	print("  comparator_hi:   0x%08x\n", regs->comparator_hi);
	print("  auto_inc:        0x%08x\n", regs->auto_inc);
}

#endif // TIMER_H
