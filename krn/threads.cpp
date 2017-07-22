//##################################################################################################
//
//  Threads container.
//
//##################################################################################################

#include "threads.h"

// ...

// static data
Int_thread_t      Threads_t::_int_threads [Kcfg::Ints_max];
Thread_t          Threads_t::_threads     [Kcfg::Threads_max];
Threads_t::bits_t Threads_t::_ready_bits[(Thread_t::Prio_max+1) / (sizeof(bits_t)*8)];
threads_t         Threads_t::_ready_threads[Thread_t::Prio_max+1];
threads_t         Threads_t::_send_threads;
threads_t         Threads_t::_timeout_waiting_rcv_threads;
threads_t         Threads_t::_timeout_waiting_snd_threads;

Thread_t*         threads_find(L4_thrid_t id)                           { return Threads_t::find(id); }
Thread_t*         threads_get_highest_prio_ready_thread()               { return Threads_t::get_highest_prio_ready_thread(); }
threads_t::iter_t threads_add_ready(Thread_t* thr)                      { return Threads_t::add_ready(thr); }
threads_t::iter_t threads_del_ready(threads_t::iter_t it)               { return Threads_t::del_ready(it); }
threads_t::iter_t threads_add_send(Thread_t* thr)                       { return Threads_t::add_send(thr); }
threads_t::iter_t threads_del_send(threads_t::iter_t it)                { return Threads_t::del_send(it); }
threads_t::iter_t threads_add_rcv_timeout_waiting(Thread_t* thr)        { return Threads_t::add_rcv_timeout_waiting(thr); }
threads_t::iter_t threads_del_rcv_timeout_waiting(threads_t::iter_t it) { return Threads_t::del_rcv_timeout_waiting(it); }
threads_t::iter_t threads_add_snd_timeout_waiting(Thread_t* thr)        { return Threads_t::add_snd_timeout_waiting(thr); }
threads_t::iter_t threads_del_snd_timeout_waiting(threads_t::iter_t it) { return Threads_t::del_snd_timeout_waiting(it); }
threads_t::iter_t threads_timeslice_expired(Thread_t* thr)              { return Threads_t::timeslice_expired(thr); }
