
#ifndef KMALLOC_H
#define KMALLOC_H

#include "kernel.h"

void  kmalloc_init();
void* kmalloc();
void  kmfree(void* ptr);

#endif
