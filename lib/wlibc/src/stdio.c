//##################################################################################################
//
//  Implementation of some funcs for stdio.h
//
//##################################################################################################

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "wlibc_cb.h"
#include "wlibc_panic.h"

FILE* stdin = 0;
FILE* stdout = 0;
FILE* stderr = 0;

enum Flags
{
	No_flags           = 0,
	Align_left         = 1 << 0,
	Force_sign         = 1 << 1,
	Space_instead_plus = 1 << 2,
	Alternate          = 1 << 3,
	Null_pad           = 1 << 4
};

enum Parse_stage
{
	//                          // print, scan
	None,                       //   +     +
	Flags,                      //   +     -   may by flags sequence or absent
	Width_may_be_asterisk,      //   +     -   may be '*', digits or absent
	Width_digits,               //   +     +   digits only
	Precision_dot_or_absent,    //   +     -   may be '.' or absent
	Precision_may_be_asterisk,  //   +     -   may be '*' or digits
	Precision_digits,           //   +     -   digits only
	Length,                     //   +     +   may be 'h', 'l', 'j', 'z', 't', 'L' or absent
	Length_may_be_h,            //   +     +   may be 'h' or absent
	Length_may_be_l,            //   +     +   may be 'l' or absent
	Specifier                   //   +     +   may be  d,i,u,o,x,X,f,F,e,E,g,G,a,A,c,s,p,n,%
};

enum Length_spec
{
	Length_none,
	Length_hh,
	Length_h,
	Length_l,
	Length_ll,
	Length_j,
	Length_z,
	Length_t,
	Length_L
};

enum Base
{
	Oct =  8,
	Dec = 10,
	Hex = 16
};

enum
{
	Char_mask  = (1 << (8 * sizeof(char)))  - 1,  // 0xff
	Short_mask = (1 << (8 * sizeof(short))) - 1   // 0xffff
};

// is_present flags
enum
{
	Flags_present     = 1 << 0,
	Width_present     = 1 << 1,
	Precision_present = 1 << 2,
	Length_present    = 1 << 3,
	Specifier_present = 1 << 4 
};

typedef struct
{
	unsigned present;    // is_present flags
	unsigned flags;      //
	unsigned width;      // integer
	unsigned precision;  // integer
	unsigned length;     // enum Length_spec
	unsigned specifier;  // char
} Spec_t;

static inline unsigned min(unsigned a, unsigned b) { return a < b ? a : b; }
static inline unsigned max(unsigned a, unsigned b) { return a > b ? a : b; }

//--------------------------------------------------------------------------------------------------
//  vsnprintf()
//--------------------------------------------------------------------------------------------------

static inline unsigned add_char(char* buf, unsigned sz, unsigned buf_ptr, char ch)
{
	if (buf_ptr < sz)
		buf[buf_ptr] = ch;
	return 1;
}

static inline unsigned add_chars(char* buf, unsigned sz, unsigned buf_ptr, char ch, unsigned count)
{
	for (int i=0; i<count; ++i)
		buf_ptr += add_char(buf, sz, buf_ptr, ch);
	return count;
}

static inline unsigned add_str(char* buf, unsigned sz, unsigned buf_ptr, const char* str, unsigned count)
{
	for (int i=0; i<count; ++i)
		buf_ptr += add_char(buf, sz, buf_ptr, str[i]);
	return count;
}

static int snprint_64bit_int(char* buf, size_t sz, uint64_t val, const Spec_t* spec, int base, bool positive)
{
	// PREPARATION

	if (base!=Oct && base!=Dec && base!=Hex)
		return 0; // unsupportes base

	// prepare digits
	enum { Len = 22 }; // 64 bits --> 22 digits max for Oct
	char digits[Len];
	unsigned digits_num = 0;
	do
	{
		// NOTE:  for Oct/Hex it bay be optimized:  val >>= 4/8
		uint64_t mod = val % base;
		val = val / base;
		digits[digits_num++] = mod<0xa ? '0'+mod : 'a'+mod-10;
	} while (val);

	// prepare sign
	char sign = 0; // vals:  no_sign(0), ' ', '+', '-'
	if (base == Dec)
	{
		if (positive)
		{
			if (spec->flags & Space_instead_plus)
				sign = ' ';

			if (spec->flags & Force_sign)
				sign = '+';
		}
		else // negative
			sign = '-';
	}

	// prepare prefix:  "0" for Oct, 0x for Hex
	char prefix = 0; // vals:  no_pref(0), Oct --> '0', Hex --> '0x'
	if ((spec->flags & Alternate)  &&  (base == Oct  ||  base == Hex))
	{
		prefix = base;
	}

	// calculate value length
	unsigned len = max(digits_num, spec->precision) + (sign ? 1 : 0); // length with prec and sign
	len += prefix==Hex ? 2 : (prefix==Oct ? 1 : 0);

	// OUTPUT

	unsigned buf_ptr = 0;

	// output padding for width (right alignment case)
	if (!(spec->flags & Align_left)  &&  len < spec->width)
	{
		char pad = (spec->flags & Null_pad) ? '0' : ' ';
		buf_ptr += add_chars(buf, sz, buf_ptr, pad, spec->width - len);
	}

	// output sign
	if (sign)
	{
		buf_ptr += add_char(buf, sz, buf_ptr, sign);
	}

	// output prefix
	if (prefix)
	{
		buf_ptr += add_char(buf, sz, buf_ptr, '0');
		if (prefix == Hex)
			buf_ptr += add_char(buf, sz, buf_ptr, 'x');
	}

	// output padding for precision
	if (digits_num < spec->precision)
	{
		buf_ptr += add_chars(buf, sz, buf_ptr, '0', spec->precision - digits_num);
	}

	// output digits
	for (int i=digits_num-1; i>=0; --i)
	{
		buf_ptr += add_char(buf, sz, buf_ptr, digits[i]);
	}

	// output padding for width (left alignment case)
	if ((spec->flags & Align_left)  &&  len < spec->width)
	{
		char pad = (spec->flags & Null_pad) ? '0' : ' ';
		buf_ptr += add_chars(buf, sz, buf_ptr, pad, spec->width - len);
	}

	return buf_ptr;
}

