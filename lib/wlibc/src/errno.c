//##################################################################################################
//
//  Errno implementation.
//
//##################################################################################################

#include "wlibc_panic.h"

// FIXME:  for SMP need to use TLS
int* __errno_location(void)
{
	static int _errno;
	return &_errno;
}

char* strerror(int errnum)
{
	panic("%s:  Implement me!\n", __func__);
	return 0;
}
