//##################################################################################################
//
//  Ram FS - content file's headers and bodies.
//
//##################################################################################################

#ifndef WRM_RAMFS_H
#define WRM_RAMFS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int wrm_ramfs_get_file(const char* filename, addr_t* addr, size_t* size);

#ifdef __cplusplus
}
#endif

#endif // WRM_MPOOL_H
