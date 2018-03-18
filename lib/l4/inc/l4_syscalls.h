//##################################################################################################
//
//  Syscalls IDs and constants.
//
//##################################################################################################

#ifndef L4_SYSCALLS_H
#define L4_SYSCALLS_H

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

enum
{
	L4_exreg_ctl_d = 1 << 9, // deliver reg value
	L4_exreg_ctl_h = 1 << 8, // change H-flag
	L4_exreg_ctl_p = 1 << 7, // change pager
	L4_exreg_ctl_u = 1 << 6, // change user-defined-handle
	L4_exreg_ctl_f = 1 << 5, // change flags
	L4_exreg_ctl_i = 1 << 4, // change ip
	L4_exreg_ctl_s = 1 << 3, // change sp
	L4_exreg_ctl_S = 1 << 2, // cancel waiting of snd-ipc or abort of abort processing snd-ipc
	L4_exreg_ctl_R = 1 << 1, // cancel waiting of rcv-ipc or abort of abort processing rcv-ipc
	L4_exreg_ctl_H = 1 << 0  // halt thread
};

enum
{
	L4_flags_frcexc = 1 << 1,  // force exception
	L4_flags_fpu    = 1 << 0   // FPU is allowed
};

#endif // L4_SYSCALLS_H
