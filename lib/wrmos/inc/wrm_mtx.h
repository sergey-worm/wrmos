//##################################################################################################
//
//  Mutex - mutex implementation.
//
//##################################################################################################

#ifndef WRM_MTX_H
#define WRM_MTX_H

#include "wrm_sem.h"

typedef Wrm_sem_t Wrm_mtx_t;

#define Wrm_mtx_initializer           Wrm_sem_binary_unlocked_initializer
#define Wrm_mtx_recursive_initializer Wrm_sem_binary_recursive_unlocked_initializer

#ifdef __cplusplus
extern "C" {
#endif

inline int wrm_mtx_init(Wrm_mtx_t* mtx)
{
	return wrm_sem_init(mtx, Wrm_sem_binary, 1);
}

inline int wrm_mtx_init_recursive(Wrm_mtx_t* mtx)
{
	return wrm_sem_init(mtx, Wrm_sem_binary | Wrm_sem_recursive, 1);
}

inline int wrm_mtx_destroy(Wrm_mtx_t* mtx)
{
	return wrm_sem_destroy(mtx);
}

inline int wrm_mtx_lock(Wrm_mtx_t* mtx, int timeout_usec Default(Wrm_sem_timeout_infinite))
{
	return wrm_sem_wait(mtx, timeout_usec);
}

inline int wrm_mtx_unlock(Wrm_mtx_t* mtx)
{
	return wrm_sem_post(mtx);
}

#ifdef __cplusplus
}
#endif

#endif // WRM_MTX_H

