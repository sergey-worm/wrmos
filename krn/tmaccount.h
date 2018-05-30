//##################################################################################################
//
//  Time account - account time spans for profile kernel and applications.
//
//##################################################################################################

#ifndef TIME_ACCOUNT_H
#define TIME_ACCOUNT_H

#include "wlibc_assert.h"

class Time_account_t
{
	// timepoints
	enum
	{
		// thread's live cycle
		Tp_resume,                  // resume thread point
		Tp_kexit_start,             // last kernel C-code
		Tp_kexit_end,               // last kernel ASM-code, next will user code
		Tp_kentry_start,            // first kernel ASM-code
		Tp_kentry_end,              // first kernel C-code
		Tp_suspend,                 // suspend thread point
		// profiler points          
		Tp_kwork_1s,                // start of span kwork1
		Tp_kwork_2s,                // start of span kwork2
		Tp_kwork_3s,                // start of span kwork3
		// array size
		Tp_max
	};

	// timespans
	enum
	{
		// thread's live cycle
		Ts_execution,               // whole execution timespan
		Ts_kentry,                  // ASM entry code timespan
		Ts_kwork,                   // kernel work timespan
		Ts_kexit,                   // ASM exit code timespan
		// profiler intervals
		Ts_kwork_1,
		Ts_kwork_2,
		Ts_kwork_3,
		// array size
		Ts_max
	};

	const char* thr_name;           // thread name
	L4_clock_t  points[Tp_max];     // profiler points
	L4_clock_t  spans[Ts_max];      // profiler timespans
	unsigned    prev_point;         //

public:

	Time_account_t(const char* n) : thr_name(n), prev_point(Tp_suspend)
	{
		for (int i=0; i<Tp_max; ++i)
			points[i] = 0;
		for (int i=0; i<Ts_max; ++i)
			spans[i] = 0;
	}

	L4_clock_t tmspan_exec()     const { return spans[Ts_execution]; }
	L4_clock_t tmspan_uexec()    const { return tmspan_exec() - tmspan_kexec(); }
	L4_clock_t tmspan_kexec()    const { return tmspan_kentry() + tmspan_kwork() + tmspan_kexit(); }
	L4_clock_t tmspan_kentry()   const { return spans[Ts_kentry]; }
	L4_clock_t tmspan_kwork()    const { return spans[Ts_kwork]; }
	L4_clock_t tmspan_kexit()    const { return spans[Ts_kexit]; }
	L4_clock_t tmspan_kwork1()   const { return spans[Ts_kwork_1]; }
	L4_clock_t tmspan_kwork2()   const { return spans[Ts_kwork_2]; }
	L4_clock_t tmspan_kwork3()   const { return spans[Ts_kwork_3]; }
	L4_clock_t tmpoint_suspend() const { return points[Tp_suspend]; }

	void tmevent_tick(L4_clock_t now)
	{
		(void)now;
	}

	const char* name(int point)
	{
		switch (point)
		{
			case Tp_resume:            return "resume";
			case Tp_kexit_start:       return "kexit_start";
			case Tp_kexit_end:         return "kexit_end";
			case Tp_kentry_start:      return "kentry_start";
			case Tp_kentry_end:        return "kentry_end";
			case Tp_suspend:           return "suspend";
		};
		return "__unknown_tp_name__";
	}

	// point #1 - start new accounting period
	void tmevent_resume(L4_clock_t now)
	{
		//printf("%s:  %s:  prev_point=%d/%s.\n", __func__, thr_name, prev_point, name(prev_point));
		wassert(prev_point == Tp_suspend);
		prev_point = Tp_resume;
		points[prev_point] = now;
	}

