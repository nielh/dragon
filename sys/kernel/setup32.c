#include "kernel.h"
#include "multiboot2.h"


void setup32(u32 magic, void* addr)
{
    struct multiboot_tag *tag;
    unsigned size;

    /* Am I booted by a Multiboot-compliant boot loader?  */
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
        printk ("Invalid magic number: 0x%x\n", (unsigned) magic);
        return;
    }

    size = *(unsigned *) addr;
    printk ("Announced mbi size 0x%x\n", size);
    for (tag = (struct multiboot_tag *) (addr + 8);
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag *) ((u8 *) tag
                    + ((tag->size + 7) & ~7)))
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
            struct multiboot_tag_mmap * mmap = *(struct multiboot_tag_mmap *)tag;
            break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
        {
            framebuffer = *((struct multiboot_tag_framebuffer *)tag)->common;
        }
        }
    }
}
