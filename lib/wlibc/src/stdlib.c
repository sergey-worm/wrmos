//##################################################################################################
//
//  Implementation of some funcs for stdlib.h.
//
//##################################################################################################

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "sys_utils.h"
#include "wlibc_cb.h"

#if 0
 #include <ctype.h> // FIXME:  undefined reference to `__ctype_b'
#else
 int isdigit(int c);
 int isspace(int c);
#endif


void exit(int status)
{
	assert(0 && "Implement me!");
	while (1);
}

int rand()
{
	assert(0 && "Implement me!");
	return 0;
}

void* malloc(size_t sz)
{
	//printf("%s:  size=%u.\n", __func__, sz);

	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	void* res = cb->malloc ? cb->malloc(sz) : 0;
	return res;
}

void free(void* ptr)
{
	assert(0 && "Implement me!");
}

void* calloc(size_t num, size_t size)
{
	//printf("calloc:  sz=%u.\n", num * size);

	size_t sz = num * size;
	void* res = malloc(sz);

	if (sz && res)
		memset(res, 0, sz);

	//printf("calloc:  sz=%u, ok.\n", sz);
	return res;
}

void qsort(void* base, size_t num, size_t size, int (*compar)(const void*,const void*))
{
	assert(0 && "Implement me!");
}

float strtof(const char* str, char** endptr)
{
	assert(0 && "Implement me!");
	return 0;
}

char* itoa(int value, char* str, int base)
{
	if (base == 8)
		sprintf(str, "%o", value);
	else if (base == 10)
		sprintf(str, "%d", value);
	else if (base == 16)
		sprintf(str, "%x", value);
	return str;
}

int atoi(const char* str)
{
	int v = strtoul(str, 0, 10);
	//printf("str=%s, v=%d.\n", str, v);
	return v;
}

long int strtol(const char* str, char** endptr, int base)
{
	assert(0 && "Implement me!");
	return 0;
}

