#include "kernel.h"
#include "printk.h"
#include "vmalloc.h"
#include "libc.h"
#include "page.h"
#include "page_map.h"
#include "kmalloc.h"
#include "isr.h"
#include "thread.h"

vma_t* vma_find(process_t* proc, void* vaddr)
{
	DListNode* n = proc->vma_list.head;

	while(n)
    {
    	vma_t* vma = container_of(n, vma_t, node);
    	n = DListGetNext(n);

        if((vaddr >= vma->vma_start) && (vaddr < vma->vma_start + vma->length))
            return vma;
    }

    return NULL;
}

void* vma_alloc(process_t* proc, s64 size, int top)
{
	assert(size > 0);
	assert((size & 0xFFF) == 0);// 4K align

	size += PAGE_SIZE; //
	u64 ret;

	static spinlock_t vm_lock = {0}; // spinlock

	spin_lock(&vm_lock);

	if(top)
    {
        proc->vm_free_top -= size;
        ret = proc->vm_free_top;
    }
    else
    {
        ret = proc->vm_free_bottom;
        proc->vm_free_bottom += size;
    }

	spin_unlock(&vm_lock);

    assert(proc->vm_free_bottom < proc->vm_free_top);

	return (void*)ret;
}

int vma_map(process_t* proc, void* vaddr, void* file, u64 offset, u64 length, u32 flag)
{
	assert(length > 0);

	if(vma_find(proc, vaddr))
        panic("vma override");

	vma_t* vma = (vma_t*)kmalloc(sizeof(vma_t));
	assert(vma != 0);

	vma->vma_start = vaddr;
	vma->length = length;
	vma->file = file;
	vma->offset = offset;
	vma->flag = flag;

	// add to list
	DListAddTail(&proc->vma_list, &vma->node);

	LOG("VMA:0x%x Len:0x%06x file:0x%x Pos:0x%06x Flag:%02d\n", vaddr, length, file, offset, flag);

	return 0;
}

int vma_unmap(process_t* proc)
{
	DListNode* n = proc->vma_list.head;

	while(n)
    {
    	vma_t* vma = container_of(n, vma_t, node);
    	n = DListGetNext(n);

    	kmfree(vma);
    }

    return 0;
}

static void page_fault(void* data, registers_t* regs)
{
    ENTER();

	s64 linearAddress;
	ASM( "mov %%cr2, %0" : "=r" (linearAddress));

	LOG("page_fault address:0x%x\n", linearAddress);

    assert(thread_current != NULL);
    assert(thread_current->process != NULL);

    DListNode* n = thread_current->process->vma_list.head;

    while(n)
	{
		vma_t* vma = container_of(n, vma_t, node);
		n = DListGetNext(n);

		if(linearAddress >= (u64)vma->vma_start && linearAddress < (u64)(vma->vma_start + vma->length))
		{
			void* page_addr = (void*)(linearAddress & ~0xFFF); //align to page

			if(vma->file == 0) // stack rw
			{
				page_t* page = page_alloc();
                page_map(thread_current->process, page_addr, page,
                              PTE_USER | PTE_WRITE | PTE_PRESENT);

                LEAVE();
                return;
			}
			else
			{
			    s64 offset = (s64)page_addr - (s64)vma->vma_start + vma->offset;

			    if(vma->flag | IMAGE_SCN_MEM_WRITE) // data rw
                {
                    page_t* page = page_alloc();

                    int bytes = read(vma->file, (void*)PHY2VIR(page->address), PAGE_SIZE, offset);
                    assert(bytes == PAGE_SIZE);

                    page_map(thread_current->process, page_addr, page,
                                  PTE_USER | PTE_WRITE | PTE_PRESENT);
                    LEAVE();
                    return;
                }
                else
                {/*
                    void* page_buf = (void*)ioctl(vma->file, IOCTL_LOCKPAGE, (void*)offset, PAGE_SIZE);
                    s64 phy_page = vir_to_phy(_PT4, page_buf);

                    page_map(thread_current->process, (void*)page_addr,
                                  phy_page, PTE_USER | PTE_PRESENT);

                    return;*/
                    page_t* page = page_alloc();

                    int bytes = read(vma->file, (void*)PHY2VIR(page->address), PAGE_SIZE, offset);
                    assert(bytes == PAGE_SIZE);

                    page_map(thread_current->process, page_addr, page, PTE_USER | PTE_PRESENT);
                    LEAVE();
                    return;
                }
			}
		}
	}

	assert(0);
}

void vm_init()
{
	isr_register(ISR_PAGE_FALUT, page_fault, 0);
}