static inline int snprint_uint64(char* buf, size_t sz, uint64_t val, const Spec_t* spec, int base)
{
	return snprint_64bit_int(buf, sz, val, spec, base, true);
}

static inline int snprint_int64(char* buf, size_t sz, int64_t val, const Spec_t* spec, int base)
{
	return snprint_64bit_int(buf, sz, val<0?-val:val, spec, base, val>=0);
}

int wlibc_isnan(double x);
int wlibc_isinf(double x);

int __tolower(int c)
{
	if (c >= 'A'  &&  c <= 'Z')
		return c - 'A' + 'a';
	else
		return c;
}

static int snprint_float(char* buf, size_t sz, double val, const Spec_t* spec, int for_g)
{
	// PREPARATION

	// format:  sign|integer|fraction
	//          fraction may be:
	//            - scientific notation (mantissa/exponent) - 3.9265e+2
	//            - decimal float point                     - 392.65

	enum
	{
		Max_int          = 40,
		Max_frac         = 40,
		Max_exp          = 5,
		Default_prec     = 6,
		Default_prec_alt = 5
	};

	unsigned buf_ptr = 0;

	// check for negative
	int neg = 0;
	if (val < 0.0)
	{
		neg = 1;
		val *= -1.0;
	}

	// check for uppercase
	int upper = isupper(spec->specifier);
	char specc = __tolower(spec->specifier);  // TODO: use tolower()

	assert(specc=='f' || specc=='e' || specc=='a');  // %g was transformed at %e or %e

	// check for NAN
	if (wlibc_isnan(val))
	{
		int tot = neg + 3;
		if (spec->present & Width_present  &&  spec->width > tot)
			buf_ptr += add_chars(buf, sz, buf_ptr, ' ', spec->width - tot);
		if (neg)
			buf_ptr += add_char(buf, sz, buf_ptr, '-');
		buf_ptr += add_str(buf, sz, buf_ptr, upper ? "NAN" : "nan", 3);
		return buf_ptr;
	}

	// check for infinity
	if (wlibc_isinf(val))
	{
		int tot = neg + 3;
		if (spec->present & Width_present  &&  spec->width > tot)
			buf_ptr += add_chars(buf, sz, buf_ptr, ' ', spec->width - tot);
		if (neg)
			buf_ptr += add_char(buf, sz, buf_ptr, '-');
		buf_ptr += add_str(buf, sz, buf_ptr, upper ? "INF" : "inf", 3);
		return buf_ptr;
	}

	if (specc == 'a')
	{
		// TODO:  support hex float format
		specc = 'e';
	}

	// do simple normalization:  x.xxxx + exp
	int exp = 0;
	if (val != 0.0)
	{
		for (; val>=10; val/=10, exp++);
		for (; val<1.0;    val*=10, exp--);
	}

	// prepare integer part
	int int_len = 0;
	char int_str[Max_int];
	// format with exponent
	if (specc == 'e')
	{
		int i = (int) val;
		int_str[int_len++] = i + '0';
		val -= (double)i;
		val *= 10;
	}
	// format without exponent
	else if (exp > (int)sizeof(int_str)-1)
	{
		buf_ptr += add_str(buf, sz, buf_ptr, "OVERFLOW", 8);
		return buf_ptr;
	}
	else if (exp < 0)
	{
		int_str[int_len++] = '0';
	}
	else
	{
		// denormalize decimal float point format
		do
		{
			int i = (int) val;
			int_str[int_len++] = i + '0';
			val -= (double)i;
			val *= 10;
		} while (exp--);
	}

	// prepare fraction part
	int prec = -1;
	if (spec->present & Precision_present)
		prec = spec->precision;
	if (prec > Max_frac) prec = Max_frac;
	if (prec < 0)        prec = spec->present & Alternate ? Default_prec_alt : Default_prec;

	char frac_str [Max_frac + Max_exp];
	unsigned frac_len = 0;

	int round = 1;
	if (specc == 'f')  // format without exponent
	{
		for (; frac_len<prec && exp<-1; frac_len++, exp++)
			frac_str[frac_len] = '0';

		if (exp < -1)  // don't round off if we ran out of space
			round = 0;
	}

	for (; frac_len<prec; frac_len++)
	{
		int i = (int) val;
		frac_str[frac_len] = i + '0';
		val -= i;
		val *= 10;
	}

	// round off last digit
	if (round && val >= 5.0)
	{
		int i;
		for (i=frac_len-1; frac_str[i]=='9' && i>=0; --i)
			frac_str[i] = '0';

		if (i >= 0)
		{
			frac_str[i]++;
		}
		else
		{
			for (i=int_len-1; int_str[i]=='9' && i>=0 ; i--)
				int_str[i] = '0';

			if (i >= 0)
			{
				int_str[i]++;
			}
			else
			{
				for (i=int_len++; i>0; i--)
					int_str[i] = int_str[i-1];
				int_str[0] = '1';
			}
		}
	}

	if (for_g  &&  !(spec->flags & Alternate))
	{
		// discard nulls at the end of fraction part
		for (int i=frac_len-1; frac_str[i]=='0' && i>=0; --i)
			--frac_len;
	}

	// add exponent
	if (specc == 'e') // format with exponent
	{
		frac_str[frac_len++] = upper ? 'E' : 'e';
		if (exp < 0)
		{
			frac_str[frac_len++] = '-';
			exp *= -1;
		}
		else
		{
			frac_str[frac_len++] = '+';
		}
		// TODO:  to optimize may use for() instead of snprint_uint64()
		Spec_t exp_spec = { .width = 2, .flags = Null_pad }; // exponent always contains at least two digits
		frac_len += snprint_uint64(frac_str + frac_len, sizeof(frac_str) - frac_len, exp, &exp_spec, 10);
	}

	// prepare sign
	char sign = 0; // vals:  no_sign(0), ' ', '+', '-'
	if (neg)
	{
		sign = '-';
	}
	else // negative
	{
		if (spec->flags & Space_instead_plus)
			sign = ' ';
		if (spec->flags & Force_sign)
			sign = '+';
	}
	int sign_len = !!sign;

	// check for decimal point
	int point_len = (prec  ||  spec->flags & Alternate) ? 1 : 0;

	// prepare padding
	int pad_len = 0;
	int width = 0;
	if (spec->present & Width_present)
		width = spec->width;
	if ((spec->flags & Null_pad)  &&  !(spec->flags & Align_left))
	{
		pad_len = width - (sign_len +    frac_len + point_len + int_len);
		if (pad_len < 0)
			pad_len = 0;
	}

	// OUTPUT

	int tot = sign_len + frac_len + point_len + int_len + pad_len;

	if (!(spec->flags & Align_left)  &&  width > tot)
		buf_ptr += add_chars(buf, sz, buf_ptr, ' ', width - tot);

	if (sign)
		buf_ptr += add_char(buf, sz, buf_ptr, sign);

	buf_ptr += add_chars(buf, sz, buf_ptr, '0', pad_len);

	buf_ptr += add_str(buf, sz, buf_ptr, int_str, int_len);

	if (point_len  &&  !(for_g && (!frac_len || frac_str[0]=='e' || frac_str[0]=='E')))
		buf_ptr += add_char(buf, sz, buf_ptr, '.');

	buf_ptr += add_str(buf, sz, buf_ptr, frac_str, frac_len);

	if (spec->flags & Align_left  &&  width > tot)
		buf_ptr += add_chars(buf, sz, buf_ptr, ' ', width - tot);

	return buf_ptr;
}

