//##################################################################################################
//
//  Functions to set wlibc callbacks.
//
//##################################################################################################

#include <wlibc_cb.h>

static Wlibc_callbacks_t wlibc_callbacks;

Wlibc_callbacks_t* wlibc_callbacks_get()
{
	return &wlibc_callbacks;
}
