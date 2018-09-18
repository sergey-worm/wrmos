//##################################################################################################
//
//  Implementation of some funcs for locale.h.
//
//##################################################################################################

#include <locale.h>
#include <string.h>
#include <stdio.h>
#include "wlibc_panic.h"

char* setlocale(int category, register const char* locale)
{
	//printf("%s:  category=%d, locale=%s.\n", __func__, category, locale ? locale : "<null>");
	char* res = ((((unsigned int)(category)) <= LC_ALL)
				&& ((!locale)  // request for locale category string
				|| (!*locale)  // implementation-defined default is C
				|| ((*locale == 'C') && !locale[1])
				|| (!strcmp(locale, "POSIX"))))
		? (char*)"C"           // always in C/POSIX locale
		: 0;
	//printf("%s:  res=%p/%s.\n", __func__, res, res ? res : "<null>");
	return res;
}
