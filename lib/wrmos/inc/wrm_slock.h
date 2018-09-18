//##################################################################################################
//
//  Spinlock - spinlock implementation.
//
//##################################################################################################

#ifndef WRM_SPINLOCK_H
#define WRM_SPINLOCK_H

#include "l4_types.h"

typedef unsigned Wrm_spinlock_t;

#ifdef __cplusplus
extern "C" {
#endif

void wrm_spinlock_init(Wrm_spinlock_t* spinlock);
void wrm_spinlock_lock(Wrm_spinlock_t* spinlock);
void wrm_spinlock_unlock(Wrm_spinlock_t* spinlock);

#ifdef __cplusplus
}
#endif

#endif // WRM_SPINLOCK_H

