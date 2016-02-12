
#ifndef PORT_H
#define PORT_H

#include "kernel.h"

static inline u8 inb(u16 port)
{
	u8 rv;
	ASM( "inb %1, %0" : "=a" ( rv ) : "dN" ( port ) );
	return rv;
}

static inline u16 inw(u16 port)
{
	u16 rv;
	ASM( "inw %1, %0" : "=a" ( rv ) : "dN" ( port ) );
	return rv;
}

static inline u32 ind(u16 port)
{
	u32 rv;
	ASM( "inl %1, %0" : "=a" ( rv ) : "dN" ( port ) );
	return rv;
}

static inline void outb(u16 port, u8 data)
{
	ASM( "outb %1, %0" : : "dN" ( port ), "a" ( data ) );
}

static inline void outw(u16 port, u16 data)
{
	ASM( "outw %1, %0" : : "dN" ( port ), "a" ( data ) );
}

static inline void outd(u16 port, u32 data)
{
	ASM( "outl %1, %0" : : "dN" ( port ), "a" ( data ) );
}

static inline void insw(u16 port, void *data, int flag)
{
	ASM("cld;rep insw"::"d" (port),"D" (data),"c" (flag));
}

static inline void insd(u16 port, void *data, int flag)
{
	ASM("cld;rep insl"::"d" (port),"D" (data),"c" (flag));
}

static inline void outsw(u16 port, void *data, int flag)
{
	ASM("cld;rep outsw"::"d" (port),"S" (data),"c" (flag));
}

static inline void outsd(u16 port, void *data, int flag)
{
	ASM("cld;rep outsl"::"d" (port),"S" (data),"c" (flag));
}

#endif // PORT_H
