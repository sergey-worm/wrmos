//##################################################################################################
//
//  Pthread conditional variables handling.
//
//##################################################################################################

#include <pthread.h>
#include <stdio.h>

// Initialize condition variable COND using attributes ATTR, or use
// the default values if later is NULL.
int pthread_cond_init(pthread_cond_t* restrict cond, const pthread_condattr_t* restrict cond_attr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Destroy condition variable COND.
int pthread_cond_destroy(pthread_cond_t* cond)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Wake up one thread waiting for condition variable COND.
int pthread_cond_signal(pthread_cond_t* cond)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Wake up all threads waiting for condition variables COND.
int pthread_cond_broadcast(pthread_cond_t* cond)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Wait for condition variable COND to be signaled or broadcast.
// MUTEX is assumed to be locked before.
//
// This function is a cancellation point and therefore not marked with __THROW.
int pthread_cond_wait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Wait for condition variable COND to be signaled or broadcast until
// ABSTIME.  MUTEX is assumed to be locked before.  ABSTIME is an
// absolute time specification; zero is the beginning of the epoch
// (00:00:00 GMT, January 1, 1970).
//
// This function is a cancellation point and therefore not marked with __THROW.
int pthread_cond_timedwait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex,
                           const struct timespec* restrict abstime)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

