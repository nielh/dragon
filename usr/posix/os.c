
#include "os.h"

enum SYSCALL
{
	SYS_NULL = 0,
	SYS_OPEN,
	SYS_CLOSE,
	SYS_READP,
	SYS_WRITEP,
	SYS_IOCTL,
	SYS_EXIT,

	SYS_LOADSYSMODULE,
	SYS_PROCESSCREATE,
	SYS_THREADCREATE,
	SYS_MSLEEP,

	/*
	SYS_SENDMSG,
	SYS_GETPID,
	SYS_GETTID,
	SYS_CREATEWINDOW,
	SYS_GETMESSAGE*/
};

#define SYSCALL0(num)\
{\
	__asm__ volatile("mov %0, %%rax;"\
					 "syscall;"\
					 ::"N"(num));\
}

#define SYSCALL1(num, a0)\
{\
	__asm__ volatile("mov %0, %%rax;"\
					 "mov %1, %%r10;"\
					 "syscall;"\
					 ::"N"(num), "m"(a0));\
}

#define SYSCALL2(num, a0, a1)\
{\
	__asm__ volatile("mov %0, %%rax;"\
					 "mov %1, %%r10;"\
					 "mov %2, %%rdx;"\
					 "syscall;"\
					 ::"N"(num), "m"(a0), "m"(a1));\
}


#define SYSCALL3(num, a0, a1, a2)\
{\
	__asm__ volatile("mov %0, %%rax;"\
					 "mov %1, %%r10;"\
					 "mov %2, %%rdx;"\
					 "mov %3, %%r8;"\
					 "syscall;"\
					 ::"N"(num), "m"(a0), "m"(a1), "m"(a2));\
}


#define SYSCALL4(num, a0, a1, a2, a3)\
{\
	__asm__ volatile("mov %0, %%rax;"\
					 "mov %1, %%r10;"\
					 "mov %2, %%rdx;"\
					 "mov %3, %%r8;"\
					 "mov %4, %%r9;"\
					 "syscall;"\
					 ::"N"(num), "m"(a0), "m"(a1), "m"(a2), "m"(a3));\
}


// open call close
s64 Open(char* path, s64 flag, s64 mode)
{
	SYSCALL3(SYS_OPEN, path, flag, mode);
}

s64 Close(s64 fd)
{
	SYSCALL1(SYS_CLOSE, fd);
}

// read write
s64 Read(s64 fd, void* buf, s64 size, s64 pos)
{
	SYSCALL4(SYS_READP, fd, buf, size, pos);
}

s64 Write(s64 fd, void* buf, s64 size, s64 pos)
{
	SYSCALL4(SYS_WRITEP, fd, buf, size, pos);
}

s64 Ioctl(s64 fd, s64 cmd, void* pArg, s64 lArg)
{
	SYSCALL4(SYS_IOCTL, fd, cmd, pArg, lArg);
}

s64 Exit(s64 code)
{
	SYSCALL1(SYS_EXIT, code);
}

s64 LoadSysModule(char* cmdline)
{
	SYSCALL1(SYS_LOADSYSMODULE, cmdline);
}

s64 ProcessCreate(char* cmdline)
{
	SYSCALL1(SYS_PROCESSCREATE, cmdline);
}

s64 ThreadCreate(thread_fnc fnc, void* param)
{
	SYSCALL2(SYS_THREADCREATE, fnc, param);
}

s64 MSleep(s64 ms)
{
	SYSCALL1(SYS_MSLEEP, ms);
}
/*
s64 printk(char* str)
{
	SYSCALL1(SYS_PRINTK, str);
}

s64 getpid()
{
	SYSCALL0(SYS_GETPID);
}

s64 gettid()
{
	SYSCALL0(SYS_GETTID);
}


struct WndProcMap_t
{
	_u32 handle;
	WndProc proc;
};

struct WndProcMap_t WndProcMap[32] =
				{
					{ 0 } };

static int index = 0;

// edi(D), esi(S), ebp, esp, ebx(b), edx(d), ecx(c), eax(a)
s64 CreateWindow(s64 x, s64 y, s64 w, s64 h, s64 style, WndProc proc)
{
	int ret;
	__asm__ volatile("int $63"
					: "=a" (ret)
					: "0" (SYS_CREATEWINDOW),
					"b" (x), "c" (y), "d" (w), "S" (h), "D" (style));

	if (ret != 0)
	{
		WndProcMap[index].handle = ret; // window_t*
		WndProcMap[index++].proc = proc;
	}

	return index;
}

s64 GetMessage(msg_t* msg)
{
	int ret;
	__asm__ volatile("int $63"
					: "=a" (ret)
					: "0" (SYS_GETMESSAGE),
					"b" (msg));

	int index = 0;
	while (1)
	{
		if (WndProcMap[index].handle == 0)
			break;

		if (WndProcMap[index].handle == msg->wnd)
		{
			msg->wnd = index;
			return 1;
		}

		index++;
		break;
	}

	return 0;
}

s64 DispatchMsg(msg_t* msg)
{
	if (msg->wnd <= 0)
		return -1;

	return WndProcMap[msg->wnd].proc(msg->wnd, msg->msgid, msg->arg0, msg->arg1);
}

s64 DefWndProc(s64 wnd, s64 msgid, s64 arg0, s64 arg1)
{
	return 0;
}
*/
