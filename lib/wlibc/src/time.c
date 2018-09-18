//##################################################################################################
//
//  Implementation of some funcs for time.h.
//
//##################################################################################################

#include <time.h>
#include "wlibc_panic.h"

size_t strftime(char* ptr, size_t maxsize, const char* format, const struct tm* timeptr)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
