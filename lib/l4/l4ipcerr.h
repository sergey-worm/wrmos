//##################################################################################################
//
//  l4apcerr.h - IPC error codes.
//
//##################################################################################################

#ifndef L4IPCERR_H
#define L4IPCERR_H

enum L4_ipc_err_t
{
	Ipc_ok            = 0,  // success
	Ipc_timeout       = 1,  // send or receive timeout
	Ipc_no_partner    = 2,  // Non-existing partner
	Ipc_canceled      = 3,  // canceled by another thread (scall exchange registers)
	Ipc_overflow      = 4,  // message overflow
	Ipc_xfer_tout_inv = 5,  // xfer timeout during page fault in the invoker’s address space
	Ipc_xfer_tout_par = 6,  // xfer timeout during page fault in the partner’s address space
	Ipc_aborted       = 7   // aborted by another thread (scall exchange registers)
};

#endif // L4IPCERR_H
