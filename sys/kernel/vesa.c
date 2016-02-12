#include "kernel.h"
#include "printk.h"
#include "multiboot2.h"

int* fb_base;
int fb_pitch;
int fb_width, fb_height, fb_bpp;

void fill_rect(int x, int y, int w, int h, int color)
{
    for(int i =y; i< y + h; i++)
    {
        for(int j =x; j< x + w; j++)
        {
            fb_base[i * fb_width + j] = color;
        }
    }
}

void invert16x4(void* ptr);

int cursor[16*16*4];

void restore_cursor(int x, int y)
{
    int* ptr = fb_base + y * fb_width + x;
    int* sav = cursor;

    for(int i =y; i< y + 16; i++)
    {
        ASM("rep movsd"::"S"(sav),"D"(ptr),"c"(16));
        ptr += fb_width;
        sav += 16*4;
    }
}

void save_cursor(int x, int y)
{
    int* ptr = fb_base + y * fb_width + x;
    int* sav = cursor;

    for(int i =y; i< y + 16; i++)
    {
        ASM("rep movsd"::"S"(ptr),"D"(sav),"c"(16));
        ptr += fb_width;
        sav += 16*4;
    }
}

void show_cursor(int x, int y)
{
    int* ptr = fb_base + y * fb_width + x;

    for(int i =y; i< y + 16; i++)
    {
        ASM("rep stosl"::"a"(0xffffff),"D"(ptr),"c"(16));
        ptr += fb_width;
    }
}
/*
void fill_invert(int x, int y, int w, int h)
{
    int* ptr = fb_base + y * fb_width + x;

    for(int i =y; i< y + 16; i++)
    {
        ASM("rep movsd"::"S"(0xffffff),"D"(ptr),"c"(16));


        ASM("rep stosl"::"a"(0xffffff),"D"(ptr),"c"(16));
        ptr += fb_width;
        //invert16x16(ptr);
    }
}
*/
void vesa_init(void* info)
{
	ENTER();

    fb_base = (int*)PHY2VIR(multiboot_framebuffer->common.framebuffer_addr);
    fb_width = multiboot_framebuffer->common.framebuffer_width;
    fb_height = multiboot_framebuffer->common.framebuffer_height;
    fb_bpp = multiboot_framebuffer->common.framebuffer_bpp;
    fb_pitch = multiboot_framebuffer->common.framebuffer_pitch;

    LOG("Framebuffer:%dx%dx%dbpp, base:0x%x\n", fb_width, fb_height, fb_bpp, fb_base);

    fill_rect(100, 100, 20,20, 0xff0000);
    fill_rect(100, 200, 20,20, 0xff00);
    fill_rect(100, 300, 20,20, 0xff);

	LEAVE();
}
