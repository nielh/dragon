
#ifndef OS_H
#define OS_H

typedef unsigned char 		u8;
typedef unsigned short 		u16;
typedef unsigned int 		u32;
typedef unsigned long long 	u64;

typedef char 				s8;
typedef short 				s16;
typedef int 				s32;
typedef long long 			s64;



#define nil				0
//#define NULL			0

#define USER_ENV                (char*)0x7fffffc01000ULL   // 256T -4M + 4K
#define USER_ARG                (char*)0x7fffffc00000ULL   // 256T -4M

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

s64 Open(char* path, s64 flag, s64 mode);
s64 Close(s64 fd);
s64 Read(s64 fd, void* buf, s64 size, s64 pos);
s64 Write(s64 fd, void* buf, s64 size, s64 pos);
s64 Ioctl(s64 fd, s64 cmd, void* pArg, s64 lArg);
s64 Exit(s64 code);


s64 LoadSysModule(char* cmdline);
s64 ProcessCreate(char* cmdline);

typedef int (*thread_fnc)(void* param);
s64 ThreadCreate(thread_fnc fnc, void* param);
s64 MSleep(s64 ms);

s64 sendmsg(s64 obj, s64 msg, s64 src, s64 dst, s64 len);

//s64 getpid();
//s64 gettid();

typedef s64 (*WndProc)(s64 wnd, s64 msgid, s64 arg0, s64 arg1);

s64 CreateWindow(s64 x, s64 y, s64 w, s64 h, s64 style, WndProc proc);
s64 DefWndProc(s64 wnd, s64 msgid, s64 arg0, s64 arg1);
s64 GetMessage(msg_t* msg);
s64 DispatchMsg(msg_t* msg);

//
// Spawn flags
//

#endif
