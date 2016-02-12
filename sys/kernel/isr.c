#include "kernel.h"
#include "printk.h"
#include "libc.h"
#include "isr.h"
#include "port.h"

#define ISR_MAX		48

#define PIC1_CMD	    0x20
#define PIC1_DATA	    0x21
#define PIC2_CMD    	0xA0
#define PIC2_DATA	    0xA1

static struct isr_t
{
    isr_fnc fnc;
    void* data;

} isr_tab[ISR_MAX];

static char *isr_name[] =
{
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint Exception",
    "Into Detected Overflow Exception",
    "Out of Bounds Exception",
    "Invalid Opcode Exception",
    "No Coprocessor Exception",
    "Double Fault Exception",
    "Coprocessor Segment Overrun Exception",
    "Bad TSS Exception",
    "Segment Not Present Exception",
    "Stack Fault Exception",
    "General Protection Fault Exception",
    "Page Fault Exception",
    "Unknown Interrupt Exception",
    "Coprocessor Fault Exception",
    "Alignment Check Exception (486+)",
    "Machine Check Exception (Pentium/586+)",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "Reserved Exceptions",
    "IRQ0 (timer)",
    "IRQ1 (keyboard)",
    "IRQ2",
    "IRQ3 (com2)",
    "IRQ4 (com1)",
    "IRQ5",
    "IRQ6 (fdc)",
    "IRQ7 (par)",
    "IRQ8 (rtc)",
    "IRQ9",
    "IRQ10",
    "IRQ11",
    "IRQ12(mouse)",
    "IRQ13",
    "IRQ14 (hdc1)",
    "IRQ15 (hdc2)"
};

void isr_handler(registers_t* regs)
{
    assert(regs->int_no < ISR_MAX);

    if(isr_tab[regs->int_no].fnc == NULL)
    {
        panic("isr[%d](%s) is NULL\n", regs->int_no, isr_name[regs->int_no]);
    }
    else
    {
        void* data = isr_tab[regs->int_no].data;
        isr_tab[regs->int_no].fnc(data, regs); // call isr handler
    }
}

int isr_register(u8 num, isr_fnc fnc, void* data)
{
    //ENTER();
    assert(num < ISR_MAX);
    assert(fnc != 0);

    if (isr_tab[num].fnc != 0)
    {
        printk("isr already register (%d, %d):%s\n", num, data, isr_name[num]);
        return -1;
    }

    //DEBUG("isr_register(%d)", num);
    isr_tab[num].fnc = fnc;
    isr_tab[num].data = data;

    //EXIT();
    return 0;
}

void irq_disable(u8 IRQline)
{
    u16 port;
    u8 value;

    if(IRQline < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);
}

void irq_enable(u8 IRQline)
{
    if(IRQline < 8)
    {
		u8 value = inb(PIC1_DATA) & ~(1 << IRQline);
		outb(PIC1_DATA, value);
    }
    else
    {
		u8 value = inb(PIC2_DATA) & ~(1 << (IRQline -8));
		outb(PIC2_DATA, value);

		value = inb(PIC1_DATA) & ~(1 << 2);
		outb(PIC1_DATA, value);
    }
}

int irq_register(u8 num, isr_fnc fnc, void* data)
{
    return isr_register(num + 32, fnc, data);
}

int isr_init()
{
    memset(isr_tab, 0, sizeof(isr_tab));

        /* ICW1 */
    outb(PIC1_CMD, 0x11); /* Master port A */
    outb(PIC2_CMD, 0x11); /* Slave port A */

    /* ICW2 */
    outb(PIC1_DATA, 0x20); /* Master offset of 0x20 in the IDT */
    outb(PIC2_DATA, 0x28); /* Master offset of 0x28 in the IDT */

    /* ICW3 */
    outb(PIC1_DATA, 0x04); /* Slaves attached to IR line 2 */
    outb(PIC2_DATA, 0x02); /* This slave in IR line 2 of master */

    /* ICW4 */
    outb(PIC1_DATA, 0x02); /* Set as master */
    outb(PIC2_DATA, 0x02); /* Set as slave */

    irq_disable(IRQ_TIMER);// disable timer

	outb(PIC1_DATA, 0xFF);   // disable all interrupt
	outb(PIC2_DATA, 0xFF);

    return 0;
}
