//##################################################################################################
//
//  Errno implementation.
//
//##################################################################################################

#include "assert.h"

// FIXME:  for SMP need to use TLS
int* __errno_location(void)
{
	static int _errno;
	return &_errno;
}

char* strerror(int errnum)
{
	assert(0 && "Implement me!");
	return 0;
}
