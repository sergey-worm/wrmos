//##################################################################################################
//
//  Implementation of some funcs for locale.h.
//
//##################################################################################################

#include <sys/ioctl.h>
#include "wlibc_panic.h"

int ioctl(int fd, unsigned long request, ...)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
