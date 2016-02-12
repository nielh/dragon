
//
// descriptor_tables.h - Defines the interface for initialising the GDT and IDT.
//                       Also defines needed structures.
//                       Based on code from Bran's kernel development tutorials.
//                       Rewritten for JamesM's kernel development tutorials.
//

#ifndef IDT_H
#define IDT_H

#include "kernel.h"

#define KCODE_SEL		0x08
#define KDATA_SEL		0x10
#define UCODE_SEL		0x1b
#define UDATA_SEL		0x23
#define TSS64_SEL		0x28


// Initialisation function is publicly accessible.
void idt_init();

// Allows the kernel stack in the TSS to be changed.

void irq_enable(unsigned char IRQline);
void irq_disable(unsigned char IRQline);

// This structure contains the value of one GDT entry.
// We use the attribute 'packed' to tell GCC not to change
// any of the alignment in the structure.
typedef struct gdt_entry_t
{
	u16 limit_low;           // The lower 16 bits of the limit.
	u16 base_low;            // The lower 16 bits of the base.
	u8  base_middle;         // The next 8 bits of the base.
	u8  access;              // Access flags, determine what ring this segment can be used in.
	u8  granularity;
	u8  base_high;           // The last 8 bits of the base.

} PACKED gdt_entry_t;


// This struct describes a GDT pointer. It points to the start of
// our array of GDT entries, and is in the format required by the
// lgdt instruction.
typedef struct gdt_ptr_t
{
	u16 limit;               // The upper 16 bits of all selector limits.
	u64 base;                // The address of the first gdt_entry_t struct.

} PACKED gdt_ptr_t ;

// A struct describing an interrupt gate.
typedef struct idt_entry_t
{
	u16 base1;             // The lower 16 bits of the address to jump to when this interrupt fires.
	u16 sel;                 // Kernel segment selector.
	u8  always0;             // This must always be zero.
	u8  flags;               // More flags. See documentation.
	u16 base2;             // The upper 16 bits of the address to jump to.
	u32 base3;
	u32 reserved;
} PACKED idt_entry_t;

// A struct describing a pointer to an array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
typedef struct idt_ptr_t
{
	u16 limit;
	u64 base;                // The address of the first element in our idt_entry_t array.
} PACKED idt_ptr_t;

// A struct describing a Task State Segment.
typedef struct tss_t
{
	u32 pad0;
	u64 rsp[3];
	u64 pad1;
	u64 ist[7];
	u64 pad2;
	u16 pad3;
	u16 iobase;
} PACKED tss_t;


// These extern directives let us access the addresses of our ASM ISR handlers.
extern void isr0 ();
extern void isr1 ();
extern void isr2 ();
extern void isr3 ();
extern void isr4 ();
extern void isr5 ();
extern void isr6 ();
extern void isr7 ();
extern void isr8 ();
extern void isr9 ();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void isr32(); // irq0
extern void isr33();
extern void isr34();
extern void isr35();
extern void isr36();
extern void isr37();
extern void isr38();
extern void isr39();
extern void isr40();
extern void isr41();
extern void isr42();
extern void isr43();
extern void isr44();
extern void isr45();
extern void isr46();
extern void isr47(); // irq15

extern void isr63(); // syscall

#endif
