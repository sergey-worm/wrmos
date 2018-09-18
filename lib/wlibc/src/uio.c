//##################################################################################################
//
//  Implementation of some funcs for sys/uio.h.
//
//##################################################################################################

#include <sys/uio.h>
#include "wlibc_panic.h"

ssize_t readv(int fd, const struct iovec* iov, int iovcnt)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

ssize_t preadv(int fd, const struct iovec* iov, int iovcnt, off_t offset)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

ssize_t pwritev(int fd, const struct iovec* iov, int iovcnt, off_t offset)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
