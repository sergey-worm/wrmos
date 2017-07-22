//##################################################################################################
//
//  Implementation of some funcs for string.h.
//
//##################################################################################################

#include <string.h>
#include <stdarg.h>

//#warning  TODO:  optimize for target arch
void* memcpy(void* dst, const void* src, size_t sz)
{
	for (size_t i=0; i<sz; ++i)
		((char*)dst)[i] = ((char*)src)[i];
	return dst;
}

//#warning  TODO:  optimize for target arch
void* memset(void* dst, int val, size_t sz)
{
	for (size_t i=0; i<sz; ++i)
		((char*)dst)[i] = val;
	return dst;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num)
{
	const unsigned char* p1 = ptr1;
	const unsigned char* p2 = ptr2;
	while (num--)
	{
		if (*p1 != *p2)
			return *p1 - *p2;
		p1++;
		p2++;
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

void* memmove(void* dst, const void* src, size_t sz)
{
	for (size_t i=0; i<sz; ++i)
		((char*)dst)[i] = ((char*)src)[i];
	return dst;
}

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

#include "assert.h"

char* strtok(char* str, const char* delimiters)
{
	assert(0 && "Implement me!");
	return 0;
}

