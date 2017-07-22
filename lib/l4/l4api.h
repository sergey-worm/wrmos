//##################################################################################################
//
//  l4api.h - API to L4 microkernel.
//
//##################################################################################################

#ifndef L4API_H
#define L4API_H

#include "kip.h"
#include "l4types.h"
#include "l4ipcerr.h"
#include "l4kdbops.h"

// TODO
//#ifdef __cplusplus
//extern "C" {
//#endif

L4_kip_t*  l4_kip();                  // get pointer to KIP
L4_utcb_t* l4_utcb();                 // get pointer to UTCB
int        l4_ipc(L4_thrid_t to, L4_thrid_t from_spec, L4_timeouts_t timeouts, L4_thrid_t& from); // obsolete
int        l4_ipc(L4_thrid_t to, L4_thrid_t from_spec, L4_timeouts_t timeouts, L4_thrid_t* from=0);
int        l4_send(L4_thrid_t to, L4_time_t timeout);
int        l4_receive(L4_thrid_t from_spec, L4_time_t timeout, L4_thrid_t& from); // obsolete
int        l4_receive(L4_thrid_t from_spec, L4_time_t timeout, L4_thrid_t* from=0);
int        l4_thread_control(L4_thrid_t dest, L4_thrid_t space, L4_thrid_t sched, L4_thrid_t pager, addr_t utcb);
int        l4_space_control(L4_thrid_t space, word_t control, L4_fpage_t kip, L4_fpage_t utcbs, L4_thrid_t redirector);
int        l4_memory_control(word_t control, word_t attr0=0, word_t attr1=0, word_t attr2=0, word_t attr3=0); // fpages in MRs
L4_clock_t l4_system_clock();
void       l4_thread_switch(L4_thrid_t dest);
int        l4_schedule(L4_thrid_t dest, word_t time_ctl, word_t proc_ctl, word_t prio,
                       word_t preempt_ctl, word_t* state=0, word_t* ret_time_ctl=0);
int        l4_exreg(L4_thrid_t* dest, word_t ctrl, word_t* sp=0, word_t* ip=0,
                       word_t* flags=0, L4_thrid_t* pager=0, word_t* usr_def_handle=0);
void       l4_kdb(const char* str, bool error_entry = true);   // switch to KDB
int        l4_kdb(word_t opcode, word_t param, void* buf=0, size_t sz=0);

void l4_out_string(const char* str, size_t len);

// TODO
//#ifdef __cplusplus
//}
//#endif

#endif // L4API_H
