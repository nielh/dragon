
#include "kernel.h"
#include "object.h"
#include "printk.h"

void* HANDLE_ROOT = NULL;
void* HANDLE_OBJ = NULL;

void* open(void* obj, char* path, s64 mode, s64 flag)
{
	assert(obj != NULL);

	ops_t* ops = *(ops_t**)obj;
	assert(ops != NULL);

	if(ops->open == 0)
		return NULL;

	return ops->open(obj, path, mode, flag);
}

s64 read(void* obj, void* buf, s64 len, s64 pos)
{
	assert(obj != NULL);

	ops_t* ops = *(ops_t**)obj;
	assert(ops != NULL);

	if(ops->read == 0)
		return -1;

	return ops->read(obj, buf, len, pos);
}

s64 write(void* obj, void* buf, s64 len, s64 pos)
{
	assert(obj != NULL);

	ops_t* ops = *(ops_t**)obj;
	assert(ops != NULL);

	if(ops->write == 0)
		return -1;

	return ops->write(obj, buf, len, pos);
}

s64 close(void* obj)
{
	assert(obj != NULL);

	ops_t* ops = *(ops_t**)obj;
	assert(ops != NULL);

	if(ops->close == 0)
		return -1;

	return ops->close(obj);
}

s64 append(void* obj, char* name, void* child)
{
	assert(obj != NULL);

	ops_t* ops = *(ops_t**)obj;
	assert(ops != NULL);

	if(ops->append == 0)
		return -1;

	return ops->append(obj, name, child);
}

s64 delete(void* obj, char* name)
{
	assert(obj != NULL);

	ops_t* ops = *(ops_t**)obj;
	assert(ops != NULL);

	if(ops->delete == 0)
		return -1;

	return ops->delete(obj, name);
}

s64 ioctl(void *obj, s64  cmd, void* buf, s64 len)
{
	assert(obj != NULL);

	ops_t* ops = *(ops_t**)obj;
	assert(ops != NULL);

	if(ops->ioctl == 0)
		return -1;

	return ops->ioctl(obj, cmd, buf, len);
}

