//##################################################################################################
//
//  Alternatve assert implementation.
//
//##################################################################################################

#ifndef WLIBC_ASSERT_H
#define WLIBC_ASSERT_H

#include <stdio.h>
#include "wlibc_cb.h"

inline void do_assert(const char* base_file, const char* file, unsigned line, const char* func, const char* expr)
{
	printf("Assert:  %s:  %s:%u  %s():  (%s) failed.\n", base_file, file, line, func, expr);
	wlibc_callbacks_get()->break_exec("Assert.");
}

#ifndef NDEBUG
# define wassert(expr) \
	if (!(expr)) \
	{ \
		do_assert(__BASE_FILE__, __FILE__, __LINE__, __func__, #expr); \
	}
#else
# define wassert(expr)
#endif

#endif // WLIBC_ASSERT_H
