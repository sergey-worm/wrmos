//##################################################################################################
//
//  Implementation of some funcs for poll.h.
//
//##################################################################################################

#include <sys/poll.h>
#include "wlibc_panic.h"

int poll(struct pollfd* ufds, nfds_t nfds, int timeout)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
