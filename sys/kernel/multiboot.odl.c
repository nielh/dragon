#include "multiboot.h"
#include "kernel.h"
#include "printk.h"

u64 phy_mem_max;

void multiboot_info_dump(multiboot_info_t* info)
{
	// if the multiboot info contains a memory map we will use it
	if ((info->flags & MULTIBOOT_INFO_MEMORY) == 0)
		panic("no memory map info");

	printk("MultiBoot Info (flags: 0x%x)\n", info->flags);

	if (info->flags & MULTIBOOT_INFO_MEMORY)
		printk("PC Memory: lower %dK, upper %dK\n", info->mem_lower, info->mem_upper);

	if (info->flags & MULTIBOOT_INFO_CMDLINE)
		printk("Kernel command line: `%s'\n", (char *) (u64)(info->cmdline));

	if (info->flags & MULTIBOOT_INFO_MEM_MAP)
	{
		printk("Memory Map:      addr: 0x%x, length: 0x%x\n", info->mmap_addr, info->mmap_length);

		u64 mmap_addr = info->mmap_addr;
		u64 mmap_end = info->mmap_addr + info->mmap_length;

		while (mmap_addr < mmap_end)
		{
			multiboot_mmap_entry_t * segment = (multiboot_mmap_entry_t *) mmap_addr;

			u64 start = segment->addr;
			u64 len   = segment->len;
			u64 end   = start + len;

			phy_mem_max = end;

			/*
			 * I'd rather use .0 precision for the High
			 * entries (since they're guaranteed to be 0
			 * on a PC), but this works too.  */
			printk("  0x%p - 0x%p :%10dK  type:", start, end, len >> 10 );

			switch (segment->type)
			{
			case 1:
				printk("Memory\n");
				break;
			case 2:
				printk("System ROM\n");
				break;
			case 3:
				printk("ACPI Reclaim\n");
				break;
			case 4:
				printk("ACPI NVS\n");
				break;
			default:
				printk("undef(%ld)\n", segment->type);
			}

			mmap_addr += segment->size + 4;
		}
	}

	if (info->flags & MULTIBOOT_INFO_MODS)
	{
		multiboot_module_t* mod_list = (multiboot_module_t*) (u64)(info->mods_addr);
		int mod_count = info->mods_count;

		for (int i = 0; i < mod_count; i++)
		{
			multiboot_module_t* mod = &mod_list[i];

			printk("module:%s\n", mod->cmdline);
			printk("  start:0X%X, end:0X%X\n", mod->mod_start, mod->mod_end);
		}
	}
}
