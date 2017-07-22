//##################################################################################################
//
//  System Clock - count time from system startup.
//
//##################################################################################################

#ifndef SYS_CLOCK_T
#define SYS_CLOCK_T

#include "l4types.h"
#include "krn-config.h"
#include "ktimer.h"
#include "kintc.h"

class SystemClock_t
{
	static L4_clock_t _sys_clock;       // updated every system's timer tick

public:

	static void init() {}

	static inline void tick() { _sys_clock += Cfg_krn_tick_usec; /*printf("new tick=%llu.\n", _sys_clock);~*/ }

	// NOTE:  inside irq handler pending bit is 0, to allow use sys_clock() inside
	//        timer irq handler befor tick() need to use is_tick_irq_pending = 1
	static L4_clock_t sys_clock(const char* place = 0, int is_tick_irq_pending = 0)
	{
		int pnd[3];       // for debug
		uint64_t val[3];  // for debug
		pnd[0] = pnd[1] = pnd[2] = 0;
		val[0] = val[1] = val[2] = 0;
		uint64_t res = 0;
		if (Timer::inited() && Intc::inited())
		{
			if (is_tick_irq_pending)
			{
				// timer irq is processing but tick() is not called yet
				res = _sys_clock + Cfg_krn_tick_usec + Timer::value_usec();
			}
			else
			{
				pnd[0] = Intc::is_pending(Cfg_krn_timer_irq);
				val[0] = Timer::value_usec();
				if (Intc::is_pending(Cfg_krn_timer_irq))
				{
					res = _sys_clock + Cfg_krn_tick_usec + Timer::value_usec();
				}
				else
				{
					res = _sys_clock + Timer::value_usec();
					pnd[1] = Intc::is_pending(Cfg_krn_timer_irq);
					val[1] = Timer::value_usec();
					if (Intc::is_pending(Cfg_krn_timer_irq))
						res = _sys_clock + Cfg_krn_tick_usec + Timer::value_usec();
				}
			}
			static L4_clock_t prev = 0;

			//printf("irq_pending:   %d %d %d:  %d:  %s\n", pnd[0], pnd[1], pnd[2],
			//	Intc::is_pending(Cfg_krn_timer_irq), place?place:"---");

			#if defined(Cfg_arch_x86) or defined(Cfg_arch_x86_64)
			// WA:  for x86-qemu PIC may receive IRQ from PIT with lag ~100 usec
			if (res < prev  &&  Timer::value_usec() < 100)
				res = _sys_clock + Cfg_krn_tick_usec + Timer::value_usec();
			#endif

			if (0   &&   res < prev)
			{
				pnd[2] = Intc::is_pending(Cfg_krn_timer_irq);
				val[2] = Timer::value_usec();
				printf("ERROR:  SystemClock failed, too long in kernel mode (%s):\n", place ? place : "---");
				printf("ERROR:  prev:            %llu\n", prev);
				printf("ERROR:  res:             %llu\n", res);
				printf("ERROR:  sys_clock_base:  %llu\n", _sys_clock);
				printf("ERROR:  tmr_value_usec:  %llu %llu %llu\n", val[0], val[1], val[2]);
				printf("ERROR:  irq_pending:     %d %d %d\n", pnd[0], pnd[1], pnd[2]);
				assert(0 && "too long in kernel mode");
			}
			prev = res;
		}
		return res;
	}
};

#endif // SYS_CLOCK_T
