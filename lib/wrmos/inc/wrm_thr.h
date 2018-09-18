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
	Wrm_thr_flag_fpu = 1 << 0,

	Wrm_thr_state_free = 1,  // no such thread
	Wrm_thr_state_busy = 2,  // thread in use
	Wrm_thr_state_done = 3,  // thread terminated
};

typedef long (*L4_thread_func_t)(long arg);

int wrm_thr_create(L4_fpage_t utcb, L4_thread_func_t entry, long arg, addr_t stack, size_t stack_sz,
                   unsigned prio, const char* short_name, word_t flags, L4_thrid_t* id,
                   L4_thrid_t space = L4_thrid_t::Any,
                   L4_thrid_t sched = L4_thrid_t::Any,
                   L4_thrid_t pager = L4_thrid_t::Any);

int wrm_thr_delete(L4_thrid_t id, long* term_code, int* state, L4_fpage_t* utcb_fp,
                   addr_t* stack, size_t* stack_sz);

int wrm_tsk_create(L4_fpage_t utcb, L4_thread_func_t entry, long arg, addr_t stack, size_t stack_sz,
                   unsigned prio, const char* short_name, word_t flags, L4_thrid_t pager,
                   L4_fpage_t kip_area, L4_fpage_t utcbs_area, L4_thrid_t* id);

#ifdef __cplusplus
}
#endif

#endif // WRM_THREAD_H

