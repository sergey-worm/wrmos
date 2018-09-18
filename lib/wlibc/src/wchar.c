//##################################################################################################
//
//  Implementation of some funcs for wchar.h.
//
//##################################################################################################

#include <wchar.h>
#include <stdio.h>
#include "wlibc_panic.h"

size_t wcslen(const wchar_t* str)
{
	panic("%s:  implement me!\n", __func__);
	const wchar_t* s;
	for (s = str; *s; ++s);
	return (s - str);
}

size_t wcsxfrm(wchar_t* restrict ws1, const wchar_t* restrict ws2, size_t n)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int wcscoll(const wchar_t* wcs1, const wchar_t* wcs2)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

size_t wcsftime(wchar_t* ptr, size_t maxsize, const wchar_t* format, const struct tm* timeptr)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

size_t wcrtomb(char* pmb, wchar_t wc, mbstate_t* ps)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int wctob(wint_t wc)
{
	//printf("%s:  wc=%d/%c.\n", __func__, wc, wc);
	return wc;  // workaround
	panic("%s:  implement me!\n", __func__);
	return EOF;
}

wint_t btowc(int c)
{
	//printf("%s:  wc=%d/%c.\n", __func__, c, c);
	return c;  // workaround
	panic("%s:  implement me!\n", __func__);
	return 0;
}

size_t mbrtowc(wchar_t* pwc, const char* pmb, size_t max, mbstate_t* ps)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t num)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wint_t putwc(wchar_t wc, FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wint_t getwc(FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wint_t ungetwc(wint_t wc, FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wchar_t* wmemset(wchar_t* ptr, wchar_t wc, size_t num)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wchar_t* wmemmove(wchar_t* dst, const wchar_t* src, size_t num)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int wmemcmp(const wchar_t* ptr1, const wchar_t* ptr2, size_t num)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
