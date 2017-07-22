//##################################################################################################
//
//  startup.cpp - entry point of user applications.
//
//##################################################################################################

#include "stack.h"
#include "l4api.h"
#include "libc_io.h"
#include "wrm_thr.h"
#include "wrm_log.h"
#include "panic.h"

int main(int, const char**);

static void init_io()
{
	Libc_io_callbacks_t io;
	io.out_char    = NULL;
	io.out_string  = l4_out_string;
	libc_init_io(&io); // init libc handlers
}

static void before_cb(size_t addr)
{
	//wrm_logd("wrm:  call ctor:  0x%x  before.\n", addr);
}

static void after_cb(size_t addr)
{
	//wrm_logd("wrm:  call ctor:  0x%x  after.\n", addr);
}

// application entry point
// there are 3 params on top of the stack:  fpu_enable, argc, argv.
extern "C" void _start()
{
	#if defined (Cfg_arch_sparc)
	asm volatile
	(
		"ld [%sp + 0], %o0   \n"
		"ld [%sp + 4], %o1   \n"
		"ld [%sp + 8], %o2   \n"
		"add %sp, 12, %sp    \n"
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
		"call _startup"
	);
	#elif defined (Cfg_arch_x86_64)
	asm volatile
	(
		"pop %rdi    \n"
		"pop %rsi    \n"
		"pop %rdx    \n"
		"call _startup"
	);
	#else
	#error Unknown arch.
	#endif
}

//extern "C" void _start()
extern "C" void _startup(word_t fpu, word_t argc, word_t argv)
{
	/*  DELME:  bad code, function epilogue influens on SP
	#if defined (Cfg_arch_sparc)
	addr_t sp = Proc::fp();
	#elif defined (Cfg_arch_arm)
	addr_t sp = Proc::sp();
	#endif
	Stack::pop(&sp); // skip
	word_t fpu = Stack::pop(&sp);
	word_t argc = Stack::pop(&sp);
	word_t argv = Stack::pop(&sp);
	*/

	init_bss();                        // clear .bss section
	init_io();                         // initialize IO

	// enable fpu if need
	if (fpu)
	{
		int rc = wrm_thread_flags(l4_utcb()->local_id(), Wrm_thr_flag_fpu);
		if (rc)
			panic("wrm_thread_flags(fpu) - rc=%u.\n", rc);
	}

	call_ctors(before_cb, after_cb);   // call user ctors

	int rc = main(argc, (const char**)argv);
	(void)rc;

	//wrm_loge("wrm:  main() terminated with rc=%d.\n\n", rc);

	call_dtors();

	// TODO:  now need to terminate app
	// now just workaround:
	l4_kdb("App terminated.");

	while (1);
}
