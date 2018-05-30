//##################################################################################################
//
//  System Clock - count time from system startup.
//
//##################################################################################################

#ifndef SYS_CLOCK_T
#define SYS_CLOCK_T

#include "l4_types.h"
#include "krn-config.h"
#include "ktimer.h"
#include "kintc.h"
#include "kkip.h"
#include "wlibc_panic.h"
#include <stdio.h>

#ifdef DEBUG
#  define DBG
#endif

class SystemClock_t
{
	static L4_clock_t _sys_clock;   // value is updated every system's timer tick
	static L4_kip_t*  _kip;         // kip pointer
	static int        _inside_kdb;  // inside kdb flag, don't read timer value if 1, use prev value
	static int        _not_check;   // don't check how long CPU in krn mode, it is used while system startup

public:

	static void init()
	{
		_kip = get_kip();
		_not_check = 1;
	}

	static inline void tick()
	{
		_sys_clock += Cfg_krn_tick_usec;
		//printf("new tick=%llu.\n", _sys_clock);
	}

	static void inside_kdb(int v)
	{
		_inside_kdb = v;
	}

	static void set_check(int v)
	{
		_not_check = !v;
	}

	// NOTE:  inside irq handler pending bit is 0, to allow use sys_clock() inside
	//        timer irq handler befor tick() need to use is_tick_irq_pending = 1
	static L4_clock_t sys_clock(const char* place = 0, int is_tick_irq_pending = 0)
	{
		#ifdef DBG
		int pnd[4];       // for debug
		uint64_t val[4];  // for debug
		pnd[0] = pnd[1] = pnd[2] = 0;
		val[0] = val[1] = val[2] = 0;
		#endif

		static L4_clock_t prev = 0;

		if (_inside_kdb)
			return prev;

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
				#ifdef DBG
				pnd[0] = Intc::is_pending(Cfg_krn_timer_irq);
				val[0] = Timer::value_usec();
				#endif
				uint64_t tmrval = 0;
				if (Intc::is_pending(Cfg_krn_timer_irq))
				{
					tmrval = Cfg_krn_tick_usec + Timer::value_usec();
				}
				else
				{
					tmrval = Timer::value_usec();
					#ifdef DBG
					pnd[1] = Intc::is_pending(Cfg_krn_timer_irq);
					val[1] = Timer::value_usec();
					#endif
					if (Intc::is_pending(Cfg_krn_timer_irq))
						tmrval = Cfg_krn_tick_usec + Timer::value_usec();
				}
				res = _sys_clock + tmrval;
			}

			(void) place;

			#ifdef DEBUG
			if (!_not_check)
			{

				// for QEMU sometimes may be lag between timer-event and intc-event,
				// it is lag between timer tick and timer IRQ,
				// to take this feature into account there is parameter
				// Cfg_krn_timer_lag_usec:  if timer value < Lag --> add tick,
				// tick-irq will process a little late
				#if (Cfg_krn_timer_lag_usec > 0)
				if (res < prev  &&  Timer::value_usec() < Cfg_krn_timer_lag_usec)
					res = _sys_clock + Cfg_krn_tick_usec + Timer::value_usec();
				#endif
    
				if (res < prev)
				{
					#ifdef DBG
					pnd[2] = Intc::is_pending(Cfg_krn_timer_irq);
					val[2] = Timer::value_usec();
					#endif
					printf("ERROR:  SysClock failed, too long in kernel mode (%s,pend=%d):\n",
						place ? place : "---", is_tick_irq_pending);
					printf("ERROR:  prev:            %llu\n", prev);
					printf("ERROR:  now:             %llu\n", res);
					printf("ERROR:  sys_clock_base:  %llu\n", _sys_clock);
					printf("ERROR:  allow timer lag: %u\n", Cfg_krn_timer_lag_usec);
					#ifdef DBG
					pnd[3] = Intc::is_pending(Cfg_krn_timer_irq);
					val[3] = Timer::value_usec();
					uint64_t* v = val;
					printf("ERROR:  tmr_value_usec:  %llu %llu %llu %llu\n", v[0], v[1], v[2], v[3]);
					printf("ERROR:  irq_pending:     %d %d %d %d\n", pnd[0], pnd[1], pnd[2], pnd[3]);
					#endif

					void dprint_tmr(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
					Timer::dump(dprint_tmr);

					panic("too long in kernel mode");
				}
			}
			#endif // DEBUG

			prev = res;

			#if 0 // debug
			//printf("raw=%llu.\n", Timer::raw_value());
			//printf("new prev=%llu (%s,fpend=%d,pend=%d), raw=%llu.\n", prev, place ? place : "---",
			//	is_tick_irq_pending, Intc::is_pending(Cfg_krn_timer_irq), Timer::raw_value());
			//void dprint_tmr(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
			//Timer::dump(dprint_tmr);
			#endif
		}

		// update kip.uptime
		if (_kip)
			l4_kip_set_uptime_usec(_kip, _sys_clock);

		//printf("%s:  base=%llu, return %llu, pend=%d.\n", __func__,
		//	_sys_clock, res, Intc::inited() ? Intc::is_pending(Cfg_krn_timer_irq) : -1);

		return res;
	}
};

#endif // SYS_CLOCK_T
