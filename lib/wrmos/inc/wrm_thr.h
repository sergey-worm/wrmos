//##################################################################################################
//
//  Thread - to create and manage by thread.
//
//##################################################################################################

#ifndef WRM_THREAD_H
#define WRM_THREAD_H

#include "l4types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*thread_entry_t)(int arg);

int wrm_thread_create(addr_t utcb, thread_entry_t entry, int arg, addr_t stack, size_t stack_sz,
                      unsigned prio, const char* short_name, word_t flags, L4_thrid_t* id);

int wrm_thread_flags(L4_thrid_t id, word_t flags);

int wrm_thread_create_priv(L4_thrid_t id, L4_thrid_t space, addr_t utcb, addr_t entry, addr_t stack,
                           size_t stack_sz, unsigned prio, word_t short_name);

enum
{
	Wrm_thr_flag_no  = 0,
	Wrm_thr_flag_fpu = 1 << 0
};

#ifdef __cplusplus
}
#endif

#endif // WRM_THREAD_H

