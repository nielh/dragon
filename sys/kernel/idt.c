
#include "libc.h"
#include "idt.h"
#include "printk.h"
#include "port.h"
#include "isr.h"

// Lets us access our ASM functions from our C code.
extern void gdt_flush(void*);
extern void idt_flush(void*);
extern void tss_flush();

gdt_entry_t gdt_entries[8];
gdt_ptr_t   gdt_ptr;
idt_entry_t idt_entries[48];
idt_ptr_t   idt_ptr;
//tss_t 		tss;

extern u64 _KSTACK[];

// Set the value of one GDT entry.
static void gdt_set_gate(s32 num, u32 base, u16 limit, u8 access, u8 gran)
{
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);

    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

// Initialise our task state segment structure.
static void gdt_set_tss()
{
    // Ensure the descriptor is initially zero.
    //memset(&tss, 0, sizeof(tss));

    //tss.rsp[0]  = (u64)_KSTACK;  // Set the kernel stack segment.

    // Here we set the cs, ss, ds, es, fs and gs entries in the TSS. These specify what
    // segments should be loaded when the processor switches to kernel mode. Therefore
    // they are just our normal kernel code/data segments - 0x08 and 0x10 respectively,
    // but with the last two bits set, making 0x0b and 0x13. The setting of these bits
    // sets the RPL (requested privilege level) to 3, meaning that this TSS can be used
    // to switch to kernel mode from ring 3.
    //tss.cs   = 0x0b;
    //tss.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13;

    // Firstly, let's compute the base and limit of our entry into the GDT.
    //u64 base = (u64)&tss;
    //u32 limit = sizeof(tss) -1;

    // Now, add our TSS descriptor's address to the GDT.
   // gdt_set_gate(5, (u32)base, limit, 0xE9, 0x00);

    //u32* base_hi = (u32*)&gdt_entries[6]; // TSS64, set base[63:32]
    //*base_hi = (u32)(base>>32);
}

void gdt_init()
{
	memset(&gdt_entries, 0, sizeof(gdt_entries));

    gdt_set_gate(0, 0, 0, 0, 0);            // 0x0  Null segment
    gdt_set_gate(1, 0, 0xFFFF, 0x98, 0x20); 		// 0x8  Kernel Code segment
    gdt_set_gate(2, 0, 0xFFFF, 0x90, 0x00); 		// 0x10 Kernel Data segment
    gdt_set_gate(3, 0, 0xFFFF, 0xF8, 0x20); 		// 0x18 User mode code segment
    gdt_set_gate(4, 0, 0xFFFF, 0xF0, 0x00); 		// 0x20 User mode data segment

    gdt_set_tss();						 	        // 0x28 TSS64

    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base  = (u64)&gdt_entries;

    gdt_flush(&gdt_ptr);

    tss_flush();
}

void isr_unknown(void* data, registers_t* regs)
{
    panic("isr not set [%d]", regs->int_no);
}
/*
static void set_intr_gate(u8 num, u8 dpl, void (*isr)())
{
    idt_entries[num].base1 = (u64)isr & 0xFFFF;
    idt_entries[num].base2 = ((u64)isr >> 16) & 0xFFFF;
    idt_entries[num].base3 = ((u64)isr >> 32) & 0xFFFFFFFF;

    idt_entries[num].sel     = KCODE_SEL;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags   = 14 | (dpl << 5) | 0x80; // Type=1110 interrupt gate
}
*/

static void idt_set_gate(u8 num, u8 type, u8 dpl, void (*isr)())
{
    idt_entries[num].base1 = (u64)isr & 0xFFFF;
    idt_entries[num].base2 = ((u64)isr >> 16) & 0xFFFF;
    idt_entries[num].base3 = ((u64)isr >> 32) & 0xFFFFFFFF;

    idt_entries[num].sel     = KCODE_SEL;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags   = type | (dpl << 5) | 0x80;
}


static void set_intr_gate(unsigned int n, void (*addr)) //
{
    idt_set_gate(n,14,0,addr); // Type=1110
}

static void set_trap_gate(unsigned int n, void (*addr)) //
{
    idt_set_gate(n,15,0,addr); // Type=1111
}

static void set_system_gate(unsigned int n, void (*addr)) //
{
    idt_set_gate(n,15,3,addr); // Type=1111
}

void idt_init()
{
    memset(&idt_entries, 0, sizeof(idt_entries));

    set_trap_gate( 0, isr0);
    set_trap_gate( 1, isr1);

    set_intr_gate( 2, isr2);
    set_system_gate( 3, isr3);
    set_system_gate( 4, isr4);
    set_system_gate( 5, isr5);

    set_intr_gate( 6, isr6 );
    set_intr_gate( 7, isr7 );
    set_intr_gate( 8, isr8 );
    set_intr_gate( 9, isr9 );
    set_intr_gate(10, isr10);
    set_intr_gate(11, isr11);
    set_intr_gate(12, isr12);
    set_intr_gate(13, isr13);
    set_intr_gate(14, isr14);
    set_intr_gate(15, isr15);
    set_intr_gate(16, isr16);
    set_intr_gate(17, isr17);
    set_intr_gate(18, isr18);
    set_intr_gate(19, isr19);

    set_intr_gate(32, isr32); // irq0
    set_intr_gate(33, isr33);
    set_intr_gate(34, isr34);
    set_intr_gate(35, isr35);
    set_intr_gate(36, isr36);
    set_intr_gate(37, isr37);
    set_intr_gate(38, isr38);
    set_intr_gate(39, isr39);
    set_intr_gate(40, isr40);
    set_intr_gate(41, isr41);
    set_intr_gate(42, isr42);
    set_intr_gate(43, isr43);
    set_intr_gate(44, isr44);
    set_intr_gate(45, isr45);
    set_intr_gate(46, isr46);
    set_intr_gate(47, isr47);

    idt_ptr.limit = sizeof(idt_entries) -1;
    idt_ptr.base  = (u64)idt_entries;
    idt_flush(&idt_ptr);
}

