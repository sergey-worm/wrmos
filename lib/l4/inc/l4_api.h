//##################################################################################################
//
//  API to L4 microkernel.
//
//##################################################################################################

#ifndef L4_API_H
#define L4_API_H

#include "l4_kip.h"
#include "l4_types.h"
#include "l4_ipcerr.h"
#include "l4_kdbops.h"
#include "l4_syscalls.h"

// TODO
//#ifdef __cplusplus
//extern "C" {
//#endif

L4_kip_t*  l4_kip();                  // get pointer to KIP
L4_utcb_t* l4_utcb();                 // get pointer to UTCB
int        l4_ipc(L4_thrid_t to, L4_thrid_t from_spec, L4_timeouts_t timeouts, L4_thrid_t* from=0);
int        l4_send(L4_thrid_t to, L4_time_t timeout);
int        l4_receive(L4_thrid_t from_spec, L4_time_t timeout, L4_thrid_t* from=0);
void       l4_unmap(word_t control); // fpages in MRs
int        l4_thread_control(L4_thrid_t dest, L4_thrid_t space, L4_thrid_t sched, L4_thrid_t pager, addr_t utcb);
int        l4_space_control(L4_thrid_t space, word_t control, L4_fpage_t kip, L4_fpage_t utcbs, L4_thrid_t redirector);
int        l4_memory_control(word_t control, word_t attr0=0, word_t attr1=0, word_t attr2=0, word_t attr3=0); // fpages in MRs
#ifdef __cplusplus
extern "C" {
#endif
L4_clock_t l4_system_clock();
#ifdef __cplusplus
}
#endif
void       l4_thread_switch(L4_thrid_t dest);
int        l4_schedule(L4_thrid_t dest, word_t time_ctl, word_t proc_ctl, word_t prio,
                       word_t preempt_ctl, word_t* state=0, word_t* ret_time_ctl=0);
int        l4_exreg(L4_thrid_t* dest, word_t ctrl, word_t* sp=0, word_t* ip=0, word_t* flags=0,
                    L4_thrid_t* pager=0, word_t* usr_def_handle=0);
int        l4_kdb(word_t opcode, word_t param, void* buf=0, size_t sz=0);

#ifdef __cplusplus
extern "C" {
#endif

void       l4_kdb(const char* str, int error_entry = true);
void       l4_kdb_putsn(const char* str, size_t len);

#ifdef __cplusplus
}
#endif

static inline int l4_exreg_ip(L4_thrid_t dest, word_t* ip_new, word_t* ip_old)
{
	if (!ip_new && !ip_old)
		return -1;

	if (!ip_new)  // only read ip_old
		return l4_exreg(&dest, L4_exreg_ctl_d, 0, ip_old);

	if (!ip_old)  // only write ip_new
		return l4_exreg(&dest, L4_exreg_ctl_i, 0, ip_new);

	*ip_old = *ip_new;
	return l4_exreg(&dest, L4_exreg_ctl_d | L4_exreg_ctl_i, 0, ip_old);
}

static inline int l4_exreg_flags(L4_thrid_t dest, word_t* flags_new, word_t* flags_old)
{
	if (!flags_new && !flags_old)
		return -1;

	if (!flags_new)  // only read flags_old
		return l4_exreg(&dest, L4_exreg_ctl_d, 0, 0, flags_old);

	if (!flags_old)  // only write flags_new
		return l4_exreg(&dest, L4_exreg_ctl_f, 0, 0, flags_new);

	*flags_old = *flags_new;
	return l4_exreg(&dest, L4_exreg_ctl_d | L4_exreg_ctl_f, 0, 0, flags_old);
}

static inline int l4_exreg_pager(L4_thrid_t dest, L4_thrid_t* pgr_new, L4_thrid_t* pgr_old)
{
	if (!pgr_new && !pgr_old)
		return -1;

	if (!pgr_new)  // only read pgr_old
		return l4_exreg(&dest, L4_exreg_ctl_d, 0, 0, 0, pgr_old);

	if (!pgr_old)  // only write pgr_new
		return l4_exreg(&dest, L4_exreg_ctl_p, 0, 0, 0, pgr_new);

	*pgr_old = *pgr_new;
	return l4_exreg(&dest, L4_exreg_ctl_d | L4_exreg_ctl_p, 0, 0, 0, pgr_old);
}

// TODO
//#ifdef __cplusplus
//}
//#endif

#endif // L4API_H
