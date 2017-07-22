//##################################################################################################
//
//  Low-level driver for Aeroflex Gaisler general purpose timer.
//
//##################################################################################################

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#ifdef Cfg_debug
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

typedef struct
{
	uint32_t scaler;         // 0x00 scaler value
	uint32_t scaler_reload;  // 0x04 scaler reload value
	uint32_t config;         // 0x08 configuration register
	uint32_t latch_config;   // 0x0C timer latch configuration register
	struct
	{
		uint32_t counter;    // 0x10 timer N counter value register
		uint32_t reload;     // 0x14 timer N reload value register
		uint32_t control;    // 0x18 timer N control register
		uint32_t latch;      // 0x1C timer N latch register
	} timer[];
} Timer_regs_t;

enum
{
	Scaller_mask        = 0xffff,
	Scaller_reload_mask = 0xffff,

	Config_ev           = 13,  // external events
	Config_es           = 12,  // enable set
	Config_el           = 11,  // enable latching
	Config_ee           = 10,  // enable external clock source
	Config_df           =  9,  // disable timer freeze
	Config_si           =  8,  // separate interrupt for each timer, read-only
	Config_irq_mask     = 0x1f,// IRQ num for common int, IRQ+n for separate int, read-only
	Config_irq_shift    =  3,  //
	Config_timers_mask  = 0x7, // number of implemented timers, read-only
	Config_timers_shift =  0,  //

	Control_ws          =  8,  // disable watchdog output
	Control_wn          =  7,  // enable watchdog NMI
	Control_dh          =  6,  // debug halt, read-only
	Control_ch          =  5,  // chain with preceding timer
	Control_ip          =  4,  // interrupt pending
	Control_ie          =  3,  // interrupt enable
	Control_ld          =  2,  // load from Reload to Сounter register
	Control_rs          =  1,  // restart, when underflow - reload Reload to Counter register
	Control_en          =  0,  // enable the timer

	Ktmr                =  0,  // number of timer for kernel purpose
};

typedef void (*Timer_print_t)(const char* format, ...);

static inline unsigned irq_start(uintptr_t base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	return (regs->config >> Config_irq_shift) & Config_irq_mask;
}

static inline unsigned num_timers(uintptr_t base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	return (regs->config >> Config_timers_shift) & Config_timers_mask;
}

inline void timer_init(uintptr_t base_addr, Timer_print_t dprint)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;

	print("entry:  base_addr=0x%x.\n", base_addr);

	// disable all timers
	unsigned timers = num_timers(base_addr);
	for (unsigned i=0; i<timers; ++i)
		regs->timer[i].control = 0x0;
	
	print("Sparc GPTIMER:  %d timers, %s IRQ #%d.\n",
		timers, regs->config & Config_si ? "separate" : "shared", irq_start(base_addr));
}

/*
inline void timer_set(unsigned base_addr, unsigned ticks, unsigned short scaler)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->scaler = scaler;
	regs->scaler_reload = scaler;
	regs->timer[Ktmr].counter = ticks;
	regs->timer[Ktmr].reload = ticks;
	regs->timer[Ktmr].control = (1<<Control_rs) | (1<<Control_ie) | (1<<Control_ld) | (1<<Control_ip);
}
*/

inline int timer_set(uintptr_t base_addr, unsigned sysclock_hz, unsigned period_usec)
{
	enum { Usec_per_sec = 1000*1000 };
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	unsigned ticks = period_usec;
	unsigned scaler = sysclock_hz / Usec_per_sec; // preload 1 usec
	regs->scaler = scaler - 1;
	regs->scaler_reload = scaler - 1;
	regs->timer[Ktmr].counter = ticks - 1;
	regs->timer[Ktmr].reload = ticks - 1;
	regs->timer[Ktmr].control = (1<<Control_rs) | (1<<Control_ie) | (1<<Control_ld) | (1<<Control_ip);
	return 0;
}

inline void timer_start(uintptr_t base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->timer[Ktmr].control |= 1 << Control_en;
}

inline void timer_stop(uintptr_t base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->timer[Ktmr].control &= ~(1 << Control_en);
}

inline uint64_t timer_usec_from_raw_value(uintptr_t base_addr, unsigned sysclock_hz, unsigned reload_usec, unsigned value_reg)
{
	(void) reload_usec;  // use reload value from timer register
	enum { Usec_per_sec = 1000*1000 };
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	unsigned value  = value_reg + 1;
	unsigned reload = regs->timer[Ktmr].reload + 1;
	unsigned scaler = regs->scaler_reload + 1;
	return ((uint64_t)reload - value) * scaler * Usec_per_sec / sysclock_hz;
}

inline uint64_t timer_value_usec(uintptr_t base_addr, unsigned sysclock_hz, unsigned reload_usec)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	return timer_usec_from_raw_value(base_addr, sysclock_hz, reload_usec, regs->timer[Ktmr].counter);
}

inline void timer_irq_ack(uintptr_t base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	(void) regs;
	//timer_dump();
	//regs->timer[Ktmr].control |= 1 << Control_ip; // XXX:  why not need?
	//timer_dump();
}

// TODO support separate timer IRQs
inline unsigned timer_irq_num(uintptr_t base_addr)
{
	return irq_start(base_addr);
}

inline void timer_dump(uintptr_t base_addr, Timer_print_t dprint)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;

	print("Timer:   scaler=%d, scaler_reload=%d, config=0x%x, latch_config=0x%x\n",
		regs->scaler, regs->scaler_reload, regs->config, regs->latch_config);

	print("Timers:  ctrl:  en rs ld ie ip ch dh wn ws\n");

	unsigned timers = num_timers(base_addr);
	for (unsigned i=0; i<timers; ++i)
	{
		// skip unused timers
		if (i != Ktmr)
			continue;

		uint32_t ctrl = regs->timer[i].control;
		(void)ctrl;
	   	print("    #%d           %d  %d  %d  %d  %d  %d  %d  %d  %d  rld=%u, cnt=%u, irq=%u\n", i,
			(ctrl >> Control_en) & 1, (ctrl >> Control_rs) & 1, (ctrl >> Control_ld) & 1,
			(ctrl >> Control_ie) & 1, (ctrl >> Control_ip) & 1, (ctrl >> Control_ch) & 1,
			(ctrl >> Control_dh) & 1, (ctrl >> Control_wn) & 1, (ctrl >> Control_ws) & 1,
			regs->timer[i].reload, regs->timer[i].counter, irq_start(base_addr) + i);
	}
}

#endif // TIMER_H
