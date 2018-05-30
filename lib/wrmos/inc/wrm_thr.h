//##################################################################################################
//
//  Thread - to create and manage by thread.
//
//##################################################################################################

#ifndef WRM_THREAD_H
#define WRM_THREAD_H

#include "l4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	Wrm_thr_flag_no  = 0,
	Wrm_thr_flag_fpu = 1 << 0
};

typedef int (*thread_entry_t)(int arg);

int wrm_thread_create(L4_fpage_t utcb, thread_entry_t entry, int arg, addr_t stack, size_t stack_sz,
                      unsigned prio, const char* short_name, word_t flags, L4_thrid_t* id,
                      L4_thrid_t space = L4_thrid_t::Any,
                      L4_thrid_t sched = L4_thrid_t::Any,
                      L4_thrid_t pager = L4_thrid_t::Any);

int wrm_task_create(L4_fpage_t utcbs_area, thread_entry_t entry, int arg, addr_t stack, size_t stack_sz,
                    unsigned prio, const char* short_name, word_t flags, L4_thrid_t pager,
                    L4_fpage_t kip_area, L4_thrid_t* id);

#ifdef __cplusplus
}
#endif

#endif // WRM_THREAD_H

