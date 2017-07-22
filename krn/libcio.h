//##################################################################################################
//
//  Helpers for set libc io backends.
//
//##################################################################################################

#ifndef LIBCIO_H
#define LIBCIO_H

#include "libc_io.h"
#include "log.h"
#include "kuart.h"

// after call this function printk()/printf() will be write to Uart
inline void libcio_set_uart()
{
	Libc_io_callbacks_t io =
	{
		.out_char    = Uart::putch,
		.out_string  = NULL,
		.in_char     = Uart::getch,
	};
	libc_init_io(&io); // init libc handlers
}

// after call this function printk()/printf() will be write to Log
inline void libcio_set_log()
{
	Libc_io_callbacks_t io =
	{
		.out_char    = NULL,
		.out_string  = Log::write,
		.in_char     = NULL,
	};
	libc_init_io(&io); // init libc handlers
}

// after call this function printk()/printf() will be write to nothing
inline void libcio_set_null()
{
	Libc_io_callbacks_t io =
	{
		.out_char    = NULL,
		.out_string  = NULL,
		.in_char     = NULL,
	};
	libc_init_io(&io); // init libc handlers
}

#endif // LIBCIO_H
