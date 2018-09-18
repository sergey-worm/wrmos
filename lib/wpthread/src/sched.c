//##################################################################################################▫
//
//  Pthread scheduling control.
//
//##################################################################################################

#include <pthread.h>
#include <stdio.h>

// Set the scheduling parameters for TARGET_THREAD according to POLICY
// and *PARAM.
int pthread_setschedparam(pthread_t target_thread, int policy, const struct sched_param* param)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return in *POLICY and *PARAM the scheduling parameters for TARGET_THREAD.
int pthread_getschedparam(pthread_t target_thread, int* restrict policy,
                          struct sched_param* restrict param)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the scheduling priority for TARGET_THREAD.
int pthread_setschedprio(pthread_t target_thread, int prio)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Get thread name visible in the kernel and its interfaces.
int pthread_getname_np (pthread_t target_thread, char* buf, size_t buflen)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set thread name visible in the kernel and its interfaces.
int pthread_setname_np(pthread_t target_thread, const char* name)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Determine level of concurrency.
int pthread_getconcurrency(void)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set new concurrency level to LEVEL.
int pthread_setconcurrency(int level)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Yield the processor to another thread or process.
// This function is similar to the POSIX `sched_yield' function but
// might be differently implemented in the case of a m-on-n thread
// implementation.
int pthread_yield(void)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}


// Limit specified thread TH to run only on the processors represented
// in CPUSET.
int pthread_setaffinity_np(pthread_t th, size_t cpusetsize, const cpu_set_t* cpuset)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Get bit set in CPUSET representing the processors TH can run on.
int pthread_getaffinity_np(pthread_t th, size_t cpusetsize, cpu_set_t* cpuset)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}
