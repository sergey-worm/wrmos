//##################################################################################################
//
//  Some singleton static data.
//
//##################################################################################################

#include "ktimer.h"

uintptr_t Timer::_addr = -1;
unsigned  Timer::_sysclock_hz = 0;
unsigned  Timer::_period_usec = 0;

#include "kintc.h"

uintptr_t Intc::_addr = -1;

#include "kuart.h"

addr_t Uart::_addr = -1;

#include "sysclock.h"

L4_clock_t SystemClock_t::_sys_clock = 0;

#include "kmem.h"

Aspace::aspace_t Aspace::_kspace;
Aspace::ranges_t Aspace::_ranges;
psize_t Aspace::_diff_kva_kpa = 0;
unsigned Aspace::_cur_mmu_ctxid = 0;
Kmem::memory_t Kmem::memory;
uint8_t Kmem::premapped_mem [Pool_init_sz] __attribute__((aligned(Cfg_page_sz))); // premapped memory
size_t Kmem::pool_sz = Pool_init_sz;

#include "ptable.h"

Ktab_t* Pgtab::kerntb[Ktbs];
#ifdef USE_CTXTB
word_t* Pgtab::ctxtb = 0;
#endif

#include "task.h"

unsigned Task_t::_counter = 0;
Tasks_t::tasks_t Tasks_t::_tasks;

#include "sched.h"

Thread_t* Sched_t::_current = 0;

#include "log.h"

Log_buf_t Log::_cbuf;