static int snprint_char(char* buf, size_t sz, int ch, const Spec_t* spec)
{
	unsigned buf_ptr = 0;
	const int len = 1;  // NOTE:  failed for wchar_t

	// output padding for width (right alignment case)
	if (!(spec->flags & Align_left)  &&  len < spec->width)
	{
		buf_ptr += add_chars(buf, sz, buf_ptr, ' ', spec->width - len);
	}

	// output char
	buf_ptr += add_char(buf, sz, buf_ptr, ch);

	// output padding for width (left alignment case)
	if ((spec->flags & Align_left)  &&  len < spec->width)
	{
		buf_ptr += add_chars(buf, sz, buf_ptr, ' ', spec->width - len);
	}

	return buf_ptr;
}

static int snprint_str(char* buf, size_t sz, const char* str, const Spec_t* spec)
{
	unsigned buf_ptr = 0;

	unsigned len = strlen(str);
	if (spec->present & Precision_present)
		len = min(len, spec->precision);

	// output padding for width (right alignment case)
	if (!(spec->flags & Align_left)  &&  len < spec->width)
	{
		buf_ptr += add_chars(buf, sz, buf_ptr, ' ', spec->width - len);
	}

	// output string
	for (int i=0; i<len; ++i)
	{
		buf_ptr += add_char(buf, sz, buf_ptr, str[i]);
	}

	// output padding for width (left alignment case)
	if ((spec->flags & Align_left)  &&  len < spec->width)
	{
		buf_ptr += add_chars(buf, sz, buf_ptr, ' ', spec->width - len);
	}

	return buf_ptr;
}

