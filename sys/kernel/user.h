
#ifndef USER_H
#define USER_H

typedef unsigned char 		u8;
typedef unsigned short 		u16;
typedef unsigned int 		u32;
typedef unsigned long long 	u64;

typedef char 				s8;
typedef short 				s16;
typedef int 				s32;
typedef long long 			s64;

#define nil				0
#define NULL			0

enum MSG
{
	MSG_PAINT, MSG_CLOS, MSG_KEYDOWN, MSG_MOUSE
};

typedef struct msg_t
{
	u32 wnd;
	u32 msgid;
	u32 arg0;
	u32 arg1;

} msg_t;

#endif // #define USER_H



