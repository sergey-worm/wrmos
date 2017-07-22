//##################################################################################################
//
//  Workarounds:  some dummy funcs.
//
//##################################################################################################

#include "sys-config.h"

#ifdef Cfg_arch_arm

#include "assert.h"

void __aeabi_unwind_cpp_pr0(void)
{
	assert(0);
}

void __aeabi_unwind_cpp_pr1(void)
{
	assert(0);
}

/*
int __div0(int return_value)
{
	return 0;
}
*/

int __aeabi_idiv0(int return_value)
{
	assert(0);
	return 0;
}

long long __aeabi_ldiv0(long long return_value)
{
	assert(0);
	return 0;
}

void raise()
{
	assert(0 && "IMPME");
}

#endif // arm
