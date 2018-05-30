//##################################################################################################
//
//  startup - kernel high-level entry point and stack.
//
//##################################################################################################

#ifndef STARTUP_H
#define STARTUP_H

#include "sys-config.h"

enum { Startup_stack_sz = 0x1000 };
extern char startup_stack[Cfg_max_cpus][Startup_stack_sz];
void startup();

#endif // STARTUP_H
