//##################################################################################################
//
//  Kdb - kernel debugger.
//
//##################################################################################################

#include "kdb.h"
#include "ktimer.h"
#include "threads.h"
#include "log.h"
#include "libcio.h"
#include "sys_proc.h"

extern "C" unsigned long int strtoul(const char* str, char** endptr, int base);


void kdb_console_entry_wrapper(bool krn_mode, bool error_entry, const char* prompt)
{
	if (Timer::inited())
		Timer::stop();
	Kdb::entry(krn_mode, error_entry, prompt);
	if (Timer::inited())
		Timer::start();
}

static void dprint(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("  ");
	vprintf(fmt, args);
	va_end(args);
}

static int cmd_exit(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	return 1;
}

static int cmd_log(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	printf("argc=0x%x, argv=0x%x.\n", argc, argv);
	for (int i=0; i<argc; i++)
		printf("arg[%d] = %s.\n", i, argv[i]);
	Log::dump("klog:  ", -1);
	return 0;
}

static int cmd_intc(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Intc::dump(dprint);
	return 0;
}

static int cmd_timer(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Timer::dump(dprint);
	return 0;
}

static int cmd_threads(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Threads_t::dump();
	return 0;
}

static int cmd_cpuusage(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Threads_t::dump_cpuusage();
	return 0;
}

// incomming:  [thread_id] [u]
static int cmd_entry_frame(unsigned argc, char** argv)
{
	bool need_print_usr_mem = false;
	unsigned thread_id = 0;  // 0 - mean cur thread

	if (argc == 2)
	{
		if (!strcmp(argv[1], "u"))
			need_print_usr_mem = true;
		else
			thread_id = strtoul(argv[1], NULL, 0);
	}
	else if (argc == 3)
	{
		thread_id = strtoul(argv[1], NULL, 0);
		if (!strcmp(argv[2], "u"))
			need_print_usr_mem = true;
	}

	Thread_t* thr = Sched_t::current();
	if (thread_id)
		thr = Threads_t::find(thread_id);

	if (thr)
	{
		printf(" thread:  name=%s, id=%u:\n", thr->name(), thr->globid().number());
		thr->entry_frame()->dump(printf, need_print_usr_mem);
	}
	else
	{
		printf(" Wrong thread_id %u.\n", thread_id);
	}
	return 0;
}

static int cmd_banner(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	printf("\n");
	printf("   _    _ ___ __  __   ___  ___ \n");
	printf("  | |  | | _ \\  \\/  | / _ \\/ __|\n");
	printf("  | |/\\| |   / |\\/| || (_) \\__ \\\n");
	printf("  |__/\\__|_|_\\_|  |_(_)___/|___/\n");
	printf("          From Russia with love!\n");
	printf("\n");
	return 0;
}

static int cmd_show_memory(unsigned argc, char** argv)
{
	// get inc params

	if (argc < 2  ||  argc > 3)
	{
		printf("Use:  mem <addr> [sz]\n");
		return 0;
	}

	addr_t addr = strtoul(argv[1], NULL, 0);
	size_t size = 32;

	if (argc == 3)
		size = strtoul(argv[2], NULL, 0);

	// read line by line, each line contents 4 long values

	addr_t start = round_down(addr, 4*sizeof(long));
	addr_t end = round_up(addr + size, 4*sizeof(long));

	for (addr_t a=start; a<end; a+=4*sizeof(long))
	{
		unsigned width = 2*sizeof(long);
		printf("  0x%0*lx:  %0*lx %0*lx %0*lx %0*lx\n",
			width, a,
			width, *(long*)(a + 0*sizeof(long)),
			width, *(long*)(a + 1*sizeof(long)),
			width, *(long*)(a + 2*sizeof(long)),
			width, *(long*)(a + 3*sizeof(long)));
	}

	printf("\n");
	return 0;
}

static int cmd_show_mapping(unsigned argc, char** argv)
{
	unsigned thread_no = argc==2 ? strtoul(argv[1], NULL, 0) : 0;
	Thread_t* thr = thread_no ? Threads_t::find(thread_no) : Sched_t::current();
	thr->task()->dump();
	return 0;
}

void Kdb::init()
{
	_shell.add_cmd("exit",          cmd_exit);
	_shell.add_cmd("log",           cmd_log);
	_shell.add_cmd("intc",          cmd_intc);
	_shell.add_cmd("timer",         cmd_timer);
	_shell.add_cmd("threads",       cmd_threads);
	_shell.add_cmd("cpuusage",      cmd_cpuusage);
	_shell.add_cmd("entry_frame",   cmd_entry_frame);
	_shell.add_cmd("banner",        cmd_banner);
	_shell.add_cmd("mem",           cmd_show_memory);
	_shell.add_cmd("mapping",       cmd_show_mapping);

	_shell.prompt("\x1b[1;33mkdb> \x1b[0m");
}

// entry may be kernel/user and error/noerror
void Kdb::entry(bool krn_mode, bool error_entry, const char* prompt)
{
	static unsigned depth = 0;
	if (depth++ > 3)
		Proc::halt();

	libcio_set_uart();

	const char* line    = "+------------------------------------------------------------------------------\n";
	const char* color   = "\x1b[1;33m";
	const char* nocolor = "\x1b[0m";
	const char* kind    = error_entry ? "Error" : "Debug";
	const char* mode    = krn_mode ? "kernel" : "user";

	puts("");
	printf(color);
	printf(line);
	printf("|  %s kdb entry from %s mode:  %s\n", kind, mode, prompt);
	printf(line);
	printf(nocolor);

	Log::dump("last klog:  ", 10);

	_shell.shell();

	if (krn_mode && error_entry)
	{
		printf("Error in kernel mode - halt.\r\n");
		Proc::halt();
	}

	libcio_set_log();
	depth--;
}

// static data for Kdb
Shell_t Kdb::_shell;
