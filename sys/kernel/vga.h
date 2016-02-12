
#ifndef VGA_H
#define VGA_H

#define VGA_WIDTH 		80
#define VGA_HEIGHT 		25

enum vga_color
{
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 0xA,
	COLOR_LIGHT_CYAN = 0xB,
	COLOR_LIGHT_RED = 0xC,
	COLOR_LIGHT_MAGENTA = 0xD,
	COLOR_LIGHT_BROWN = 0xE,
	COLOR_WHITE = 0xF
};

void vga_clear(char bgcolor);
void vga_cursor(int x, int y);
void vga_color(char bgcolor, char fgcolor);
void vga_print(char *str);

//void vga_printf(char fgcolor, char bgcolor, char* fmt, ...);
//void kprintf(char* fmt, ...);

#endif
