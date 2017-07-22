//##################################################################################################
//
//  hello - simple app example.
//
//##################################################################################################

#include "l4api.h"
#include "wrmos.h"
#include <stdio.h>
#include <unistd.h>


class Class_t
{
	static unsigned cnt;
	unsigned id;

public:
	Class_t() : id(cnt++) { wrm_logw("Class_t::ctor() - id=%u, hello!\n", id); }
	~Class_t() { wrm_logw("Class_t::dtor() - id=%u, bye-bye!\n", id); }
};

unsigned Class_t::cnt = 0;

Class_t myclass1;
Class_t myclass2;
Class_t myclass3;

int main(int argc, const char* argv[])
{
	wrm_logi("hello.\n");
	wrm_logi("argc=0x%x, argv=0x%x.\n", argc, argv);

	for (int i=0; i<argc; i++)
	{
		wrm_logi("arg[%d] = %s.\n", i, argv[i]);
	}

	const char* app_name = argv[0];

	//l4_kdb("xxx");

	for (int i=0; i<10; ++i)
	{
		wrm_logi("I'am %s. I'm alive.\n", app_name);
		sleep(1);
	}
	return 0;
}
