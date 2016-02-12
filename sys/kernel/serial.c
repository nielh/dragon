
#include "serial.h"
#include "port.h"
#include "isr.h"
#include "printk.h"
#include "thread.h"

/* Defines Serial Ports Base Address */
#define COM1    0x3F8
#define COM2    0x2F8
#define COM3    0x3E8
#define COM4    0x2E8

static int com2_inited = 0;
void com2_print(char* str)
{
    if(com2_inited == 0)
    {
        com2_inited = 1;
        outb(COM2 + 1, 0);         /* Turn off interrupts - Port1 */

        /*         PORT 1 - Communication Settings         */
        outb(COM2 + 3, 0x80);   /* SET DLAB ON */
        outb(COM2 + 0, 0x01);   /* Set Baud rate - Divisor Latch Low Byte */
        /* Default 0x03 =  38,400 BPS */
        /*         0x01 = 115,200 BPS */
        /*         0x02 =  57,600 BPS */
        /*         0x06 =  19,200 BPS */
        /*         0x0C =   9,600 BPS */
        /*         0x18 =   4,800 BPS */
        /*         0x30 =   2,400 BPS */
        outb(COM2 + 1, 0x00);   /* Set Baud rate - Divisor Latch High Byte */
        outb(COM2 + 3, 0x03);   /* 8 Bits, No Parity, 1 Stop Bit */
        outb(COM2 + 2, 0xC7);   /* FIFO Control Register */
        outb(COM2 + 4, 0x0B);   /* Turn on DTR, RTS, and OUT2 */
    }

    while(*str)
    {
        while ((inb(COM2 + 5) & 0x20) == 0)
            ASM("pause");

        outb(COM2, *str++);
    }
}

// Receiving data
static int rx_is_full()
{
    return inb(COM1 + 5) & 1;
}

char serial_getc()
{
    while (rx_is_full() == 0)
		ASM("pause");

    return inb(COM1);
}

// Sending data
static int tx_is_empty()
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char a)
{
    while (tx_is_empty() == 0)
        ASM("pause");

    outb(COM1,a);
}

void handle_exception (void* data, registers_t* regs);

static void serial_irq(void* data, registers_t* regs)
{
    regs->int_no = 3; // ctrl + c
    handle_exception(data,regs );
}

void serial_init()
{
    ENTER();

    outb(COM1 + 1, 0);         /* Turn off interrupts - Port1 */

    /*         PORT 1 - Communication Settings         */
    outb(COM1 + 3, 0x80);   /* SET DLAB ON */
    outb(COM1 + 0, 0x01);   /* Set Baud rate - Divisor Latch Low Byte */
    /* Default 0x03 =  38,400 BPS */
    /*         0x01 = 115,200 BPS */
    /*         0x02 =  57,600 BPS */
    /*         0x06 =  19,200 BPS */
    /*         0x0C =   9,600 BPS */
    /*         0x18 =   4,800 BPS */
    /*         0x30 =   2,400 BPS */
    outb(COM1 + 1, 0x00);   /* Set Baud rate - Divisor Latch High Byte */
    outb(COM1 + 3, 0x03);   /* 8 Bits, No Parity, 1 Stop Bit */
    outb(COM1 + 2, 0xC7);   /* FIFO Control Register */
    outb(COM1 + 4, 0x0B);   /* Turn on DTR, RTS, and OUT2 */

    /* COM1 - 0x0C */
    /* COM2 - 0x0B */
    /* COM3 - 0x0C */
    /* COM4 - 0x0B */
    outb(COM1 + 1, 0x01);   /* Interrupt when data received */

    irq_register(IRQ_COM1, serial_irq, NULL);
    irq_enable(IRQ_COM1);

    LEAVE();
}
