

#include "libc.h"



/*
static char cmdbuf[256] = {0};
static int index = 0;

static int shell_help()
{
	printk("\t------ ------------------------\n");
	printk("\thelp   print this message\n");
	printk("\tps     print thread list\n");

	return 0;
}

static int shell_ps()
{
//	Thread* t;

//	for(t = thread_list; t != 0; t = t->next) {
//		printk("%s %d %d %d\n", t->name, t->status, t->ticks, t->quota);
//	}

	return 0;
}

static int shell_execute(char *cmd)
{
	printk("\n");
	if(strcmp(cmd, "help") == 0)
		return shell_help();
	else if(strcmp(cmd, "ps") == 0)
		return shell_ps();
	else {
		printk("unsupported command\n");
		return -1;
	}

	return 0;
}

int main()
{
	void* kbd =  __("Object", SEARCH, "kbd", 0, 0, 0);
	assert(kbd != 0);

	while(1) {
		printk(">>");
		index = 0;

		while(1) {
			char ch;

			_(kbd, READ, &ch, 0, 0, 0);

			cmdbuf[index] = ch;
			if(cmdbuf[index] == '\n') {
				cmdbuf[index] = 0;
				break;
			}

			printk("%c", cmdbuf[index]);
			index ++;
		}

		//printk("cmdbuf=[%s] index=%d\n", cmdbuf, index);
		shell_execute(cmdbuf);
	}

	return 0;
}
*/

int _main(int argc, char* argv[])
{
	printk("hello shell.exe");
	while(1){}
}
