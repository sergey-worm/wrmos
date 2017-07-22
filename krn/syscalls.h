//##################################################################################################
//
//  L4 syscalls.
//
//##################################################################################################

#ifndef SYSCALLS_H
#define SYSCALLS_H

class Thread_t;
class Entry_frame_t;

void syscall_exchange_registers(Thread_t& cur, Entry_frame_t& eframe);
void syscall_thread_control(Thread_t& cur, Entry_frame_t& eframe);
void syscall_thread_switch(Thread_t& cur, Entry_frame_t& eframe);
void syscall_schedule(Thread_t& cur, Entry_frame_t& eframe);
void syscall_ipc(Thread_t& cur, Entry_frame_t& eframe);
void syscall_space_control(Thread_t& cur, Entry_frame_t& eframe);
void syscall_memory_control(Thread_t& cur, Entry_frame_t& eframe);
void syscall_kdb(Thread_t& cur, Entry_frame_t& eframe);

#endif // SYSCALLS_H
