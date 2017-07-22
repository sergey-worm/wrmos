//##################################################################################################
//
//  Thread - thread implementation.
//
//##################################################################################################

#include "thread.h"
#include "arch.h"

// static data
unsigned Thread_t::_counter = 0;
unsigned Int_thread_t::_counter = 0;


void Thread_t::context_switch(Thread_t* next)
{
	//static unsigned cnt = 0;
	//printk("switch %u:  %s (%s, _ksp=%x, fstk=%u)  -->  %s (%s, _ksp=%x, fstk=%u)\n",
	//	cnt++, name(), state_str(), _ksp, unused_kstack_sz(),
	//	next->name(), next->state_str(), next->_ksp, next->unused_kstack_sz());

	// set kernel entry stack pointer
	arch_set_ksp(next->kentry_sp());

	// check stack overflow
	assert(check_canary());
	assert(next->check_canary());

	// switch aspace
	if (task() != next->task())
		next->task()->set_current();

	// time accounting
	L4_clock_t now = SystemClock_t::sys_clock(__func__);
	tmevent_suspend(now);
	next->tmevent_resume(now);

	arch_switch_cpu(&this->_ksp, next->ksp(), (word_t)next->utcb());
}
