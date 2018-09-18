//##################################################################################################
//
//  Test wlibc funcs.
//
//##################################################################################################

#include "stdio.h"
#include "stdint.h"
#include "string.h"

//--------------------------------------------------------------------------------------------------
//  test:  snprintf
//--------------------------------------------------------------------------------------------------

/*
// This func compare result sprintf() and snprintf(). It is unesuful for me
// because my sprintf() is based on snprintf() and result will be the same.
static void complex_snprintf_test(void)
{
	char buf1[1024];
	char buf2[1024];

	// floats
	const char *fp_fmt[] = {
		"%1.1f",
		"%-1.5f",
		"%1.5f",
		"%123.9f",
		"%10.5f",
		"% 10.5f",
		"%+22.9f",
		"%+4.9f",
		"%01.3f",
		"%4f",
		"%3.1f",
		"%3.2f",
		"%.0f",
		"%f",
		"-16.16f",
		NULL
	};
	double fp_nums[] = { 6442452944.1234, -1.5, 134.21, 91340.2, 341.1234, 0203.9,
	                     0.96, 0.996, 0.9996, 1.996, 4.136, 0 };

	// integers
	const char *int_fmt[] = {
		"%-1.5d",
		"%1.5d",
		"%123.9d",
		"%5.5d",
		"%10.5d",
		"% 10.5d",
		"%+22.33d",
		"%01.3d",
		"%4d",
		"%d",
		NULL
	};
	long int_nums[] = { -1, 134, 91340, 341, 0203, 0 };

	// strings
	const char *str_fmt[] = {
		"10.5s",
		"5.10s",
		"10.1s",
		"0.10s",
		"10.0s",
		"1.10s",
		"%s",
		"%.1s",
		"%.10s",
		"%10s",
		NULL
	};
	const char *str_vals[] = { "hello", "a", "", "a longer string", NULL };

	int fail = 0;
	int num = 0;

	puts("Testing snprintf format codes against system sprintf...\n");

	for (int x=0; fp_fmt[x] ; x++)
	{
		for (int y=0; fp_nums[y]; y++)
		{
			int l1 = snprintf(NULL, 0, fp_fmt[x], fp_nums[y]);
			int l2 = snprintf(buf1, sizeof(buf1), fp_fmt[x], fp_nums[y]);
			sprintf(buf2, fp_fmt[x], fp_nums[y]);
			if (strcmp(buf1, buf2))
			{
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n", 
				       fp_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2)
			{
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, fp_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	for (int x=0; int_fmt[x]; x++)
	{
		for (int y=0; int_nums[y] ; y++)
		{
			int l1 = snprintf(NULL, 0, int_fmt[x], int_nums[y]);
			int l2 = snprintf(buf1, sizeof(buf1), int_fmt[x], int_nums[y]);
			sprintf (buf2, int_fmt[x], int_nums[y]);
			if (strcmp (buf1, buf2))
			{
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n", 
				       int_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2)
			{
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, int_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	for (int x=0; str_fmt[x]; x++)
	{
		for (int y=0; str_vals[y]; y++)
		{
			int l1 = snprintf(NULL, 0, str_fmt[x], str_vals[y]);
			int l2 = snprintf(buf1, sizeof(buf1), str_fmt[x], str_vals[y]);
			sprintf (buf2, str_fmt[x], str_vals[y]);
			if (strcmp (buf1, buf2))
			{
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n", 
				       str_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2)
			{
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, str_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	printf ("%d tests failed out of %d.\n", fail, num);

	printf("seeing how many digits we support\n");
	{
		double v0 = 0.12345678901234567890123456789012345678901;
		for (int x=0; x<100; x++)
		{
			snprintf(buf1, sizeof(buf1), "%1.1f", v0*pow(10, x));
			sprintf(buf2,                "%1.1f", v0*pow(10, x));
			if (strcmp(buf1, buf2))
			{
				printf("we seem to support %d digits\n", x-1);
				v0 = 0;
				break;
			}
		}
		if (v0 != 0)
			printf("  ... something going wrong.\n");
	}
}
*/

static char* get_buf(unsigned sz)
{
	static char buf_1 [1];
	static char buf_2 [2];
	static char buf_3 [3];
	static char buf_4 [4];
	static char buf_5 [5];
	static char buf_6 [6];
	static char buf_7 [7];
	static char buf_8 [8];
	static char buf_9 [9];
	static char buf_10 [10];
	static char buf_11 [11];
	static char buf_12 [12];
	static char buf_13 [13];
	static char buf_14 [14];
	static char buf_15 [15];
	static char buf_16 [16];
	static char buf_17 [17];
	static char buf_18 [18];
	static char buf_19 [19];
	static char buf_20 [20];
	static char buf_21 [21];
	static char buf_22 [22];
	static char buf_23 [23];
	static char buf_24 [24];
	static char buf_25 [25];
	static char buf_26 [26];
	static char buf_27 [27];
	static char buf_28 [28];
	static char buf_29 [29];
	static char buf_30 [30];
	switch (sz)
	{
		case  1:  return buf_1;
		case  2:  return buf_2;
		case  3:  return buf_3;
		case  4:  return buf_4;
		case  5:  return buf_5;
		case  6:  return buf_6;
		case  7:  return buf_7;
		case  8:  return buf_8;
		case  9:  return buf_9;
		case 10:  return buf_10;
		case 11:  return buf_11;
		case 12:  return buf_12;
		case 13:  return buf_13;
		case 14:  return buf_14;
		case 15:  return buf_15;
		case 16:  return buf_16;
		case 17:  return buf_17;
		case 18:  return buf_18;
		case 19:  return buf_19;
		case 20:  return buf_20;
		case 21:  return buf_21;
		case 22:  return buf_22;
		case 23:  return buf_23;
		case 24:  return buf_24;
		case 25:  return buf_25;
		case 26:  return buf_26;
		case 27:  return buf_27;
		case 28:  return buf_28;
		case 29:  return buf_29;
		case 30:  return buf_30;
	}
	return 0;
}

