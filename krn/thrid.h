//##################################################################################################
//
//  Thread ID helpers.
//
//##################################################################################################

#ifndef THRID_H
#define THRID_H

#include "l4_types.h"

inline L4_thrid_t thrid_sigma0()
{
	static L4_thrid_t id = L4_thrid_t::create_global(Kcfg::Kip_thrid_user_base + 0, 1);
	return id;
}

inline L4_thrid_t thrid_roottask()
{
	static L4_thrid_t id = L4_thrid_t::create_global(Kcfg::Kip_thrid_user_base + 1, 1);
	return id;
}

inline L4_thrid_t thrid_int(unsigned irq)
{
	assert(irq < Kcfg::Ints_max);
	return L4_thrid_t::create_global(irq, 1);
}

inline bool thrid_is_int(L4_thrid_t id)
{
	return id.is_global()  &&
	       id.number()  <  Kcfg::Ints_max  &&
	       id.version() == 1;
}

inline bool thrid_is_global_user(L4_thrid_t id)
{
	return id.is_global()  &&
	       id.number()  >=  Kcfg::Kip_thrid_user_base  &&
	       id.number()  <=  Kcfg::Kip_thrid_number_max;
}

#endif // THRID_H
