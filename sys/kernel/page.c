#include "kernel.h"
#include "page.h"
#include "vga.h"
#include "printk.h"
#include "multiboot2.h"
#include "libc.h"
#include "msr.h"


u64 table_start, table_end;
u64 pages_start, pages_end;

/* MTRR region types */
#define MTRR_TYPE_UNCACHE	0
#define MTRR_TYPE_WRCOMB	1
#define MTRR_TYPE_WRTHROUGH	4
#define MTRR_TYPE_WRPROT	5
#define MTRR_TYPE_WRBACK	6

#define MTRR_DEF_TYPE_MSR	0x2ff
#define MTRR_DEF_TYPE_EN	(1 << 11)
#define MTRR_DEF_TYPE_FIX_EN	(1 << 10)

#define MTRR_PHYS_BASE_MSR(reg)	(0x200 + 2 * (reg))
#define MTRR_PHYS_MASK_MSR(reg)	(0x200 + 2 * (reg) + 1)
#define MTRR_PHYS_MASK_VALID	(1 << 11)

static int mtrr_idx = 0;

void mtrr_set(u64 base, u64 len, int type)
{
    assert((base & 0xFFF) == 0);
    //assert((len  & 0xFFF) == 0);

    if(mtrr_idx == 0) // set default mtrr to uc
        wrmsr(MTRR_DEF_TYPE_MSR, MTRR_DEF_TYPE_EN | MTRR_TYPE_UNCACHE);

    wrmsr(MTRR_PHYS_BASE_MSR(mtrr_idx), base | type);

	u32	mask = ~(len - 1);
    mask &= (1ULL << 52) - 1;
    wrmsr(MTRR_PHYS_MASK_MSR(mtrr_idx), mask | MTRR_PHYS_MASK_VALID);

    mtrr_idx ++;
    assert(mtrr_idx < 8);
}

// 核心映射
void kmem_map()
{
    table_start = (u64)_PT4;

    int count_1g = (phy_space_end + SIZ_1G -1) >> 30; // PT2页表项数量 512 对齐，计算PT3页表数
    int count_2m = count_1g * 512; //  PT2页表项数量 512 对齐，计算PT2页表数
    int valid_2m = phy_space_end >> 21; // 对齐到2M,计算PT2页表项数量,每个PT2页表项映射 2M 物理空间

    // 设置 PT3 表项，每个 PT3 页表项映射 1G 物理空间
    u64 pt2_phy_addr = (u64)_PT2_PHY; // PT2 页表物理地址

    for(int i=0; i < count_1g; i ++)
    {
        _PT3[i] = pt2_phy_addr | PTE_WRITE | PTE_PRESENT ;
        pt2_phy_addr += PAGE_SIZE; // 下一个PT2页表
    }

    // 设置 PT2 表项，每个 PT2 页表项映射 2M 物理空间
    u64 phy_addr = 0;
    for(int i=0; i < count_2m; i ++)
    {
        if(i < valid_2m)
        {
            _PT2[i] = phy_addr | PTE_PSIZE | PTE_WRITE | PTE_PRESENT;
            phy_addr += 0x200000;  // 每个PT2 页表项映射 2M 物理空间
        }
        else
            _PT2[i] = 0;// 清零 最后的页表项
    }

    table_end = (u64)&_PT2[count_2m];
}

DList FreePageList = {0};

void page_init()
{
    // page start , end
    pages_start = table_end; // after PT2
    s64 count = (phy_mem_size - (VIR2PHY(pages_start) - SIZ_1M)) / (sizeof(page_t) + PAGE_SIZE);

    pages_end = pages_start + count *sizeof(page_t);

    page_t* page = (page_t*)pages_start;
	struct multiboot_mmap_entry * ent = multiboot_mmap->entries;

    while((u64)ent < (u64)multiboot_mmap + multiboot_mmap->size)
    {
        if(ent->type == MULTIBOOT_MEMORY_AVAILABLE) // memory
        {
            u64 len = ent->len & (~0xFFFULL); // 把grub检测的内存区域长度对齐到 4K
            u64 end = ent->addr + len;

            for(s64 phy = ent->addr; phy < end; phy += PAGE_SIZE)
            {
                if(phy >= SIZ_1M && phy < VIR2PHY(pages_end)) // skip kernel memory
                    continue;

                memset(page, 0, sizeof(page_t)); // clear
                page->address = phy;
                DListAddTail(&FreePageList, &page->node);

                page ++;
            }
        }

        ent ++;
    }

    //len_align4k
    printk("Free memory %dM \n", count >> 8);
}

page_t* page_alloc()
{/*
    DListNode* n = DListRemoveHead(&FreePageList);
    page_t* p = container_of(n, page_t, node);

    if(p == 0)
    {
        n = DListRemoveHead(&FreePageList);
        p = container_of(n, page_t, node);

        assert(p >= 0);
        memset((void*)PHY2VIR(p->address), 0, PAGE_SIZE);
    }
*/
    DListNode* nd = DListRemoveHead(&FreePageList);
    assert(nd);

    page_t* pg = container_of(nd, page_t, node);
	memset((void*)PHY2VIR(pg->address), 0, PAGE_SIZE);

    return pg;
}

void page_free(page_t* p)
{
    DListAddHead(&FreePageList, &p->node);
}
