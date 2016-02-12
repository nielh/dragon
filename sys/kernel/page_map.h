
#ifndef PAGE_MAP_H
#define PAGE_MAP_H

#include "kernel.h"
#include "process.h"

s64 vir_to_phy(pte_t* page_dir, void* vir_addr);
void page_map(process_t* proc, void* vir_addr, page_t* page, u64 flag);

#endif
