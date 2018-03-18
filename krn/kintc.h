//##################################################################################################
//
//  Intc - kernel abstraction for interrupt controller.
//
//##################################################################################################

#ifndef KINTCTRL_H
#define KINTCTRL_H

#include "intc.h"
#include <assert.h>

class Intc
{
	static uintptr_t _addr;  // device base address

public:

	typedef void (*Print_t)(const char* format, ...);

	static inline void init(uintptr_t base_addr, Print_t dprint = 0)
	{
		_addr = base_addr;
		intc_init(_addr, dprint);
	}

	static inline bool inited()
	{
		return _addr != -1;
	}

	static void dump(Print_t dprint)
	{
		assert(_addr != -1);
		return intc_dump(_addr, dprint);
	}

	static inline bool is_pending(unsigned irq)
	{
		assert(_addr != -1);
		return intc_is_pending(_addr, irq);
	}

	#ifdef Cfg_arch_sparc
	static inline unsigned real_irq(unsigned irq)
	{
		assert(_addr != -1);
		return intc_real_irq(_addr, irq);
	}
	#endif

	#ifdef Cfg_arch_arm
	static inline unsigned irq()
	{
		assert(_addr != -1);
		return intc_irq(_addr);
	}
	#endif

	static inline void mask(unsigned irq)
	{
		assert(_addr != -1);
		int rc = intc_mask(_addr, irq);
		(void)rc;
		assert(!rc && "intc_mask() failed");
	}

	static inline void unmask(unsigned irq)
	{
		assert(_addr != -1);
		int rc = intc_unmask(_addr, irq);
		(void)rc;
		assert(!rc && "intc_unmask() failed");
	}

	static inline void clear(unsigned irq)
	{
		assert(_addr != -1);
		int rc = intc_clear(_addr, irq);
		(void)rc;
		assert(!rc && "intc_clear() failed");
	}
};

#endif // KINTCTRL_H
