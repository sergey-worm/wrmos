//##################################################################################################
//
//  Implementation of some funcs for string.h.
//
//##################################################################################################

#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include "sys_utils.h"

#define OPTIMISE

#define is_aligned2(n1, n2, a)       \
	(                                \
		is_aligned((size_t)n1, a) && \
		is_aligned((size_t)n2, a)    \
	)

#define is_aligned3(n1, n2, n3, a)   \
	(                                \
		is_aligned((size_t)n1, a) && \
		is_aligned((size_t)n2, a) && \
		is_aligned((size_t)n3, a)    \
	)

unsigned wlibc_stat_memcpy8  = 1;
unsigned wlibc_stat_memcpy32 = 1;
unsigned wlibc_stat_memcpy64 = 1;
unsigned wlibc_stat_memset8  = 1;
unsigned wlibc_stat_memset32 = 1;
unsigned wlibc_stat_memcmp8  = 1;
unsigned wlibc_stat_memcmp32 = 1;

void* memcpy(void* dst, const void* src, size_t sz)
{
	#ifdef OPTIMISE
	// try to use 64-bit instructions
	if (sz >= 32  &&  is_aligned3(dst, src, sz, sizeof(uint64_t)))
	{
		//wlibc_stat_memcpy64 += sz;
		for (size_t i=0; i<sz/sizeof(uint64_t); ++i)
			((uint64_t*)dst)[i] = ((uint64_t*)src)[i];
	}
	// try to use long instructions
	else if (sz >= 32  &&  is_aligned3(dst, src, sz, sizeof(long)))
	{
		//wlibc_stat_memcpy32 += sz;
		for (size_t i=0; i<sz/sizeof(long); ++i)
			((long*)dst)[i] = ((long*)src)[i];
	}
	// byte operations
	else
	#endif
	{
		//wlibc_stat_memcpy8 += sz;
		for (size_t i=0; i<sz; ++i)
			((char*)dst)[i] = ((char*)src)[i];
	}
	return dst;
}

void* memmove(void* dst, const void* src, size_t sz)
{
	#ifdef OPTIMISE
	// try to use memcpy()
	if (sz >= 32  &&  (dst>src ? (dst-src)>=8 : (src-dst)>=8))
	{
		memcpy(dst, src, sz);
	}
	// byte operations
	else
	#endif
	{
		for (size_t i=0; i<sz; ++i)
			((char*)dst)[i] = ((char*)src)[i];
	}
	return dst;
}

void* memset(void* dst, int val, size_t sz)
{
	#ifdef OPTIMISE
	// try to use 64-bit instructions
	if (sz >= 32  &&  is_aligned2(dst, sz, sizeof(uint64_t)))
	{
		//wlibc_stat_memset64 += sz;
		val &= 0xff;
		uint64_t v = val<<24 | val<<26 | val<<8 | val;
		v |= v << 32;
		for (size_t i=0; i<sz/sizeof(uint64_t); ++i)
			((uint64_t*)dst)[i] = v;
	}
	// try to use long instructions
	else if (sz >= 32  &&  is_aligned2(dst, sz, sizeof(long)))
	{
		//wlibc_stat_memset32 += sz;
		val &= 0xff;
		long v = val<<24 | val<<26 | val<<8 | val;
		if (sizeof(long) > sizeof(uint32_t))
			v |= (uint64_t)v << 32; // long have 64-bit size
		for (size_t i=0; i<sz/sizeof(long); ++i)
			((long*)dst)[i] = v;
	}
	// byte operations
	else
	#endif
	{
		//wlibc_stat_memset8 += sz;
		for (size_t i=0; i<sz; ++i)
			((char*)dst)[i] = val;
	}
	return dst;
}

