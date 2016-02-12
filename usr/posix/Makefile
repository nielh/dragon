
TARGET = ..\..\bin\posix.a
OBJS = assert.o ctype.o printf.o scanf.o stdlib.o string.o strtod.o strtol.o vsprintf.o time.o stdio.o

CFLAGS = -c -Wall -O0 -g -std=c99 -ffreestanding -nostdlib -mno-red-zone -mno-ms-bitfields -I..\include\posix
CC = gcc

$(TARGET): $(OBJS)
	ar cqs $@ $^

%.o:%.c
	$(CC) $(CFLAGS) $<

.PHONY : clean
clean :
	del /q $(TARGET) $(OBJS)