static int snprint_wstr(char* buf, size_t sz, const wchar_t* wstr, const Spec_t* spec)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

// noinline function to work with floats, to prevent using FPU registers in vsnprint_float()
__attribute__((noinline))
static int vsnprint_float(char* buf, size_t sz, const va_list* args, const Spec_t* spec)
{
	int res = 0;
	double val = 0;
	switch (spec->length)
	{
		case Length_none:  val = va_arg(*args, double);       break;
		//case Length_L:   val = va_arg(*args, long double);  break;  // unsupported
		default:
			return -1;  // unexpected length
	}
	if (spec->specifier == 'g'  ||  spec->specifier == 'G')
	{
		// dry ryn to determine the shortest representation: %e or %f
		Spec_t s;
		memcpy(&s, spec, sizeof(s));
		s.present |= Length_present;
		s.length = 0;
		s.specifier = 'f';
		int len_f = snprint_float(NULL, 0, val, &s, 1);
		s.specifier = 'e';
		int len_e = snprint_float(NULL, 0, val, &s, 1);
		s.specifier = (len_f < len_e) ? (spec->specifier - 'g' + 'f' ) : (spec->specifier - 'g' + 'e' );
		res = snprint_float(buf, sz, val, &s, 1);
	}
	else
	{
		res = snprint_float(buf, sz, val, spec, 0);
	}
	return res;
}

static int vsnprint_spec(char* buf, size_t sz, const va_list* args, unsigned written, const Spec_t* spec)
{
	int res = 0;
	switch (spec->specifier)
	{
		case 'd':
		case 'i':
		{
			// NOTE:  here need print appropriate size, but for simplicity it is used
			//        big var int64_t, it may be optimized
			int64_t val = 0;
			switch (spec->length)
			{
				case Length_none:  val = va_arg(*args, int);               break;
				case Length_hh:    val = va_arg(*args, int) & Char_mask;   break; // signed char
				case Length_h:     val = va_arg(*args, int) & Short_mask;  break; // short int
				case Length_l:     val = va_arg(*args, long int);          break;
				case Length_ll:    val = va_arg(*args, long long int);     break;
				case Length_j:     val = va_arg(*args, intmax_t);          break;
				case Length_z:     val = va_arg(*args, size_t);            break;
				case Length_t:     val = va_arg(*args, ptrdiff_t);         break;
				case Length_L:     val = va_arg(*args, int64_t);           break; // not standart
				default:
					return -1;  // unexpected length
			}
			res = snprint_int64(buf, sz, val, spec, Dec);
			break;
		}
		case 'u':
		case 'o':
		case 'x':
		case 'X':
		{
			// NOTE:  here need print appropriate size, but for simplicity it is used
			//        big var uint64_t, it may be optimized
			uint64_t val = 0;
			switch (spec->length)
			{
				case Length_none:  val = va_arg(*args, unsigned int);               break;
				case Length_hh:    val = va_arg(*args, unsigned int) & Char_mask;   break; // unsigned char
				case Length_h:     val = va_arg(*args, unsigned int) & Short_mask;  break; // unsigned short int
				case Length_l:     val = va_arg(*args, unsigned long int);          break;
				case Length_ll:    val = va_arg(*args, unsigned long long int);     break;
				case Length_j:     val = va_arg(*args, uintmax_t);                  break;
				case Length_z:     val = va_arg(*args, size_t);                     break;
				case Length_t:     val = va_arg(*args, ptrdiff_t);                  break;
				case Length_L:     val = va_arg(*args, uint64_t);                   break; // not standart
				default:
					return -1;  // unexpected length
			}
			int base = spec->specifier == 'u' ? Dec : (spec->specifier == 'o' ? Oct : Hex);
			res = snprint_uint64(buf, sz, val, spec, base);
			break;
		}
		case 'f':
		case 'F':
		case 'e':
		case 'E':
		case 'g':  // shortest representation: %e or %f
		case 'G':  // shortest representation: %E or %F
		case 'a':
		case 'A':
		{
			#if 0
			double val = 0;
			switch (spec->length)
			{
				case Length_none:  val = va_arg(*args, double);       break;
				//case Length_L:   val = va_arg(*args, long double);  break;  // unsupported
				default:
					return -1;  // unexpected length
			}
			if (spec->specifier == 'g'  ||  spec->specifier == 'G')
			{
				// dry ryn to determine the shortest representation: %e or %f
				Spec_t s;
				memcpy(&s, spec, sizeof(s));
				s.present |= Length_present;
				s.length = 0;
				s.specifier = 'f';
				int len_f = snprint_float(NULL, 0, val, &s, 1);
				s.specifier = 'e';
				int len_e = snprint_float(NULL, 0, val, &s, 1);
				s.specifier = (len_f < len_e) ? (spec->specifier - 'g' + 'f' ) : (spec->specifier - 'g' + 'e' );
				res = snprint_float(buf, sz, val, &s, 1);
			}
			else
			{
				res = snprint_float(buf, sz, val, spec, 0);
			}
			#else
			res = vsnprint_float(buf, sz, args, spec);
			#endif
			break;
		}
		case 'c':
		{
			int val = 0;
			switch (spec->length)
			{
				case Length_none:  val = va_arg(*args, int);     break;
				case Length_l:     val = va_arg(*args, wint_t);  break;
				default:
					return -1;  // unexpected length
			}
			res = snprint_char(buf, sz, val, spec);
			break;
		}
		case 's':
		{
			switch (spec->length)
			{
				case Length_none:
				{
					const char* val = va_arg(*args, char*);
					res = snprint_str(buf, sz, val, spec);
					break;
				}
				case Length_l:
				{
					const wchar_t* val = va_arg(*args, wchar_t*);
					res = snprint_wstr(buf, sz, val, spec);
					break;
				}
				default:
					return -1;  // unexpected length
			}
			break;
		}
		case 'p':
		{
			// NOTE:  here need print appropriate size, but for simplicity it is used
			//        big var uint64_t, it may be optimized
			switch (spec->length)
			{
				case Length_none:
				{
					uintptr_t val = (uintptr_t) va_arg(*args, void*);
					res = snprint_uint64(buf, sz, val, spec, Hex);
					break;
				}
				default:
					return -1;  // unexpected length
			}
			break;
		}
		case 'n':
		{
			switch (spec->length)
			{
				case Length_none:  *va_arg(*args, int*)           = written;  break;
				case Length_hh:    *va_arg(*args, signed char*)   = written;  break;
				case Length_h:     *va_arg(*args, short int*)     = written;  break;
				case Length_l:     *va_arg(*args, long int*)      = written;  break;
				case Length_ll:    *va_arg(*args, long long int*) = written;  break;
				case Length_j:     *va_arg(*args, intmax_t*)      = written;  break;
				case Length_z:     *va_arg(*args, size_t*)        = written;  break;
				case Length_t:     *va_arg(*args, ptrdiff_t*)     = written;  break;
				case Length_L:     *va_arg(*args, uint64_t*)      = written;  break; // not standart
				default:
					return -1;  // unexpected length
			}
			res = 0;
			break;
		}
		case '%':
		{
			if (spec->present == Specifier_present)
			{
				if (sz > 0)
					buf[0] = '%';
				res = sizeof(char);
			}
			else
				return -1;
			break;
		}
		default:
			res = -1;  // unknown spec
	}
	return res;
}

