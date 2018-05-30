//##################################################################################################
//
//  Functions to set wlibc callbacks.
//  This allows tune wlibc for kernel or for user usage.
//
//##################################################################################################

#ifndef WLIBC_IO_H
#define WLIBC_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void  (*wlibc_out_char_t)(int ch);
typedef void  (*wlibc_out_string_t)(const char* str, size_t len);
typedef int   (*wlibc_in_char_t)(void);
typedef int   (*wlibc_in_string_t)(char* buf, size_t len);
typedef void* (*wlibc_malloc_t)(size_t sz);
typedef void  (*wlibc_break_exec_t)(const char* str) __attribute__((noreturn));

typedef struct
{
	wlibc_out_char_t    out_char;
	wlibc_out_string_t  out_string;
	wlibc_in_char_t     in_char;
	wlibc_in_string_t   in_string;
	wlibc_malloc_t      malloc;
	wlibc_break_exec_t  break_exec;
} Wlibc_callbacks_t;

Wlibc_callbacks_t* wlibc_callbacks_get(void);

#ifdef __cplusplus
}
#endif

#endif // WLIBC_IO_H
