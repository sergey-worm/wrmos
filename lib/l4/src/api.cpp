//##################################################################################################
//
//  API to L4 microkernel.
//
//##################################################################################################

#include "l4_api.h"

// run kdb console
extern "C" void l4_kdb(const char* str, int error_entry)
{
	l4_kdb(L4_kdb_console, error_entry, (void*)str, 0);
}

// slow output string via IPC to kernel
extern "C" void l4_kdb_putsn(const char* str, size_t len)
{
	bool error_entry = false;
	l4_kdb(L4_kdb_print, error_entry, (void*)str, len);
}
