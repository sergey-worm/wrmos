//##################################################################################################
//
//  Assert backend implementation.
//
//##################################################################################################

#include <assert.h>
#include <stdio.h>
#include "wlibc_cb.h"

// this prints an "Assertion failed" message and aborts
void __assert(const char* str_expr, const char* file, unsigned int line, const char* func)
{
	printf("\nAssert:  %s:%u  %s:  (%s) failed.\n", file, line, func, str_expr);
	wlibc_callbacks_get()->break_exec("Assert."); //break_execution("Assert.");
	while (1);
}
