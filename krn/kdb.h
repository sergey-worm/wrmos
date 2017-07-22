//##################################################################################################
//
//  Kdb - kernel debugger.
//
//##################################################################################################

#ifndef KDB_H
#define KDB_H

#include "shell.h"
#include "processor.h"
#include "kintc.h"
#include "ktimer.h"
#include "threads.h"
#include "log.h"
#include "libcio.h"

//--------------------------------------------------------------------------------------------------
static void dprint(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("  ");
	vprintf(fmt, args);
	va_end(args);
}

//--------------------------------------------------------------------------------------------------
static int cmd_exit(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	return 1;
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
static int cmd_intc(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Intc::dump(dprint);
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int cmd_timer(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Timer::dump(dprint);
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int cmd_threads(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Threads_t::dump();
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int cmd_cpuusage(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Threads_t::dump_cpuusage();
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int cmd_entry_frame(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	Sched_t::current()->entry_frame()->dump();
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int cmd_banner(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	printf("\n");
	printf("[ldr]   _    _ ___ __  __   ___  ___ \n");
	printf("[ldr]  | |  | | _ \\  \\/  | / _ \\/ __|\n");
	printf("[ldr]  | |/\\| |   / |\\/| || (_) \\__ \\\n");
	printf("[ldr]  |__/\\__|_|_\\_|  |_(_)___/|___/\n");
	printf("[ldr]          From Russia with love!\n");
	printf("[ldr]\n");
	printf("\n");
	return 0;
}

//--------------------------------------------------------------------------------------------------
class Kdb
{
	static Shell_t _shell;

public:

	static void init()
	{
		_shell.add_cmd("exit",          cmd_exit);
		_shell.add_cmd("log",           cmd_log);
		_shell.add_cmd("intc",          cmd_intc);
		_shell.add_cmd("timer",         cmd_timer);
		_shell.add_cmd("threads",       cmd_threads);
		_shell.add_cmd("cpuusage",      cmd_cpuusage);
		_shell.add_cmd("entry_frame",   cmd_entry_frame);
		_shell.add_cmd("banner",        cmd_banner);

		_shell.prompt("\x1b[1;33mkdb> \x1b[0m");
	}

	// entry may be kernel/user and error/noerror
	static void entry(bool krn_mode, bool error_entry, const char* prompt)
	{
		static unsigned depth = 0;
		if (depth++ > 3)
			Proc::halt();

		libcio_set_uart();

		const char* line    = "+--------------------------------------------------------------\n";
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

		Log::dump("last klog:  ", 6);

		_shell.shell();

		if (krn_mode && error_entry)
		{
			printf("Error in kernel mode - halt.\r\n");
			Proc::halt();
		}

		libcio_set_log();
		depth--;
	}
};

// static data for Kdb
Shell_t Kdb::_shell;

//FIXME
void kdb_console_entry_wrapper(bool krn_mode, bool error_entry, const char* prompt)
{
	if (Timer::inited())
		Timer::stop();
	Kdb::entry(krn_mode, error_entry, prompt);
	if (Timer::inited())
		Timer::start();
}

#endif // CTRL_H
