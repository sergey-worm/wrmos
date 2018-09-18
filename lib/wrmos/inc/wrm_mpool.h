//##################################################################################################
//
//  Memory pool - contents flex pages and allow to store and allocate fpage.
//
//##################################################################################################

#ifndef WRM_MPOOL_H
#define WRM_MPOOL_H

#include "l4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void       wrm_pgpool_dump();
void       wrm_pgpool_add(L4_fpage_t fpage);
L4_fpage_t wrm_pgpool_alloc(size_t sz);
size_t     wrm_pgpool_size();

void       wrm_mpool_dump();
int        wrm_mpool_add(void* buf, size_t sz);
void*      wrm_mpool_alloc(size_t sz);
void       wrm_mpool_free(void* ptr);
size_t     wrm_mpool_size();

#ifdef __cplusplus
}
#endif

#endif // WRM_MPOOL_H