// type va_list may be implemented as pointer to the stack (sparc, arm) or array (x86-64),
// therefore to according of system arch passing va_list as pointer has differents
#ifdef __x86_64
#  define MAKE_PTR_FROM_VA_LIST(args) ((va_list*)(args))  /* va_list is array */
#else
#  define MAKE_PTR_FROM_VA_LIST(args) (&(args))
#endif

int vsnprintf(char* buf, size_t sz, const char* format, va_list args)
{
	//%[flags][width][.precision][length]specifier

	unsigned parse = None;
	char ch = 0;
	unsigned frm_ptr = 0;
	unsigned buf_ptr = 0;
	Spec_t spec;

	while ((ch = format[frm_ptr++]))
	{
		if (parse == None)
		{
			if (ch == '%')
			{
				memset(&spec, 0, sizeof(spec));
				parse = Flags;
			}
			else
			{
				buf_ptr += add_char(buf, sz, buf_ptr, ch);
				if (ch == '\n')
					buf_ptr += add_char(buf, sz, buf_ptr, '\r');
			}
		}
		else
		{
			if (parse == Flags)
			{
				// may be:  '-', '+', ' ', '#', '0'
				unsigned old = spec.present;
				spec.present |= Flags_present;
				switch (ch)
				{
					case '-':  spec.flags |= Align_left;          continue;
					case '+':  spec.flags |= Force_sign;          continue;
					case ' ':  spec.flags |= Space_instead_plus;  continue;
					case '#':  spec.flags |= Alternate;           continue;
					case '0':  spec.flags |= Null_pad;            continue;
					default:
						spec.present = old;
						parse = Width_may_be_asterisk;
				}
			}

			if (parse == Width_may_be_asterisk)
			{
				if (ch == '*')
				{
					spec.width = va_arg(args, unsigned);
					spec.present |= Width_present;
					parse = Precision_dot_or_absent;
					continue;
				}
				else
					parse = Width_digits;
			}

			if (parse == Width_digits)
			{
				if (ch >= '0'  &&  ch <= '9')
				{
					spec.width = spec.width * 10 + (ch - '0');
					spec.present |= Width_present;
					continue;
				}
				else
					parse = Precision_dot_or_absent;
			}

			if (parse == Precision_dot_or_absent)
			{
				if (ch == '.')
				{
					spec.present |= Precision_present;
					parse = Precision_may_be_asterisk;
					continue;
				}
				else
					parse = Length;
			}

			if (parse == Precision_may_be_asterisk)
			{
				if (ch == '*')
				{
					spec.precision = va_arg(args, unsigned);
					parse = Length;
					continue;
				}
				else
					parse = Precision_digits;
			}

			if (parse == Precision_digits)
			{
				if (ch >= '0'  &&  ch <= '9')
				{
					spec.precision = spec.precision * 10 + (ch - '0');
					continue;
				}
				else
					parse = Length;
			}

			if (parse == Length)
			{
				// may be:  hh h l ll j z t L
				spec.present |= Length_present;
				switch (ch)
				{
					case 'h':  spec.length = Length_h;  parse = Length_may_be_h;  continue;
					case 'l':  spec.length = Length_l;  parse = Length_may_be_l;  continue;
					case 'j':  spec.length = Length_j;  parse = Specifier;  continue;
					case 'z':  spec.length = Length_z;  parse = Specifier;  continue;
					case 't':  spec.length = Length_t;  parse = Specifier;  continue;
					case 'L':  spec.length = Length_L;  parse = Specifier;  continue;
					default:
						spec.present &= ~Length_present;
						parse = Specifier;
				}
			}

			if (parse == Length_may_be_h)
			{
				if (ch == 'h')
				{
					spec.length = Length_hh;
					parse = Specifier;
					continue;
				}
				else
					parse = Specifier;
			}

			if (parse == Length_may_be_l)
			{
				if (ch == 'l')
				{
					spec.length = Length_ll;
					parse = Specifier;
					continue;
				}
				else
					parse = Specifier;
			}

			if (parse == Specifier)
			{
				spec.present |= Specifier_present;
				spec.specifier = ch;
				unsigned p = min(sz, buf_ptr);
				int res = vsnprint_spec(buf + p, sz - p, MAKE_PTR_FROM_VA_LIST(args), buf_ptr, &spec);
				if (res < 0)
				{
					buf_ptr = res;
					break;
				}
				buf_ptr += res;
				parse = None;
			}
		}
	}

	// set string termination
	if (sz)
	{
		if ((int)buf_ptr < 0)    // error
			buf[0] = '\0';
		else if (buf_ptr < sz)   // normal
			buf[buf_ptr] = '\0';
		else                     // overflow
			buf[sz-1] = '\0';
	}

	/**/
	if ((int)buf_ptr < 0)
	{
		printf("WRN:  %s:  res=%d.\n", __func__, buf_ptr);
		buf_ptr = 0;
	}
	/*~*/

	return buf_ptr;
}

