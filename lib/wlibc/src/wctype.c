//##################################################################################################
//
//  Implementation of some funcs for wctype.h.
//
//##################################################################################################

#include <wctype.h>
#include <string.h>
#include <stdio.h>
#include "wlibc_panic.h"

wctype_t wctype(const char* property)
{
	enum
	{
		Type_unclassified,
		Type_alnum,
		Type_alpha,
		Type_blank,
		Type_cntrl,
		Type_digit,
		Type_graph,
		Type_lower,
		Type_print,
		Type_punct,
		Type_space,
		Type_upper,
		Type_xdigit
	};
	static const char* name[] = 
	{
		"alnum"
		"alpha"
		"blank"
		"cntrl"
		"digit"
		"graph"
		"lower"
		"print"
		"punct"
		"space"
		"upper"
		"xdigit"
	};

	//printf("%s:  prop=%s.\n", __func__, property);
	int i;
	for (i=0; i<sizeof(name)/sizeof(name[0]); ++i)
	{
		if (!strcmp(property, name[i]))
			return i + Type_alnum;
	}
	return Type_unclassified;
}

int iswctype(wint_t c, wctype_t desc)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wint_t towlower(wint_t c)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}

wint_t towupper(wint_t c)
{
	panic("%s:  implement me!\n", __func__);
	return 0;
}
