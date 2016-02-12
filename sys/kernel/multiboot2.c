#include "kernel.h"
#include "printk.h"
#include "libc.h"
#include "multiboot2.h"

char cmdline[128];
struct multiboot_tag_mmap* multiboot_mmap;
struct multiboot_tag_framebuffer* multiboot_framebuffer;
s64 phy_mem_size;
s64 phy_space_end;

static char mbi[4096];

void multiboot(s64 magic, s64 addr)
{
    struct multiboot_tag *tag;

    char* data = (char*)(addr + 8);
    memcpy(mbi, data, sizeof(mbi));

    //printk ("Announced mbi size 0x%x\n", size);

    for (   tag = (struct multiboot_tag *) mbi;
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag *) (((s64)(tag) + tag->size + 7) & ~7))
    {
        printk ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            //strcpy(cmdline,  ((struct multiboot_tag_string *) tag)->string);
            printk ("Command line = %s\n",
                    ((struct multiboot_tag_string *) tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
            printk ("Boot loader name = %s\n",
                    ((struct multiboot_tag_string *) tag)->string);
            break;
        case MULTIBOOT_TAG_TYPE_MODULE:
            printk ("Module at 0x%x-0x%x. Command line %s\n",
                    ((struct multiboot_tag_module *) tag)->mod_start,
                    ((struct multiboot_tag_module *) tag)->mod_end,
                    ((struct multiboot_tag_module *) tag)->cmdline);
            break;
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
            printk ("mem_lower = %uKB, mem_upper = %uKB\n",
                    ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                    ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
            break;
        case MULTIBOOT_TAG_TYPE_BOOTDEV:
            printk ("Boot device 0x%x,%u,%u\n",
                    ((struct multiboot_tag_bootdev *) tag)->biosdev,
                    ((struct multiboot_tag_bootdev *) tag)->slice,
                    ((struct multiboot_tag_bootdev *) tag)->part);
            break;
        case MULTIBOOT_TAG_TYPE_MMAP:
            {
                multiboot_mmap = (struct multiboot_tag_mmap *)tag;
                struct multiboot_mmap_entry * ent = multiboot_mmap->entries;

                while((u64)ent < (u64)multiboot_mmap + multiboot_mmap->size)
                {
                    if(ent->type == MULTIBOOT_MEMORY_AVAILABLE)
                        phy_mem_size += ent->len; // total free memory

                    static char* mtype[] = {NULL, "Avaliable", "Reserved", "ACPI", "NVS", "Bad ram"};

                    printk("  0x%p - 0x%p :%10dK  type:%s\n",
                           ent->addr, ent->addr + ent->len, ent->len >> 10, mtype[ent->type] );

                    phy_space_end = ent->addr + ent->len; // end of physical space
                    ent ++;
                }
            }

            break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
        {
            multiboot_framebuffer = (struct multiboot_tag_framebuffer *)tag;
            break;
        }
        }
    }
}
