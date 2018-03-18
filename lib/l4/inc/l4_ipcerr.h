//##################################################################################################
//
//  IPC error codes.
//
//##################################################################################################

#ifndef L4_IPCERR_H
#define L4_IPCERR_H

enum L4_ipc_err_t
{
	L4_ipc_ok            = 0,  // success
	L4_ipc_timeout       = 1,  // send or receive timeout
	L4_ipc_no_partner    = 2,  // Non-existing partner
	L4_ipc_canceled      = 3,  // canceled by another thread (scall exchange registers)
	L4_ipc_overflow      = 4,  // message overflow
	L4_ipc_xfer_tout_inv = 5,  // xfer timeout during page fault in the invoker’s address space
	L4_ipc_xfer_tout_par = 6,  // xfer timeout during page fault in the partner’s address space
	L4_ipc_aborted       = 7   // aborted by another thread (scall exchange registers)
};

static inline const char* l4_ipcerr2str(int ipcerr)
{
	switch (ipcerr)
	{
		case L4_ipc_ok:             return "ok";
		case L4_ipc_timeout:        return "timeout";
		case L4_ipc_no_partner:     return "no_partner";
		case L4_ipc_canceled:       return "canceled";
		case L4_ipc_overflow:       return "overflow";
		case L4_ipc_xfer_tout_inv:  return "xfer_tout_inv";
		case L4_ipc_xfer_tout_par:  return "xfer_tout_par";
		case L4_ipc_aborted:        return "aborted";
	}
	return "__unknown_ips_error__";
}

#endif // L4_IPCERR_H
