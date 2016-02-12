#include "libc.h"

/*
s64 MyWndProc(s64 wnd, s64 msgid, s64 arg0, s64 arg1)
{
	switch (msgid)
	{
	case MSG_PAINT:
		break;

	default:
		return DefWndProc( wnd, msgid, arg0, arg1);
		break;
	}

	return 0;
}

void InitApp()
{
	CreateWindow(100, 100, 200, 100, 0, MyWndProc);
	CreateWindow(300, 300, 200, 200, 0, MyWndProc);
}
*/
int test_run = 1;

int test_thread(void* a)
{
	while(test_run)
	{
		writep(stdout, "test\n", 5, 0);
		MSleep(2000);
	}

	exit(0);
}

char buffer[256];
int exe_cmd()
{
	if(strcmp(buffer, "help") == 0)
	{
		char str[] = "help string\n";
        writep(stdout, str, sizeof(str), 0);
	}
	else if(strcmp(buffer, "exit") == 0)
	{
		char str[] = "exit ...\n";
        writep(stdout, str, sizeof(str), 0);
        exit(0);
	}
	else if(strcmp(buffer, "arg") == 0)
	{
        writep(stdout, USER_ARG, strlen(USER_ARG), 0);
	}
	else if(strcmp(buffer, "testrun") == 0)
	{
        ThreadCreate(test_thread, 0);
	}
	else if(strcmp(buffer, "teststop") == 0)
	{
        test_run = 0;
	}
	else if(strcmp(buffer, "loadsys") == 0)
	{
        LoadSysModule("sys/test.sys");
	}
	else if(strcmp(buffer, "process") == 0)
	{
        ProcessCreate("sys/init.exe");
	}
	else if(strcmp(buffer, "env") == 0)
	{
        writep(stdout, USER_ENV, strlen(USER_ENV), 0);
	}
	else
    {
		char str[] = "unsupport\n";
        writep(stdout, str, sizeof(str), 0);
    }

    return 0;
}

s64 _main(s64 argc, char* argv[])
{
	int idx = 0;
	writep(stdout, ">", 1, 0);

	while(1)
    {
    	int bytes = readp(stdin, &buffer[idx], sizeof(buffer) -idx, 0);
    	writep(stdout, &buffer[idx], bytes, 0);
    	idx += bytes;

		if(buffer[idx -1] == '\n')
		{
			buffer[idx -1] = 0;
			idx = 0;

			exe_cmd();
			writep(stdout, ">", 1, 0);
		}
    }

    // for(int i = 0; i < 200000000; i ++);
	//printf("hello world!\n");

	//__asm__ __volatile__ ("syscall");
	//return 1;
}
/*
s64 _main(s64 argc, char* argv[])
{
	s64 i = 0;

	s64 fd = open("/sys/hello.txt", 0);

	char buf[64] = {0};

	read(fd, buf, sizeof(buf), 0);

	printk(buf);

	close(fd);

	while (1)
	{
		for (i = 0; i < 40000000; i++)
		{
		}

		printk(".");
	}

//	InitApp();
//
//	msg_t msg;
//
//	while (1)
//	{
//		if (!GetMessage(&msg))
//			break;
//
//		DispatchMsg(&msg);
//	}
//
//	return 0;
}
*/
