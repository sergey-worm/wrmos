//##################################################################################################
//
//  Helpers for stack usage.
//
//##################################################################################################

#ifndef STACK_H
#define STACK_H

#include "sys_proc.h"

class Stack
{
public:

	// push value on the current stack
	static inline void push(word_t v)
	{
		addr_t sp = Proc::sp();
		sp -= sizeof(word_t);
		Proc::sp(sp);
		*(word_t*)sp = v;
	}

	// push value on the stack by sp
	static inline void push(addr_t* sp, word_t v)
	{
		*sp -= sizeof(word_t);
		*(word_t*)(*sp) = v;
	}

	// pop value from the current stack
	static inline word_t pop()
	{
		addr_t sp = Proc::sp();
		word_t res = *(word_t*)sp;
		Proc::sp(sp + sizeof(word_t));
		return res;
	}

	// pop value from the stack by sp
	static inline word_t pop(addr_t* sp)
	{
		word_t res = *(word_t*)(*sp);
		*sp += sizeof(word_t);
		return res;
	}
};

#endif // STACK_H
