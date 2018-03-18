//##################################################################################################
//
//  Panic implementation.
//
//##################################################################################################

#include "wlibc_panic.h"
#include "wlibc_cb.h"
#include <stdarg.h>
#include <stdio.h>

void panic(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("\nPanic:  ");
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	wlibc_callbacks_get()->break_exec("Panic.");
}
