//##################################################################################################
//
//  startup.cpp - entry point of user applications.
//
//##################################################################################################

#include "sys_stack.h"
#include "l4_api.h"
#include "wlibc_cb.h"
#include "wrm_thr.h"
#include "wrm_mem.h"
#include "wrm_mpool.h"
#include "wrm_log.h"
#include "wlibc_panic.h"
#include <string.h>
#include <assert.h>

int main(int, const char**);

static void break_execution(const char* str)
{
	bool error_entry = true;
	l4_kdb(str, error_entry);
}

static void init_wlibc_callbacks()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = NULL;
	cb->out_string = l4_kdb_putsn;
	cb->in_char    = NULL;
	cb->in_string  = NULL;
	cb->malloc     = wrm_malloc;
	cb->break_exec = break_execution;
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
	addr_t heap_vspace = 0xe0000000;  // TODO:  allocate vspave

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
		wrm_mpool_add(fpage);
	}

	//wrm_logd("got memory:  %#zx bytes.\n", wrm_mpool_size());
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

//extern "C" void _start()
extern "C" void _startup(word_t fpu, word_t argc, word_t argv)
{
	init_bss();                        // clear .bss section
	init_wlibc_callbacks();            // initialize wlibc callbacks
	init_heap();                       // get memory from alpha

	// enable fpu if need
	if (fpu)
	{
		word_t flags = Wrm_thr_flag_fpu;
		int rc = l4_exreg_flags(l4_utcb()->local_id(), &flags, NULL);
		if (rc)
			panic("l4_exreg_flags(fpu) - rc=%u.\n", rc);
	}

	call_ctors(before_cb, after_cb);   // call user ctors

	int rc = main(argc, (const char**)argv);
	(void)rc;

	//wrm_loge("wrm:  main() terminated with rc=%d.\n\n", rc);

	call_dtors();

	// TODO:  now need to terminate app - send msg to alph
	// now just workaround:
	l4_kdb("App terminated.");

	while (1);
}