const char* line = "#-------------------------------------------------------------\n";

static void test_snprintf()
{
	printf("%s#  %s\n%s", line, __func__, line);

	typedef struct
	{
		// inc
		const char* fmt;
		int32_t     val;
		unsigned    bufsz;
		// out
		const char* true_str;
		int         true_res;
	} Test_32_t;

	typedef struct
	{
		// inc
		const char* fmt;
		int64_t     val;
		unsigned    bufsz;
		// out
		const char* true_str;
		int         true_res;
	} Test_64_t;

	typedef struct
	{
		// inc
		const char* fmt;
		const char* val;
		unsigned    bufsz;
		// out
		const char* true_str;
		int         true_res;
	} Test_str_t;

	typedef struct
	{
		// inc
		const char* fmt;
		double      val;
		unsigned    bufsz;
		// out
		const char* true_str;
		int         true_res;
	} Test_float_t;

	typedef struct
	{
		// inc
		const char* fmt;
		int32_t     val;
		unsigned    bufsz;
		unsigned    width;
		unsigned    precision;
		// out
		const char* true_str;
		int         true_res;
	} Test_32_asterisk_t;

	typedef struct
	{
		// inc
		const char* fmt;
		int32_t     val;
		unsigned    bufsz;
		// out
		const char* true_str;
		int         true_res;
		int         true_n;
	} Test_32_n_t;

	static Test_32_t tests_32 [] = 
	{
		{ "INT32 FORMATS" },
		{ "%d",     -123, 8, "-123",  4  },
		{ "%d",     -123, 3, "-1",    4  },
		{ "%u",      123, 8, "123",   3  },
		{ "%u",      123, 3, "12",    3  },
		{ "%o",   012345, 8, "12345", 5  },
		{ "%o",   012345, 4, "123",   5  },
		{ "%x",  0xabcde, 8, "abcde", 5  },
		{ "%x",  0xabcde, 4, "abc",   5  },

		{ "INT32 WIDTH" },
		{ "%6d",    -234, 8,  "  -234", 6  },
		{ "%6d",    -234, 4,  "  -",    6  },

		{ "INT32 PRECISION" },
		{ "%.6d",    -345, 8,  "-000345", 7  },
		{ "%.6d",    -345, 4,  "-00",     7  },
		{ "%.2d",    -345, 8,  "-345",    4  },

		{ "INT32 FLAGS" },
		{ "%-6d",    -456, 8, "-456  ", 6  },
		{ "%+6d",     456, 8, "  +456", 6  },
		{ "% d",      456, 8, " 456",   4  },
		// #
		{ "%#d",      456, 8, "456",    3  },
		{ "%#o",     0456, 8, "0456",   4  },
		{ "%#x",    0x456, 8, "0x456",  5  },
		// +#
		{ "%+#d",     456, 8, "+456",   4  },
		{ "%+#o",    0456, 8, "0456",   4  },
		{ "%+#x",   0x456, 8, "0x456",  5  },
		// +# for negatives
		{ "%+#d",    -456, 16, "-456",          4  },
		{ "%+#o",   -0456, 16, "037777777322", 12  },
		{ "%+#x",  -0x456, 16, "0xfffffbaa",   10  },
		// +# for negatives and small buf
		{ "%+#d",    -456, 8, "-456",           4  },
		{ "%+#o",   -0456, 8, "0377777",       12  },
		{ "%+#x",  -0x456, 8, "0xfffff",       10  },
		// null padding
		{ "%06d",     456, 8, "000456", 6  },

		{ "INT32 LENGTH" },
		{ "%d",         567, 16, "567",  3  },
		{ "%ld",        567, 16, "567",  3  },
		{ "%hhx",     0x567, 16, "67",   2  },
		{ "%hx",   0x123456, 16, "3456", 4  },
		{ "%zd",        567, 16, "567",  3  },
		{ "%td",        567, 16, "567",  3  },

		{ "CHAR" },
		{ "%c",    'A', 20, "A",     1 },
		{ "%0.4c", 'B', 20, "B",     1 },
		{ "%5c",   'C', 20, "    C", 5 },
		{ "%#5c",  'D', 4,  "   ",   5 },
		{ "%- 5c", 'E', 10, "E    ", 5 },
	};

	static Test_64_t tests_64 [] = 
	{
		{ "INT64 FORMATS" },
		{ "%lld",    -123, 8, "-123",  4  },
		{ "%lld",    -123, 3, "-1",    4  },
		{ "%Lu",      123, 8, "123",   3  },
		{ "%Lu",      123, 3, "12",    3  },
		{ "%llo",  012345, 8, "12345", 5  },
		{ "%llo",  012345, 4, "123",   5  },
		{ "%Lx",  0xabcde, 8, "abcde", 5  },
		{ "%Lx",  0xabcde, 4, "abc",   5  },

		{ "INT64 WIDTH" },
		{ "%6jd",    -234, 8,  "  -234", 6  },
		{ "%6jd",    -234, 4,  "  -",    6  },

		{ "INT64 PRECISION" },
		{ "%.6lld",   -345, 8,  "-000345", 7  },
		{ "%.6Ld",    -345, 4,  "-00",     7  },
		{ "%.2jd",    -345, 8,  "-345",    4  },

		{ "INT64 FLAGS" },
		{ "%-6Ld",    -456, 8, "-456  ", 6  },
		{ "%+6Ld",     456, 8, "  +456", 6  },
		{ "% lld",     456, 8, " 456",   4  },
		// #
		{ "%#jd",      456, 8, "456",    3  },
		{ "%#jo",     0456, 8, "0456",   4  },
		{ "%#jx",    0x456, 8, "0x456",  5  },
		// +#
		{ "%+#lld",    456, 8, "+456",   4  },
		{ "%+#Lo",    0456, 8, "0456",   4  },
		{ "%+#jx",   0x456, 8, "0x456",  5  },
		// +# for negatives
		{ "%+#Ld",    -456, 16, "-456",                     4  },
		{ "%+#Lo",   -0456, 25, "01777777777777777777322", 23  },
		{ "%+#llx", -0x456, 25, "0xfffffffffffffbaa",      18  },
		// +# for negatives and small buf
		{ "%+#jd",    -456, 8, "-456",                      4  },
		{ "%+#jo",   -0456, 16, "017777777777777",         23  },
		{ "%+#jx",  -0x456, 16, "0xfffffffffffff",         18  },
		// null padding
		{ "%06jd",     456, 8, "000456", 6  },
	};

	static Test_str_t tests_str [] = 
	{
		{ "STRING" },
		{ "%s",    "hello my friends", 20, "hello my friends", 16 },
		{ "%.4s",  "hello my friends", 20, "hell",              4 },
		{ "%5s",   "hello my friends", 20, "hello my friends", 16 },
		{ "%#5s",  "Hi",               10, "   Hi",             5 },
		{ "%- 5s", "Hi",               10, "Hi   ",             5 },
	};

	static Test_float_t tests_float [] = 
	{
		{ "FLOAT BASE" },
		{ "%f",    123.456,         20, "123.456000",             10 },
		{ "%F",    123.456,         20, "123.456000",             10 },
		{ "%e",    123.456,         20, "1.234560e+02",           12 },
		{ "%E",    123.456,         20, "1.234560E+02",           12 },
		{ "%f",    100.0,           20, "100.000000",             10 },
		{ "%F",    100.0,           20, "100.000000",             10 },
		{ "%g",    123.456,         20, "123.456",                 7 },
		{ "%G",    123.456,         20, "123.456",                 7 },
		{ "%g",    100.0,           20, "100",                     3 },
		{ "%G",    100.0,           20, "100",                     3 },
		{ "%g",    1000000,         20, "1e+06",                   5 },
		{ "%G",    1000000,         20, "1E+06",                   5 },
		//{ "%a",    123.456,         30, "0x1.edd2f1a9fbe77p+6",   20 }, // TODO
		//{ "%A",    123.456,         30, "0X1.EDD2F1A9FBE77P+6",   20 }, // TODO
		{ "%f",    -123.456,        20, "-123.456000",            11 },
		{ "%F",    -123.456,        20, "-123.456000",            11 },
		{ "%e",    -123.456,        20, "-1.234560e+02",          13 },
		{ "%E",    -123.456,        20, "-1.234560E+02",          13 },
		{ "%g",    -123.456,        20, "-123.456",                8 },
		{ "%G",    -123.456,        20, "-123.456",                8 },
		//{ "%a",    -123.456,        30, "-0x1.edd2f1a9fbe77p+6",  21 }, // TODO
		//{ "%A",    -123.456,        30, "-0X1.EDD2F1A9FBE77P+6",  21 }, // TODO
		{ "%f",    0.000056,        20, "0.000056",                8 },
		{ "%F",    0.000056,        20, "0.000056",                8 },
		{ "%e",    0.000056,        20, "5.600000e-05",           12 },
		{ "%E",    0.000056,        20, "5.600000E-05",           12 },
		{ "%f",    0.0056,          20, "0.005600",                8 },
		{ "%e",    0.0056,          20, "5.600000e-03",           12 },
		{ "%g",    0.0056,          20, "0.0056",                  6 },
		{ "%f",    0.000056,        20, "0.000056",                8 },
		{ "%e",    0.000056,        20, "5.600000e-05",           12 },
		{ "%g",    0.000056,        20, "5.6e-05",                 7 },
		{ "%G",    0.0056,          20, "0.0056",                  6 },
		{ "%G",    0.000056,        20, "5.6E-05",                 7 },
		//{ "%a",    0.000056,        30, "0x1.d5c31593e5fb7p-15",  21 }, // TODO
		//{ "%A",    0.000056,        30, "0X1.D5C31593E5FB7P-15",  21 }, // TODO
		//{ "%f",    1.0e+100,        30, "10000000000000000159028911097",  108 }, // OVERFLOW for wlibc
		//{ "%f",    1.0e+39,         30, "99999999999999993970916637160",  46 }, // bad round for wlibc
		{ "%f",    1.0e-100,        30, "0.000000",                8 },
		{ "%e",    1.0e+100,        30, "1.000000e+100",          13 },
		{ "%e",    1.0e-100,        30, "1.000000e-100",          13 },

		{ "FLOAT NUL/INF/NAN" },
		{ "%f",   0.0,              20, "0.000000",                8 },
		{ "%g",   0.0,              20, "0",                       1 },
		{ "%f",   1.0 / 0.0,        20, "inf",                     3 },
		{ "%f",   0.0 / 0.0,        20, "nan",                     3 },
		{ "%f",  -1.1 / 0.0,        20, "-inf",                    4 },
		//{ "%f",  -0.0 / 0.0,        20, "-nan",                    4 }, // no -NaN on Sparc
		{ "%5f",  1.0 / 0.0,        20, "  inf",                   5 },
		{ "%5f",  0.0 / 0.0,        20, "  nan",                   5 },
		{ "%5f", -1.1 / 0.0,        20, " -inf",                   5 },
		//{ "%5f", -0.0 / 0.0,        20, " -nan",                   5 }, // no -NaN on Sparc
		{ "%5e",  1.0 / 0.0,        20, "  inf",                   5 },
		{ "%5E",  0.0 / 0.0,        20, "  NAN",                   5 },
		{ "%5g", -1.1 / 0.0,        20, " -inf",                   5 },
		//{ "%5G", -0.0 / 0.0,        20, " -NAN",                   5 }, // no -Nan on Sparc
		{ "%5a", -1.1 / 0.0,        20, " -inf",                   5 },
		//{ "%5A", -0.0 / 0.0,        20, " -NAN",                   5 }, // no -Nan on Sparc

		{ "FLOAT DEC POINT" },
		//{ "%f",    0.0,             20, "0",                       1 }, // glibc out as for %#f
		{ "%#f",   0.0,             20, "0.000000",                8 },
		{ "%#f",   123456789.0,     20, "123456789.000000",       16 },
		{ "%#5f",  123456789.0,     20, "123456789.000000",       16 },
		{ "%#e",   0.0,             20, "0.000000e+00",           12 },
		{ "%#e",   123456789.0,     20, "1.234568e+08",           12 },
		{ "%#5e",  123456789.0,     20, "1.234568e+08",           12 },
		{ "%f",    0.0,             20, "0.000000",                8 },
		{ "%e",    0.0,             20, "0.000000e+00",           12 },
		{ "%g",    0.0,             20, "0",                       1 },
		{ "%#f",   0.0,             20, "0.000000",                8 },
		{ "%#e",   0.0,             20, "0.000000e+00",           12 },
		{ "%#g",   0.0,             20, "0.00000",                 7 },
		{ "%#g",   123456789.0,     20, "1.23457e+08",            11 },
		{ "%#5g",  123456789.0,     20, "1.23457e+08",            11 },
		{ "%f",    123456789.0,     20, "123456789.000000",       16 },
		{ "%#f",   123456789.0,     20, "123456789.000000",       16 },
		{ "%.f",   123456789.0,     20, "123456789",               9 },
		{ "%#.f",  123456789.0,     20, "123456789.",             10 },

		{ "FLOAT ALIGN" },
		{ "%#f",   0.0,             20, "0.000000",                8 },
		{ "%#f",   123456789.0,     10, "123456789",              16 },
		{ "%#5f",  123456789.0,     10, "123456789",              16 },
		{ "%20f",  123456789.0,     22, "    123456789.000000",   20 },
		{ "%-20f",  123456789.0,    22, "123456789.000000    ",   20 },
		{ "%- 5f", 0.123456789,     10, " 0.123457",               9 },
		{ "%#f",   0.0,             20, "0.000000",                8 },
		{ "%#f",   123456789.0,     10, "123456789",              16 },
		{ "%#5f",  123456789.0,     10, "123456789",              16 },
		{ "%- 5f", 0.123456789,     10, " 0.123457",               9 },

		{ "FLOAT WIDTH/PRECISION 1" },
		{ "%1.1f", 6442452944.1234, 20, "6442452944.1",           12 },
		{ "%1.1f", -1.5,            20, "-1.5",                    4 },
		{ "%1.1f", 134.21,          20, "134.2",                   5 },
		{ "%1.1f", 91340.2,         20, "91340.2",                 7 },
		{ "%1.1f", 341.1234,        20, "341.1",                   5 },
		{ "%1.1f", 0203.9,          20, "203.9",                   5 },
		{ "%1.1f", 0.96,            20, "1.0",                     3 },
		{ "%1.1f", 0.996,           20, "1.0",                     3 },
		{ "%1.1f", 0.9996,          20, "1.0",                     3 },
		{ "%1.1f", 1.996,           20, "2.0",                     3 },
		{ "%1.1f", 4.136,           20, "4.1",                     3 },
		{ "%1.1f", 0,               20, "0.0",                     3 },

		{ "FLOAT WIDTH/PRECISION 2" },
		{ "%-1.5f", 6442452944.1234, 20, "6442452944.12340",      16 },
		{ "%-1.5f", -1.5,            20, "-1.50000",               8 },
		{ "%-1.5f", 134.21,          20, "134.21000",              9 },
		{ "%-1.5f", 91340.2,         20, "91340.20000",           11 },
		{ "%-1.5f", 341.1234,        20, "341.12340",              9 },
		{ "%-1.5f", 0203.9,          20, "203.90000",              9 },
		{ "%-1.5f", 0.96,            20, "0.96000",                7 },
		{ "%-10.5f", 0.996,           20, "0.99600   ",           10 },
		{ "%-10.5f", 0.9996,          20, "0.99960   ",           10 },
		{ "%-10.5f", 1.996,           20, "1.99600   ",           10 },
		{ "%-10.5f", 4.136,           20, "4.13600   ",           10 },
		{ "%-10.5f", 0,               20, "0.00000   ",           10 },

		{ "FLOAT WIDTH/PRECISION 3" },
		{ "%1.5f", 6442452944.1234, 20, "6442452944.12340",       16 },
		{ "%1.5f", -1.5,            20, "-1.50000",                8 },
		{ "%1.5f", 134.21,          20, "134.21000",               9 },
		{ "%1.5f", 91340.2,         20, "91340.20000",            11 },
		{ "%1.5f", 341.1234,        20, "341.12340",               9 },
		{ "%1.5f", 0203.9,          20, "203.90000",               9 },
		{ "%1.5f", 0.96,            20, "0.96000",                 7 },
		{ "%10.5f", 0.996,           20, "   0.99600",            10 },
		{ "%10.5f", 0.9996,          20, "   0.99960",            10 },
		{ "%10.5f", 1.996,           20, "   1.99600",            10 },
		{ "%10.5f", 4.136,           20, "   4.13600",            10 },
		{ "%10.5f", 0,               20, "   0.00000",            10 },

		{ "FLOAT WIDTH/PRECISION 4" },
		//{ "%-16.16f", 6442452944.1234, 20, "6442452944.12339973", 27 } // bad round for wlibc,
		{ "%-16.16f", -1.5,            20, "-1.5000000000000000", 19 },
		//{ "%-16.16f", 134.21,          20, "134.210000000000008", 20 } // bad round for wlibc,
		//{ "%-16.16f", 91340.2,         20, "91340.1999999999970", 22 } // bad round for wlibc,
		//{ "%-16.16f", 341.1234,        20, "341.123400000000003", 20 } // bad round for wlibc,
		//{ "%-16.16f", 0203.9,          20, "203.900000000000005", 20 } // bad round for wlibc,
		{ "%-16.16f", 0.96,            20, "0.9600000000000000",  18 },
		//{ "%-16.16f", 0.996,           20, "0.9960000000000000",  18 } // bad round for wlibc,
		{ "%-16.16f", 0.9996,          20, "0.9996000000000000",  18 },
		//{ "%-16.16f", 1.996,           20, "1.9960000000000000",  18 }, // bad round for wlibc
		{ "%-16.16f", 4.136,           20, "4.1360000000000001",  18 },
		{ "%-16.16f", 0,               20, "0.0000000000000000",  18 },

		{ "FLOAT WIDTH/PRECISION 5" },
		{ "%10.5f", 6442452944.1234, 20, "6442452944.12340",      16 },
		{ "%10.5f", -1.5,            20, "  -1.50000",            10 },
		{ "%10.5f", 134.21,          20, " 134.21000",            10 },
		{ "%10.5f", 91340.2,         20, "91340.20000",           11 },
		{ "%10.5f", 341.1234,        20, " 341.12340",            10 },
		{ "%10.5f", 0203.9,          20, " 203.90000",            10 },
		{ "%10.5f", 0.96,            20, "   0.96000",            10 },
		{ "%10.5f", 0.996,           20, "   0.99600",            10 },
		{ "%10.5f", 0.9996,          20, "   0.99960",            10 },
		{ "%10.5f", 1.996,           20, "   1.99600",            10 },
		{ "%10.5f", 4.136,           20, "   4.13600",            10 },
		{ "%10.5f", 0,               20, "   0.00000",            10 },

		{ "FLOAT WIDTH/PRECISION 6" },
		{ "% 10.5f", 6442452944.1234, 20, " 6442452944.12340",    17 },
		{ "% 10.5f", -1.5,            20, "  -1.50000",           10 },
		{ "% 10.5f", 134.21,          20, " 134.21000",           10 },
		{ "% 10.5f", 91340.2,         20, " 91340.20000",         12 },
		{ "% 10.5f", 341.1234,        20, " 341.12340",           10 },
		{ "% 10.5f", 0203.9,          20, " 203.90000",           10 },
		{ "% 10.5f", 0.96,            20, "   0.96000",           10 },
		{ "% 10.5f", 0.996,           20, "   0.99600",           10 },
		{ "% 10.5f", 0.9996,          20, "   0.99960",           10 },
		{ "% 10.5f", 1.996,           20, "   1.99600",           10 },
		{ "% 10.5f", 4.136,           20, "   4.13600",           10 },
		{ "% 10.5f", 0,               20, "   0.00000",           10 },

		{ "FLOAT WIDTH/PRECISION 7" },
		//{ "%+22.9f", 6442452944.1234, 20, " +6442452944.123399",  22 }, // bad rounf for wlibc
		{ "%+22.9f", -1.5,            20, "          -1.500000",  22 },
		{ "%+22.9f", 134.21,          20, "        +134.210000",  22 },
		{ "%+22.9f", 91340.2,         20, "      +91340.200000",  22 },
		{ "%+22.9f", 341.1234,        20, "        +341.123400",  22 },
		{ "%+22.9f", 0203.9,          20, "        +203.900000",  22 },
		{ "%+22.9f", 0.96,            20, "          +0.960000",  22 },
		{ "%+22.9f", 0.996,           20, "          +0.996000",  22 },
		{ "%+22.9f", 0.9996,          20, "          +0.999600",  22 },
		{ "%+22.9f", 1.996,           20, "          +1.996000",  22 },
		{ "%+22.9f", 4.136,           20, "          +4.136000",  22 },
		{ "%+22.9f", 0,               20, "          +0.000000",  22 },

		{ "FLOAT WIDTH/PRECISION 8" },
		//{ "%+4.9f", 6442452944.1234, 20, "+6442452944.1233997",   21 }, // bad rounf for wlibc
		{ "%+4.9f", -1.5,            20, "-1.500000000",          12 },
		{ "%+4.9f", 134.21,          20, "+134.210000000",        14 },
		{ "%+4.9f", 91340.2,         20, "+91340.200000000",      16 },
		{ "%+4.9f", 341.1234,        20, "+341.123400000",        14 },
		{ "%+4.9f", 0203.9,          20, "+203.900000000",        14 },
		{ "%+4.9f", 0.96,            20, "+0.960000000",          12 },
		{ "%+4.9f", 0.996,           20, "+0.996000000",          12 },
		{ "%+4.9f", 0.9996,          20, "+0.999600000",          12 },
		{ "%+4.9f", 1.996,           20, "+1.996000000",          12 },
		{ "%+4.9f", 4.136,           20, "+4.136000000",          12 },
		{ "%+4.9f", 0,               20, "+0.000000000",          12 },

		{ "FLOAT WIDTH/PRECISION 9" },
		{ "%01.3f", 6442452944.1234, 20, "6442452944.123",        14 },
		{ "%01.3f", -1.5,            20, "-1.500",                 6 },
		{ "%01.3f", 134.21,          20, "134.210",                7 },
		{ "%01.3f", 91340.2,         20, "91340.200",              9 },
		{ "%01.3f", 341.1234,        20, "341.123",                7 },
		{ "%01.3f", 0203.9,          20, "203.900",                7 },
		{ "%01.3f", 0.96,            20, "0.960",                  5 },
		{ "%01.3f", 0.996,           20, "0.996",                  5 },
		{ "%01.3f", 0.9996,          20, "1.000",                  5 },
		{ "%01.3f", 1.996,           20, "1.996",                  5 },
		{ "%01.3f", 4.136,           20, "4.136",                  5 },
		{ "%01.3f", 0,               20, "0.000",                  5 },

		{ "FLOAT WIDTH/PRECISION 10" },
		{ "%4f", 6442452944.1234, 20, "6442452944.123400",        17 },
		{ "%4f", -1.5,            20, "-1.500000",                 9 },
		{ "%4f", 134.21,          20, "134.210000",               10 },
		{ "%4f", 91340.2,         20, "91340.200000",             12 },
		{ "%4f", 341.1234,        20, "341.123400",               10 },
		{ "%4f", 0203.9,          20, "203.900000",               10 },
		{ "%4f", 0.96,            20, "0.960000",                  8 },
		{ "%4f", 0.996,           20, "0.996000",                  8 },
		{ "%4f", 0.9996,          20, "0.999600",                  8 },
		{ "%4f", 1.996,           20, "1.996000",                  8 },
		{ "%4f", 4.136,           20, "4.136000",                  8 },
		{ "%4f", 0,               20, "0.000000",                  8 },

		{ "FLOAT WIDTH/PRECISION 11" },
		{ "%3.1f", 6442452944.1234, 20, "6442452944.1",           12 },
		{ "%3.1f", -1.5,            20, "-1.5",                    4 },
		{ "%3.1f", 134.21,          20, "134.2",                   5 },
		{ "%3.1f", 91340.2,         20, "91340.2",                 7 },
		{ "%3.1f", 341.1234,        20, "341.1",                   5 },
		{ "%3.1f", 0203.9,          20, "203.9",                   5 },
		{ "%3.1f", 0.96,            20, "1.0",                     3 },
		{ "%3.1f", 0.996,           20, "1.0",                     3 },
		{ "%3.1f", 0.9996,          20, "1.0",                     3 },
		{ "%3.1f", 1.996,           20, "2.0",                     3 },
		{ "%3.1f", 4.136,           20, "4.1",                     3 },
		{ "%3.1f", 0,               20, "0.0",                     3 },

		{ "FLOAT WIDTH/PRECISION 12" },
		//{ "%3.2g", 6442452944.1234, 20, "6.4e+09",                 7 }, // glibc ignore precision
		{ "%3.2g", -1.5,            20, "-1.5",                    4 },
		//{ "%3.2g", 134.21,          20, "1.3e+02",                 7 }, // glibc ignore precision
		//{ "%3.2G", 91340.2,         20, "9.1E+04",                 7 }, // glibc ignore precision
		//{ "%3.2G", 341.1234,        20, "3.4E+02",                 7 }, // glibc ignore precision
		//{ "%3.2G", 0203.9,          20, "2E+02",                   5 }, // glibc ignore precision
		{ "%3.2F", 0.96,            20, "0.96",                    4 },
		{ "%3.2F", 0.996,           20, "1.00",                    4 },
		{ "%3.2F", 0.9996,          20, "1.00",                    4 },
		//{ "%3.2g", 1.996,           20, "  2",                     3 }, // glibc ignore precision
		{ "%3.2f", 4.136,           20, "4.14",                    4 },
		//{ "%3.2a", 0,               20, "0x0.00p+0",               9 }, // TODO hex float

		{ "FLOAT WIDTH/PRECISION 13" },
		{ "%.0f", 6442452944.1234, 20, "6442452944",              10 },
		{ "%.0f", -1.5,            20, "-2",                       2 },
		{ "%.0f", 134.21,          20, "134",                      3 },
		{ "%.0f", 91340.2,         20, "91340",                    5 },
		{ "%.0f", 341.1234,        20, "341",                      3 },
		{ "%.0f", 0203.9,          20, "204",                      3 },
		{ "%.0f", 0.96,            20, "1",                        1 },
		{ "%.0f", 0.996,           20, "1",                        1 },
		{ "%.0f", 0.9996,          20, "1",                        1 },
		{ "%.0f", 1.996,           20, "2",                        1 },
		{ "%.0f", 4.136,           20, "4",                        1 },
		{ "%.0f", 0,               20, "0",                        1 },

		{ "FLOAT OVERFLOW" },
		{ "%123.9f", 6442452944.1234, 20, "                   ", 123 },
		{ "%123.9f", -1.5,            20, "                   ", 123 },
		{ "%123.9f", 134.21,          20, "                   ", 123 },
		{ "%123.9f", 91340.2,         20, "                   ", 123 },
		{ "%123.9f", 341.1234,        20, "                   ", 123 },
		{ "%123.9f", 0203.9,          20, "                   ", 123 },
		{ "%123.9f", 0.96,            20, "                   ", 123 },
		{ "%123.9f", 0.996,           20, "                   ", 123 },
		{ "%123.9f", 0.9996,          20, "                   ", 123 },
		{ "%123.9f", 1.996,           20, "                   ", 123 },
		{ "%123.9f", 4.136,           20, "                   ", 123 },
		{ "%123.9f", 0,               20, "                   ", 123 },
	};

	static Test_32_asterisk_t tests_32_ast [] =
	{
		{ "INT32 WIDTH and PRECISION" },
		{ "%*.*d",     -123, 16,  8,  5, "  -00123",    8 },
		{ "%+*.*d",     123, 16,  5,  8, "+00000123",   9 },
		{ "%-*.*d",     123, 16,  2, 10, "0000000123", 10 },
		{ "%#+*.*x",  0x123, 16, 10,  2, "     0x123", 10 },
		{ "%#-+*.*x", 0x123, 16, 10,  2, "0x123     ", 10 },
		{ "%#-*.*x",  0x123, 16,  0,  0, "0x123",       5 },
		{ "%*.*d",     -123, 10,  3,  3, "-123",        4 },
	};

	static Test_32_n_t tests_32_n [] =
	{
		{ "INT32 POSITION" },
		{ "%.d%n",              -123, 26, "-123", 4, 4},
		{ "%.d qw %n qwerty",   -123, 26, "-123 qw  qwerty", 15, 8 },
		{ "%.d qw %n qwerty",   -123, 4, "-12", 15, 8 },
	};

	enum
	{
		Int32_tests_num     = sizeof(tests_32)     / sizeof(tests_32[0]),
		Int64_tests_num     = sizeof(tests_64)     / sizeof(tests_64[0]),
		Str_tests_num       = sizeof(tests_str)    / sizeof(tests_str[0]),
		Float_tests_num     = sizeof(tests_float)  / sizeof(tests_float[0]),
		Int32_ast_tests_num = sizeof(tests_32_ast) / sizeof(tests_32_ast[0]),
		Int32_n_tests_num   = sizeof(tests_32_n)   / sizeof(tests_32_n[0]),
	};

	unsigned errors = 0;
	unsigned tests = 0;

	for (unsigned i=0; i<Int32_tests_num; ++i)
	{
		Test_32_t* t = &tests_32[i];
		if (t->fmt[0] != '%')
		{
			puts("");
			puts(t->fmt);
			continue;
		}
		char* buf = get_buf(t->bufsz);
		int res = snprintf(buf, t->bufsz, t->fmt, t->val);
		int e = strcmp(buf, t->true_str)  ||  res != t->true_res;
		const char* err = e ? "ERR" : "OK ";
		errors += e ? 1 : 0;
		printf("  test #%3d:  %s  res='%s'/%2d,  expected='%s'/%2d.\n",
			i, err, buf, res, t->true_str, t->true_res);
	}
	tests += Int32_tests_num;

	for (unsigned i=0; i<Int64_tests_num; ++i)
	{
		Test_64_t* t = &tests_64[i];
		if (t->fmt[0] != '%')
		{
			puts("");
			puts(t->fmt);
			continue;
		}
		char* buf = get_buf(t->bufsz);
		int res = snprintf(buf, t->bufsz, t->fmt, t->val);
		int e = strcmp(buf, t->true_str)  ||  res != t->true_res;
		const char* err = e ? "ERR" : "OK ";
		errors += e ? 1 : 0;
		printf("  test #%3d:  %s  buf='%s'/%d,  expected='%s'/%d.\n",
			tests + i, err, buf, res, t->true_str, t->true_res);
	}
	tests += Int64_tests_num;

	for (unsigned i=0; i<Str_tests_num; ++i)
	{
		Test_str_t* t = &tests_str[i];
		if (t->fmt[0] != '%')
		{
			puts("");
			puts(t->fmt);
			continue;
		}
		char* buf = get_buf(t->bufsz);
		int res = snprintf(buf, t->bufsz, t->fmt, t->val);
		int e = strcmp(buf, t->true_str)  ||  res != t->true_res;
		const char* err = e ? "ERR" : "OK ";
		errors += e ? 1 : 0;
		printf("  test #%3d:  %s  buf='%s'/%d,  expected='%s'/%d.\n",
			tests + i, err, buf, res, t->true_str, t->true_res);
	}
	tests += Str_tests_num;

	for (unsigned i=0; i<Float_tests_num; ++i)
	{
		Test_float_t* t = &tests_float[i];
		if (t->fmt[0] != '%')
		{
			puts("");
			puts(t->fmt);
			continue;
		}
		char* buf = get_buf(t->bufsz);
		int res = snprintf(buf, t->bufsz, t->fmt, t->val);
		int e = strcmp(buf, t->true_str)  ||  res != t->true_res;
		const char* err = e ? "ERR" : "OK ";
		errors += e ? 1 : 0;
		printf("  test #%3d:  %s  buf='%s'/%d,  expected='%s'/%d.\n",
			tests + i, err, buf, res, t->true_str, t->true_res);
if (e) return;
	}
	tests += Float_tests_num;

	for (unsigned i=0; i<Int32_ast_tests_num; ++i)
	{
		Test_32_asterisk_t* t = &tests_32_ast[i];
		if (t->fmt[0] != '%')
		{
			puts("");
			puts(t->fmt);
			continue;
		}
		char* buf = get_buf(t->bufsz);
		int res = snprintf(buf, t->bufsz, t->fmt, t->width, t->precision, t->val);
		int e = strcmp(buf, t->true_str)  ||  res != t->true_res;
		const char* err = e ? "ERR" : "OK ";
		errors += e ? 1 : 0;
		printf("  test #%3d:  %s  buf='%s'/%d,  expected='%s'/%d.\n",
			tests + i, err, buf, res, t->true_str, t->true_res);
	}
	tests += Int32_ast_tests_num;

	for (unsigned i=0; i<Int32_n_tests_num; ++i)
	{
		Test_32_n_t* t = &tests_32_n[i];
		if (t->fmt[0] != '%')
		{
			puts("");
			puts(t->fmt);
			continue;
		}
		char* buf = get_buf(t->bufsz);
		int n = 0;
		int res = snprintf(buf, t->bufsz, t->fmt, t->val, &n);
		int e = strcmp(buf, t->true_str)  ||  res != t->true_res  ||  n != t->true_n;
		const char* err = e ? "ERR" : "OK ";
		errors += e ? 1 : 0;
		printf("  test #%3d:  %s  buf='%s'/%d/%d,  expected='%s'/%d/%d.\n",
			tests + i, err, buf, res, n, t->true_str, t->true_res, t->true_n);
	}

	printf("\nErrors:  %d.\n\n", errors);
}

