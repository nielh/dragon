#include "kernel.h"
#include "printk.h"
#include "libc.h"
#include "isr.h"
#include "object.h"
#include "idt.h"
#include "vsnprintf.h"
#include "port.h"

#define ESC		128
#define CLOCK	129
#define SHIFT	130
#define CTRL	131
#define F1		132
#define F2		133
#define F3		134
#define F4		135
#define F5		136
#define F6		137
#define F7		138
#define F8		139
#define F9		140
#define F10		141
#define F11		142
#define F12		143
#define ALT		144
#define WAKUP	145
#define SLEEP	146
#define POWER	147
#define PRTSCR	149
#define SLOCK	150
#define PBREAK	151
#define INSERT	152
#define HOME	153
#define PGUP	154
#define DELETE	155
#define END		156
#define PGDN	157
#define UP		158
#define DOWN	159
#define LEFT	160
#define RIGHT	161
#define	NLOCK	162
#define MID		163

unsigned char keymap[3][128] =
{
    {
        '?',
        ESC,
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        '0',
        '-',
        '=',
        '\b',
        '\t',
        'q',
        'w',
        'e',
        'r',
        't',
        'y',
        'u',
        'i',
        'o',
        'p',
        '[',
        ']',
        '\n',
        CTRL,
        'a',
        's',
        'd',
        'f',
        'g',
        'h',
        'j',
        'k',
        'l',
        ';',
        '\'',
        '`',
        SHIFT,
        '\\',
        'z',
        'x',
        'c',
        'v',
        'b',
        'n',
        'm',
        ',',
        '.',
        '/',
        SHIFT,
        '*',
        ALT,
        ' ',
        CLOCK,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        NLOCK,
        SLOCK,
        HOME,
        UP,
        PGUP,
        '-',
        LEFT,
        MID,
        RIGHT,
        '+',
        END,
        DOWN,
        PGDN,
        INSERT,
        DELETE,
        '\n',
        '?',
        '?',
        F11,
        F12,
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?'
    },
    {
        '?',
        ESC,
        '!',
        '@',
        '#',
        '$',
        '%',
        '^',
        '&',
        '*',
        '(',
        ')',
        '_',
        '+',
        '\b',
        '\t',
        'Q',
        'W',
        'E',
        'R',
        'T',
        'Y',
        'U',
        'I',
        'O',
        'P',
        '{',
        '}',
        '\n',
        CTRL,
        'A',
        'S',
        'D',
        'F',
        'G',
        'H',
        'J',
        'K',
        'L',
        ':',
        '"',
        '~',
        SHIFT,
        '|',
        'Z',
        'X',
        'C',
        'V',
        'B',
        'N',
        'M',
        '<',
        '>',
        '?',
        SHIFT,
        '*',
        ALT,
        ' ',
        CLOCK,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        NLOCK,
        SLOCK,
        HOME,
        UP,
        PGUP,
        '-',
        LEFT,
        MID,
        RIGHT,
        '+',
        END,
        DOWN,
        PGDN,
        INSERT,
        DELETE,
        '\n',
        '?',
        '?',
        F11,
        F12,
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?'
    },
    {
        '?',
        ESC,
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        '0',
        '-',
        '=',
        '\b',
        '\t',
        'q',
        'w',
        'e',
        'r',
        't',
        'y',
        'u',
        'i',
        'o',
        'p',
        '[',
        ']',
        '\n',
        CTRL,
        'a',
        's',
        'd',
        'f',
        'g',
        'h',
        'j',
        'k',
        'l',
        ';',
        '\'',
        '`',
        SHIFT,
        '\\',
        'z',
        'x',
        'c',
        'v',
        'b',
        'n',
        'm',
        ',',
        '.',
        '/',
        SHIFT,
        '*',
        ALT,
        ' ',
        CLOCK,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        NLOCK,
        SLOCK,
        '7',
        '8',
        '9',
        '-',
        '4',
        '5',
        '6',
        '+',
        '1',
        '2',
        '3',
        '0',
        '.',
        '\n',
        '?',
        '?',
        F11,
        F12,
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?',
        '?'
    }
};

static int key_mode = 0;
static char shift_flg = 0;
static char ctrl_flg = 0;
static char alt_flg = 0;
static char capslck_flg = 0;
static char numlck_flg = 0;

static char mapascii(char scancode)
{
    unsigned char code = (unsigned char) scancode;
    char ret = 0;

    if (code >= 128)
    {
        if (code == 170 || code == 182) //shift
            shift_flg = 0;
        else if (code == 157) // ctrl
            ctrl_flg = 0;
        else if (code == 184) // alt
            alt_flg = 0;
    }
    else
    {
        if (code == 42 || code == 54) //shift
            shift_flg = 1;
        else if (code == 29) // ctrl
            ctrl_flg = 1;
        else if (code == 56) // alt
            alt_flg = 1;
        else if (code == 58) // capslock
            capslck_flg = !capslck_flg;
        else if (code == 69) // numlock
            numlck_flg = !numlck_flg;
        else
        {
            if (numlck_flg)
                key_mode = 2;
            else if (shift_flg || capslck_flg)
                key_mode = 1;
            else
                key_mode = 0;

            ret = (char) keymap[key_mode][code];
        }
    }

    return ret;
}

int console_fifo_write(char ch);

static void kbd_handler(void* data, registers_t* regs)
{
    //ENTER();
    char scancode = inb(0x60);
    char ch = mapascii(scancode);

    if (ch)
    {
		console_fifo_write(ch);
		//LOG("kbd_irq:%c\n", ch);
    }

    //if (ch == '1')
    //	handle_exception(NULL, regs);
    //wm_keybd_event(ch);

    //LEAVE();
    return;
}

/*
static long kbd_proc(obj_t* this, long msg, long param[])
{
	switch (msg)
	{
	case OPEN:
		return (long) this;
	case READ:
		strcpy((char*) param[0], "kbd driver");
		return 0;
	default:
		return -1;
	}
}
obj_t kbd = {0};

int kbd_init()
{
	kbd.proc = kbd_proc;
	kbd.data = open(HANDLE_OBJ, "wm", 0);

	assert(kbd.data != NULL);

	append(HANDLE_OBJ, "kbd", &kbd);

	isr_register(ISR_KEYBOARD, kbd_handler, NULL); //

	return 0;
}
*/
int kbd_init()
{
    irq_register(IRQ_KEYBOARD, kbd_handler, NULL); //
    irq_enable(IRQ_KEYBOARD);

    return 0;
}
