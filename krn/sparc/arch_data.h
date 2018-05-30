//##################################################################################################
//
//  Arch_data - arch specifics kernel data.
//
//##################################################################################################

#ifndef ARCH_DATA_H
#define ARCH_DATA_H

#include "wlibc_panic.h"
#include <string.h>

struct Arch_task_t
{
	void set_ioperm_all(int enable)
	{
		(void)enable;
		// nothing for this arch
	}

	void set_ioperm(unsigned port, unsigned sz, int enable)
	{
		(void)port; (void)sz; (void)enable;
		panic("unexpected %s() for this arch.", __func__);
	}

	int is_ioperm(unsigned port, unsigned sz, int enable)
	{
		(void)port; (void)sz; (void)enable;
		panic("unexpected %s() for this arch.", __func__);
		return 0;
	}

	void set_current()
	{
		// nothing for this arch
	}
};

#endif // ARCH_DATA_H
