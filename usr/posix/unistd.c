
#include <os.h>
#include <unistd.h>

handle_t open(const char *name, int oflags, int mode)
{
    return Open(name, oflags, mode);
}

int close(handle_t h)
{
    return Close(h);
}

static long long position = 0;

int read(handle_t f, void *data, size_t size)
{
    s64 len = Read(f, data, size, position);
    position += len;
    return len;
}

int write(handle_t f, const void *data, size_t size)
{
    s64 len = Write(f, data, size, position);
    position += len;
    return len;
}

void exit(int status)
{
    Exit(status);
}

