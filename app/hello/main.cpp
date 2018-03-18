//##################################################################################################
//
//  hello - simple app example.
//
//##################################################################################################

#include <stdio.h>
#include <unistd.h>


int main(int argc, const char* argv[])
{
	printf("hello.\n");
	printf("argc=0x%x, argv=0x%p.\n", argc, argv);

	for (int i=0; i<argc; i++)
	{
		printf("arg[%d] = %s.\n", i, argv[i]);
	}

	const char* app_name = argv[0];
	for (int i=0; i<5; ++i)
	{
		printf("%s:  I'm alive.\n", app_name);
		sleep(1);
	}

	printf("%s:  bye-bye.\n", app_name);
	return 0;
}
