//##################################################################################################
//
//  Semaphore - semaphore implementation.
//
//##################################################################################################

#include "wrm_sem.h"
#include "wrm_log.h"
#include "l4_api.h"
#include <assert.h>
#include <string.h>

// return ipc err code
static int suspend(int timeout_usec)
{
	//wrm_logd("sem:  suspend:  wait unlock ipc (sw irq) for %d usec.\n", timeout_usec);

	L4_time_t timeout = timeout_usec == Wrm_sem_timeout_infinite ?
	                    L4_time_t::Never : L4_time_t::create_rel(timeout_usec);

	L4_utcb_t* utcb = l4_utcb();
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_msgtag_t tag;
	tag.ipc_label(0x202); // 0x202 - sw irq trigger
	utcb->msgtag(tag);    // NOTE:  label for rcv

	// wait sw irq
	int rc = l4_receive(utcb->global_id(), timeout, &from);

	// checkings
	//wrm_logd("sem:  suspend:  wait unlock ipc for %d usec: rc=%d, from=%u, id=%u.\n",
	//	timeout_usec, rc, from.number(), utcb->global_id().number());
	assert(!rc || rc == L4_ipc_timeout);
	assert(rc  || from == utcb->global_id());
	//assert(rc  || tag.ipc_label() == 0x202);  // 0x202 - sw irq msg

	return rc;
}

// trigger software irq to thr
static void resume(L4_thrid_t thr)
{
	//wrm_logd("sem:  resume:  send ipc (sw irq) to=%u.\n", thr.number());
	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.ipc_label(0x202); // 0x202 - sw irq trigger
	utcb->msgtag(tag);

	int rc = l4_send(thr, L4_time_t::Zero);

	if (rc)
	{
		wrm_loge("sem:  resume:  sent ipc (sw irq), rc=%d, to=0x%lx/%u.\n", rc, thr.raw(), thr.number());
		assert(0 && "l4_send() failed.");
	}

	utcb->msgtag(L4_msgtag_t()); // to avoid wrong 'sw irq trigger' // DELME
}

extern "C" int wrm_sem_init(Wrm_sem_t* sem, int mode, unsigned value)
{
	if (mode & Wrm_sem_recursive  &&  !(mode & Wrm_sem_binary))
	{
		wrm_loge("sem=0x%p:  init:  wrong mode.\n", sem);
		return 1;
	}

	if (value > Wrm_sem_value_max)
	{
		wrm_loge("sem=0x%p:  init:  too big value.\n", sem);
		return 2;
	}

	memset(sem, 0, sizeof(Wrm_sem_t));
	wrm_spinlock_init(&sem->lock);
	sem->mode = mode;
	sem->value = value;
	if (mode & Wrm_sem_binary  &&  value)
		sem->value = 1;
	return 0;
}

extern "C" int wrm_sem_destroy(Wrm_sem_t* sem)
{
	memset(sem, 0, sizeof(Wrm_sem_t));
	return 0;
}

extern "C" int wrm_sem_setvalue(Wrm_sem_t* sem, unsigned value)
{
	if (value > Wrm_sem_value_max)
	{
		wrm_loge("sem=0x%p:  init:  too big value.\n", sem);
		return 2;
	}

	wrm_spinlock_lock(&sem->lock);
	sem->value = value;
	if (sem->mode & Wrm_sem_binary  &&  value)
		sem->value = 1;
	wrm_spinlock_unlock(&sem->lock);
	return 0;
}

extern "C" int wrm_sem_getvalue(Wrm_sem_t* sem, unsigned* value)
{
	wrm_spinlock_lock(&sem->lock);
	*value = sem->value;
	wrm_spinlock_unlock(&sem->lock);
	return 0;
}

// return 1 if found and 0 if not found
static int dequeue(Wrm_sem_t* sem, L4_thrid_t thr)
{
	//wrm_logd("sem=0x%p:  wait:  timeout, erase ptr (rp=%u, wp=%u).\n", sem, sem->rp, sem->wp);
	unsigned p = sem->rp;
	bool found = false;
	while (p != sem->wp)
	{
		if (sem->wait_queue[p] == thr)
			found = true;
		unsigned next_p = (p + 1) == Wrm_sem_queue_length  ?  0  :  (p + 1);
		if (found)
			sem->wait_queue[p] = sem->wait_queue[next_p]; // move
		p = next_p;
	}
	if (found)
		sem->wp = !sem->wp ? (Wrm_sem_queue_length - 1)  : (sem->wp - 1);
	//wrm_logd("sem=0x%p:  wait:  timeout, erased (rp=%u, wp=%u).\n", sem, sem->rp, sem->wp);
	return found;
}

