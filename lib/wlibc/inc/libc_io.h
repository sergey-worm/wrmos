//##################################################################################################
//
//  Functions to set wlibc callbacks.
//
//##################################################################################################

#ifndef LIBC_IO_H
#define LIBC_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*libc_out_char_t)(int ch);
typedef void (*libc_out_string_t)(const char* str, size_t len);
typedef int  (*libc_in_char_t)();

typedef struct
{
	libc_out_char_t    out_char;
	libc_out_string_t  out_string;
	libc_in_char_t     in_char;
} Libc_io_callbacks_t;

void libc_init_io(Libc_io_callbacks_t* callbacks);

#ifdef __cplusplus
}
#endif

#endif // LIBC_IO_H
