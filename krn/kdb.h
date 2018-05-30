//##################################################################################################
//
//  Kdb - kernel debugger.
//
//##################################################################################################

#ifndef KDB_H
#define KDB_H

#include "shell.h"

void kdb_console_entry_wrapper(bool krn_mode, bool error_entry, const char* prompt);

#define kdb(...) \
    { \
        char b[64]; \
        snprintf(b, sizeof(b), "%s:%s:%u", __FILE__, __func__, __LINE__); \
        kdb_console_entry_wrapper(1, 0, b); \
    }

class Kdb
{
	static Shell_t _shell;

public:

	static void init();

	// entry may be kernel/user and error/debug
	static void entry(bool krn_mode, bool error_entry, const char* prompt);
};

#endif // KDB_H
