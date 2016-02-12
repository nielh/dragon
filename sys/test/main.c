
#include "../kernel/printk.h"

int module_init()
{
	printk("hello from test.sys\n");
	return 0;
}

int module_exit()
{
	return 0;
}
