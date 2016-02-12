#include "kernel.h"
#include "printk.h"
#include "process.h"
#include "object.h"
#include "kmalloc.h"
#include "libc.h"
#include "pe.h"
#include "page.h"
#include "thread.h"
#include "fat.h"
#include "DList.h"

#define RVA(hmod, rva) (((char *) (hmod)) + (rva))

#define MODLOAD_NOINIT          1
#define MODLOAD_NOSHARE         2

#define MODULE_LOADED           0x0001
#define MODULE_IMPORTS_REFED    0x0002
#define MODULE_RESOLVED         0x0004
#define MODULE_RELOCATED        0x0008
#define MODULE_BOUND            0x0010
#define MODULE_PROTECTED        0x0020
#define MODULE_INITIALIZED      0x0040


static DList kernel_module_list = {0};

struct image_header *get_image_header(void *image)
{
    struct dos_header *doshdr = (struct dos_header *) image;

    if (doshdr == NULL)
        return NULL;

    return (struct image_header *)RVA(image, doshdr->e_lfanew);
}

static void *get_image_directory(void *image, int dir)
{
    struct image_header *header = get_image_header(image);
   unsigned long addr = header->optional.data_directory[dir].virtual_address;
    return addr ? RVA(image, addr) : (void *)0;
}

void *get_entrypoint(void *image)
{
    return RVA(image, get_image_header(image)->optional.address_of_entry_point);
}

static void *get_proc_by_name(void *image, int hint, char *procname)
{
    struct image_export_directory *exp;
    unsigned int *names;
    unsigned int i;

    exp = (struct image_export_directory *) get_image_directory(image, IMAGE_DIRECTORY_ENTRY_EXPORT);
    if (!exp)
        return NULL;

    names = (unsigned int *) RVA(image, exp->address_of_names);

    if (hint >= 0 && hint < (int) exp->number_of_names && strcmp(procname, RVA(image, names[hint])) == 0)
    {
        unsigned short idx;

        idx = *((unsigned short *) RVA(image, exp->address_of_name_ordinals) + hint);
        return RVA(image, *((unsigned long *) RVA(image, exp->address_of_functions) + idx));
    }

    for (i = 0; i < exp->number_of_names; i++)
    {
        char *name = RVA(image, *names);
        //int order = *((unsigned short *) RVA(image, exp->address_of_name_ordinals) + i);

        //printk("%s :%d\n", name, order);

        if (strcmp(name, procname) == 0)
        {
            unsigned short idx;

            idx = *((unsigned short *) RVA(image, exp->address_of_name_ordinals) + i);
            return RVA(image, *((unsigned long *) RVA(image, exp->address_of_functions) + idx));
        }

        names++;
    }

    return NULL;
}

static void *get_proc_by_ordinal(void *image, unsigned int ordinal)
{
    struct image_export_directory *exp;

    exp = (struct image_export_directory *) get_image_directory(image,
            IMAGE_DIRECTORY_ENTRY_EXPORT);
    if (!exp)
        return NULL;

    if (ordinal < exp->base || ordinal >= exp->number_of_functions + exp->base)
        panic("invalid ordinal\n");
    return RVA(image, *((unsigned long *) RVA(image, exp->address_of_functions) + (ordinal - exp->base)));
}

static module_t *get_module(int sys_mod, char *name)
{
	DListNode* n;
	if(sys_mod)
		n = kernel_module_list.head;
    else
		n = thread_current->process->module_list.head;

    UpperStr(name);

    while (n)
    {
		module_t* mod = container_of(n, module_t, node);
		n = DListGetNext(n);

        if (strcmp(name, mod->path) == 0)
            return mod;
    }

    return 0;
}

