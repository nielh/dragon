#include "kernel.h"
#include "object.h"
#include "printk.h"
#include "dir.h"
#include "libc.h"
#include "kmalloc.h"

typedef struct dir_item_t
{
	struct dir_item_t* next;
	char* name;
	void* obj;

} dir_item_t;

typedef struct dir_t
{
	ops_t* ops;
	dir_item_t* list;

} dir_t;

static void* dir_open(void* obj, char* path, s64 flag, s64 mode)
{
	assert(obj != 0);
	assert(path != 0);

    char filename[24];
    char* end = strchr(path, '/');
    strncpy(filename, path, end -path);

    if(*end == '/') // skip '/'
        end ++;

	dir_item_t* item = ((dir_t*)obj)->list;

	while(item != 0)
	{
		if(strcmp(filename, item->name) == 0)
			return open(item->obj, end, flag, mode);

		item = item->next;
	}

	return 0;
}

//
static s64 dir_append(void* obj, char* name, void* child)
{
	dir_item_t* item = ((dir_t*)obj)->list;

	dir_item_t* last_item = 0;

	// check name
	while(item != 0)
	{
		if(strcmp(name, item->name) == 0)
			return -1;

		last_item = item;
		item = item->next;
	}

	dir_item_t* new_item = kmalloc(sizeof(dir_item_t));

	new_item->next = NULL;
	new_item->name = strdup(name);
	new_item->obj = child;

	if(last_item == NULL)
		((dir_t*)obj)->list =  new_item;
	else
		last_item->next = new_item;

	return 0;
}
//
static s64 dir_delete(void* obj, char* path)
{
	assert(obj != 0);
	assert(path != 0);

    char filename[24];
    char* end = strchr(path, '/');
    strncpy(filename, path, end -path);

    if(*end == '/') // skip '/'
        end ++;

	dir_item_t* item = ((dir_t*)obj)->list;
	dir_item_t* prev = 0;

	while(item != 0)
	{
		if(strcmp(filename, item->name) == 0)
        {
            if(*end == NULL) // last filename
            {
                if(prev == 0) // head
                    ((dir_t*)obj)->list = item->next;
                else
                    prev->next = item->next;

                kmfree(item);
                return 0;
            }
            else
                return delete(item->obj, end);
        }

        prev = item;
		item = item->next;
	}

	return -1;

}

static ops_t dir_ops =
{
    .open = dir_open,
    .append = dir_append,
    .delete = dir_delete
};

void* dir_create()
{
	dir_t* obj = (dir_t*)kmalloc(sizeof(dir_t));
	memset(obj, 0, sizeof(dir_t));
	obj->ops = &dir_ops;
	return obj;
}
