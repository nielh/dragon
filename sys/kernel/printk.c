
#include "vga.h"
#include "libc.h"
#include "vsnprintf.h"
#include "thread.h"

int indent_num = 0;

void com2_print(char* str);

EXPORT int printk(char* fmt, ...)
{
    PUSIF();

	va_list args;
	char buf[128];

	va_start(args, fmt);
	int ret = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	//vga_print(buf);
	com2_print(buf);


	POPIF();

	return ret;
}

int printk_indent(int indent, char* fmt, ...)
{
    PUSIF();

	va_list args;
	char buf[256];
	int tid = thread_current ? thread_current->id : 0;
    snprintf(buf, sizeof(buf), "[%3d]", tid);

	int len = indent << 2;

	for(int i=0; i < len; i++)
        buf[5 +i] = ' ';

	va_start(args, fmt);
	vsnprintf(&buf[5 +len], sizeof(buf) -len, fmt, args);
	va_end(args);

	//vga_print(buf);
	com2_print(buf);

	POPIF();

	return 0;
}


