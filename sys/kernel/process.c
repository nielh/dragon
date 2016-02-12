
#include "kernel.h"
#include "printk.h"
#include "libc.h"
#include "kmalloc.h"
#include "thread.h"
#include "object.h"
#include "vsnprintf.h"
//#include "fat.h"
#include "vmalloc.h"
#include "process.h"
#include "page.h"
#include "pe.h"
#include "page_map.h"



void* load_module(process_t* proc,char* path)
{
    void* file = open(HANDLE_ROOT, path, NULL, NULL);
    if(file == NULL)
		return NULL;

    page_t* page = page_alloc();

	struct dos_header* dos_header = (struct dos_header*)PHY2VIR(page->address);

	int bytes = read(file, dos_header, 0x500, 0);
	assert(bytes == 0x500);

	assert(dos_header->e_magic == 0x5A4D);

    if(dos_header->e_magic != 0x5A4D ) // "MZ"
	{
		page_free(page);
		return NULL;
	}

	struct image_header* img_hdr = get_image_header(dos_header);

	assert(img_hdr->signature == IMAGE_PE_SIGNATURE);

	if(img_hdr->signature != IMAGE_PE_SIGNATURE)
	{
		page_free(page);
		return NULL;
	}

	assert(img_hdr->header.machine == IMAGE_FILE_MACHINE_AMD64);

    if(img_hdr->header.machine != IMAGE_FILE_MACHINE_AMD64)
	{
		page_free(page);
		return NULL;
	}

	assert(img_hdr->optional.magic == IMAGE_OPTIONAL_HDR64_MAGIC);

    if(img_hdr->optional.magic != IMAGE_OPTIONAL_HDR64_MAGIC)
	{
		page_free(page);
		return NULL;
	}
/*
    if(!(img_hdr->header.characteristics | IMAGE_FILE_DLL))
	{
		page_free(page);
		return NULL;
	}
*/
    u32 size_of_image = img_hdr->optional.size_of_image;

    void* image_base = vma_alloc(proc, size_of_image, VMA_BOTTOM);
    if(image_base == NULL)
	{
		page_free(page);
		return NULL;
	}

    // map header
	page_map(proc, image_base, page, PTE_USER | PTE_PRESENT);

    for(int i=0; i < img_hdr->header.number_of_sections; i ++)
    {
        struct image_section_header* sh = &img_hdr->sections[i];

        if(sh->characteristics & IMAGE_SCN_MEM_DISCARDABLE)
            continue;

        void* map_addr = (void*)(image_base + sh->virtual_address);

		if(sh->size_of_raw_data > 0)
			vma_map(proc, map_addr, file, sh->pointer_to_raw_data, sh->virtual_size, sh->characteristics);
		else
			vma_map(proc, map_addr, 0, 0, sh->virtual_size, sh->characteristics);
    }

    module_t* mod = (module_t *) kmalloc(sizeof(module_t));
    memset(mod, 0 , sizeof(module_t));
    assert(mod != NULL);

    mod->path = strdup(path);
    UpperStr(mod->path);

    mod->file = file;
	mod->image = image_base;
    mod->entry = image_base + img_hdr->optional.address_of_entry_point;

    assert(relocate_module(mod) == 0);
    assert(bind_imports(0, mod)== 0);

	DListAddTail(&proc->module_list, &mod->node);
	DListAddTail(&proc->page_list, &page->node);

	return mod;
}


void unload_module(process_t* proc)
{
	DListNode* n = proc->module_list.head;

	while(n)
	{
		module_t* mod = container_of(n, module_t, node);
		n = DListGetNext(n);

		close(mod->file);
		kmfree(mod->path);
		kmfree(mod);
	}
}

#define USER_STACK_SIZE        (SIZ_1M)

void jmp_to_user(void* user_entry, void* user_param, void* vma_stack_top);

void user_thread(void* param1, void* param2)
{
	void* user_entry = param1;
	void* user_param = param2;
	process_t* proc = thread_current->process;

	if(user_entry == NULL) // main thread
	{
		char* cmdline = (char*)USER_ARG;
		char* tail = strchr(cmdline, ' ');

		char path[256];
		strncpy(path, cmdline, tail -cmdline);

		// map image file
		module_t* mod = load_module(proc, path);
		assert(mod);

		user_entry = mod->entry;
	}

    // alloc & map user stack
	void* user_stack = vma_alloc(proc, USER_STACK_SIZE, VMA_TOP);  // stack has a page hole
	assert(user_stack);

	void* user_stack_top = user_stack + USER_STACK_SIZE -32; // 堆栈上需要留 4* 8 个rcx, rdx r8 r9 的参数空间
	vma_map(proc, user_stack, NULL, NULL, USER_STACK_SIZE, 0);

    jmp_to_user(user_entry, user_param, user_stack_top);
}

static u32 next_process_id = 0;

void map_write_user(process_t* proc, char* vaddr, char* data)
{
    page_t* page = page_alloc();

    char* buf = (char*)PHY2VIR(page->address);
    strcpy(buf, data);

    // map header
	page_map(proc, vaddr, page, PTE_USER | PTE_WRITE | PTE_PRESENT);
    DListAddTail(&proc->page_list, &page->node);
}

char* default_env = NULL;

int process_create(char * cmdline)
{
	ENTER();

    LOG("process_create %s ...\n", cmdline);
    assert(cmdline != 0);

    process_t* proc = kmalloc(sizeof(process_t)); // alloc process struct
    assert(proc != 0);
    memset(proc, 0, sizeof(process_t));

    page_t* page = page_alloc();
    DListAddTail(&proc->page_list, &page->node);

    proc->page_dir = (pte_t*)PHY2VIR(page->address); // alloc page_dir
    proc->page_dir[256] = _PT4[256]; // map kernel page

    proc->id = __sync_add_and_fetch(&next_process_id, 1); // process id start value

    proc->vm_free_bottom = VMA_START;
    proc->vm_free_top = VMA_END;

    char* envstr = (thread_current->process) ? (char*)USER_ENV: "";
    map_write_user(proc, (char*)USER_ENV, envstr);
    map_write_user(proc, (char*)USER_ARG, cmdline);

    create_kthread(proc, user_thread, THREAD_PRI_NORMAL, NULL, NULL); // first thread ,start address = 2G + 4K

    LEAVE();

    return 0;
}

int ProcessFree(process_t* proc)
{
    for(int i=0; i < MAX_FD; i ++)
	{
		void* p = proc->fds[i];
		if(p != 0)
			close(p);
	}

	vma_unmap(proc);
	unload_module(proc);

    while(1)
    {
    	DListNode* n = DListRemoveHead(&proc->page_list);
    	if(n == 0)
			break;

    	page_t* pg = container_of(n, page_t, node);
    	page_free(pg);
    }

	kmfree(proc);

	return 0;
}

long alloc_fd(process_t* p, void* file)
{
    static spinlock_t alloc_lock = {0}; // spinlock

    spin_lock(&alloc_lock);

    int i= -1;
    for(i=0 ; i < MAX_FD; i ++)
    {
        if(p->fds[i] == NULL)
        {
            p->fds[i] = file;
            spin_unlock(&alloc_lock);
            return i;
        }
    }

    spin_unlock(&alloc_lock);
    return -1;
}

