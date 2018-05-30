//##################################################################################################
//
//  Thread ID helpers.
//
//##################################################################################################

#ifndef L4_THRID_H
#define L4_THRID_H

#include "l4_types.h"
#include "l4_kip.h"

static inline L4_thrid_t l4_thrid_sigma0()
{
	// TODO:  use 'static'
	/*static*/ L4_thrid_t sigma0 = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 0);
	return sigma0;
}

static inline L4_thrid_t l4_thrid_roottask()
{
	// TODO:  use 'static'
	/*static*/ L4_thrid_t roottask = L4_thrid_t::create_global(l4_kip()->thread_info.user_base() + 1);
	return roottask;
}

#endif // L4_THRID_H
