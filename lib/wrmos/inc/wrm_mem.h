//##################################################################################################
//
//  Memory - API to get named memory, named memory is configured in project config.
//
//##################################################################################################

#ifndef WRM_MEMORY_H
#define WRM_MEMORY_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int wrm_memory_get(const char* mem_name, addr_t* addr, size_t* size, paddr_t* paddr,
                   unsigned* access = 0, unsigned* cached = 0, unsigned* contig = 0);

#ifdef __cplusplus
}
#endif

#endif // WRM_MEMORY_H
