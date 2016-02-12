
#include "vga.h"

#include "page.h"
#include "idt.h"
#include "isr.h"
#include "dir.h"
#include "vga.h"
#include "pci.h"
#include "fat.h"
#include "pe.h"
#include "printk.h"
#include "process.h"
#include "kmalloc.h"
#include "thread.h"
#include "timer.h"
#include "printk.h"
#include "syscall.h"
#include "serial.h"
#include "kbd.h"
#include "gdbstub.h"
#include "ahci.h"
#include "libc.h"
#include "multiboot2.h"

void idle_thread(void* param1, void* param2)
{
    while(1)
    {
        mdelay(2000);
        printk(".");
    }
}

void init_thread(void* param1, void* param2)
{
    ahci_init();
    //ahci_test();
    fat_init();
    pe_init();

    //LoadSysModule("sys/test.sys");
    //for(int i=0; i <16; i++)
	process_create("sys/init.exe");

    while(1)
    {
        mdelay(2000);
        printk("1");
    }
}

void cpuid_print();

static void root_init()
{
	ENTER();

    HANDLE_ROOT = dir_create();
    HANDLE_OBJ = dir_create();

    append(HANDLE_ROOT, "obj", HANDLE_OBJ);

    LEAVE();
}

void kmem_map();
void vm_init();
void console_init();
void vesa_init();
void mouse_init();

void kmain(s64 magic, s64 info)
{
	//vga_clear(COLOR_BLACK);
    idt_init();
    isr_init();

    serial_init();
	set_debug_traps();
    BREAKPOINT();

	cpuid_print();
	multiboot(magic, info);
	kmem_map();
    page_init();
    kmalloc_init();
    //vesa_init();

    root_init();
    pci_init();
    vm_init();
    syscall_init();
    timer_init();
    kbd_init();
    //mouse_init();

    console_init();

 	create_kthread(NULL, idle_thread, THREAD_PRI_LOW, NULL, NULL);
 	create_kthread(NULL, init_thread, THREAD_PRI_NORMAL, NULL, NULL);

    thread_schedule();
}
