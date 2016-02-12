#ifndef KERNEL_H
#define KERNEL_H

#include "user.h"

#define TRUE			1
#define FALSE			0

#define ASM				__asm__ __volatile__
#define PACKED			__attribute__( (packed) )

#define KERNEL_END 	        0xffffffffff000000ULL		    // 16M reserv
#define KERNEL_START 	    0xffff800000000000ULL

#define USER_END 	            0x800000000000ULL   // 256T
#define USER_ENV                0x7fffffc01000ULL   // 256T -4M + 4K
#define USER_ARG                0x7fffffc00000ULL   // 256T -4M

#define VMA_END                 0x7fffffc00000ULL	// 256T -4M
#define VMA_START                     0x400000ULL	// 4M



#define IS_PTR(p)       ((u64)(p) > KERNEL_START && (u64)(p) < KERNEL_END)

#define VIR2PHY(a)		((u64)(a) & (~KERNEL_START))
#define PHY2VIR(a)		((u64)(a) | KERNEL_START)

#define SIZ_4G 			0x100000000
#define SIZ_1G 			0x40000000
#define SIZ_2M 			0x200000
#define SIZ_1M 			0x100000
#define SIZ_4K 			0x1000
#define SIZ_1K 			0x400

#define EXPORT 			__declspec(dllexport)

#define         PUSIF()       u64 rflag; ASM("pushf; pop %0; cli;":"=m"(rflag))
#define         POPIF()        ASM("push %0; popf"::"m"(rflag))

#define offsetof(TYPE, MEMBER) ((char*) &((TYPE *)0)->MEMBER)
/*
#define container_of(ptr, type, member) ({            \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) );})
*/
#define container_of(ptr, type, member) (type *)( (char *)ptr - offsetof(type,member) )

#endif // KERNEL_H
