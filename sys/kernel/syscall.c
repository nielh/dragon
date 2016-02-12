#include "syscall.h"
#include "printk.h"
#include "isr.h"
#include "thread.h"
#include "process.h"
#include "msr.h"
#include "kmalloc.h"
#include "timer.h"

s64 sys_open(char* path, s64 flag, s64 mode)
{
	void* file;

	if (path[0] == '/')
		file = open(HANDLE_ROOT, &path[1], flag, mode);
	else
		file = open(thread_current->process->current_dir, path, flag, mode);

		assert(IS_PTR(file));

	if(IS_PTR(file))
        return alloc_fd(thread_current->process, file);
    else
		return (s64)file;
}

s64 sys_close(s64 fd)
{
	if (fd < 0 || fd > MAX_FD)
		return -1;

	void* file = thread_current->process->fds[fd];
	close(file);
	thread_current->process->fds[fd] = NULL;

	return 0;
}

s64 sys_readp(s64 fd, void* buf, s64 len, s64 pos)
{
	if (fd < 0 || fd > MAX_FD)
		return -1;

	void* file = thread_current->process->fds[fd];
	if(file == NULL)
		return -2;

	return read(file, buf, len, pos);
}

s64 sys_writep(s64 fd, void* buf, s64 len, s64 pos)
{
	if (fd < 0 || fd > MAX_FD)
		return -1;

	void* file = thread_current->process->fds[fd];
	if(file == NULL)
		return -2;

	return write(file, buf, len, pos);
}

s64 sys_ioctl(s64 fd, s64 cmd, void* pArg, s64 lArg)
{
	if (fd < 0 || fd > MAX_FD)
		return -1;

	void* file = thread_current->process->fds[fd];
	if(file == NULL)
		return -2;

	return ioctl(file, cmd, pArg, lArg);
}

s64 sys_exit(s64 code)
{
	thread_exit();
	return 0;
}

s64 sys_LoadSysModule(char* cmdline)
{
	return LoadSysModule(cmdline);
}

s64 sys_ProcessCreate(char* cmdline)
{
	return process_create(cmdline);
}

void user_thread(void* param1, void* param2);

s64 sys_ThreadCreate(kthread_fnc fnc, void* param)
{
    return create_kthread(thread_current->process, user_thread, THREAD_PRI_NORMAL, fnc, param); // first thread ,start address = 2G + 4K
}

s64 sys_MSleep(s64 ms)
{
    return msleep(ms);
}

/*
u64 sys_call(obj_t* obj, u64 msg, arg1, arg1, arg1)
{
	return obj->proc(obj, msg, &msg + 1);
}
*/
/*
u64 sys_getpid(u64* args)
{
	return thread_current->process->id;
}

u64 sys_gettid(u64* args)
{
	return thread_current->id;
}


u64 sys_CreateWindow(syscall_parm* args)
{
	u64 x = args->a1;
	u64 y = args->a2;
	u64 w = args->a3;
	u64 h = args->a4;
	u64 style = args->a5;

	// x , y, w, h, style
	//return window_create(x, y, w, h, style);
}

u64 sys_GetMessage(syscall_parm* args)
{
	msg_t* user_msg = (msg_t*) args->a1;

	thread_msg_t* msg = get_thread_msg(thread_current);

	user_msg->wnd = msg->wnd;
	user_msg->msgid = msg->msgid;
	user_msg->arg0 = msg->arg0;
	user_msg->arg1 = msg->arg1;

	kmfree(msg);

	return 0;
}
*/

////////////////////////////////////////////////////

void* syscall_tab[] =
{
    NULL,
	sys_open,
	sys_close,
	sys_readp,
	sys_writep,
	sys_ioctl,
	sys_exit,
	sys_LoadSysModule,
	sys_ProcessCreate,
	sys_ThreadCreate,
	sys_MSleep

/*
    sys_getpid,
    sys_gettid,
    sys_CreateWindow,
    sys_GetMessage
    */
};

typedef s64 (*syscall_fnc)(s64 a0, s64 a1, s64 a2, s64 a3);

s64 syscall(s64 a0, s64 a1, s64 a2, s64 a3)
{
    int num;
    ASM("mov %%eax, %0":"=m"(num));

    //printk("thread %d syscall %d\n", thread_current->id, num);
    //return 0;

    assert(num >= 0 && num < sizeof(syscall_tab) / sizeof(void*));
    return ((syscall_fnc)(syscall_tab[num]))(a0, a1, a2, a3); // return
}

extern void syscall_asm();

void syscall_init()
{
	ENTER();

    // SYSRET  CS and SS Selectors:0x18 + 3,    SYSCALL CS and SS Selectors:0x8
    wrmsr(STAR,  ((u64)(0x18 + 3) << 48) | ((u64)0x8 << 32));
    wrmsr(LSTAR, (u64)syscall_asm ); // LSTAR :syscall_handler
    wrmsr(CSTAR, 0 ); // CSTAR
    wrmsr(FMASK, (1 << 9) ); // SYSCALL Flag Mask, clear IF

    LEAVE();
}

