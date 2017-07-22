//##################################################################################################
//
//  Assert implementation.
//
//##################################################################################################

#ifndef ASSERT_H
#define ASSERT_H

#if 0

#include <assert.h>

#else

#include <stdio.h>

#ifdef __cplusplus
extern "C"
#endif
void break_execution(const char*);

inline void do_assert(const char* base_file, const char* file, unsigned line, const char* func, const char* expr)
{
	printf("Assert:  %s:  %s:%u  %s():  (%s) failed.\n", base_file, file, line, func, expr);
	break_execution("Assert.");
}

#ifdef Cfg_debug
#  define assert(expr) \
	if (!(expr)) \
	{ \
		do_assert(__BASE_FILE__, __FILE__, __LINE__, __func__, #expr); \
	}
#else
#  define assert(expr)
#endif

#endif // 0

#endif // ASSERT_H
