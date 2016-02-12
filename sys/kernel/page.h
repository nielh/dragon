
#ifndef PAGE_H
#define PAGE_H

#include "multiboot2.h"
#include "DList.h"

#define PAGE_SIZE	0x1000
#define PAGE_ORDER	12

#define PAGE_ALIGN( address )	((((u64)address) + PAGE_SIZE - 1)& ~(u64)(PAGE_SIZE - 1 ))
#define PAGE_ALIGNED(address)	((((u64)address) & 0xfff) == 0)

#define PT4_IDX(v)      (((u64)(v) >> 39) & 0x1FF)
#define PT3_IDX(v)      (((u64)(v) >> 30) & 0x1FF)
#define PT2_IDX(v)      (((u64)(v) >> 21) & 0x1FF)
#define PT1_IDX(v)      (((u64)(v) >> 12) & 0x1FF)
#define OFFSET(v)       ((u64)(v)& 0xFFF)

#define SHIFT_R( v )	( (u64)(v) >> 12 )
#define SHIFT_L( v )	( (u64)(v) << 12 )


#define PTE_PRESENT			(1 << 0)
#define PTE_WRITE			(1 << 1)
#define PTE_USER			(1 << 2)
#define PTE_PWT				(1 << 3)
#define PTE_PCD				(1 << 4)
#define PTE_ACC				(1 << 5)
#define PTE_DIRTY			(1 << 6)
#define PTE_PSIZE			(1 << 7)
#define PTE_GLOBAL			(1 << 8)
#define PTE_AVL			    (7 << 9)
#define PTE_ADDR			(0xffffffffffULL << 12)
#define PDE_ADDR			(0x3fffffffULL << 21)
#define PTE_NX				(1ULL << 63)



typedef u64 pte_t;

typedef struct page_t
{
    DListNode node;
	s64 address;
	s64 vaddr;

} page_t;

void  page_init();

page_t* page_alloc();
void  page_free(page_t* page);

extern u64 _PT4[];
extern u64 _PT4_PHY[];

extern u64 _PT3[];
extern u64 _PT3_PHY[];

extern u64 _PT2[];
extern u64 _PT2_PHY[];

extern u64 vm_start;


#endif