int bind_imports(int sys_mod, module_t *mod)
{
    struct image_import_descriptor *imp;
    int errs = 0;

    // Find import directory in image
    imp = (struct image_import_descriptor *) get_image_directory(mod->image,
            IMAGE_DIRECTORY_ENTRY_IMPORT);

    if (!imp)
        return 0;

    // Update Import Address Table (IAT)
    while (imp->characteristics != 0)
    {
        if (imp->forwarder_chain != 0 && imp->forwarder_chain != 0xFFFFFFFF)
        {
            printk("import forwarder chains not supported (%s)\n", mod->path);
            return -1;
        }

        u64 *thunks;
        u64 *origthunks;
        struct image_import_by_name *ibn;
        char *name;
        struct module_t *expmod;

        name = RVA(mod->image, imp->name);

        //expmod = get_module(mod->db, name);
        expmod = get_module(sys_mod, name);

        if (!expmod)
        {
            printk("module %s not loaded\n", name);
            return -1;
        }

        thunks = (u64 *) RVA(mod->image, imp->first_thunk);
        origthunks = (u64 *) RVA(mod->image, imp->original_first_thunk);
        while (*thunks)
        {
            if (*origthunks & IMAGE_ORDINAL_FLAG)
            {
                // Import by ordinal
                u64 ordinal = *origthunks & ~IMAGE_ORDINAL_FLAG;
                *thunks = (u64) get_proc_by_ordinal(expmod->image, ordinal);

                if (*thunks == 0)
                {
                    printk("unable to resolve %s:#%d in %s\n", expmod->path, ordinal, mod->path);
                    errs++;
                }
            }
            else
            {
                // Import by name (and hint)
                ibn = (struct image_import_by_name *) RVA(mod->image, *origthunks);
                *thunks = (u64) get_proc_by_name(expmod->image, ibn->hint, ibn->name);

                if (*thunks == 0)
                {
                    printk("unable to resolve %s:%s in %s\n", expmod->path, ibn->name, mod->path);
                    errs++;
                }
            }

            thunks++;
            origthunks++;
        }

        imp++;
    }

    if (errs)
        return -1;

    mod->flags |= MODULE_BOUND;
    return 0;
}

int relocate_module(module_t *mod)
{
    s64 offset;
    char *pagestart;
    unsigned short *fixup;
    int i;
    struct image_base_relocation *reloc;
    int nrelocs;

    struct image_header * img_hdr = get_image_header(mod->image);

    offset = (s64)(mod->image) - img_hdr->optional.image_base;

    if (offset == 0)
        return 0;

    if (img_hdr->header.characteristics & IMAGE_FILE_RELOCS_STRIPPED)
    {
        printk("relocation info missing for %s\n", mod->path);
        return -1;
    }

    reloc = (struct image_base_relocation *) get_image_directory(mod->image, IMAGE_DIRECTORY_ENTRY_BASERELOC);

    if (!reloc)
        return 0;

    while (reloc->virtual_address != 0 || reloc->size_of_block != 0)
    {
        pagestart = RVA(mod->image, reloc->virtual_address);
        nrelocs = (reloc->size_of_block - sizeof(struct image_base_relocation)) / 2;
        fixup = (unsigned short *) (reloc + 1);
        for (i = 0; i < nrelocs; i++, fixup++)
        {
            unsigned short type = *fixup >> 12;
            unsigned short pos = *fixup & 0xfff;

            if (type == IMAGE_REL_BASED_HIGHLOW)
                *(unsigned long *) (pagestart + pos) += offset;
            else if (type != IMAGE_REL_BASED_ABSOLUTE)
            {
                printk("unsupported relocation type %d in %s\n", type, mod->path);
                return -1;
            }
        }

        reloc = (struct image_base_relocation *) fixup;
    }

    mod->flags |= MODULE_RELOCATED;
    return 0;
}

char* get_file_name(char* path)
{
    char* t = path + strlen(path) - 1;
    while (t >= path)
    {
        if (*t == '/')
            break;

        t--;
    }

    return t + 1;
}

int LoadSysModule(char* path)
{
	LOG("loading %s ...\n", path);

	void* file = open(HANDLE_ROOT, path, NULL, NULL);
	assert(IS_PTR(file));

    struct FAT_ENTRY stat;
    ioctl(file, IOCTL_GETSTAT, &stat, sizeof(stat));
	int file_size = stat.file_size;

	void* img = kmalloc(file_size);

	int read_bytes = read(file, img, file_size, 0);
	assert(read_bytes == file_size);

	struct module_t * mod = (struct module_t *) kmalloc(sizeof(struct module_t));
	mod->path = strdup(path);
	UpperStr(mod->path);
	mod->image = img;
	mod->file = file;

	assert(relocate_module(mod) == 0);
	assert(bind_imports(1, mod)== 0);

	typedef int (*MODULE_INIT)(void* hmod);

	MODULE_INIT fnc = (MODULE_INIT) get_proc_by_name(mod->image, -1,	"module_init");

	if (fnc == 0)
	{
		printk("not found 'module_init' in '%s' \n", mod->path);
		return -1;
	}

	int ret = fnc(mod->image);
	//printk("module [%s] init [%d]\n", mod->name, ret);

	if (ret != 0)
		return -1;

	DListAddTail(&kernel_module_list, &mod->node);
	return 0;
}

int pe_init()
{
    module_t *mod = (module_t *) kmalloc(sizeof(module_t));
    memset(mod, 0, sizeof(module_t));
    mod->path = "KERNEL.SYS";
    mod->image = (void*)(KERNEL_START + 0x100000);

    DListAddTail(&kernel_module_list, &mod->node);

    return 0;
}
