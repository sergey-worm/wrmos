//##################################################################################################
//
//  Kcfg - kernel constants.
//
//##################################################################################################

#ifndef KCONFIG_H
#define KCONFIG_H

class Kcfg
{
public:

	enum
	{
		Ints_max        = 128,  // [0, Ints_max)                               - interrupt threads
		Kthreads_max    =   1,  // [Ints_max, Ints_max + Kthreads_max)         - kernel threads
		Thr_num_width   =   8,  // [Ints_max + Kthreads_max, 2^Thr_num_width)  - user threads
		Uthreads_max    = (1 << Thr_num_width) - (Ints_max + Kthreads_max),   // user threads max
		Threads_max     = Kthreads_max + Uthreads_max,

		Kip_thrid_system_base = Ints_max,
		Kip_thrid_user_base   = Ints_max + Kcfg::Kthreads_max,
		Kip_thrid_number_max  = (1 << Thr_num_width) - 1,

		Timeslice_usec  = 50*1000
	};
};

#endif // KCONFIG_H
