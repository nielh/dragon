#ifndef PRINTK_H
#define PRINTK_H

#include "kernel.h"
#include "gdbstub.h"
#include "spinlock.h"

#define PRINT_COLOR 0x0F

#define PANIC_COLOR 0x04
#define ERROR_COLOR 0x0E
#define TRACE_COLOR 0x02
#define DEBUG_COLOR 0x08

int printk(const char *fmt, ...);

#define panic(format, args...) 			\
	do{\
		ASM("cli");\
		printk("PANIC! %s:%d\n"format, __FILE__, __LINE__, ##args);\
		BREAKPOINT();\
		for(;;){ASM("pause");}\
    }while(0)

//for(;;){ASM("pause");}BREAKPOINT();
#define assert(exp) 			do{if(!(exp)) panic("assert failed '%s'\n", #exp);}while(0)

extern int indent_num;
void printk_indent(int indent, char* fmt, ...);

#ifdef DEBUG
#define DBGTRAP()				BREAKPOINT()
#define ENTER()                 printk_indent(indent_num++, "%s() Enter\n", __func__)
#define LOG(format, args...) 	printk_indent(indent_num, format, ##args)
#define LEAVE()                 printk_indent(--indent_num, "%s() Leave\n", __func__)
#else
#define DBGTRAP()
#define ENTER()
#define LOG(format, args...)
#define LEAVE()
#endif //

#endif //
