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

typedef int (*thread_entry_t)(int arg);

int wrm_thread_create2(L4_fpage_t utcb, thread_entry_t entry, int arg, addr_t stack, size_t stack_sz,
                      unsigned prio, const char* short_name, word_t flags, L4_thrid_t* id,
                      L4_thrid_t space = L4_thrid_t::Any,
                      L4_thrid_t sched = L4_thrid_t::Any,
                      L4_thrid_t pager = L4_thrid_t::Any);

// obsolete
inline int wrm_thread_create(addr_t utcb, thread_entry_t entry, int arg, addr_t stack, size_t stack_sz,
                      unsigned prio, const char* short_name, word_t flags, L4_thrid_t* id,
                      L4_thrid_t space = L4_thrid_t::Any,
                      L4_thrid_t sched = L4_thrid_t::Any,
                      L4_thrid_t pager = L4_thrid_t::Any)
{
	return wrm_thread_create2(L4_fpage_t::create(utcb, Cfg_page_sz, Acc_rw),
	                         entry, arg, stack, stack_sz, prio, short_name, flags, id,
	                         space, sched, pager);
}

int wrm_task_create(L4_fpage_t utcbs_area, thread_entry_t entry, int arg, addr_t stack, size_t stack_sz,
                    unsigned prio, const char* short_name, word_t flags, L4_thrid_t pager,
                    L4_fpage_t kip_area, L4_thrid_t* id);

int wrm_thread_flags(L4_thrid_t id, word_t flags);

enum
{
	Wrm_thr_flag_no  = 0,
	Wrm_thr_flag_fpu = 1 << 0
};

#ifdef __cplusplus
}
#endif

#endif // WRM_THREAD_H