int memcmp(const void* ptr1, const void* ptr2, size_t sz)
{
	#ifdef OPTIMISE
	#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ // for little endian difficult calc return value
	// try to use long instructions
	if (sz >= 32  &&  is_aligned3(ptr1, ptr2, sz, sizeof(long)))
	{
		//wlibc_stat_memcmp32 += sz;
		const unsigned long* p1 = ptr1;
		const unsigned long* p2 = ptr2;
		sz /= sizeof(long);
		while (sz--)
		{
			if (*p1 != *p2)
				return ((*p1 - *p2) < 0) ? -1 : 1;
			p1++;
			p2++;
		}
	}
	// byte operations
	else
	#endif
	#endif
	{
		//wlibc_stat_memcmp8 += sz;
		const unsigned char* p1 = ptr1;
		const unsigned char* p2 = ptr2;
		while (sz--)
		{
			if (*p1 != *p2)
				return *p1 - *p2;
			p1++;
			p2++;
		}
	}
	return 0;
}

#ifndef strcmp  // string.h may have implementation
int strcmp(const char* s1, const char* s2)
{
	while (*s1 && (*s1 == *s2))
		s1++, s2++;
	return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
#endif

#ifndef strncmp  // string.h may have implementation
int strncmp(const char *s1, const char *s2, size_t n)
{
	for (; n > 0; s1++, s2++, --n)
		if (*s1 != *s2)
			return ((*(unsigned char*)s1 < *(unsigned char*)s2) ? -1 : +1);
		else if (*s1 == '\0')
			return 0;
	return 0;
}
#endif

size_t strlen(const char* str)
{
	const char *s;
	for (s = str; *s; ++s);
	return (s - str);
}

char* strcpy(char* dst, const char* src)
{
	char* ret = dst;
	while ((*dst++ = *src++));
	return ret;
}

// copy a string returning a pointer to its end
char* stpcpy(char* dst, const char* src)
{
	while ((*dst++ = *src++));
	return dst - 1;
}

char* strncpy(char* dst, const char* src, size_t n)
{
	size_t i;
	for (i=0; i<n && src[i]!='\0'; i++)
		dst[i] = src[i];
	for (; i<n; i++)
		dst[i] = '\0';
	return dst;
}

#ifndef strchr  // string.h may have implementation
char* strchr(const char *s, int c)
{
	while (*s != (char)c)
		if (!*s++)
			return 0;
	return (char*)s;
}
#endif

char* strcat(char *dst, const char* src)
{
	char* ret = dst;
	while (*dst)
		dst++;
	while ((*dst++ = *src++));
	return ret;
}

char* strupr(char* s)
{
	char* ret = s;

	while (*s)
	{
		if (*s >= 'a'  &&  *s <= 'z')
			*s += 'A' - 'a';
		s++;
	}
	return ret;
}

#ifndef strspn  // string.h may have implementation
// returns the length of the initial portion of str1 which
// consists only of characters that are part of str2
size_t strspn(const char* str1, const char* str2)
{
	size_t n;
	const char* p;
	for (n=0; *str1; str1++, n++)
	{
		for (p=str2; *p && *p!=*str1; p++);
		if (!*p)
			break;
	}
	return n;
}
#endif

#ifndef strpbrk  // string.h may have implementation
// Returns a pointer to the first occurrence in str1 of any of the characters
// that are part of str2, or a null pointer if there are no matches.
char* strpbrk(const char* str1, const char* str2)
{
	int c;
	while ((c = *str1++) != 0)
	{
		const char* scanp;
		int sc;
		for (scanp = str2; (sc = *scanp++) != 0;)
			if (sc == c)
				return ((char*)(str1 - 1));
	}
	return 0;
}
#endif

// uClibsc declare args as nonnull
char* strstr(const char* str, const char* substr)
{
	for (; *str; str++)
	{
		const char *s, *ss;
		for (s=str, ss=substr; *s && *ss && (*s == *ss); ++s, ++ss);
		if (*ss == '\0')
			return (char*)str;
	}
	return 0;
}

char* strtok(char* str, const char* delimiters)
{
	assert(0 && "Implement me!");
	return 0;
}