void wlibc_test_snprintf()
{
	test_snprintf();
}

//--------------------------------------------------------------------------------------------------
//  SSCANF
//--------------------------------------------------------------------------------------------------

static void test_sscanf()
{
	printf("%s#  %s\n%s", line, __func__, line);

	typedef struct
	{
		const char* str;    // string to scaning,         "123"
		const char* fmt;    // format to scanning,        "%d"
		int true_scanned;   // true scanned params count, 1
		int true_vals[3];   // true scanned values,       123
		int print_test;
	} Test_32;

	typedef struct
	{
		const char* str;
		const char* fmt;
		int true_scanned;
		char true_vals[3];
		int print_test;
	} Test_char;

	static Test_32 tests_32 [] = 
	{
		{ "#INT32 FORMATS" },
		{ "123",               "%d",           1, { 123           }, 1 },
		{ "123,456, 789",      "%d,%d, %d",    3, { 123, 456, 789 }, 1 },
		{ "XX-111, 222",       "XX%d, %d",     2, { -111, 222     }, 1 },
		{ "123 0x444",         "%d 0x%x",      2, { 123, 0x444    }, 1 },
		{ "0333-456",          "0%o-%u",       2, { 0333, 456     }, 1 },
		{ "..-0x111, 222",     "..-0x%x, %d",  2, { 0x111, 222    }, 1 },

		{ "#INT32 WIDTH" },
		{ "123",               "%3d",          1, { 123           }, 1 },
		{ "123",               "%2d",          1, { 12            }, 0 }, // will lose '3'
		{ "123",               "%2d%d",        2, { 12, 3         }, 1 },
		{ "123456009",         "%3d,%3d, %3d", 1, { 123           }, 0 }, // will scan only '123'
		{ "123456009",         "%3d%3d%3d",    3, { 123, 456, 9   }, 0 }, // will lose '00'

		{ "#INT32 OTHERS" },
		{ "DL00C,DLINK,0,@12", "DL%03X",       1, { 0xc           }, 0 }, // will lose tail
		{ "DL02C,DLINK,1,{C,T,0000,999,0000,0000,100.00},@78", "DL%03X", 1, { 0x2c }, 0 }, // will lose tail
	};

	static Test_char tests_char [] = 
	{
		{ "#CHARS" },
		{ "abcd",           "%cb%c%c",       3, { 'a', 'c', 'd' }, 1 },
	};

	enum
	{
		Int32_tests_num    = sizeof(tests_32)     / sizeof(tests_32[0]),
		Char_tests_num     = sizeof(tests_char)   / sizeof(tests_char[0]),
	};

	unsigned errors = 0;
	unsigned tests = 0;

	for (unsigned i=0; i<Int32_tests_num; ++i)
	{
		Test_32* t = &tests_32[i];

		if (t->str[0] == '#')
		{
			puts("");
			puts(t->str + 1);
			continue;
		}

		char buf[64];
		int vals[3] = {0, 0, 0};
		int scanned = sscanf(t->str, t->fmt, &vals[0], &vals[1], &vals[2]);
		snprintf(buf, sizeof(buf), t->fmt, vals[0], vals[1], vals[2]);
		int e = scanned != t->true_scanned;
		if (!e && t->print_test)
			e = strcmp(buf, t->str);
		if (!e)
			for (unsigned p=0; p<(unsigned)scanned; ++p)
				if (vals[p] != t->true_vals[p])
					e = 1;
		errors += e ? 1 : 0;
		const char* err = e ? "ERR" : "OK ";
		printf("  test #%3u:  %s:  scanned=%d(true=%d), val=%d/%d/%d(true=%d/%d/%d);  buf='%s', expected='%s'.\n",
			tests + i, err, scanned, t->true_scanned, vals[0], vals[1], vals[2],
			t->true_vals[0], t->true_vals[1], t->true_vals[2], buf, t->str);
	}

	for (unsigned i=0; i<Char_tests_num; ++i)
	{
		Test_char* t = &tests_char[i];

		if (t->str[0] == '#')
		{
			puts("");
			puts(t->str + 1);
			continue;
		}

		char buf[64];
		char vals[3] = {0, 0, 0};
		int scanned = sscanf(t->str, t->fmt, &vals[0], &vals[1], &vals[2]);
		snprintf(buf, sizeof(buf), t->fmt, vals[0], vals[1], vals[2]);
		int e = scanned != t->true_scanned;
		if (!e)
			e = strcmp(buf, t->str);
		if (!e)
			for (unsigned p=0; p<(unsigned)scanned; ++p)
				if (vals[p] != t->true_vals[p])
					e = 1;
		errors += e ? 1 : 0;
		const char* err = e ? "ERR" : "OK ";
		printf("  test #%3u:  %s:  scanned=%d(true=%d), val=%c/%c/%c(true=%c/%c/%c);  buf='%s', expected='%s'.\n",
			tests + i, err, scanned, t->true_scanned, vals[0], vals[1], vals[2],
			t->true_vals[0], t->true_vals[1], t->true_vals[2], buf, t->str);
	}

	// NOTE:  need FPU
	// complex test
	{
		puts("\nCOMPLEX TEST:");
		const char* str = "{C,T,0000,999,0000,0000,100.00},@78";
		const char* fmt = "{%c,%c,%d,%d,%d,%d,%f}";
		char c[2];
		int i[4];
		float f[0];
		int scanned = sscanf(str, fmt, c+0, c+1, i+0, i+1, i+2, i+3, f+0);
		printf("  str=%s\n", str);
		printf("  fmt=%s\n", fmt);
		printf("  res:  scanned=%d:  c=%c, c=%c, d=%d, d=%d, d=%d, d=%d, (int)f=%d.\n",
			scanned, c[0], c[1], i[0], i[1], i[2], i[3], (int)f[0]);
	}

	printf("\nErrors:  %d.\n\n", errors);
}

void wlibc_test_sscanf()
{
	test_sscanf();
}
