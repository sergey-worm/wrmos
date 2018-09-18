//##################################################################################################
//
//  Pthread mutex attributes hangling.
//
//##################################################################################################

#include <pthread.h>
#include <stdio.h>

// Initialize mutex attribute object ATTR with default attributes
// (kind is PTHREAD_MUTEX_TIMED_NP).
int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Destroy mutex attribute object ATTR.
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Get the process-shared flag of the mutex attribute ATTR.
int pthread_mutexattr_getpshared(const pthread_mutexattr_t* restrict attr,
                                 int *__restrict __pshared)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the process-shared flag of the mutex attribute ATTR.
int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return in *KIND the mutex kind attribute in *ATTR.
int pthread_mutexattr_gettype(const pthread_mutexattr_t* restrict attr, int* restrict kind)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the mutex kind attribute in *ATTR to KIND (either PTHREAD_MUTEX_NORMAL,
// PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ERRORCHECK, or
// PTHREAD_MUTEX_DEFAULT).
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int kind)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return in *PROTOCOL the mutex protocol attribute in *ATTR.
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t* restrict attr,
                                  int* restrict protocol)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the mutex protocol attribute in *ATTR to PROTOCOL (either
// PTHREAD_PRIO_NONE, PTHREAD_PRIO_INHERIT, or PTHREAD_PRIO_PROTECT).
int pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr, int protocol)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Return in *PRIOCEILING the mutex prioceiling attribute in *ATTR.
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t* restrict attr,
                                     int* restrict prioceiling)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the mutex prioceiling attribute in *ATTR to PRIOCEILING.
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t* attr, int prioceiling)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Get the robustness flag of the mutex attribute ATTR.
int pthread_mutexattr_getrobust(const pthread_mutexattr_t* attr, int* robustness)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

int pthread_mutexattr_getrobust_np(const pthread_mutexattr_t* attr, int* robustness)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

// Set the robustness flag of the mutex attribute ATTR.
int pthread_mutexattr_setrobust(pthread_mutexattr_t* attr, int robustness)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}

int pthread_mutexattr_setrobust_np(pthread_mutexattr_t* attr, int robustness)
{
	printf("%s:  implement me!\n", __func__);
	return 0;
}