//--------------------------------------------------------------------------------------------------
//  vsscanf()
//--------------------------------------------------------------------------------------------------

// return number of scanned chars from str
static int vsscan_spec(const char* str, const va_list* args, const Spec_t* spec)
{
	// FIXME:  make funcs:
	//           strtoull(str, sz, end, base)
	//           strtold(str, sz, end)
	char buf[128];
	const char* s = str;
	if (spec->present & Width_present)
	{
		if (spec->width >= sizeof(buf))
			return -1;
		strncpy(buf, str, spec->width);
		buf[spec->width] = '\0';
		s = buf;
	}

	char* end = 0;
	switch (spec->specifier)
	{
		case 'd':
		case 'i':
		{
			// NOTE:  here need scan appropriate size, but for simplicity it is used
			//        big var int64_t, it may be optimized

			uint64_t val = strtoul(s, &end, 10);  // FIXME:  now scanned only unsugned long, not uint64_t
			switch (spec->length)
			{
				case Length_none:  *va_arg(*args, int*)           = val;  break;
				case Length_hh:    *va_arg(*args, signed char*)   = val;  break;
				case Length_h:     *va_arg(*args, short int*)     = val;  break;
				case Length_l:     *va_arg(*args, long int*)      = val;  break;
				case Length_ll:    *va_arg(*args, long long int*) = val;  break;
				case Length_j:     *va_arg(*args, intmax_t*)      = val;  break;
				case Length_z:     *va_arg(*args, size_t*)        = val;  break;
				case Length_t:     *va_arg(*args, ptrdiff_t*)     = val;  break;
				case Length_L:     *va_arg(*args, int64_t*)       = val;  break; // not standart
				default:
					return -1;  // unexpected length
			}
			break;
		}
		case 'u':
		case 'o':
		case 'x':
		case 'X': // not standart
		{
			// NOTE:  here need scan appropriate size, but for simplicity it is used
			//        big var uint64_t, it may be optimized
			int base = spec->specifier == 'u' ? Dec : (spec->specifier == 'o' ? Oct : Hex);
			uint64_t val = strtoul(s, &end, base);  // FIXME:  now scanned only unsugned long, not uint64_t
			switch (spec->length)
			{
				case Length_none:  *va_arg(*args, unsigned int*)           = val;  break;
				case Length_hh:    *va_arg(*args, unsigned char*)          = val;  break;
				case Length_h:     *va_arg(*args, unsigned short int*)     = val;  break;
				case Length_l:     *va_arg(*args, unsigned long int*)      = val;  break;
				case Length_ll:    *va_arg(*args, unsigned long long int*) = val;  break;
				case Length_j:     *va_arg(*args, uintmax_t*)              = val;  break;
				case Length_z:     *va_arg(*args, size_t*)                 = val;  break;
				case Length_t:     *va_arg(*args, ptrdiff_t*)              = val;  break;
				case Length_L:     *va_arg(*args, uint64_t*)               = val;  break; // not standart
				default:
					return -1;  // unexpected length
			}
			break;
		}
		case 'f':
		case 'F': // not standart
		case 'e':
		case 'E': // not standard
		case 'g':
		case 'G': // not standard
		case 'a':
		case 'A': // not standard
		{
			switch (spec->length)
			{
				case Length_none:
				{
					double val = strtod(s, &end);  // FIXME:  now scanned double, but use as float
					*va_arg(*args, float*) = val;
					break;
				}
				case Length_l:
				{
					double val = strtod(s, &end);
					*va_arg(*args, double*) = val;
					break;
				}
				/* TODO
				case Length_L:
				{
					double val = strtod(s, &end);  // FIXME:  now scanned double, but use as long double
					*va_arg(*args, long double*) = val;
					break;
				}
				*/
				default:
					return -1;  // unexpected length
			}
			break;
		}
		case 'c':
		{
			switch (spec->length)
			{
				case Length_none:
				{
					*va_arg(*args, char*) = *(char*) s;
					end = (char*)s + sizeof(char);
					break;
				}
				case Length_l:
				{
					*va_arg(*args, wchar_t*) = *(wchar_t*) s;
					end = (char*)s + sizeof(wchar_t);
					break;
				}
				default:
					return -1;  // unexpected length
			}
			break;
		}
		/* TODO:
		case 's':
		{
			switch (spec->length)
			{
				case Length_none:
				{
					const char* val = va_arg(*args, char*);
					res = snprint_str(buf, sz, val, spec);
					break;
				}
				case Length_l:
				{
					const wchar_t* val = va_arg(*args, wchar_t*);
					res = snprint_wstr(buf, sz, val, spec);
					break;
				}
				
				default:
					return -1;  // unexpected length
			}
			break;
		}
		case 'p':
		case 'n':
		{
			// NOTE:  here need print appropriate size, but for simplicity it is used
			//        big var int64_t, it may be optimized
			switch (spec->length)
			{
				case Length_none:  *va_arg(*args, int*)           = written;  break;
				case Length_hh:    *va_arg(*args, signed char*)   = written;  break;
				case Length_h:     *va_arg(*args, short int*)     = written;  break;
				case Length_l:     *va_arg(*args, long int*)      = written;  break;
				case Length_ll:    *va_arg(*args, long long int*) = written;  break;
				case Length_j:     *va_arg(*args, intmax_t*)      = written;  break;
				case Length_z:     *va_arg(*args, size_t*)        = written;  break;
				case Length_t:     *va_arg(*args, ptrdiff_t*)     = written;  break;
				case Length_L:     *va_arg(*args, uint64_t*)      = written;  break;
				default:
					return -1;  // unexpected length
			}
			res = 0;
			break;
		}
		case '%':
		{
			if (sz > 0)
				buf[0] = '%';
			res = sizeof(char);
		}
		*/
		default:
			return -1;  // unknown spec
	}
	return end - s;
}

