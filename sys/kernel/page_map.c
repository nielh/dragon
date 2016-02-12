
#include "page.h"
#include "page_map.h"
#include "libc.h"
#include "printk.h"

static spinlock_t page_lock = {0}; // spinlock

s64 vir_to_phy(pte_t* page_dir, void* vir_addr)
{
    //PT4
    pte_t* pt4_e = &page_dir[PT4_IDX(vir_addr)];
    assert(*pt4_e | PTE_PRESENT);

    // PT3
    pte_t* pt3 = (pte_t*)PHY2VIR(*pt4_e & PTE_ADDR);
    pte_t* pt3_e = &pt3[PT3_IDX(vir_addr)];
    assert(*pt3_e | PTE_PRESENT);

    // PT2
    pte_t* pt2 = (pte_t*)PHY2VIR(*pt3_e & PTE_ADDR);
    pte_t* pt2_e = &pt2[PT2_IDX(vir_addr)];
    assert(*pt2_e | PTE_PRESENT);

    if(*pt2_e & PTE_PSIZE)
    {
        u64 offset = (u64)vir_addr & 0x1fffff;
        return (*pt2_e & PDE_ADDR)|offset; // 2M page
    }

    // PT1
    pte_t* pt1 = (pte_t*)PHY2VIR(*pt2_e & PTE_ADDR);
    pte_t* pt1_e = &pt1[PT1_IDX(vir_addr)];
    assert(*pt1_e | PTE_PRESENT);

    return (*pt1_e & PTE_ADDR)|OFFSET(vir_addr);
}

extern u64 phy_end;
extern u64 kmalloc_start;
extern u64 kmalloc_end;

void page_map(process_t* proc, void* vir_addr, page_t* page, u64 flag)
{
	assert(!((u64)vir_addr & 0xfff));

    spin_lock(&page_lock);

    pte_t* page_dir = (proc == NULL) ?_PT4 :proc->page_dir;

    //PT4
    pte_t* pt4_e = &page_dir[PT4_IDX(vir_addr)];

    if((*pt4_e & PTE_PRESENT) == 0)
    {
        page_t* pg = page_alloc();
		*pt4_e = pg->address | PTE_USER | PTE_WRITE | PTE_PRESENT; // page protect by PTE

		if(proc)
			DListAddTail(&proc->page_list, &pg->node);
    }

    // PT3
    pte_t* pt3 = (pte_t*)PHY2VIR(*pt4_e & PTE_ADDR);
    pte_t* pt3_e = &pt3[PT3_IDX(vir_addr)];

    if((*pt3_e & PTE_PRESENT) == 0)
    {
		page_t* pg = page_alloc();
		*pt3_e = pg->address | PTE_USER | PTE_WRITE | PTE_PRESENT; // page protect by PTE

		if(proc)
			DListAddTail(&proc->page_list, &pg->node);
    }

    // PT2
    pte_t* pt2 = (pte_t*)PHY2VIR(*pt3_e & PTE_ADDR);
    pte_t* pt2_e = &pt2[PT2_IDX(vir_addr)];

    if((*pt2_e & PTE_PRESENT) == 0)
    {
		page_t* pg = page_alloc();
        *pt2_e = pg->address |PTE_USER | PTE_WRITE | PTE_PRESENT;

        if(proc)
			DListAddTail(&proc->page_list, &pg->node);
    }

    // PT1
    pte_t* pt1 = (pte_t*)PHY2VIR(*pt2_e & PTE_ADDR);
    pte_t* pt1_e = &pt1[PT1_IDX(vir_addr)];

    assert((*pt1_e & PTE_PRESENT) == 0);
    *pt1_e = (u64)page->address | flag;

    if(proc)
		DListAddTail(&proc->page_list, &page->node);

	spin_unlock(&page_lock);

	//LOG("page map vir 0x%x to phy 0x%x\n", vir_addr, phy_addr + flag);
}
