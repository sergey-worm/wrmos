//##################################################################################################
//
//  Low-level driver for ARM sp804 timer.
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

#define SPACE_SIZE(addr1, addr2) (((addr2) - (addr1)) / sizeof(uint32_t))

typedef struct
{
	struct
	{
		uint32_t load;       // 0x00/0x20  rw  0x00000000  Load Register
		uint32_t value;      // 0x04/0x24  r   0xFFFFFFFF  Current Value Register
		uint32_t control;    // 0x08/0x28  rw        0x20  Control Register
		uint32_t intclr;     // 0x0c/0x2c  w            -  Interrupt Clear Register
		uint32_t ris;        // 0x10/0x30  r          0x0  Raw Interrupt Status Register
		uint32_t mis;        // 0x14/0x34  r          0x0  Masked Interrupt Status Register
		uint32_t bgload;     // 0x18/0x38  rw         0x0  Background Load Register
		uint32_t _space1;    // 0x1c/0x3c  alignment
	} tmr[2];

	uint32_t _space2 [SPACE_SIZE(0x40, 0xf00)];  // 0x40-0xefc  Reserved for future expansio
	uint32_t itcr;           // 0xf00      rw         0x0  Integration Test Control Register
	uint32_t itop;           // 0xf04      w          0x0  Integration Test Output Set Register
	uint32_t _space3 [SPACE_SIZE(0xf08, 0xfe0)]; // alignment
	uint32_t periph_id0;     // 0xfe0      r         0x04  Timer Peripheral ID0 Register
	uint32_t periph_id1;     // 0xfe4      r         0x18  Timer Peripheral ID1 Register
	uint32_t periph_id2;     // 0xfe8      r         0x14  Timer Peripheral ID2 Register
	uint32_t periph_id3;     // 0xfeC      r         0x00  Timer Peripheral ID3 Register
	uint32_t pcell_id0;      // 0xff0      r         0x0D  PrimeCell ID0 Register
	uint32_t pcell_id1;      // 0xff4      r         0xF0  PrimeCell ID1 Register
	uint32_t pcell_id2;      // 0xff8      r         0x05  PrimeCell ID2 Register
	uint32_t pcell_id3;      // 0xffc      r         0xB1  PrimeCell ID3 Register
} Timer_regs_t;

enum
{
	Control_en          =  7,   // (0) disabled, (1) enabled
	Control_mode        =  6,   // (0) free-running, (1) periodic
	Control_int         =  5,   // (0) interrupt disabled, (1) enabled
	Control_pre         =  2,   // prescaller:  (0)->1, (1)->16, (2)->256
	Control_pre_mask    =  0x3, // prescaller:  (0)->1, (1)->16, (2)->256
	Control_size        =  1,   // (0) 16-bit counter, (1) 32-bit counter
	Control_oneshot     =  0,   // (0) wrapping mode, (1) one-shot mode

	Ktmr = 0,
};

typedef void (*Timer_print_t)(const char* format, ...);

static inline unsigned irq_start(unsigned base_addr)
{
	(void)base_addr;
	return -1;
}

static inline unsigned num_timers(unsigned base_addr)
{
	(void)base_addr;
	return -1;
}

inline void timer_init(unsigned base_addr, Timer_print_t dprint)
{
	(void)base_addr;
	(void)dprint;
}

inline int timer_set(unsigned base_addr, unsigned sysclock_hz, unsigned period_usec)
{
	// define prescale and load values, try to use minimal prescaler
	enum
	{
		Pre_min = 0,
		Pre_max = 2,
		Usec_per_sec = 1000*1000
	};
	unsigned div_by_pre[3] = { 1, 16, 256 };
	unsigned load = 0;
	unsigned pre = 0;
	for (pre = Pre_min; pre <= Pre_max; ++pre)
	{
		uint64_t ld = (uint64_t)sysclock_hz * period_usec / div_by_pre[pre] / Usec_per_sec;
		if (ld <= 0xffffffff)
		{
			load = ld;
			break;
		}
	}

	if (!load)
		return 1;

	// set timer params
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->tmr[Ktmr].load = load;
	regs->tmr[Ktmr].control = (0   << Control_en)   |    // don't enable timer now
	                          (1   << Control_mode) |    // pariodic mode
	                          (1   << Control_int)  |    // enable interrupt
	                          (pre << Control_pre)  |    // prescale (1)->16
	                          (1   << Control_size) |    // 32-bit
	                          (0   << Control_oneshot);  // wrapping mode
	return 0;
}

