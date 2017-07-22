//##################################################################################################
//
//  Spinlock - spinlock implementation.
//
//##################################################################################################

#include "wrm_slock.h"
#include "l4api.h"
#include <unistd.h>

// set 0xff to spinlock address and return old value
inline unsigned test_and_set(unsigned* val_ptr)
{
	unsigned old_val;
// FIXME
#ifdef Cfg_arch_sparc
	asm volatile ("ldstub %[mem], %[reg]" :  [mem]"=m"(*val_ptr), [reg]"=r"(old_val));
#else
	old_val = 0;
#endif
	return old_val;
}

extern "C" void wrm_spinlock_init(Wrm_spinlock_t* spinlock)
{
	*spinlock = 0;
}

extern "C" void wrm_spinlock_lock(Wrm_spinlock_t* spinlock)
{
	int cnt = 0;

	while (test_and_set(spinlock))
	{
		if (cnt < 50)
		{
			// allow to free lock by thread with prio >= current
			l4_thread_switch(L4_thrid_t::Nil);
			cnt++;
		}
		else
		{
			// allow to free lock by thread with prio < current
			usleep(2000);  // 2 ms
			cnt = 0;
		}
	}
}

extern "C" void wrm_spinlock_unlock(Wrm_spinlock_t* spinlock)
{
	*spinlock = 0;
}

