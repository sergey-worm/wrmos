//##################################################################################################
//
//  hello - simple app example.
//
//##################################################################################################

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "l4_api.h"
#include "wrmos.h"
#include "console.h"

// uClibc/stdio.h contents putchar() and getchar() as macro, avoid it
#undef putchar
#undef getchar

int main(int argc, const char* argv[])
{
	wrm_logi("hello.\n");
	wrm_logi("argc=0x%x, argv=0x%p.\n", argc, argv);

	for (int i=0; i<argc; i++)
	{
		wrm_logi("arg[%d] = %s.\n", i, argv[i]);
	}

	// attach to system console
	int rc = w4console_init();
	if (!rc)
		rc = w4console_open();
	if (rc)
		wrm_logw("Failed to attach to system console, rc=%d. Use kdb output, no input.\n", rc);
	else
		wrm_logi("App attached to system console.\n");
	w4console_set_wlibc_cb();

	// some output
	for (int i=0; i<5; ++i)
	{
		wrm_logi("%s:  I'm alive.\n", argv[0]);
		sleep(1);
	}

	// some input
	wrm_logi("Input a chars or Enter to exit:\n");
	while (1)
	{
		int ch = getchar();
		if (ch == '\r')
		{
			putchar('\n');
			break;
		}
		putchar(ch);
	}

	// terminate
	rc = w4console_close();
	if (rc)
		wrm_loge("w4console_close() - rc=%d.\n", rc);

	printf("%s:  bye-bye.\n", argv[0]);

	while (1)
	{
		usleep(10*1000*1000);
	}

	return 0;
}
