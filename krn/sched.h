//##################################################################################################
//
//  Kernel scheduler.
//
//##################################################################################################

#ifndef SCHED_T
#define SCHED_T

#include "thread.h"

Thread_t* threads_get_highest_prio_ready_thread();

//extern unsigned irq_entry_depth;

/*
inline void _preemtion_point()
{
	if (irq_entry_depth < 2)
	{
		//Proc::enable_irq();
		//Proc::disable_irq();
	}
}
*/

class Sched_t
{
	static Thread_t* _current;

private:

	// get next Ready thread
	static Thread_t* next()
	{
		//_preemtion_point();
		return threads_get_highest_prio_ready_thread();
	}

public:

	static Thread_t* current()
	{
		return _current;
	}

	// TODO:  private
	static void current(Thread_t* thr)
	{
		_current = thr;
	}

	static void switch_to(Thread_t* nxt)
	{
		Thread_t* cur = current();
		if (cur != nxt)
		{
			//printk("%s:  nxt=%s, ra=0x%x.\n", __func__, nxt->name(), ((word_t*)nxt->ksp())[0]);
			cur->store_floats();
			current(nxt);
			cur->context_switch(nxt); // after that 'cur' is changed to 'nxt'
			cur->restore_floats();
		}
	}

	static void switch_to_next()
	{
		Thread_t* nxt = Sched_t::next();
		switch_to(nxt);
	}
};

#endif // SCHED_T
