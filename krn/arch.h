//##################################################################################################
//
//  Arch - arch specifics kernel functions.
//
//##################################################################################################

#ifndef ARCH_H
#define ARCH_H

#include "sys_types.h"
//#include "arch_data.h"

void arch_init();
void arch_set_timer_va(long va);
void arch_set_ksp(long ksp);
long arch_get_ksp();
void arch_user_invoke();
void arch_switch_cpu(addr_t* cur_ksp, addr_t nxt_ksp, word_t nxt_utcb);
void arch_store_floats(void* float_frame);
void arch_restore_floats(const void* float_frame);

#endif // ARCH_H
