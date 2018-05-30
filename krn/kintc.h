//##################################################################################################
//
//  Intc - kernel abstraction for interrupt controller.
//
//##################################################################################################

#ifndef KINTCTRL_H
#define KINTCTRL_H

#include "krn-config.h"
#if Cfg_max_cpus > 1
#  define SMP
#endif
#include "intc.h"
#include "wlibc_assert.h"
#include "sys_types.h"

class Intc
{
	static addr_t _addr;  // device base address

public:

	typedef void (*Print_t)(const char* format, ...) __attribute__((format(printf, 1, 2)));

	static inline void init(addr_t base_addr, Print_t dprint = 0)
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
		wassert(_addr != -1);
		return intc_dump(_addr, dprint);
	}

	static inline bool is_pending(unsigned irq)
	{
		wassert(_addr != -1);
		return intc_is_pending(_addr, irq);
	}

	#ifdef Cfg_arch_sparc
	static inline unsigned real_irq(unsigned irq)
	{
		wassert(_addr != -1);
		return intc_real_irq(_addr, irq);
	}
	#endif

	#ifdef Cfg_arch_arm
	static inline unsigned irq()
	{
		wassert(_addr != -1);
		return intc_irq(_addr);
	}
	#endif

	static inline void eoi(unsigned irq)
	{
		wassert(_addr != -1);
		int rc = intc_eoi(_addr, irq);
		(void) rc;
		wassert(!rc && "intc_eoi() failed");
	}

	static inline void mask(unsigned irq)
	{
		wassert(_addr != -1);
		int rc = intc_mask(_addr, irq);
		(void) rc;
		wassert(!rc && "intc_mask() failed");
	}

	static inline void unmask(unsigned irq)
	{
		wassert(_addr != -1);
		int rc = intc_unmask(_addr, irq);
		(void) rc;
		wassert(!rc && "intc_unmask() failed");
	}

	static inline void clear(unsigned irq)
	{
		wassert(_addr != -1);
		int rc = intc_clear(_addr, irq);
		(void) rc;
		wassert(!rc && "intc_clear() failed");
	}

	static inline unsigned ncpu()
	{
		wassert(_addr != -1);
		return intc_ncpu(_addr);
	}
};

#endif // KINTCTRL_H
