//##################################################################################################
//
//  Mutex - mutex implementation.
//
//##################################################################################################

#ifndef WRM_MTX_H
#define WRM_MTX_H

#include "wrm_sem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Wrm_sem_t Wrm_mtx_t;

inline int wrm_mtx_init(Wrm_mtx_t* mtx)
{
	return wrm_sem_init(mtx, Wrm_sem_binary, 1);
}

inline int wrm_mtx_destroy(Wrm_mtx_t* mtx)
{
	return wrm_sem_destroy(mtx);
}

#ifdef __cplusplus
inline int wrm_mtx_lock(Wrm_mtx_t* mtx, int timeout_usec = Wrm_sem_timeout_infinite)
#else
inline int wrm_mtx_lock(Wrm_mtx_t* mtx, int timeout_usec)
#endif
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

