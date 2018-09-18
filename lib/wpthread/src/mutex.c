//##################################################################################################
//
//  Pthread mutex handling.
//
//##################################################################################################

#include <pthread.h>
#include <stdio.h>
#include "wlibc_panic.h"
#include "wrm_mtx.h"

enum { Pthread_as_wrm_magic = 0x117733aa };
typedef struct
{
	int magic;
	Wrm_mtx_t* wmtx;
} Mutex_t;

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* mutexattr)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
	//printf("%s:  mtx=%p, implement me!\n", __func__, mutex);

	Mutex_t* m = (Mutex_t*) mutex;

	// initialize mutex if need
	if (m->magic != Pthread_as_wrm_magic)
	{
		static Wrm_mtx_t _mtx = Wrm_mtx_initializer;
		int rc = wrm_mtx_lock(&_mtx, -1);
		if (rc)
			return rc;

		if (m->magic != Pthread_as_wrm_magic)
		{
			m->wmtx = malloc(sizeof(Wrm_mtx_t));
			if (m->wmtx)
			{
				wrm_mtx_init(m->wmtx);
				m->magic = Pthread_as_wrm_magic;
			}
		}

		rc = wrm_mtx_unlock(&_mtx);
		if (rc || !m->wmtx)
			return rc | 100;

	}

	// lock mutex
	int rc = wrm_mtx_lock(m->wmtx, -1);
	return rc;
}

int pthread_mutex_timedlock(pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
	//printf("%s:  mtx=%p, implement me!\n", __func__, mutex);

	Mutex_t* m = (Mutex_t*) mutex;

	// unlock mutex if it's initialized
	if (m->magic == Pthread_as_wrm_magic)
	{
		int rc = wrm_mtx_unlock(m->wmtx);
		return rc;
	}
	return 0;
}
