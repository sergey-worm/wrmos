//##################################################################################################
//
//  Implementation of some funcs for unistd.h.
//
//##################################################################################################

#include <unistd.h>
#include "l4_api.h"
#include "l4_ipcerr.h"

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
