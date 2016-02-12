#define ASM_FILE	1
#include "multiboot2.h"

.code32
.global _start

	// Multiboot header.
multiboot_header:
	// magic
	.long	MULTIBOOT2_HEADER_MAGIC
	// ISA: i386
	.long   MULTIBOOT_ARCHITECTURE_I386
	// Header length.
	.long	multiboot_header_end - multiboot_header
	// checksum
	.long	-(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))

#ifndef __ELF__
	.align	8
address_tag_start:
	.short MULTIBOOT_HEADER_TAG_ADDRESS
	.short MULTIBOOT_HEADER_TAG_OPTIONAL
	.long address_tag_end - address_tag_start
	// header_addr
	.long	multiboot_header
	// load_addr
	.long	0x100000
	// load_end_addr
	.long	__load_end
	// bss_end_addr
	.long	__bss_end
address_tag_end:
	.align	8
entry_address_tag_start:
	.short MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
	.short MULTIBOOT_HEADER_TAG_OPTIONAL
	.long entry_address_tag_end - entry_address_tag_start
	// entry_addr
	.long _start
entry_address_tag_end:
#endif // __ELF__
/*
	.align	8
framebuffer_tag_start:
	.short MULTIBOOT_HEADER_TAG_FRAMEBUFFER
	.short MULTIBOOT_HEADER_TAG_OPTIONAL
	.long framebuffer_tag_end - framebuffer_tag_start
	.long 800
	.long 600
	.long 32
framebuffer_tag_end:
*/
	.align	8
	.short MULTIBOOT_HEADER_TAG_END
	.short 0
	.long 8
multiboot_header_end:

_start:
	cli
	cld

	movl  $_KSTACK_PHY, %esp 			// Initialize the stack pointer.
	push $0
	popf

	// Push the pointer to the Multiboot information structure.
	pushl $0xffff8000;
	pushl %ebx 			// mboot_info 64 rsp

	// Push the magic value.
	pushl $0;
	pushl %eax 			// mboot_magic 64 rsp

 	// build the page table.
 	#define PAGE_PRESENT    (1 << 0)
	#define PAGE_WRITE  	(1 << 1)
	#define PAGE_PS 		(1 << 7)

	movl $(_PT3_PHY + PAGE_PRESENT + PAGE_WRITE), _PT4_PHY   		    // set PT4[0]
	movl $(_PT3_PHY + PAGE_PRESENT + PAGE_WRITE), _PT4_PHY + 256*8   	// set PT4[256]	map kernel from 64T
	movl $(_PT2_PHY + PAGE_PRESENT + PAGE_WRITE), _PT3_PHY   		    // set PT3[0]
    movl $( PAGE_PS + PAGE_PRESENT + PAGE_WRITE), _PT2_PHY			    // set PT2[0] => 0~2M

    movl $_PT4_PHY, %eax        // set page dir base
    movl %eax, %cr3

    movl %cr4, %eax             // enable pae, pse, pge, osfxsr of CR4
    orl $0x2b0, %eax
    mov %eax, %cr4

    movl $0xC0000080, %ecx      // enable long mode and SYSCALL/SYSRET of EFER
    rdmsr
    orl $0x101, %eax
    wrmsr

    movl %cr0, %eax                 // enabling PG, NE, MP
    orl $0x80000022, %eax
    movl %eax, %cr0

    lgdt gdt32_ptr                  // load gdt64
	jmp $0x8, $long_mode

.code64
long_mode:

    lgdt gdt64_ptr                  // load gdt64
	mov  $0x10, %ax
	mov  %ax, %ds
	mov  %ax, %es
	mov  %ax, %fs
	mov  %ax, %gs
	mov  %ax, %ss

	mov  $tss64_start, %rax         // set TSS64 base
	mov  %ax, tss64_base
	mov  $0x30, %ax
	ltr  %ax

	xor  %rbp, %rbp                 // clear rbp
	pop  %rcx                       // get mboot_magic
	pop  %rdx                       // get mboot_info

	movabs $_KSTACK, %rsp
	subq  $32, %rsp             // reserv stack space for call C

	movabs $kmain, %rax
	movl $0, _PT4_PHY
	call *%rax

.data

gdt32_ptr:
	.word gdt32_end - gdt32_start - 1    // GDT limit
	.long gdt32_start   		         // linear,physical address of GDT

gdt32_start:
	.quad 0x0			 				 // NULL
	.quad 0x002f9a000000ffff			 // 0x8  KCode64
gdt32_end:

gdt64_ptr:
	.word gdt64_end - gdt64_start - 1    // GDT limit
	.long gdt64_start   		         // linear,physical address of GDT
    .long 0xffff8000

gdt64_start:
	.quad 0x0			 				// NULL
	.quad 0x002f9a000000ffff			// 0x8  KCode64
	.quad 0x00cf92000000ffff            // 0x10 KData64
	.quad 0x002ffa000000ffff			// 0x18 UCode64
	.quad 0x00cff2000000ffff            // 0x20 UData64
	.quad 0x002ffa000000ffff			// 0x28 UCode64

	.word tss64_end - tss64_start -1	// 0x30 TSS64, tss limit[15:0]
    tss64_base:
	.word 0								// Base Address[15:0]
	.byte 0x10							// Base Address[23:16] 0x10xxxx
    .byte 0x89							// P DPL 0 Type(9):TSS64
    .byte 0								// G AVL Segment Limit[19:16]
	.byte 0								// Base Address[31:24]
	.long 0xffff8000					// Base Address[63:32]
	.long 0								// reserved
gdt64_end:

tss64_start:
	.long 0                     // pad0

.global tss64_rsp0
tss64_rsp0:

	.quad _KSTACK               // rsp0
	.rept   33                  // pad
    .long   0
	.endr
tss64_end:
