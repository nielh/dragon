#include "kernel.h"
#include "isr.h"
#include "port.h"
#include "printk.h"

#define MOUSE_WAIT_DATA       0 //Waits for the mouse to respond
#define MOUSE_WAIT_SIGNAL     1 //Waits for the mouse to enter a "receive/wait for signal" state

/*
static MOUSE_STATUS mosue_current =
{ 0 };
static MOUSE_STATUS mosue_last =
{ 0 };


static void mouse_dpc(void* arg0, void* arg1, void* arg2)
{
	mosue_last = mosue_current;

	char status = (char) ((int) arg0);
	char mouse_x = (char) ((int) arg0 >> 8);
	char mouse_y = (char) ((int) arg0 >> 16);
	mouse_y = -mouse_y;

	if (mouse_x > 5 || mouse_x < -5)
		mouse_x *= 2;

	if (mouse_y > 5 || mouse_y < -5)
		mouse_y *= 2;

	mosue_current.LButton = status & 1;
	mosue_current.MButton = (status & 2) >> 1;
	mosue_current.RButton = (status & 4) >> 2;

	//int dx = (byte0 & 0x10) ? (byte1 - 256) : byte1;
	//int dy = (byte0 & 0x20) ? (byte2 - 256) : byte2;
	//dy = -dy;
	//printk("byte1 = %d byte2=%d\n", byte1, byte2 );

	//printk("=dx=%d =dy=%d\n", mouse_x, mouse_y);

	mosue_current.X += mouse_x;
	mosue_current.Y += mouse_y;

	if (mosue_current.X < 0)
		mosue_current.X = 0;

	if (mosue_current.X > 800)
		mosue_current.X = 800;

	if (mosue_current.Y < 0)
		mosue_current.Y = 0;

	if (mosue_current.Y > 600)
		mosue_current.Y = 600;

	static void* desk = 0;

	if (desk == 0)
	{
		desk = _(ROOT, SEARCH, "Object/Desk", 0, 0);
	}

	_(desk, MOUSE, &mosue_current, &mosue_last, 0);
}
*/

void mouse_event(int data[]);

static void mouse_handler(void* data, registers_t* regs)
{
	static int mouse_cycle = -1;
	static int mouse_byte[3] =	{ 0 };

	if (mouse_cycle == -1)
	{
		inb(0x60);
		mouse_cycle = 0;
		return;
	}

	mouse_byte[mouse_cycle++] = (char)inb(0x60);

	if (mouse_cycle == 3)
	{
		mouse_cycle = 0;

		// the mouse only sends information about overflowing, do not care about it and return
		if ((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40))
			return;

		//int data = (mouse_byte[2] << 16)|(mouse_byte[1] << 8)|mouse_byte[0] ;
		mouse_event(mouse_byte);
		//printk("mouse: dy=%d dx=%d btn=%d\n", mouse_byte[2], mouse_byte[1], mouse_byte[0]);
	}
}

static void mouse_wait(char a_type) //unsigned char
{
	unsigned int _time_out = 100000; //unsigned int
	if (a_type == MOUSE_WAIT_DATA)
	{
		while (_time_out--) //Data
		{
			if ((inb(0x64) & 1) == 1)
			{
				return;
			}
		}
		return;
	}
	else
	{
		while (_time_out--) //Signal
		{
			if ((inb(0x64) & 2) == 0)
			{
				return;
			}
		}
		return;
	}
}

static void mouse_write(char a_write) //unsigned char
{
	//Wait to be able to send a command
	mouse_wait(MOUSE_WAIT_SIGNAL);
	//Tell the mouse we are sending a command
	outb(0x64, 0xD4);
	//Wait for the final part
	mouse_wait(MOUSE_WAIT_SIGNAL);
	//Finally write
	outb(0x60, a_write);
}

static u8 mouse_read()
{
	//Get's response from mouse
	mouse_wait(MOUSE_WAIT_DATA);
	return inb(0x60);
}

void mouse_init()
{
	mouse_write(0xFF); //reset
	if (mouse_read() != 0xFA)
	{
		printk("ps2m: no mouse detected!\n");
		return;
	}
	mouse_read();
	if (mouse_read() != 0x00)
		panic("ps2m: detected an unknown mouse!\n");

	printk("ps2m: detected standard PS/2 mouse.\n");

	//Enable the auxiliary mouse device
	mouse_wait(MOUSE_WAIT_SIGNAL);
	outb(0x64, 0xA8);

	//Enable the interrupts
	mouse_wait(MOUSE_WAIT_SIGNAL);
	outb(0x64, 0x20);
	mouse_wait(MOUSE_WAIT_DATA);
	unsigned char status = (inb(0x60) | 2);
	mouse_wait(MOUSE_WAIT_SIGNAL);
	outb(0x64, 0x60);
	mouse_wait(MOUSE_WAIT_SIGNAL);
	outb(0x60, status);

	//Tell the mouse to use default settings
	mouse_write(0xF6);
	mouse_read(); //Acknowledge

	//Enable the mouse
	mouse_write(0xF4);
	mouse_read(); //Acknowledge

	//Setup the mouse handler
    irq_register(IRQ_MOUSER, mouse_handler, NULL); //
    irq_enable(IRQ_MOUSER);
}

