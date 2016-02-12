
#ifndef VSNPRINTF_H
#define VSNPRINTF_H

#include "kernel.h"
#include <stdarg.h>

/*
typedef char * va_list;
#define _INTSIZEOF(n)    ((sizeof(n) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))
#define va_start(ap, v)  (ap = (va_list) &v + _INTSIZEOF(v))
#define va_arg(ap, t)    (*(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)))
#define va_end(ap)       (ap = (va_list) 0)

typedef __builtin_va_list	 __gnuc_va_list;
typedef __gnuc_va_list 		va_list

#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
*/

int snprintf(char *buf, int buf_size, const char *fmt, ...);
int vsnprintf(char *buf, int buf_size, const char *fmt, va_list args);

#endif //
