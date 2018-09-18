//##################################################################################################
//
//  startup.cpp - entry point of user applications.
//
//##################################################################################################

#include "sys_stack.h"
#include "l4_api.h"
#include "l4_thrid.h"
#include "wlibc_cb.h"
#include "wrm_thr.h"
#include "wrm_mem.h"
#include "wrm_log.h"
#include "wrm_mpool.h"
#include "wrm_labels.h"
#include "wlibc_panic.h"
#include <string.h>
#include <assert.h>
#include <unistd.h>

int main(int, const char**);

static void break_execution(const char* str)
{
	bool error_entry = true;
	l4_kdb(str, error_entry);
}

// Declare 'weak' to allow user code redefenite function.
// It allows use user-defined malloc-empty callback in ctors context.
int wrm_malloc_empty_default_callback(size_t sz) __attribute__((weak));
int wrm_malloc_empty_default_callback(size_t sz)
{
	//wrm_logw("%s:  req=0x%zx bytes.\n", __func__, sz);
	sz = (sz <= Cfg_page_sz) ? Cfg_page_sz : (1 << get_log2sz_top_range(sz));
	L4_fpage_t fp = wrm_pgpool_alloc(sz);
	if (fp.is_nil())
	{
		wrm_loge("%s:  wrm_pgpool_alloc() - failed.\n", __func__);
		return -1;
	}
	//wrm_logw("%s:  wrm_pgpool_alloc() - ok.\n", __func__);
	int rc = wrm_mpool_add((void*)fp.addr(), fp.size());
	if (rc)
	{
		wrm_loge("%s:  wrm_mpool_add() - rc=%d.\n", __func__, rc);
		return -2;
	}
	//wrm_logw("%s:  wrm_mpool_add() - ok.\n", __func__);
	return 0;
}

static void init_wlibc_callbacks()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char     = NULL;
	cb->out_string   = l4_kdb_putsn;
	cb->in_char      = NULL;
	cb->in_string    = NULL;
	cb->malloc       = wrm_mpool_alloc;
	cb->malloc_empty = wrm_malloc_empty_default_callback;
	cb->free         = wrm_mpool_free;
	cb->break_exec   = break_execution;
}

static void before_cb(size_t addr)
{
	//wrm_logd("wrm:  call ctor:  0x%x  before.\n", addr);
}

static void after_cb(size_t addr)
{
	//wrm_logd("wrm:  call ctor:  0x%x  after.\n", addr);
}

// get from alpha fpages begin from 2GB till 1page
static void init_heap()
{
	//wrm_logd("get memory from alpha.\n");

	size_t req_size    = 0x10000000;  // 256 MB
	addr_t heap_vspace = 0xe0000000;  // TODO:  allocate vspace

	while (1)
	{
		//wrm_logd("req memory:  addr=0x%lx, sz=0x%zx.\n", heap_vspace, req_size);
		if (heap_vspace + req_size > 0xf0000000)
		{
			wrm_loge("not enough vspace for heap:  addr=0x%lx, sz=0x%zx.\n", heap_vspace, req_size);
			break;
		}
		int rc = wrm_mem_get_usual(heap_vspace, req_size);
		if (rc == -5)  // heap not enough
		{
			// map reject
			//wrm_logd("map reject.\n");
			if (req_size == Cfg_page_sz)
				break;
			req_size >>= 1;
			continue;
		}
		else if (rc)
		{
			wrm_loge("wrm_mem_get_usual() - rc=%d.\n", rc);
			break;
		}

		L4_fpage_t fpage = L4_fpage_t::create(heap_vspace, req_size, L4_fpage_t::Acc_rw);
		assert(!fpage.is_nil());
		heap_vspace += req_size;
		memset((void*)fpage.addr(), 0, fpage.size());  // to check unmapped addresses
		wrm_pgpool_add(fpage);
	}

	//wrm_logd("got memory:  %#zx bytes.\n", wrm_pgpool_size());
}

// application entry point
// there are 3 params on top of the stack:  fpu_enable, argc, argv
// use assm code to avoid function epilogue influens on SP
extern "C" void _start()
{
	#if defined (Cfg_arch_sparc)
	asm volatile
	(
		"ld [%sp + 0], %o0   \n"
		"ld [%sp + 4], %o1   \n"
		"ld [%sp + 8], %o2   \n"
		"add %sp, 12, %sp    \n"
		"add %sp, -96, %sp   \n"
		"b _startup          \n"
		" nop                \n"
	);
	#elif defined (Cfg_arch_arm)
	asm volatile
	(
		"pop {r0, r1, r2}    \n"
		"b _startup          \n"
	);
	#elif defined (Cfg_arch_x86)
	asm volatile
	(
		"call _startup       \n"
	);
	#elif defined (Cfg_arch_x86_64)
	asm volatile
	(
		"pop %rdi            \n"
		"pop %rsi            \n"
		"pop %rdx            \n"
		"call _startup       \n"
	);
	#else
	#error Unknown arch.
	#endif
}

//#include "l4_api.h"

// app data - used inside wrm_thr_init_tls()
extern word_t tls_s;
extern word_t tls_e;
extern word_t tdata_s;
extern word_t tdata_e;
extern word_t tbss_s;
extern word_t tbss_e;

extern "C" void wrm_thr_init_tls();

static void init_tls_ranges()
{
	extern int _tls_start;
	extern int _tls_end;
	extern int _tdata_start;
	extern int _tdata_end;
	extern int _tbss_start;
	extern int _tbss_end;
	tls_s   = (word_t) &_tls_start;
	tls_e   = (word_t) &_tls_end;
	tdata_s = (word_t) &_tdata_start;
	tdata_e = (word_t) &_tdata_end;
	tbss_s  = (word_t) &_tbss_start;
	tbss_e  = (word_t) &_tbss_end;
}

extern "C" void _startup(word_t fpu, word_t argc, word_t argv)
{
	init_bss();                        // clear .bss section
	init_wlibc_callbacks();            // initialize wlibc callbacks
	init_heap();                       // get memory from alpha
	init_tls_ranges();                 //
	wrm_thr_init_tls();                // alocate and setup TLS

	// enable fpu if need
	if (fpu)
	{
		word_t flags = Wrm_thr_flag_fpu;
		int rc = l4_exreg_flags(l4_utcb()->local_id(), &flags, NULL);
		if (rc)
			panic("l4_exreg_flags(fpu) - rc=%u.\n", rc);
	}

	call_ctors(before_cb, after_cb);   // call user ctors

	long rc = main(argc, (const char**)argv);

	call_dtors();

	L4_utcb_t* utcb = l4_utcb();
	L4_thrid_t thrid = utcb->global_id();
	//wrm_logw("wrm:  main thread=%u terminated with rc=%ld.\n", thrid.number(), rc);

	// notify alpha about thread termination - alpha will delete all app threads and free all resources
	int term_state;
	L4_fpage_t utcb_fp;
	addr_t stack;
	size_t stack_sz;
	wrm_thr_delete(thrid, &rc, &term_state, &utcb_fp, &stack, &stack_sz);
	l4_kdb("main thread terminated but is executing");
}
