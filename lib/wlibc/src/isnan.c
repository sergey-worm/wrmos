//##################################################################################################
//
//  Implementation isnan() function.
//
//##################################################################################################

#include <stdint.h>

typedef union
{
	double value;
	struct
	{
		#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		uint32_t msw;
		uint32_t lsw;
		 #elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		uint32_t lsw;
		uint32_t msw;
		#endif
	} parts;
} ieee_double_shape_type;

// get two 32 bit ints from a double
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

// get a 32 bit int from a float
#define GET_FLOAT_WORD(i, d)          \
	do {                              \
		ieee_float_shape_type gf_u;   \
		gf_u.value = (d);             \
		(i) = gf_u.word;              \
	} while (0)

int wlibc_isnan(double x)
{
	int32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	hx &= 0x7fffffff;
	hx |= (uint32_t)(lx|(-lx))>>31;
	hx = 0x7ff00000 - hx;
	return (int)(((uint32_t)hx)>>31);
}

int wlibc_isnanf(float x)
{
	int32_t ix;
	GET_FLOAT_WORD(ix, x);
	ix &= 0x7fffffff;
	ix = 0x7f800000 - ix;
	return (int)(((uint32_t)(ix))>>31);
}

int wlibc_isinf(double x)
{
	int32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	lx |= (hx & 0x7fffffff) ^ 0x7ff00000;
	lx |= -lx;
	return ~(lx >> 31) & (hx >> 30);
}

int wlibc_finite(double x)
{
	int32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	(void)lx;
	// Finite numbers have at least one zero bit in exponent.
	// All other numbers will result in 0xffffffff after OR:
	return (hx | 0x800fffff) != 0xffffffff;
}
