//##################################################################################################
//
//  Semaphore - semaphore implementation.
//
//##################################################################################################

#ifndef WRM_SEM_H
#define WRM_SEM_H

#include "wrm_slock.h"
#include "wrm_default.h"

enum
{
	//Wrm_sem_counting,
	Wrm_sem_binary    = 1 << 0,
	Wrm_sem_recursive = 1 << 1
};

enum
{
	Wrm_sem_timeout_infinite = -1
};

enum
{
	Wrm_sem_err_ok      =  0,
	Wrm_sem_err_timeout = 99
};


enum
{
	Wrm_sem_queue_length = 8,
	Wrm_sem_value_max    = 256
};

typedef struct
{
	Wrm_spinlock_t lock;                      //
	unsigned       value;                     //
	L4_thrid_t     owner;                     // used only for Binary semaphore
	L4_thrid_t     wait_queue[Wrm_sem_queue_length]; // capacity=Queue_length-1
	unsigned       rp;                        // read pointer
	unsigned       wp;                        // write pointer
	int            mode;                      //
} Wrm_sem_t;

#define Wrm_sem_binary_locked_initializer \
  { 0, 0, 0, {0}, 0, 0, Wrm_sem_binary }

#define Wrm_sem_binary_unlocked_initializer \
  { 0, 1, 0, {0}, 0, 0, Wrm_sem_binary }

#define Wrm_sem_binary_recursive_locked_initializer \
  { 0, 0, 0, {0}, 0, 0, Wrm_sem_binary | Wrm_sem_recursive 

#define Wrm_sem_binary_recursive_unlocked_initializer \
  { 0, 1, 0, {0}, 0, 0, Wrm_sem_binary | Wrm_sem_recursive }

#ifdef __cplusplus
extern "C" {
#endif

int wrm_sem_init(Wrm_sem_t* sem, int mode, unsigned value);
int wrm_sem_destroy(Wrm_sem_t* sem);
int wrm_sem_setvalue(Wrm_sem_t* sem, unsigned value);
int wrm_sem_getvalue(Wrm_sem_t* sem, unsigned* value);
int wrm_sem_wait(Wrm_sem_t* sem, int timeout_usec Default(Wrm_sem_timeout_infinite));
int wrm_sem_post(Wrm_sem_t* sem);

#ifdef __cplusplus
}
#endif

#endif // WRM_SEM_H