unsigned long strtoul(const char* str, char** endptr, int base)
{
	// convert from ASCII charecter to a digit
	static char char2digit[] =
	{
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9,                // '0'..'9'
		99, 99, 99, 99, 99, 99, 99,                           //
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,   // 'A'..'Z'
		23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,   //
		99, 99, 99, 99, 99, 99,                               //
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,   // 'a'..'z'
		23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35    //
	};

	const char* p = str;

	// skip spaces
	while (isspace(*p))
		p++;

	int negative = 0;
	if (*p == '-')
	{
		negative = 1;
		p++;
	}
	else if (*p == '+')
	{
		p++;
	}

	// if no base - try to define it
	int any_digits = 0;
	if (base == 0)
	{
		if (*p == '0')
		{
			p++;
			if ((*p == 'x') || (*p == 'X'))
			{
				p++;
				base = 16;
			}
			else
			{
				any_digits = 1;
				base = 8;
			}
		}
		else
			base = 10;
	}
	else if (base == 16)
	{
		// skip "0x" if need
		if ((p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X')))
			p += 2;
	}

	unsigned long result = 0;
	int overflow = 0;

	if (base == 8)
	{
		unsigned long maxres = ULONG_MAX >> 3;
		for (;; ++p)
		{
			unsigned digit = *p - '0';
			if (digit > 7)
				break;
			if (result > maxres)
				overflow = 1;
			result = (result << 3);
			if (digit > (ULONG_MAX - result))
				overflow = 1;
			result += digit;
			any_digits = 1;
		}
	}
	else if (base == 10)
	{
		unsigned long maxres = ULONG_MAX / 10;
		for (;; p++)
		{
			unsigned digit = *p - '0';
			if (digit > 9)
				break;
			if (result > maxres)
				overflow = 1;
			result *= 10;
			if (digit > (ULONG_MAX - result))
				overflow = 1;
			result += digit;
			any_digits = 1;
		}
	}
	else if (base == 16)
	{
		unsigned long maxres = ULONG_MAX >> 4;
		for (;; p++)
		{
			unsigned digit = *p - '0';
			if (digit > ('z' - '0'))
				break;
			digit = char2digit[digit];
			if (digit > 15)
				break;
			if (result > maxres)
				overflow = 1;
			result = (result << 4);
			if (digit > (ULONG_MAX - result))
				overflow = 1;
			result += digit;
			any_digits = 1;
		}
	}
	else if (base >= 2 && base <= 36 )
	{
		unsigned long maxres = ULONG_MAX / base;
		for (;; p++)
		{
			unsigned digit = *p - '0';
			if (digit > ('z' - '0'))
				break;
			digit = char2digit[digit];
			if (digit >= ((unsigned)base))
				break;
			if (result > maxres)
				overflow = 1;
			result *= base;
			if (digit > (ULONG_MAX - result))
				overflow = 1;
			result += digit;
			any_digits = 1;
		}
	}

	// see if there were any digits at all
	if (!any_digits)
		p = str;

	if (endptr)
		*endptr = (char*)p;

	if (overflow)
	{
		//errno = ERANGE; // FIXME
		return ULONG_MAX;
	}

	return negative ? -result : result;
}

// "SI.FEsX"
//   S - +/- or omitted
//   I - integer part of the mantissa, may be omitted
//   F - fractional part of the mantissa, may be omitted
//   E - e/E
//   s - +/-
//   X - exponent, "EsX" may be omitted
double strtod(const char* str, char** endptr)
{
	static double pow10[] = { 1.0e1, 1.0e2, 1.0e4, 1.0e8, 1.0e16, 1.0e32, 1.0e64, 1.0e128, 1.0e256 };
	enum { Max_exponent = 511 };
	const char* p = str;

	// skip spaces
	while (isspace(*p))
		p++;

	// parse "S"

	int negative = 0;
	if (*p == '-')
	{
		negative = 1;
		p++;
	}
	else if (*p == '+')
	{
		p++;
	}

	// parse "I.F"

	int point = -1;    // position of decimal point
	int mant_sz = 0;   // digits in mantissa
	for (;; mant_sz++)
	{
		char c = *p;
		if (!isdigit(c))
		{
			if ((c != '.') || (point >= 0))
				break;
			point = mant_sz;
		}
		p++;
	}

	const char* expPtr = p;   // location of exponent in str
	p -= mant_sz;
	if (point < 0)
		point = mant_sz;
	else
		mant_sz--;

	int fracExp = 0;    // exponent from fractional
	if (mant_sz > 18)
	{
		fracExp = point - 18;
		mant_sz = 18;
	}
	else
	{
		fracExp = point - mant_sz;
	}

	double fraction = 0.0;
	if (!mant_sz)
	{
		p = str;
		goto done;
	}
	else
	{
		int frac1 = 0;
		for (; mant_sz > 9; mant_sz--)
		{
			char c = *p;
			p++;
			if (c == '.')
			{
				c = *p;
				p++;
			}
			frac1 = 10*frac1 + (c - '0');
		}
		int frac2 = 0;
		for (; mant_sz > 0; mant_sz--)
		{
			char c = *p;
			p++;
			if (c == '.')
			{
				c = *p;
				p++;
			}
			frac2 = 10*frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	// parse "EsX"

	int expSign = 0;
	int exp = 0;
	p = expPtr;
	if ((*p == 'E') || (*p == 'e'))
	{
		p++;
		if (*p == '-')
		{
			expSign = 1;
			p++;
		}
		else
		{
			if (*p == '+')
				p++;
			expSign = 0;
		}
		if (!isdigit(*p))
		{
			p = expPtr;
			goto done;
		}
		while (isdigit(*p))
		{
			exp = exp * 10 + (*p - '0');
			p++;
		}
	}

	if (expSign)
		exp = fracExp - exp;
	else
		exp = fracExp + exp;

	if (exp < 0)
	{
		expSign = 1;
		exp = -exp;
	}
	else
	{
		expSign = 0;
	}

	if (exp > Max_exponent)
	{
		exp = Max_exponent;
		//errno = ERANGE;  // FIXME
	}

	double dblExp = 1.0;
	for (double* d = pow10; exp != 0; exp >>= 1, d++)
		if (exp & 1)
			dblExp *= *d;

	if (expSign)
		fraction /= dblExp;
	else
		fraction *= dblExp;

done:

	if (endptr)
		*endptr = (char*)p;

	return negative ? -fraction : fraction;
}
