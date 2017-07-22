//##################################################################################################
//
//  Implementation of some funcs for math.h.
//
//##################################################################################################

// FIXME:  replace it to lib/libm

#include <math.h>
#include <stdint.h>

#error USE LIBM INSTEAD

typedef union
{
	double value;
	struct
	{
		#if Cfg_endianess == big
		uint32_t msw;
		uint32_t lsw;
		#else
		uint32_t lsw;
		uint32_t msw;
		#endif
	} parts;
} ieee_double_shape_type;

// Get two 32 bit ints from a double.
#define EXTRACT_WORDS(ix0, ix1, d)    \
	do                                \
	{                                 \
		ieee_double_shape_type ew_u;  \
		ew_u.value = (d);             \
		(ix0) = ew_u.parts.msw;       \
		(ix1) = ew_u.parts.lsw;       \
	} while (0);

typedef union
{
	float value;
	uint32_t word;
} ieee_float_shape_type;

// Get a 32 bit int from a float.
#define GET_FLOAT_WORD(i, d)          \
	do {                              \
		ieee_float_shape_type gf_u;   \
		gf_u.value = (d);             \
		(i) = gf_u.word;              \
	} while (0)

int __isnan(double x)
{
	int32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	hx &= 0x7fffffff;
	hx |= (uint32_t)(lx|(-lx))>>31;
	hx = 0x7ff00000 - hx;
	return (int)(((uint32_t)hx)>>31);
}

int __isnanf(float x)
{
	int32_t ix;
	GET_FLOAT_WORD(ix, x);
	ix &= 0x7fffffff;
	ix = 0x7f800000 - ix;
	return (int)(((uint32_t)(ix))>>31);
}

int __isinf(double x)
{
	int32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	lx |= (hx & 0x7fffffff) ^ 0x7ff00000;
	lx |= -lx;
	return ~(lx >> 31) & (hx >> 30);
}

int __finite(double x)
{
	int32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	(void)lx;
	// Finite numbers have at least one zero bit in exponent.
	// All other numbers will result in 0xffffffff after OR:
	return (hx | 0x800fffff) != 0xffffffff;
}

double floor(double x)            { assert(0 && "Implement me!"); return 0; }
double ceil(double x)             { assert(0 && "Implement me!"); return 0; }
double tan(double x)              { /*assert(0 && "Implement me!");*/ return 0; }
double sqrt(double x)             { /*assert(0 && "Implement me!");*/ return 0; }
double pow(double x, double y)    { /*assert(0 && "Implement me!");*/ return 0; }
double log(double x)              { assert(0 && "Implement me!"); return 0; }
double exp(double x)              { assert(0 && "Implement me!"); return 0; }
double round(double x)            { assert(0 && "Implement me!"); return 0; }
double nan(const char* tagp)      { assert(0 && "Implement me!"); return 0; }
float  nanf(const char* tagp)     { assert(0 && "Implement me!"); return 0; }


double power(double x, unsigned y)
{
	double res = 1;
	while (y--)
		res *= x;
	return res;
}

unsigned factorial(unsigned x)
{
	unsigned res = 1;
	for (unsigned i=1; i<=x; ++i)
		res *= i;;
	return res;
}

// Maclaurin series
double sin(double x)
{
	enum { N = 10 };
	double res = 0;
	for (unsigned i=0; i<N; ++i)
		res += power(-1, i) * power(x, 2 * i + 1) / factorial(2 * i + 1);
	return res;
}

// Maclaurin series
double cos(double x)
{
	enum { N = 10 };
	double res = 0;
	for (unsigned i=0; i<N; ++i)
		res += power(-1, i) * power(x, 2 * i) / factorial(2 * i);
	return res;
}

// Maclaurin series
double asin(double x)
{
	if (x < -1  ||  x > 1)
	{
		assert(0 && "atan() - wrong arguments");
		return NAN;
	}
	enum { N = 10 };
	double res = 0;
	for (unsigned i=0; i<N; ++i)
		res += factorial(2 * i) * power(x, 2 * i + 1) / (power(4, i) * power(factorial(i), 2) * (2 * i + 1));
	return res;
}

// Maclaurin series
double atan(double x)
{
	if (x < -1  ||  x > 1)
	{
		//assert(0 && "atan() - wrong arguments");
		return NAN;
	}
	enum { N = 10 };
	double res = 0;
	for (unsigned i=0; i<N; ++i)
		res += power(-1, i) * power(x, 2 * i + 1) / (2 * i + 1);
	return res;
}

double atan2(double x, double y)
{
	if (x > 0)
		return atan(y / x);

	if (x < 0  &&  y >= 0)
		return atan(y / x) + M_PI;

	if (x < 0  &&  y < 0)
		return atan(y / x) - M_PI;

	if (x == 0  &&  y > 0)
		return M_PI / 2;

	if (x == 0  &&  y < 0)
		return -M_PI / 2;

	if (x == 0  &&  y == 0)
	{
		//assert(0 && "atan2(0,0) - wrong arguments");
		return NAN;
	}
	return NAN;
}


