
#ifndef ISR_H
#define ISR_H

#include "kernel.h"

typedef struct registers_t
{
	u64 rax, rbx, rcx, rdx, rsi, rdi;
	u64 r8,r9,r10,r11,r12,r13,r14,r15;
	u64 int_no;
	u64 rbp;
	u64 rip, cs, rflags, rsp, ss; // Pushed by the processor automatically.

} registers_t;

typedef void (*isr_fnc)(void* data, registers_t* regs);

#define ISR_PAGE_FALUT	14


#define IRQ_TIMER 		0
#define IRQ_KEYBOARD 	1
#define IRQ_COM1		4
#define IRQ_MOUSER 		12
#define IRQ_HDC0		14
#define IRQ_HDC1 		15

int isr_init();
int isr_register(u8 num, isr_fnc fnc, void* data);
int irq_register(u8 num, isr_fnc fnc, void* data);
void irq_disable(u8 IRQline);
void irq_enable(u8 IRQline);

#endif //
