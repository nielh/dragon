#ifndef VMALLOC_H
#define VMALLOC_H

#include "kernel.h"
#include "DList.h"
#include "object.h"
#include "process.h"

enum
{
    VMA_BOTTOM = 0,
    VMA_TOP = 1,
};

typedef struct vma_t
{
	u32 used;

    void* vma_start;
    u64 length;

    void* file;
    u64 offset;
    u32 flag;

    DListNode node;

} vma_t;

void  vm_init();

vma_t* vma_find(process_t* proc, void* vaddr);
void* vma_alloc(process_t* proc, s64 size, int top);
int vma_unmap(process_t* proc);
int vma_map(process_t* proc, void* vaddr, void* file, u64 offset, u64 length, u32 flag);

//void  vm_free(void* ptr);

#endif //
