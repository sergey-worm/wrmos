//##################################################################################################
//
//  Device API - driver API to get access to MMIO device.
//
//##################################################################################################

#ifndef WRM_DEV_H
#define WRM_DEV_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int wrm_dev_map_io(const char* dev_name, addr_t* addr, size_t* sz = 0);
int wrm_dev_attach_int(const char* dev_name, unsigned* intno);
int wrm_dev_detach_int(const char* dev_name);
int wrm_dev_wait_int(unsigned intno, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif // WRM_DEV_H

