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

void       wrm_mpool_dump();
void       wrm_mpool_add(L4_fpage_t fpage);
L4_fpage_t wrm_mpool_alloc(size_t sz);
L4_fpage_t wrm_mpool_alloc_log2sz(size_t log2sz);
size_t     wrm_mpool_size();
void*      wrm_malloc(size_t sz);

#ifdef __cplusplus
}
#endif

#endif // WRM_MPOOL_H

