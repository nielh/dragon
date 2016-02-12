#ifndef THREAD_H
#define THREAD_H

#include "kernel.h"
#include "spinlock.h"
#include "process.h"
#include "sem.h"
#include "Dlist.h"

#define THREAD_PRI_HIGH			0
#define THREAD_PRI_NORMAL		1
#define THREAD_PRI_LOW			2

#define THREAD_PRI_MAX			3

#define HZ 	                    100
#define MS                      (1000 / HZ)
#define THREAD_TICKS			MS*10

typedef struct thread_msg_t
{
	u32 wnd;
	u32 msgid;
	u32 arg0;
	u32 arg1;

	struct thread_msg_t* next;

} thread_msg_t;

typedef struct sem_t
{
    char* name;
    spinlock_t lock;
	DList wait_list;
	volatile int flag;

} sem_t;

typedef struct
{
	void* kstack_top;
	void* kstack_end;
	void* kstack_start;

	u64 id;

	void* status;
	u64 ticks;

	u64 privilage;
	u64 ktime;

	process_t* process;

	DListNode   run_node;
	DListNode   all_node;

	thread_msg_t* msg_head;
	thread_msg_t* msg_tail;

	sem_t* wait_sem;

	sem_t thread_msg_sem;
	spinlock_t thread_msg_lock;

	u32 msleep;

	spinlock_t lock;

} thread_t;

typedef void (*kthread_fnc)(void* param1, void* param2);

void thread_init();
void thread_schedule();

EXPORT int create_kthread(process_t* proc, kthread_fnc func, int pri, void* param1, void* param2);
void thread_exit();

extern thread_t* volatile thread_current;

thread_msg_t* get_thread_msg(thread_t * t);
void add_thread_msg(thread_t * t, thread_msg_t* msg);

void sem_init(sem_t* sem, int flag, char* name);
void down(sem_t* sem);
void up_one(sem_t* res);

#endif //
