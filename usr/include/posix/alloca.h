#ifndef ALLOCA_H
#define ALLOCA_H

#include <sys/types.h>

#ifdef  __cplusplus
extern "C" {
#endif

void  *_alloca(size_t size);
#define alloca _alloca

#ifdef  __cplusplus
}
#endif

#endif