	// point #2 - update whole-execution and kernel-work timespans
	void tmevent_kexit_start(L4_clock_t now)
	{
		//printf("%s:  %s:  prev_point=%d/%s.\n", __func__, thr_name, prev_point, name(prev_point));
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume  ||  prev_point == Tp_kexit_start);
		spans[Ts_execution] += now - points[prev_point];
		spans[Ts_kwork]     += now - points[prev_point];
		prev_point = Tp_kexit_start;
		points[prev_point] = now;
	}

	// point #3 - update whole-execution and account ASM-code timespans
	void tmevent_kentry_end(L4_clock_t now)
	{
		//printf("%s:  %s:  prev_point=%d/%s.\n", __func__, thr_name, prev_point, name(prev_point));
		wassert(prev_point == Tp_kexit_start  ||  prev_point == Tp_resume  ||  prev_point == Tp_kentry_end);

		// account whole execution timespan
		spans[Ts_execution] += now - points[prev_point];
		prev_point = Tp_kentry_end;
		points[prev_point] = now;

		// account kexit timespan
		// NOTE 1:  first kentry doesn't have point Tp_kexit_start
		// NOTE 2:  in qemu timer.value=0 hangs some time --> may be (start == exit == xxx9999)
		extern int cur_kexit_end;
		if (cur_kexit_end != -1  && points[Tp_kexit_start])
		{
			L4_clock_t end = Timer::usec_from_raw_value(cur_kexit_end);
			end += points[Tp_kexit_start] / Cfg_krn_tick_usec * Cfg_krn_tick_usec;
			end += end < points[Tp_kexit_start]  ?  Cfg_krn_tick_usec  :  0;
			wassert(end >= points[Tp_kexit_start]);
			//printf("s=%llu,  e=%llu.\n", points[Tp_kexit_start], end);
			wassert(end - points[Tp_kexit_start] < 500); // for qemu timespan may be big (<500)
			points[Tp_kexit_end] = end;
			spans[Ts_kexit] += points[Tp_kexit_end] - points[Tp_kexit_start];
		}

		// account kentry timespan
		extern int cur_kentry_start;
		if (cur_kentry_start != -1) // skip first
		{
			L4_clock_t start = Timer::usec_from_raw_value(cur_kentry_start);
			start += points[Tp_kentry_end] / Cfg_krn_tick_usec * Cfg_krn_tick_usec;
			start -= ((points[Tp_kentry_end] - start) > Cfg_krn_tick_usec)  ?  Cfg_krn_tick_usec  :  0;
			wassert((points[Tp_kentry_end] - start) < Cfg_krn_tick_usec);
			wassert((points[Tp_kentry_end] - start) < 500); // for qemu timespan may be big (<500)
			points[Tp_kentry_start] = start;
			spans[Ts_kentry] += points[Tp_kentry_end] - points[Tp_kentry_start];
		}
	}

	// point #4 - end of accounting period, update whole-execution and kernel-work timespans
	void tmevent_suspend(L4_clock_t now)
	{
		//printf("%s:  %s:  prev_point=%d/%s.\n", __func__, thr_name, prev_point, name(prev_point));
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume);
		spans[Ts_execution] += now - points[prev_point];
		spans[Ts_kwork]     += now - points[prev_point];
		prev_point = Tp_suspend;
		points[prev_point] = now;
	}

	// start of profiler span 1
	void tmevent_kwork_1s(L4_clock_t now)
	{
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume);
		points[Tp_kwork_1s] = now;
	}

	// end of profiler span 1
	void tmevent_kwork_1e(L4_clock_t now)
	{
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume);
		spans[Ts_kwork_1] += now - points[Tp_kwork_1s];
	}

	// start of profiler span 2
	void tmevent_kwork_2s(L4_clock_t now)
	{
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume);
		points[Tp_kwork_2s] = now;
	}

	// end of profiler span 2
	void tmevent_kwork_2e(L4_clock_t now)
	{
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume);
		spans[Ts_kwork_2] += now - points[Tp_kwork_2s];
	}

	// start of profiler span 3
	void tmevent_kwork_3s(L4_clock_t now)
	{
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume);
		points[Tp_kwork_3s] = now;
	}

	// end of profiler span 3
	void tmevent_kwork_3e(L4_clock_t now)
	{
		wassert(prev_point == Tp_kentry_end  ||  prev_point == Tp_resume);
		spans[Ts_kwork_3] += now - points[Tp_kwork_3s];
	}
};

#endif // TIME_ACCOUNT_H
