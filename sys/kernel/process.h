#ifndef PROCESS_H
#define PROCESS_H

#include "kernel.h"
#include "DList.h"
#include "object.h"
#include "pe.h"

#define MAX_FD 64


typedef struct
{
	u64* page_dir;

	u32 id;

	void* current_dir;
	int thread_cnt;

	DList module_list;
	DList vma_list;

	void* fds[MAX_FD];

	u64 vm_free_top;
	u64 vm_free_bottom;

	spinlock_t lock;

	DList page_list;

} process_t;

int process_init();
int process_create(char* path);
int ProcessFree(process_t* proc);

long alloc_fd(process_t* p, void* file);
void proc_page_map(process_t* proc, void* vir_addr, s64 phy_addr, u64 flag);

//void* vm_map(obj_t* obj, u64 offset, u64 length, u32 mode);
void* load_module(process_t* proc,char* path);

#endif //
