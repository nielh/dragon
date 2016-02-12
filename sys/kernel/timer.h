
#ifndef TIMER_H
#define TIMER_H

#include "kernel.h"



extern volatile u64 clock_ticks;
int msleep(int ms);

void timer_init();

#endif
