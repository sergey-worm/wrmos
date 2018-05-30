//##################################################################################################
//
//  Kernel high-level startup code.
//
//##################################################################################################

#include "kintc.h"
#include "ktimer.h"
#include "kuart.h"
#include "kdb.h"
#include "log.h"
#include "task.h"
#include "libcio.h"
#include "threads.h"
#include "sched.h"
#include "startup.h"
#include "bootstrap.h"
#include <stdarg.h>
#include <assert.h>


void dprint_tmr(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void dprint_tmr(const char* fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	printf("tmr:  %s", buf);
	va_end(args);
}

void dprint_intc(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void dprint_intc(const char* fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	printf_uart("int:  %s", buf);
	va_end(args);
}

void kthread()
{
	printk("kthread:  hello.\n");

	// setup timer
	Timer::init(ktimer_va() + get_pg_offset(Cfg_krn_timer_paddr), Cfg_sys_clock_hz);
	int rc = Timer::set(Cfg_krn_tick_usec);
	if (rc)
		panic("Coud not set timer value:  clock=%u [hz], period=%u [usec].\n",
			Cfg_sys_clock_hz, Cfg_krn_tick_usec);
	Timer::start();
	//Timer::dump(dprint_tmr);

	SystemClock_t::init();

	// setup interrupt controller
	Intc::init(kintc_va() + get_pg_offset(Cfg_krn_intc_paddr));
	Intc::unmask(Cfg_krn_timer_irq);
	//Intc::dump(dprint_intc);

	// create sigma0 task
	Task_t& tsk = Tasks_t::create();
	tsk.name("sigma0");

	// map to sigma0:  1) free phys memory 2) sigma0 memory 3) roottask memory
	L4_kip_t* kip = get_kip();
	const Kip_mem_info_t* minfo = (Kip_mem_info_t*) &kip->memory_info;
	const Mem_desc_t* mdesc = (Mem_desc_t*)(void*)((char*)kip + minfo->mem_desc_ptr);
	for (unsigned i=0; i<minfo->number; ++i)
	{
		addr_t addr = mdesc[i].low << 10;
		size_t sz = ((mdesc[i].heigh + 1) << 10) - addr;
		if (mdesc[i].type == Mem_desc_t::Bootloader)
		{
			// map all memory to sigma0 only
			kacc_t acc = Acc_ufull;
			switch (mdesc[i].type_exact)
			{
				case Mem_desc_t::Bldr_sigma0_text:     acc = Acc_utext;   break;
				case Mem_desc_t::Bldr_sigma0_rodata:   acc = Acc_urodata; break;
				case Mem_desc_t::Bldr_sigma0_data:     acc = Acc_udata;   break;
				case Mem_desc_t::Bldr_roottask_text:   acc = Acc_utext;   break;
				case Mem_desc_t::Bldr_roottask_rodata: acc = Acc_urodata; break;
				case Mem_desc_t::Bldr_roottask_data:   acc = Acc_udata;   break;
				case Mem_desc_t::Bldr_ramfs:           acc = Acc_urx;     break;
				default:
					panic("Unexpected value mem_desc.t=%u.", mdesc[i].type_exact);
			}
			tsk.map(addr, addr, sz, acc, Cachable);
		}
		else if (mdesc[i].type == Mem_desc_t::Conventional)
		{
			// map free ram space
			tsk.map(addr, addr, sz, Acc_ufull, Cachable);
		}
	}

	// map to sigma0 all free address space but RAM (to use as iospace):
	//   a) [Cfg_page_sz, Phys_mem_start);
	//   b) [Phys_mem_end, 0xf0000000).
	if (Cfg_ram_start)
		tsk.map(0x1000, 0x1000, Cfg_ram_start-0x1000, Acc_uio, Cachable);
	addr_t ram_end = Cfg_ram_start + Cfg_ram_sz;
	tsk.map(ram_end, ram_end, min(Cfg_krn_vaddr, 0xf0000000) - ram_end, Acc_uio, Cachable);

	// map to sigma0 all io-ports (to use as iospace)
	tsk.set_ioperm_all(1);

	// set kip_area
	addr_t kip_uva = kmem_paddr(kip, Cfg_page_sz); // 1:1
	L4_fpage_t kip_area = L4_fpage_t::create(kip_uva, Cfg_page_sz, Acc_r);
	tsk.kip_area(kip_area);

	// allocate utcb_location
	addr_t utcb_kva = (addr_t) kmem_alloc(Cfg_page_sz, Cfg_page_sz);
	paddr_t utcb_pa = kmem_paddr(utcb_kva, Cfg_page_sz); // 1:1

	// set utcb_area
	addr_t utcb_uva = kmem_paddr(utcb_kva, Cfg_page_sz); // 1:1
	L4_fpage_t utcb_area = L4_fpage_t::create(utcb_uva, Cfg_page_sz, Acc_rw);
	tsk.utcb_area(utcb_area);

	// create sigma0 thread
	L4_thrid_t nil = L4_thrid_t::Nil;
	Thread_t* thr = Threads_t::create(thrid_sigma0(), nil, nil, &tsk, Thread_t::Prio_max, "sgm0");
	thr->utcb_location(utcb_pa);
	thr->activate();
	thr->start(kip->sigma0_ip, kip->sigma0_sp);
	Sched_t::current()->name("idle"); // rename thread from 'krnl' to 'idle'
	SystemClock_t::set_check(1);      // further kernel working time will be checked
	Sched_t::switch_to(thr);          // go to sigma0

	void idle_loop();
	idle_loop();
}

void idle_loop()
{
	Proc::enable_irq();
	while (1) // idle operation, TODO:  use arch "asr19" for sparc, "hlt" for x86, ...
	{
		//static unsigned cnt = 0;
		//printf("idle loop:  %u.\n", cnt++);
		//{ volatile unsigned x = 40*1000*1000; while (--x); }
	}
}

void init_kip()
{
	L4_kip_t* kip = get_kip();

	if (kip->magic != L4_kip_magic)
		panic("%s:  wrong kip:  addr=0x%p, magic=0x%lx/%.4s.",
			__func__, kip, kip->magic, (char*)&kip->magic);

	kip->thread_info.system_base(Kcfg::Kip_thrid_system_base);
	kip->thread_info.user_base(Kcfg::Kip_thrid_user_base);
	kip->thread_info.bits_max(Kcfg::Thr_num_width);
	printk("sys_base=%lu, usr_base=%lu, num_max=%lu.\n",
		kip->thread_info.system_base(), kip->thread_info.user_base(), kip->thread_info.number_max());

	kip->tick_usec = Cfg_krn_tick_usec;
}

#ifdef DEBUG
// some debug checkings
bool check()
{
	assert(get_log2sz(0) == 0);
	assert(get_log2sz(0x1000) == 12);
	assert(get_log2sz(0x2000) == 13);
	assert(get_log2sz(0x3000) ==  0);
	assert(get_log2sz(0x4000) == 14);
	assert(get_log2sz(0x5000) ==  0);
	assert(get_log2sz(0x40000000) == 30);
	assert(get_log2sz(0x80000000) == 31);
	assert(get_log2sz_low_range(0) == 0);
	assert(get_log2sz_low_range(0x1000) == 12);
	assert(get_log2sz_low_range(0x2000) == 13);
	assert(get_log2sz_low_range(0x3000) == 13);
	assert(get_log2sz_low_range(0x4000) == 14);
	assert(get_log2sz_low_range(0x5000) == 14);
	assert(get_log2sz_top_range(0) == 0);
	assert(get_log2sz_top_range(0x1000) == 12);
	assert(get_log2sz_top_range(0x2000) == 13);
	assert(get_log2sz_top_range(0x3000) == 14);
	assert(get_log2sz_top_range(0x4000) == 14);
	assert(get_log2sz_top_range(0x5000) == 15);

	return true;
}
bool b1 = check();
#endif // DEBUG

// SMP

enum
{
	Inter_cpu_start      = 1,  // init value     - not 0 to inter_cpu_flag be in .data, not .bss
	Inter_cpu_io_ready   = 2,  // any CPU set    - slave CPUs may to use printf()
	Inter_cpu_init_done  = 3   // master CPU set - slave CPUs may to jump at kernel_entry_point
};

volatile long cpu_ready [Cfg_max_cpus];
volatile long inter_cpu_flag = Inter_cpu_start;  // not 0 to be in .data, not .bss

static void set_inter_cpu_flag(long f)
{
	inter_cpu_flag = f;
	Proc::wmb();
	Proc::send_event();
}

static void wait_inter_cpu_flag(long f)
{
	while (inter_cpu_flag != f)
	{
		Proc::wait_event();
		Proc::rmb();
	}
}

static void slave_cpu()
{
	wait_inter_cpu_flag(Inter_cpu_io_ready);
	printk("kernel:  cpu #%u hello, sp=0x%lx.\n", Proc::cpuid(), Proc::sp());
	cpu_ready[Proc::cpuid()] = 1;
	Proc::wmb();
	wait_inter_cpu_flag(Inter_cpu_init_done);
	while (1)
	{
		Proc::wait_event();  // TODO:  support SMP
	}
}

static void wait_slave_cpus(unsigned ncpu)
{
	while (1)
	{
		unsigned slave_cpus_ready = 0;
		for (unsigned i=0; i<Cfg_max_cpus; ++i)
			if (cpu_ready[i])
				slave_cpus_ready++;

		if (slave_cpus_ready == (ncpu - 1))
			break;
	}
}

// ~SMP

// section .data instead of .bss to avoid cleaning in init_bss()
// TODO:  reuse this memory
char startup_stack [Cfg_max_cpus][Startup_stack_sz] __attribute__((aligned(0x1000), section(".data")));
char startup_exception_stack [0x1000] __attribute__((aligned(0x1000), section(".data")));


// At the start CPU is in virtual address space with rude temporary mapping.
// NEED:
//   1) initialize base subsystems (uart, kmem, klog, ...),
//   2) do fine tuned mapping,
//   3) prepare kernel thread and switch to it.
void startup()
{
	if (Proc::cpuid())
		slave_cpu();

	arch_set_ksp((addr_t)startup_exception_stack);
	bootstrap_remove_phys_mapping();

	init_bss();
	Uart::init(Cfg_krn_uart_vaddr, Cfg_krn_uart_bitrate, Cfg_sys_clock_hz);
	libcio_set_uart();
	printk("kernel:  cpu #%u hello, sp=0x%lx.\n", Proc::cpuid(), Proc::sp());

	// wake up slave CPUs and wait them
	set_inter_cpu_flag(Inter_cpu_io_ready);
	Proc::wait_event();                  // allow slave CPUs print hello, it needs for qemu
	Intc::init(Cfg_krn_intc_vaddr);      //
	if (0)
	{
		// TODO
		wait_slave_cpus(Intc::ncpu());   // wait for slave CPUs them
	}
	set_inter_cpu_flag(Inter_cpu_init_done);

	libcio_set_null();                   // disable output
	arch_init();
	call_ctors();
	Kmem::init();
	Aspace::kinit();

	arch_set_timer_va(ktimer_va() + get_pg_offset(Cfg_krn_timer_paddr));

	// init log sybsystem
	size_t log_sz = 16 * Cfg_page_sz;
	addr_t log_buf = Kmem::alloc(log_sz);
	assert(log_buf);
	Log::init((char*)log_buf, log_sz);
	libcio_set_log();                    // next printk() will write to Log

	Kdb::init();

	Task_t& tsk = Tasks_t::create();     // initial aspace

	// enable finely tuned kernel mapping
	Pgtab::activate();
	tsk.set_current(true);

	// reinit uart to new addr space
	Uart::init(kuart_va() + get_pg_offset(Cfg_krn_uart_paddr), Cfg_krn_uart_bitrate, Cfg_sys_clock_hz);

	// initialize KIP data
	init_kip();

	// alloc thread kernel stacks
	Threads_t::alloc_kstack();

	// run kernel thread
	Threads_t::create_kthread_and_go((void*)kthread, &tsk, "krnl");
}
