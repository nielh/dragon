
#include "libc.h"

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
