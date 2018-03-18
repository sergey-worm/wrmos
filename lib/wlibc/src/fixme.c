//##################################################################################################
//
//  Workarounds:  some dummy funcs.
//
//##################################################################################################

#include "sys-config.h"

#ifdef Cfg_arch_arm

#include "wlibc_panic.h"

void __aeabi_unwind_cpp_pr0(void)
{
	panic("%s:  IMPME.\n", __func__);
}

void __aeabi_unwind_cpp_pr1(void)
{
	panic("%s:  IMPME.\n", __func__);
}

/*
int __div0(int return_value)
{
	return 0;
}
*/

int __aeabi_idiv0(int return_value)
{
	panic("%s:  IMPME.\n", __func__);
	return 0;
}

long long __aeabi_ldiv0(long long return_value)
{
	panic("%s:  IMPME.\n", __func__);
	return 0;
}

void raise()
{
	panic("%s:  IMPME.\n", __func__);
}

#endif // arm
