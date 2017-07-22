//##################################################################################################
//
//  startup - kernel high-level entry point and stack.
//
//##################################################################################################

#ifndef STARTUP_H
#define STARTUP_H

enum { Startup_stack_sz = 0x1000 };
extern char startup_stack[];
void startup();

#endif // STARTUP_H
