//##################################################################################################
//
//  ...
//
//##################################################################################################

#ifndef WLIBC_PANIC_H
#define WLIBC_PANIC_H

#ifdef __cplusplus
extern "C" {
#endif

#define panic(...) do_panic(__FILE__, __LINE__, __func__, __VA_ARGS__)

void do_panic(const char* file, unsigned line, const char* func, const char* fmt, ...)
	__attribute__((format(printf, 4, 5),noreturn));

#ifdef __cplusplus
}
#endif

#endif // PANIC_H
