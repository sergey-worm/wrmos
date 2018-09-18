//##################################################################################################
//
//  Implementation of some funcs for unistd.h.
//
//##################################################################################################

#include <unistd.h>
#include "l4_api.h"
#include "l4_ipcerr.h"
#include "wlibc_panic.h"

extern "C" unsigned sleep(unsigned sec)
{
	usleep(sec*1000*1000);
	return 0;
}

// retrun 0 on success, -1 on error
extern "C" int usleep(useconds_t usec)
{
	// block execution by sending msg to self
	L4_utcb_t* utcb = l4_utcb();
	utcb->msgtag(L4_msgtag_t());
	int rc = l4_send(utcb->global_id(), L4_time_t::create_rel(usec));
	return rc == L4_ipc_timeout  ?  0  :  -1;
}

extern "C" ssize_t read(int fildes, void* buf, size_t nbyte)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

extern "C" ssize_t write(int fd, const void* buf, size_t count)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

extern "C" int fstat(int fd, struct stat* buf)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

extern "C" int stat64(const char* pathname, struct stat64* info)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

extern "C" int fstat64(int fildes, struct stat64* buf)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
