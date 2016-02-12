
#include "kernel.h"
#include "printk.h"
#include "page.h"
#include "spinlock.h"
#include "page_map.h"

typedef struct block_header
{
    struct block_header* next;

} block_header_t;

#define BLOCK_ORDER_MIN     5           // ��С��ߴ� 2^5
#define BLOCK_ORDER_MAX     25           // ��С��ߴ� 2^25

#define BLOCK_POOL_SIZE     ((u64)1 << 32)   // ÿ���� 4G �ռ�
#define BLOCK_POOL_COUNT    (BLOCK_ORDER_MAX - BLOCK_ORDER_MIN + 1)  // �Ѹ���


static block_header_t* free_start[BLOCK_POOL_COUNT];
static void* blank_area_start[BLOCK_POOL_COUNT];
static void* block_pool_start;

void* kmalloc_start;
void* kmalloc_end;

static spinlock_t kmalloc_lock = {0}; // spinlock

void kmalloc_init()
{
    kmalloc_start = block_pool_start = (void*)PHY2VIR(phy_space_end); // ���������ڴ�ռ�
    kmalloc_end = block_pool_start + BLOCK_POOL_COUNT * BLOCK_POOL_SIZE;

    // ��ʼ���ڴ��
    for(int i=0; i < BLOCK_POOL_COUNT; i ++)
    {
        free_start[i] = 0;
        blank_area_start[i] = block_pool_start + BLOCK_POOL_SIZE* i;
    }
}

static u64 get_order(u64 size)
{
    u64 idx_l;
    u64 idx_r;

    __asm__ __volatile__("bsrq %2, %0;		\
                    bsfq %2, %1;"                \
            :"=a"(idx_r),"=r"(idx_l): "c"(size));

    if(idx_r == idx_l)
        return idx_r;
    else
        return idx_r+1;
}

void* kmalloc(s64 size)
{
	assert(size > 0 && size <= (1 << BLOCK_ORDER_MAX));

    int order = get_order(size); //
    if(order < BLOCK_ORDER_MIN)
		order = BLOCK_ORDER_MIN;

    int index = order - BLOCK_ORDER_MIN;

    spin_lock(&kmalloc_lock);

    block_header_t* free = free_start[index]; // ��������

    if(free == 0) // û���ڴ��ˣ�����ҳ
    {
        int block_size = (1 << order);

        if(block_size <= PAGE_SIZE)
        {
            // ����С��һҳ��һҳ�����������
            void* vpage = blank_area_start[index];
            blank_area_start[index] += PAGE_SIZE; // ���¿հ�����ָ��

            assert(blank_area_start[index] < block_pool_start + BLOCK_POOL_SIZE* (index + 1));

            page_t* page = page_alloc();              // ����һҳ
            page_map(NULL, vpage, page, PTE_GLOBAL| PTE_WRITE | PTE_PRESENT); // ӳ��

            for(void* vaddr = vpage; vaddr < blank_area_start[index]; vaddr += block_size) // �հ׶����������
            {
                void* next = vaddr + block_size;

                if(next < blank_area_start[index])
                    ((block_header_t*)vaddr)->next = next;
                else
                    ((block_header_t*)vaddr)->next = 0;
            }

            free = vpage;
            free_start[index] = free->next;
        }
        else
        {
            // ��ҳ�Ķ���ÿ�η���һ������
            free = blank_area_start[index];
            blank_area_start[index] += block_size; // ���¿հ�����ָ��

            assert(blank_area_start[index] < block_pool_start + BLOCK_POOL_SIZE* (index + 1));

            // ӳ��
            for(void* vaddr =free; vaddr < blank_area_start[index]; vaddr += PAGE_SIZE)
            {
                page_t* page = page_alloc();              // ����ҳ��ʼ��Ϊ�����ڴ�
                page_map(NULL, vaddr, page, PTE_GLOBAL|PTE_WRITE | PTE_PRESENT); // map to virtual space
            }
        }
    }
    else
    {
        free_start[index] = free->next;
    }

    spin_unlock(&kmalloc_lock);
    return free;
}

void kmfree(void* ptr)
{
    int index = (ptr - block_pool_start) / BLOCK_POOL_SIZE;

    spin_lock(&kmalloc_lock);

    ((block_header_t*)ptr)->next = free_start[index];
    free_start[index] = ptr;

    spin_unlock(&kmalloc_lock);
}
