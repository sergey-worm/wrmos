//##################################################################################################
//
//  Semaphore - semaphore implementation.
//
//##################################################################################################

#ifndef WRM_SEM_H
#define WRM_SEM_H

#include "wrm_slock.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	Wrm_sem_counting,
	Wrm_sem_binary
};

enum
{
	Wrm_sem_timeout_infinite = -1
};

enum
{
	Wrm_sem_err_ok      =   0,
	Wrm_sem_err_timeout = 100
};

#ifndef __cplusplus
enum
{
	Queue_length = 8,                    // just limitation
	Value_max    = 256                   // just limitation
};
#endif

typedef struct
{
	#ifdef __cplusplus
	enum
	{
		Queue_length = 8,                    // just limitation
		Value_max    = 256                   // just limitation
	};
	#endif
	Wrm_spinlock_t lock;                     //
	unsigned      value;                     //
	L4_thrid_t    wait_queue [Queue_length]; // capacity = Queue_length - 1
	unsigned      rp;                        // read pointer
	unsigned      wp;                        // write pointer
	int           mode;                      //
} Wrm_sem_t;

int wrm_sem_init(Wrm_sem_t* sem, int mode, unsigned value);
int wrm_sem_destroy(Wrm_sem_t* sem);
int wrm_sem_value(Wrm_sem_t* sem, unsigned value);
#ifdef __cplusplus
 int wrm_sem_wait(Wrm_sem_t* sem, int timeout_usec = Wrm_sem_timeout_infinite);
#else
 int wrm_sem_wait(Wrm_sem_t* sem, int timeout_usec);
#endif
int wrm_sem_post(Wrm_sem_t* sem);

#ifdef __cplusplus
}
#endif

#endif // WRM_SEM_H

