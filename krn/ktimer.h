//##################################################################################################
//
//  Timer - kernel abstraction for timer.
//
//##################################################################################################

#ifndef KTIMER_H
#define KTIMER_H

#include "timer.h"
#include "wlibc_assert.h"

class Timer
{
	static uintptr_t _addr;         // device base address
	static unsigned  _sysclock_hz;  // system clock
	static unsigned  _period_usec;  // current reload value

public:

	typedef void (*Print_t)(const char* format, ...);

	static inline void init(uintptr_t base_addr, unsigned sysclock_hz, Print_t dprint = 0)
	{
		_addr = base_addr;
		_sysclock_hz = sysclock_hz;
		timer_init(_addr, dprint);
	}

	static inline bool inited()
	{
		return _addr != -1;
	}

	static void dump(Print_t dprint)
	{
		wassert(_addr != -1);
		timer_dump(_addr, dprint);
	}

	static inline int set(unsigned period_usec)
	{
		wassert(_addr != -1);
		_period_usec = period_usec;
		return timer_set(_addr, _sysclock_hz, period_usec);
	}

	static inline void start()
	{
		wassert(_addr != -1);
		timer_start(_addr);
	}

	static inline void stop()
	{
		wassert(_addr != -1);
		timer_stop(_addr);
	}

	static inline uint64_t value_usec()
	{
		wassert(_addr != -1);
		return timer_value_usec(_addr, _sysclock_hz, _period_usec);
	}

	static inline unsigned raw_value()
	{
		wassert(_addr != -1);
		return timer_raw_value(_addr);
	}

	static inline uint64_t usec_from_raw_value(unsigned val)
	{
		wassert(_addr != -1);
		return timer_usec_from_raw_value(_addr, _sysclock_hz, _period_usec, val);
	}

	static inline void irq_ack()
	{
		wassert(_addr != -1);
		return timer_irq_ack(_addr);
	}
};

#endif // KTIMER_H
