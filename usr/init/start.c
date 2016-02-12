
#include "os.h"

extern long _main(long argc, char* argv[]);

void __attribute__((section(".init"))) _start()
{

	//int a = getpid();

	long argc = 0;
	char* argv[1] = {0};
	//char* env[1] = {0};

    int stdin = open("/obj/console", NULL, NULL);

	_main(argc, argv);

    int count = 0;
	for (;;)
	{
	    count ++;
	}

	//__asm("int $3");
}
