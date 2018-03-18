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

void panic(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif // PANIC_H
