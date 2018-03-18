//##################################################################################################
//
//  App - to create and manage by application.
//
//##################################################################################################

#ifndef WRM_APP_H
#define WRM_APP_H

#include "wrm_appcfg.h"

#ifdef __cplusplus
extern "C" {
#endif

int wrm_app_create(L4_thrid_t id, const Wrm_app_cfg_t* cfg);
int wrm_app_location(L4_thrid_t id, addr_t rem_addr, size_t sz, acc_t acc, L4_fpage_t* loc_fpage);
int wrm_app_alloc_aspace(L4_thrid_t id);
int wrm_app_free_aspace(L4_thrid_t id);
int wrm_app_alloc_thrno(L4_thrid_t id, unsigned* thrno);
int wrm_app_free_thrno(L4_thrid_t id, unsigned thrno);
int wrm_app_get_thr_numbers(L4_thrid_t id, unsigned* thrno_begin, unsigned* thrno_end);
int wrm_app_max_prio(L4_thrid_t id, unsigned* max_prio);

#ifdef __cplusplus
}
#endif

#endif // WRM_APP_H

