//##################################################################################################
//
//  l4kdbops.h - KDB operation codes.
//
//##################################################################################################

#ifndef L4_KDB_OPS_H
#define L4_KDB_OPS_H

enum L4_kdb_ops_t
{
	L4_kdb_console   = 0,  // enter to kdb console
	L4_kdb_threads   = 1   // get threads info
};

// FIXME:  replace me, think!
struct L4_kdb_thread_info_t
{
	unsigned id;
	char     name[8];
	unsigned prio;
	uint64_t thr_uptime_usec;
	uint64_t sys_uptime_usec;
	uint64_t no_activity_usec;
};


#endif // L4_KDB_OPS_H
