

#ifndef GDBSTUB_H
#define GDBSTUB_H

#include "kernel.h"

void set_debug_traps();
void breakpoint (void);
#define BREAKPOINT() ASM("int $3;\r\n nop;");

#endif
