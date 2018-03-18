//##################################################################################################
//
//  Common kernel functions.
//
//##################################################################################################

#include "thread.h"
#include "threads.h"
#include "sched.h"

Thread_t* cur_thr() { return Sched_t::current(); }
const char* cur_thr_name() { return Sched_t::current() ? Sched_t::current()->name() : "----"; }

void kdb_console_entry_wrapper(bool krn_mode, bool error_entry, const char* prompt);

// it needs for wlibc assert/panic
void break_execution(const char* str)
{
	bool krn_mode = true;
	bool error_entry = true;
	kdb_console_entry_wrapper(krn_mode, error_entry, str);
}
