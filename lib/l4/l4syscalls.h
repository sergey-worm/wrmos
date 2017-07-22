//##################################################################################################
//
//  l4syscalls.h - syscalls IDs.
//
//##################################################################################################

#ifndef L4SYSCALLS_H
#define L4SYSCALLS_H

enum
{
	L4_syscall_kernel_interface   =  1,
	L4_syscall_exchange_registers =  2,
	L4_syscall_thread_control     =  3,
	L4_syscall_system_clock       =  4,
	L4_syscall_thread_switch      =  5,
	L4_syscall_schedule           =  6,
	L4_syscall_ipc                =  7,
	L4_syscall_lipc               =  8,
	L4_syscall_unmap              =  9,
	L4_syscall_space_control      = 10,
	L4_syscall_processor_control  = 11,
	L4_syscall_memory_control     = 12,
	L4_syscall_kdb                = 13  // L4 extention
};
/*
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
*/
enum
{
	L4_exreg_ctl_d = 1 << 9,
	L4_exreg_ctl_h = 1 << 8,
	L4_exreg_ctl_p = 1 << 7,
	L4_exreg_ctl_u = 1 << 6,
	L4_exreg_ctl_f = 1 << 5,
	L4_exreg_ctl_i = 1 << 4,
	L4_exreg_ctl_s = 1 << 3,
	L4_exreg_ctl_S = 1 << 2,
	L4_exreg_ctl_R = 1 << 1,
	L4_exreg_ctl_H = 1 << 0
};

#endif // L4SYSCALLS_H