int vsscanf(const char* str, const char* format, va_list args)
{
	//printf("str=%s, fmt=%s.\n", str, format);

	//%[*][width][length]specifier

	unsigned parse = None;
	char ch = 0;
	unsigned frm_ptr = 0;
	unsigned str_ptr = 0;
	Spec_t spec;
	int scanned_cnt = 0;

	while ((ch = format[frm_ptr++]))
	{
		if (parse == None)
		{
			if (ch == '%')
			{
				memset(&spec, 0, sizeof(spec));
				parse = Width_digits;
			}
			else
			{
				if (ch != str[str_ptr++])
					break;
			}
		}
		// TODO:  process '*'
		else
		{
			if (parse == Width_digits)
			{
				if (ch >= '0'  &&  ch <= '9')
				{
					spec.width = spec.width * 10 + (ch - '0');
					spec.present |= Width_present;
					continue;
				}
				else
					parse = Length;
			}

			if (parse == Length)
			{
				// may be:  hh h l ll j z t L
				spec.present |= Length_present;
				switch (ch)
				{
					case 'h':  spec.length = Length_h;  parse = Length_may_be_h;  continue;
					case 'l':  spec.length = Length_l;  parse = Length_may_be_l;  continue;
					case 'j':  spec.length = Length_j;  parse = Specifier;  continue;
					case 'z':  spec.length = Length_z;  parse = Specifier;  continue;
					case 't':  spec.length = Length_t;  parse = Specifier;  continue;
					case 'L':  spec.length = Length_L;  parse = Specifier;  continue;
					default:
						spec.present &= ~Length_present;
						parse = Specifier;
				}
			}

			if (parse == Length_may_be_h)
			{
				if (ch == 'h')
				{
					spec.length = Length_hh;
					parse = Specifier;
					continue;
				}
				else
					parse = Specifier;
			}

			if (parse == Length_may_be_l)
			{
				if (ch == 'l')
				{
					spec.length = Length_ll;
					parse = Specifier;
					continue;
				}
				else
					parse = Specifier;
			}

			if (parse == Specifier)
			{
				spec.present |= Specifier_present;
				spec.specifier = ch;
				int res = vsscan_spec(str + str_ptr, MAKE_PTR_FROM_VA_LIST(args), &spec);
				if (res < 0)
				{
					scanned_cnt = -1;
					break;
				}
				str_ptr += res;
				scanned_cnt++;
				parse = None;
			}
		}
	}
	return scanned_cnt;
}