// return 1 if found and 0 if not found
static int exist_in_queue(Wrm_sem_t* sem, L4_thrid_t thr)
{
	unsigned p = sem->rp;
	while (p != sem->wp)
	{
		if (sem->wait_queue[p] == thr)
			return 1;
		p = (p + 1) == Wrm_sem_queue_length  ?  0  :  (p + 1);
	}
	return 0;
}

extern "C" int wrm_sem_wait(Wrm_sem_t* sem, int timeout_usec)
{
	//wrm_logd("sem=0x%p:  wait:  entry.\n", sem);
	L4_thrid_t self = l4_utcb()->global_id();

	if (timeout_usec < 0  &&  timeout_usec != Wrm_sem_timeout_infinite)
	{
		wrm_loge("sem=0x%p:  wait:  wrong timeout=%d.\n", sem, timeout_usec);
		return 1;
	}

	wrm_spinlock_lock(&sem->lock);

	if (sem->mode & Wrm_sem_recursive  &&  sem->owner == self)
	{
		wrm_spinlock_unlock(&sem->lock);
		return 0;
	}

	if (sem->value)
	{
		if (sem->mode & Wrm_sem_binary)
			sem->owner = self;
		sem->value--;
		wrm_spinlock_unlock(&sem->lock);
		return 0;
	}

	if (!timeout_usec)
	{
		// null timeout, don't wait
		wrm_spinlock_unlock(&sem->lock);
		return Wrm_sem_err_timeout;
	}

	// write thread to queue
	unsigned next_wp = (sem->wp + 1) == Wrm_sem_queue_length  ?  0  :  (sem->wp + 1);
	if (next_wp == sem->rp)
	{
		wrm_loge("sem=0x%p:  wait:  waiting queue is full.\n", sem);
		wrm_spinlock_unlock(&sem->lock);
		return 2;  // waiting queue is full
	}

	sem->wait_queue[sem->wp] = self;
	sem->wp = next_wp;
	//wrm_logd("sem=0x%p:  wait:  enqueue:  rp=%u, wp=%u.\n", sem, sem->rp, sem->wp);
	wrm_spinlock_unlock(&sem->lock);

	int rc = 0;
	while (1)
	{
		rc = suspend(timeout_usec);             // wait post operation
		if (rc)                                 // timeout or ipc error
			break;
		wrm_spinlock_lock(&sem->lock);
		int exist = exist_in_queue(sem, self);  // is really post has been? (post remove ptr from queue)
		wrm_spinlock_unlock(&sem->lock);
		if (!exist)
			break;
	}

	// if rc==0 -> normal resume by post() -> don't decrement sem value after resume
	// if rc!=0 -> resume by timeout or ipc error -> erase ptr from wait_queue
	if (rc)
	{
		wrm_spinlock_lock(&sem->lock);
		int found = dequeue(sem, self);
		wrm_spinlock_unlock(&sem->lock);
		if (!found  &&  rc == L4_ipc_timeout)
		{
			wrm_logw("sem=0x%p:  wait:  post done, but timeout -> assume suspend ok.\n", sem);
			rc = 0;  // post has been done but timeout occured -> assume suspend was succes
		}
	}

	return !rc ? 0 : (rc==L4_ipc_timeout ? Wrm_sem_err_timeout : 3);
}

extern "C" int wrm_sem_post(Wrm_sem_t* sem)
{
	//wrm_logd("sem=0x%p:  post:  entry.\n", sem);
	wrm_spinlock_lock(&sem->lock);
	if (sem->value == Wrm_sem_value_max)
	{
		wrm_loge("sem=0x%p:  post:  too big value, max=%u, overflow.\n", sem, Wrm_sem_value_max);
		wrm_spinlock_unlock(&sem->lock);
		return 1;
	}

	L4_thrid_t thr = L4_thrid_t::Nil;
	if (sem->wp != sem->rp)
	{
		// exist waiting threads --> resume, don't increment sem value
		//wrm_logd("sem=0x%p:  post:  resume waiting thread.\n", sem);
		assert(!sem->value);
		thr = sem->wait_queue[sem->rp];
		assert(thr != l4_utcb()->global_id());
		sem->rp = (sem->rp + 1) == Wrm_sem_queue_length  ?  0  :  (sem->rp + 1);
	}
	else
	{
		// no waiting threads --> just increment sem value
		//wrm_logd("sem=0x%p:  post:  no waiting threads.\n", sem);
		sem->value++;
		if (sem->mode & Wrm_sem_binary)
			sem->value = 1;
	}
	if (sem->mode & Wrm_sem_binary)
		sem->owner = thr;
	wrm_spinlock_unlock(&sem->lock);

	if (!thr.is_nil())
		resume(thr);

	return 0;
}
