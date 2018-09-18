//##################################################################################################
//
//  Some functions that compiler requires.
//
//  See:
//    1. http://libcxxabi.llvm.org/spec.html
//    2. http://wiki.osdev.org/C%2B%2B
//
//##################################################################################################

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

//--------------------------------------------------------------------------------------------------
// The __cxa_pure_virtual function is an error handler
// that is invoked when a pure virtual function is called.
extern "C" void __cxa_pure_virtual()
{
	// TODO:  add some error handler
	assert(0 && "Implement me!");
	while (1);
}

//--------------------------------------------------------------------------------------------------
// Local Static Variables (GCC Only) stubs:
//   When you declare a local static variable, GCC puts a guard around
//   the variable's constructor call. This ensures that only one thread
//   can call the constructor at the same time to initialize it.
typedef uint64_t __guard __attribute__((mode(__DI__)));
extern "C" int __cxa_guard_acquire(__guard* g)
{
	return !*(char*)(g);
}

extern "C" void __cxa_guard_release(__guard* g)
{
	*(char*)g = 1;
}
 
extern "C" void __cxa_guard_abort(__guard*)
{
}

void operator delete(void* ptr)
{
	assert(0 && "Implement me!");
}

void operator delete[](void* ptr)
{
	assert(0 && "Implement me!");
}

void operator delete(void*, unsigned int)
{
	assert(0 && "Implement me!");
}

void operator delete[](void*, unsigned int)
{
	assert(0 && "Implement me!");
}

void* operator new(size_t sz) throw()
{
	//printf("operator new:  size=%u.\n", sz);
	return malloc(sz);
}

void* operator new[](size_t sz)
{
	//printf("operator new[]:  size=%u.\n", sz);
	return operator new (sz);
}

//--------------------------------------------------------------------------------------------------
// uClibc file ctype.h contents macro-implemented ctype-functions for C-code, but C++
// For C++ put it here.
//--------------------------------------------------------------------------------------------------

extern "C" int isspace(int c)
{
	return c==' ' || c=='\t' || c=='\n' || c=='\v' || c=='\f' || c=='\r';
}

extern "C" int isdigit(int c)
{
	return c >= '0'  &&  c <= '9';
}

extern "C" int toupper(int c)
{
	if (c >= 'a'  &&  c <= 'z')
		return c + 'A' - 'a';
	else
		return c;
}

extern "C" int tolower(int c)
{
	if (c >= 'A'  &&  c <= 'Z')
		return c - 'A' + 'a';
	else
		return c;
}

//extern "C" int isprint(int c)
//{
//	return 0;
//}
