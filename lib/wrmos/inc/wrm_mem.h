//##################################################################################################
//
//  Memory - API to get named memory, named memory is configured in project config.
//
//##################################################################################################

#ifndef WRM_MEMORY_H
#define WRM_MEMORY_H

#include "sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int wrm_mem_get_named(const char* mem_name, addr_t* addr, size_t* size, paddr_t* paddr,
                   unsigned* access = 0, unsigned* cached = 0, unsigned* contig = 0);

int wrm_mem_get_usual(addr_t addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif // WRM_MEMORY_H
