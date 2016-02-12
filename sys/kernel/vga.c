
#include "vga.h"
#include "port.h"

#define BUF_START 		((unsigned short*)(0xB8000 + KERNEL_START))
#define BUF_LENGTH 		(VGA_WIDTH * VGA_HEIGHT)

#define BACKGROUND 		0x0F20 // ' '

// Video Card I/O ports
#define TEXT_INDEX	0x03D4
#define TEXT_DATA	0x03D5

// Video Card Index Registers
#define TEXT_CURSOR_LO	0x0F
#define TEXT_CURSOR_HI	0x0E

static int _cursor_x = 0;
static int _cursor_y = 0;

void vga_cursor(int x, int y)
{
    if(x < 0 || x >= VGA_WIDTH)
        return;

    if(y < 0 || y >= VGA_HEIGHT)
        return;

    _cursor_x = x;
    _cursor_y = y;

    short pos = y * VGA_WIDTH + x; // �趨���λ��
    outb(TEXT_INDEX, TEXT_CURSOR_LO); // access lo cursor data reg
    outb(TEXT_DATA, (char) pos);
    outb(TEXT_INDEX, TEXT_CURSOR_HI); // access hi cursor data reg
    outb(TEXT_DATA, (char) (pos >> 8));
}

static char _color = 0x0F; // ��ɫ������ ��ɫǰ��

void vga_color(char bgcolor, char fgcolor)
{
    _color = (bgcolor << 4)|(fgcolor & 0xF); // ����λλ����ɫ�� ����λλǰ��ɫ
}

static void scroll_up_line()
{
    // copy to prev line
    for (int i = VGA_WIDTH; i < BUF_LENGTH; i++)
    {
        BUF_START[i - VGA_WIDTH] = BUF_START[i];
    }

    // clear last line
    for (int i = BUF_LENGTH - VGA_WIDTH; i < BUF_LENGTH; i++)
    {
        BUF_START[i] = BACKGROUND;
    }
}

void vga_print(char *str)
{
    int x = _cursor_x;
    int y = _cursor_y;

    while (*str)
    {
        switch(*str)
        {
        case '\n':
            x = 0;
            y++;
            break;

        case '\t':
        {
            int x_end = ((x / 8) + 1) * 8; // align to 8 byte
            while(x < x_end) // ��հ��ַ�
            {
                BUF_START[y * VGA_WIDTH + x] = (_color << 8) | ' ';
                x++;
            }
            break;
        }
        case '\xcc': // ��ɫ����
            str++;
            _color = *(str);
            break;

        default:
            BUF_START[y * VGA_WIDTH + x] = (_color << 8) | (*str);
            x++;
        }

        str++;

        if(x >= VGA_WIDTH) // ����
        {
            x = 0;
            y ++;
        }

        if (y >= VGA_HEIGHT) // ����һ��
        {
            scroll_up_line();
            y = VGA_HEIGHT - 1;
        }
    }

    vga_cursor(x, y);
}

void vga_clear(char bgcolor)
{
    for (int i = 0; i < BUF_LENGTH; i++)
    {
        BUF_START[i] = bgcolor | ' ';
    }
}
