//##################################################################################################
//
//  Pthread thread attribute handling.
//
//##################################################################################################

#include <pthread.h>
#include <stdio.h>

// Initialize thread attribute *ATTR with default attributes
// (detachstate is PTHREAD_JOINABLE, scheduling policy is SCHED_OTHER,
// no user-provided stack).
int pthread_attr_init(pthread_attr_t* attr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Destroy thread attribute.
int pthread_attr_destroy(pthread_attr_t* attr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Get detach state attribute.
int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set detach state attribute.
int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Get the size of the guard area created for stack overflow protection.
int pthread_attr_getguardsize(const pthread_attr_t* attr, size_t* guardsize)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the size of the guard area created for stack overflow protection.
int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guardsize)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}


// Return in *PARAM the scheduling parameters of *ATTR.
int pthread_attr_getschedparam(const pthread_attr_t* restrict attr, struct sched_param* restrict param)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set scheduling parameters (priority, etc) in *ATTR according to PARAM.
int pthread_attr_setschedparam(pthread_attr_t* restrict attr, const struct sched_param* restrict param)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return in *POLICY the scheduling policy of *ATTR.
int pthread_attr_getschedpolicy (const pthread_attr_t* restrict attr, int* restrict policy)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set scheduling policy in *ATTR according to POLICY.
int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return in *INHERIT the scheduling inheritance mode of *ATTR.
int pthread_attr_getinheritsched(const pthread_attr_t* restrict attr, int* restrict inherit)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set scheduling inheritance mode in *ATTR according to INHERIT.
int pthread_attr_setinheritsched(pthread_attr_t* attr, int inherit)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return in *SCOPE the scheduling contention scope of *ATTR.
int pthread_attr_getscope(const pthread_attr_t* restrict attr, int* restrict scope)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set scheduling contention scope in *ATTR according to SCOPE.
int pthread_attr_setscope(pthread_attr_t* attr, int scope)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return the previously set address for the stack.
int pthread_attr_getstackaddr(const pthread_attr_t* restrict attr, void** restrict stackaddr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the starting address of the stack of the thread to be created.
// Depending on whether the stack grows up or down the value must either
// be higher or lower than all the address in the memory block.  The
// minimal size of the block must be PTHREAD_STACK_MIN.
int pthread_attr_setstackaddr(pthread_attr_t* attr, void* stackaddr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return the currently used minimal stack size.
int pthread_attr_getstacksize(const pthread_attr_t* restrict attr, size_t* restrict stacksize)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Add information about the minimum stack size needed for the thread
// to be started.  This size must never be less than PTHREAD_STACK_MIN
// and must also not exceed the system limits.
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return the previously set address for the stack.
int pthread_attr_getstack(const pthread_attr_t* restrict attr, void** restrict stackaddr,
                          size_t* restrict stacksize)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// The following two interfaces are intended to replace the last two.  They
// require setting the address as well as the size since only setting the
// address will make the implementation on some architectures impossible.
int pthread_attr_setstack(pthread_attr_t* attr, void* stackaddr,
                          size_t stacksize)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Thread created with attribute ATTR will be limited to run only on
// the processors represented in CPUSET.
int pthread_attr_setaffinity_np(pthread_attr_t* attr, size_t cpusetsize,
                                const cpu_set_t* cpuset)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Get bit set in CPUSET representing the processors threads created with
// ATTR can run on.
int pthread_attr_getaffinity_np(const pthread_attr_t* attr, size_t cpusetsize,
                                cpu_set_t* cpuset)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Initialize thread attribute *ATTR with attributes corresponding to the
// already running thread TH.  It shall be called on uninitialized ATTR
// and destroyed with pthread_attr_destroy when no longer needed.
int pthread_getattr_np(pthread_t th, pthread_attr_t* attr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

