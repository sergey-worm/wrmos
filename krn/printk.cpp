//##################################################################################################
//
//  printk - print formatted string with prefix (thread name, time).
//
//##################################################################################################

#include "printk.h"
#include "sysclock.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

const char* cur_thr_name();
uint64_t sys_clock();

#ifdef Cfg_klog
void printk(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	uint64_t time = SystemClock_t::sys_clock("printk");
	uint32_t usec = time % 1000;
	printf("\x1b[33m[%4s:%4llu.%03u]  ", cur_thr_name(), time/1000, usec);
	vprintf(fmt, args);
	printf("\x1b[0m");
	va_end(args);
}
#endif

void force_printk(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	uint64_t time = SystemClock_t::sys_clock("force_printk");
	uint32_t usec = time % 1000;
	printf("\x1b[33m[%4s:%4llu.%03u]  ", cur_thr_name(), time/1000, usec);
	vprintf(fmt, args);
	printf("\x1b[0m");
	va_end(args);
}
