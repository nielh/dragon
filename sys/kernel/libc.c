#include "kernel.h"
#include "libc.h"
#include "kmalloc.h"
#include "vga.h"
#include "timer.h"


void memcpy(void* dest, void* src, int num)
{
	int i = 0;
	while (i < num)
	{
		((char*) dest)[i] = ((char*) src)[i];
		i++;
	}
}

void memset(void* dest, s8 ch, int num)
{
	u32 i = 0;
	while (i < num)
	{
		((char*) dest)[i] = ch;
		i++;

		//vga_printf(COLOR_WHITE, COLOR_BLACK, "i=%d\n", i);
	}
}

u32 memcmp(void *str1, void *str2, int num)
{
	while (num > 0)
	{
		if (*(char*) str1 == *(char*) str2)
		{
			(char*) str1++;
			(char*) str2++;

			num--;
		}
		else
		{
			return -1;
		}
	}

	return 0;
}

void strcpy(char *des, char *src)
{
	while (*src != 0)
	{
		*des = *src;
		des++;
		src++;
	}

	*des = 0;
}

void strncpy(char *des, char *src, int flag)
{
	int index = 0;

	while (*src != 0 && index < flag)
	{
		*des = *src;
		des++;
		src++;

		index ++;
	}

	*des = 0;
}

int strlen(s8 *str)
{
	int len = 0;

	while (*str != 0)
	{
		len++;
		str++;
	}

	return len;
}

int strnlen(const char *s, int flag)
{
	const char *sc;
	for (sc = s; *sc != '\0' && flag--; ++sc)
		;
	return sc - s;
}


int strncmp(char * src, char * dest, int flag)
{
	register signed char res = 0;

	while (flag)
	{
		if ((res = *src - *dest++) != 0 || !*src++)
			break;
		flag--;
	}

	return res;
}

int strcmp(s8 *str1, s8 *str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == 0)
		{
			return 0;
		}

		str1++;
		str2++;
	}

	return -1;
}


u32 stoi(s8* str)
{
	if (str == 0 || strlen(str) == 0)
		return 0;

	u32 neg = 0;
	u32 dec = 1;

	u32 ret = 0;

	if (*str == '-')
	{
		neg = 1;
		str++;
	}

	s8 *p = str;
	while (*p != 0)
		p++;

	p--;

	while (1)
	{
		u32 i = *p - '0';

		if (i < 0 || i > 9)
			return 0;

		ret += i * dec;
		dec *= 10;

		p--;
		if (p < str)
			break;
	}

	if (neg)
		ret = -ret;

	return ret;
}

char* strchr(char* str, char ch)
{
    while(*str != ch && *str != 0)
        str ++;

    return str;
}

u32 stou(s8* str)
{
	if (str == 0 || strlen(str) == 0)
		return 0;

	u32 dec = 1;

	u32 ret = 0;

	s8 *p = str;
	while (*p != 0)
		p++;

	p--;

	while (1)
	{
		u32 i = *p - '0';

		if (i < 0 || i > 9)
			return 0;

		ret += i * dec;
		dec *= 10;

		p--;
		if (p < str)
			break;
	}

	return ret;
}

void UpperStr(char* str)
{
	while(*str)
	{
		if(*str >= 'a' && *str <= 'z')
			*str +='A' - 'a';

		str ++;
	}
}

void LowerStr(char* str)
{
	while(str)
	{
		if(*str >= 'A' && *str <= 'Z')
			*str +='a' - 'A';

		str ++;
	}
}

char *strdup(char *s)
{
	char *t;
	int len;

	if (!s)
		return 0;

	len = strlen(s);
	t = (char *) kmalloc(len + 1);
	memcpy(t, s, len + 1);
	return t;
}


void mdelay(u32 ms)
{
	u32 ticks = clock_ticks + ms;
	while (clock_ticks < ticks)
	{
		ASM("pause");
	}
}
