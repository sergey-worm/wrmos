//##################################################################################################
//
//  Panic implementation.
//
//##################################################################################################

#include "wlibc_panic.h"
#include "wlibc_cb.h"
#include <stdarg.h>
#include <stdio.h>

void do_panic(const char* file, unsigned line, const char* func, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("\nPanic:  %s:%u:%s:  ", file, line, func);
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	wlibc_callbacks_get()->break_exec("Panic.");
}
