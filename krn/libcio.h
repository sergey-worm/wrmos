//##################################################################################################
//
//  Helpers for set libc io backends.
//
//##################################################################################################

#ifndef LIBCIO_H
#define LIBCIO_H

#include "wlibc_cb.h"
#include "log.h"
#include "kuart.h"

void break_execution(const char* str);

// after call this function printk()/printf() will be write to Uart
inline void libcio_set_uart()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = Uart::putch;
	cb->out_string = NULL;
	cb->in_char    = Uart::getch;
	cb->in_string  = NULL;
	cb->break_exec = break_execution;
}

// after call this function printk()/printf() will be write to Log
inline void libcio_set_log()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = NULL;
	cb->out_string = Log::write;
	cb->in_char    = NULL;
	cb->in_string  = NULL;
	cb->break_exec = break_execution;
}

// after call this function printk()/printf() will be write to nothing
inline void libcio_set_null()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = NULL;
	cb->out_string = NULL;
	cb->in_char    = NULL;
	cb->in_string  = NULL;
	cb->break_exec = break_execution;
}

inline void libcio_store(Wlibc_callbacks_t* cb_state)
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb_state->out_char   = cb->out_char;
	cb_state->out_string = cb->out_string;
	cb_state->in_char    = cb->in_char;
	cb->in_string        = NULL;
	cb->break_exec       = break_execution;
}

inline void libcio_restore(const Wlibc_callbacks_t* cb_state)
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = cb_state->out_char;
	cb->out_string = cb_state->out_string;
	cb->in_char    = cb_state->in_char;
	cb->in_string  = NULL;
	cb->break_exec = break_execution;
}

#endif // LIBCIO_H