inline void timer_start(unsigned base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->tmr[Ktmr].control |= (1 << Control_en);
	(void)base_addr;
}

inline void timer_stop(unsigned base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->tmr[Ktmr].control &= ~(1 << Control_en);
	(void)base_addr;
}

inline uint64_t timer_usec_from_raw_value(uintptr_t base_addr, unsigned sysclock_hz, unsigned reload_usec, unsigned value_reg)
{
	(void) reload_usec;  // use reload value from timer register
	enum { Usec_per_sec = 1000*1000 };
	unsigned div_by_pre[4] = { 1, 16, 256, 0 };  // only 3 values is valid
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	uint32_t value  = value_reg;
	uint32_t reload = regs->tmr[Ktmr].load;
	uint32_t scaler = div_by_pre[(regs->tmr[Ktmr].control >> Control_pre) & Control_pre_mask];
	return ((uint64_t)reload - value) * scaler * Usec_per_sec / sysclock_hz;
}

inline unsigned timer_raw_value(uintptr_t base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	return regs->tmr[Ktmr].value;
}

inline uint64_t timer_value_usec(uintptr_t base_addr, unsigned sysclock_hz, unsigned reload_usec)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	return timer_usec_from_raw_value(base_addr, sysclock_hz, reload_usec, regs->tmr[Ktmr].value);
}

inline void timer_irq_ack(unsigned base_addr)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;
	regs->tmr[Ktmr].intclr = 1;
}

// TODO support separate timer IRQs
inline unsigned timer_irq_num(unsigned base_addr)
{
	return irq_start(base_addr);
}

inline void timer_dump(unsigned base_addr, Timer_print_t dprint)
{
	volatile Timer_regs_t* regs = (Timer_regs_t*) base_addr;

	print("SP804 timer:\n");
	print("  tmr[0].load:      0x%08x\n", regs->tmr[0].load);
	print("  tmr[0].value:     0x%08x\n", regs->tmr[0].value);
	print("  tmr[0].control:   0x%08x\n", regs->tmr[0].control);
	print("  tmr[0].intclr:    0x%08x\n", regs->tmr[0].intclr);
	print("  tmr[0].ris:       0x%08x\n", regs->tmr[0].ris);
	print("  tmr[0].mis:       0x%08x\n", regs->tmr[0].mis);
	print("  tmr[0].bgload:    0x%08x\n", regs->tmr[0].bgload);
	print("  tmr[1].load:      0x%08x\n", regs->tmr[1].load);
	print("  tmr[1].value:     0x%08x\n", regs->tmr[1].value);
	print("  tmr[1].control:   0x%08x\n", regs->tmr[1].control);
	print("  tmr[1].intclr:    0x%08x\n", regs->tmr[1].intclr);
	print("  tmr[1].ris:       0x%08x\n", regs->tmr[1].ris);
	print("  tmr[1].mis:       0x%08x\n", regs->tmr[1].mis);
	print("  tmr[1].bgload:    0x%08x\n", regs->tmr[1].bgload);
	print("  itcr:             0x%08x\n", regs->itcr);
	print("  itop:             0x%08x\n", regs->itop);
	print("  periph_id0:       0x%08x\n", regs->periph_id0);
	print("  periph_id1:       0x%08x\n", regs->periph_id1);
	print("  periph_id2:       0x%08x\n", regs->periph_id2);
	print("  periph_id3:       0x%08x\n", regs->periph_id3);
	print("  pcell_id0:        0x%08x\n", regs->pcell_id0);
	print("  pcell_id1:        0x%08x\n", regs->pcell_id1);
	print("  pcell_id2:        0x%08x\n", regs->pcell_id2);
	print("  pcell_id3:        0x%08x\n", regs->pcell_id3);
}

#endif // TIMER_H
