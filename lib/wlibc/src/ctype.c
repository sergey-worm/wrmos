//##################################################################################################
//
//  Implementation of some funcs for ctype.h.
//
//##################################################################################################

#include <ctype.h>

#undef isspace
#undef isdigit
#undef toupper

int isspace(int c)
{
	return c==' ' || c=='\t' || c=='\n' || c=='\v' || c=='\f' || c=='\r';
}

int isdigit(int c)
{
	return c >= '0'  &&  c <= '9';
}

int toupper(int c)
{
	if (c >= 'a'  &&  c <= 'z')
		return c + 'A' - 'a';
	else
		return c;
}