//--------------------------------------------------------------------------------------------------
//  other functions ...
//--------------------------------------------------------------------------------------------------

int vsprintf(char* buf, const char* format, va_list args)
{
	enum { Big_sz = 0x7fffffff };
	return vsnprintf(buf, Big_sz, format, args);
}

int snprintf(char* buf, size_t sz, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int res = vsnprintf(buf, sz, format, args);
	va_end(args);
	return res;
}

int sprintf(char* buf, const char* format, ...)
{
	// may be optimized
	va_list args;
	va_start(args, format);
	enum { Big_sz = 0x7fffffff };
	int res = vsnprintf(buf, Big_sz, format, args);
	va_end(args);
	return res;
}

// output helpers
static int putsn(const char* str, unsigned len);

int vprintf(const char* format, va_list args)
{
	char str[256];
	int res = vsnprintf(str, sizeof(str), format, args);
	if (res > 0)
		putsn(str, res);
	return res;
}

int printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int res = vprintf(format, args);
	va_end(args);
	return res;
}

int puts(const char* str)
{
	char s[256];
	int res = snprintf(s, sizeof(s), "%s\n", str);
	if (res > 0)
		putsn(s, res);
	return res;
}

int sscanf(const char* str, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int res = vsscanf(str, format, args);
	va_end(args);
	return res;
}

static int putsn(const char* str, unsigned len)
{
	if (!len)
		return 0;

	Wlibc_callbacks_t* cb = wlibc_callbacks_get();

	if (cb->out_string)
	{
		cb->out_string(str, len);
	}
	else if (cb->out_char)
	{
		while (len--)
		{
			cb->out_char(*str);
			if (*str == '\n')
				cb->out_char('\r');
			str++;
		}
	}
	else
	{
		return 1;
	}
	return 0;
}

#undef fgetc // stdio.h did some macro
int fgetc(FILE* stream)
{
	(void)stream;

	Wlibc_callbacks_t* cb = wlibc_callbacks_get();

	if (!cb->in_char)
		return 0;

	int c = 0;
	while (!(c = cb->in_char()));
	return c;
}

#undef fputc // stdio.h did some macro
int fputc(int c, FILE* stream)
{
	(void)stream;

	Wlibc_callbacks_t* cb = wlibc_callbacks_get();

	if (cb->out_char)
	{
		cb->out_char(c);
	}
	else
	{
		char ch = c;
		putsn(&ch, 1);
	}
	return c;
}

int fputs(const char* str, FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

#undef putchar // stdio.h did some macro
int putchar(int c)
{
	return fputc(c, (FILE*)-1);
}

#undef getchar // stdio.h did some macro
int getchar()
{
	return fgetc((FILE*)-1);
}

int fflush(FILE* stream)
{
	(void)stream;
	// nothing, output is not buffered
	return 0;
}

int fileno(FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int fclose(FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream)
{
	//printf("%s:  sz=%zu, cnt=%zu, stream=%p:  '%.*s'.\n", __func__, size, count, stream, (int)(size*count), (char*)ptr);
	printf("%s:  %.*s", __func__, (int)(size*count), (char*)ptr);
	return size * count;
}

FILE* fopen64(const char* filename, const char* opentype)
{
	printf("%s:  filename=%s!\n", __func__, filename);
	//panic("%s:  implement me - filename=%s!\n", __func__, filename);
	return 0;
}

__off64_t ftello64(FILE *stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int fseeko64(FILE* stream, __off64_t offset, int whence)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

__off64_t lseek64(int fd, __off64_t offset, int whence)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

#undef getc // stdio.h did some macro
int getc(FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

#undef putc // stdio.h did some macro
int putc(int character, FILE* stream)
{
	//printf("%s:  stream=%p:  c='%c'.\n", __func__, stream, character);
	printf("%c", character);
	return 0;
}

int ungetc(int character, FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

int setvbuf(FILE* stream, char* buffer, int mode, size_t size)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

FILE* fopen(const char* path, const char* mode)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

FILE* fdopen(int fd, const char* mode)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

FILE* freopen(const char* path, const char* mode, FILE* stream)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}


