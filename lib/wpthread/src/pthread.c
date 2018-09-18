//##################################################################################################
//
//  Pthread create/terminate functions.
//
//##################################################################################################

#include <pthread.h>
#include <stdio.h>

// Create a new thread, starting with execution of START-ROUTINE
// getting passed ARG.  Creation attributed come from ATTR.  The new
// handle is stored in *NEWTHREAD.
int pthread_create(pthread_t* restrict newthread,
                   const pthread_attr_t* restrict attr,
                   void*(*start_routine)(void*),
                   void* restrict arg)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Terminate calling thread.
// The registered cleanup handlers are called via exception handling
// so we cannot mark this function with __THROW.
void pthread_exit(void* retval)
{
	printf("%s:  implement me!\n", __func__);
	while (1);
}

// Make calling thread wait for termination of the thread TH.  The
// exit status of the thread is stored in *THREAD_RETURN, if THREAD_RETURN
// is not NULL.
//
// This function is a cancellation point and therefore not marked with
// __THROW.
int pthread_join(pthread_t th, void** thread_return)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Check whether thread TH has terminated.  If yes return the status of
// the thread in *THREAD_RETURN, if THREAD_RETURN is not NULL.
int pthread_tryjoin_np(pthread_t th, void** thread_return)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Make calling thread wait for termination of the thread TH, but only
// until TIMEOUT.  The exit status of the thread is stored in
// *THREAD_RETURN, if THREAD_RETURN is not NULL.
//
// This function is a cancellation point and therefore not marked with
// __THROW.
int pthread_timedjoin_np(pthread_t th, void** thread_return, const struct timespec* abstime)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Indicate that the thread TH is never to be joined with PTHREAD_JOIN.
// The resources of TH will therefore be freed immediately when it
// terminates, instead of waiting for another thread to perform PTHREAD_JOIN
// on it.
int pthread_detach(pthread_t th)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Obtain the identifier of the current thread.
pthread_t pthread_self(void)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Compare two thread identifiers.
int pthread_equal(pthread_t __thread1, pthread_t __thread2)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

