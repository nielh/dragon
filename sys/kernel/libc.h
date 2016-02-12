
#ifndef LIBC_H
#define LIBC_H

#include "kernel.h"

void memcpy(void *dest, void *src, int num);
void memset(void *dest, s8 ch, int num);
u32 memcmp(void *str1, void *str2, int num);

void strcpy(s8 *des, s8 *src);
void strncpy(char *des, char *src, int flag);
int strlen(s8 *str);
int strnlen(const char *s, int flag);
int strcmp(s8 *str1, s8 *str2);
int strncmp(char * src, char * dest, int flag);
char* strchr(char* str, char ch);

u32 stoi(s8* str);
u32 stou(s8* str);
char *strdup(char *s);

void UpperStr(char* str);
void LowerStr(char* str);

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) < (b)) ? (b) : (a))

void mdelay(u32 ms);

#endif //
