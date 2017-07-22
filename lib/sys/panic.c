//##################################################################################################
//
//  Panic implementation.
//
//##################################################################################################

#include "panic.h"
#include <stdarg.h>
#include <stdio.h>

void break_execution(const char*);

void panic(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("\nPanic:  ");
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	break_execution("Panic.");
}
