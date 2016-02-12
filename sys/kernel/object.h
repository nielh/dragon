#ifndef OBJECT_H
#define OBJECT_H

#include "kernel.h"

enum CMD
{
    IOCTL_GETSTAT, IOCTL_LOCKPAGE
};


typedef struct ops_t
{
    void* (*open)(void* obj, char * path, s64 flag, s64 mode);
    s64 (*close )(void* obj);
    s64 (*read  )(void* obj, void* buf, s64 len, s64 pos);
    s64 (*write )(void* obj, void* buf, s64 len, s64 pos);
    s64 (*ioctl )(void *obj, s64  cmd, void* buf, s64 len);
    s64 (*append)(void* obj, char* name, void* child);
    s64 (*delete)(void* obj, char* name);

} ops_t;

void* open(void* obj, char* path, s64 flag, s64 mode);
s64 read(void* obj, void* buf, s64 len, s64 pos);
s64 write(void* obj, void* buf, s64 len, s64 pos);
s64 close(void* obj);
s64 ioctl(void *obj, s64 cmd, void* buf, s64 len);
s64 append(void* obj, char* name, void* child);
s64 delete(void* obj, char* name);
s64 getsize(void* obj);

extern void* HANDLE_ROOT;
extern void* HANDLE_OBJ;

#endif //
