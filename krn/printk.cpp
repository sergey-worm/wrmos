//##################################################################################################
//
//  printk - print formatted string with prefix (thread name, time).
//
//##################################################################################################

#include "printk.h"
#include "sysclock.h"
#include "libcio.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

const char* cur_thr_name();
uint64_t sys_clock();

static inline void print_pref(uint64_t usec)
{
	printf("\x1b[33m[%4s:%llu.%06u]  ", cur_thr_name(), usec/1000000, (int)usec%1000000);
}

#ifdef Cfg_klog
void printk(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	uint64_t time = SystemClock_t::sys_clock("printk");
	print_pref(time);
	vprintf(fmt, args);
	printf("\x1b[0m");
	va_end(args);
}
void printk_notime(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	//uint64_t time = SystemClock_t::sys_clock("printk_notime");
	print_pref(0);
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
	print_pref(time);
	vprintf(fmt, args);
	printf("\x1b[0m");
	va_end(args);
}

void force_printk_uart(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	uint64_t time = SystemClock_t::sys_clock("force_printk_uart");
	Wlibc_callbacks_t cb;
	libcio_store(&cb);
	libcio_set_uart();
	print_pref(time);
	vprintf(fmt, args);
	printf("\x1b[0m");
	libcio_restore(&cb);
	va_end(args);
}

void printf_uart(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	Wlibc_callbacks_t cb;
	libcio_store(&cb);
	libcio_set_uart();
	vprintf(fmt, args);
	libcio_restore(&cb);
	va_end(args);
}
