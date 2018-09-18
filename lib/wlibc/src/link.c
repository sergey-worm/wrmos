//##################################################################################################
//
//  Implementation of some funcs for link.h.
//
//##################################################################################################

#include <link.h>
#include "wlibc_panic.h"

struct dl_phdr_info
{
	// fake
};

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
