#include "kernel.h"
#include "thread.h"
#include "printk.h"
#include "kmalloc.h"
#include "vmalloc.h"
#include "libc.h"
#include "isr.h"
#include "idt.h"
#include "process.h"
#include "page.h"
#include "pe.h"
#include "page_map.h"

DList run_list[THREAD_PRI_MAX] = { 0 };

thread_t* volatile thread_current = NULL;

DList all_thread_list = {0};

static u32 next_thread_id = 0; //

#define KTHREAD_STACK_SIZE      PAGE_SIZE*4

int create_kthread(process_t* proc, kthread_fnc func, int pri, void* param1, void* param2)
{
	assert(func != 0);

	void* kstack_start = kmalloc(KTHREAD_STACK_SIZE);
	assert(kstack_start != 0);
	memset(kstack_start, 0, KTHREAD_STACK_SIZE); // clear 0

	void* kstack_end = kstack_start + KTHREAD_STACK_SIZE;

	u64* kstack_top = (u64*)kstack_end;

	kstack_top -= 32;
	kstack_top[16] = (u64)func;         // retrun address
	kstack_top[14] = (1 << 9);          // rflag [IF]
	kstack_top[3] = (u64)param2;         // %rdx
	kstack_top[2] = (u64)param1;         // %rcx

	thread_t* t = kmalloc(sizeof(thread_t));
	assert(t != 0);
	memset(t, 0, sizeof(thread_t));

	t->process = proc;
	t->ticks = THREAD_TICKS;
	t->id = __sync_add_and_fetch(&next_thread_id, 1);
	t->kstack_top = kstack_top;
	t->kstack_start = kstack_start;
	t->kstack_end = kstack_end;
	t->privilage = pri;

	if(proc)
		__sync_add_and_fetch(&proc->thread_cnt, 1);

	DListAddTail(&all_thread_list, &t->all_node);
	DListAddTail(&run_list[pri], &t->run_node);

	return 0;
}

void thread_exit()
{
    spin_lock(&thread_current->lock);

    process_t* proc = thread_current->process;
	if(proc)
	{
		__sync_sub_and_fetch(&proc->thread_cnt, 1);
		if(proc->thread_cnt == 0)
		{
			ProcessFree(proc);
		}
	}

    kmfree(thread_current->kstack_start);
    kmfree(thread_current);
    thread_current = NULL;

    thread_schedule();
//    spin_unlock(&thread_current->lock);
}

thread_t* thread_pick()
{
	for (int i = 0; i < THREAD_PRI_MAX; i++)
	{
		DList* list = &run_list[i];

		if (DListCount(list) > 0)
		{
			DListNode* node = DListRemove(list, list->head);
			return container_of(node, thread_t, run_node);
		}
	}

	panic("No thread");
}

void switch_context(thread_t* target, s64 new_cr3);

extern s64 tss64_rsp0[];

void thread_schedule()
{
	thread_t* target = thread_pick();
	assert(target);

	if(thread_current == target)
        return;

    s64 new_cr3 = 0;
	if(target->process)
    {
        new_cr3 = VIR2PHY(target->process->page_dir);
    }

    *(void**)PHY2VIR(tss64_rsp0) = target->kstack_end; // set rsp0
	switch_context(target, new_cr3);
}

thread_msg_t* get_thread_msg(thread_t * t)
{
	down(&t->thread_msg_sem);

	spin_lock(&t->thread_msg_lock);

	thread_msg_t* msg = t->msg_head;

	t->msg_head = msg->next;

	if (t->msg_head == NULL) // 最后一个
		t->msg_tail = NULL;

	msg->next = NULL; // tail next is NULL

	spin_unlock(&t->thread_msg_lock);

	return msg;
}

void add_thread_msg(thread_t * t, thread_msg_t* msg)
{
	spin_lock(&t->thread_msg_lock);

	if (t->msg_head == NULL) // empty list
	{
		t->msg_head = t->msg_tail = msg;
	}
	else
	{
		t->msg_tail->next = msg;
		t->msg_tail = msg; // add to list tail
	}

	t->msg_tail = NULL; // tail next is NULL

	up_one(&t->thread_msg_sem);

	spin_unlock(&t->thread_msg_lock);
}

void down(sem_t* sem)
{
    assert(thread_current != NULL);

    PUSIF();

	spin_lock(&sem->lock);

	if (sem->flag)// 有多余的资源,直接退出
	{
		sem->flag = 0;
		spin_unlock(&sem->lock);
	}
	else
	{
		DListAddTail(&sem->wait_list, &thread_current->run_node);
        //list_append_tail(&sem->wait_list, thread_current);
        spin_unlock(&sem->lock);

        thread_current->wait_sem = sem;
        thread_schedule();
	}

	POPIF();
}

void up_one(sem_t* sem)
{
	spin_lock(&sem->lock);

	if (DListCount(&sem->wait_list) > 0) // 没有线程等待资源, 增加可用资源数量
	{
		sem->flag = 0;

		// 释放等待资源的第一个线程
		DListNode* node = DListRemoveHead(&sem->wait_list);

		thread_t* t = container_of(node, thread_t, run_node);

		// 加入到对应优先级的运行队列
		DListAddTail(&run_list[t->privilage], node); // 加入运行队列

		thread_current->wait_sem = NULL;

		//LOG("thread %d wakeup\n", t->id);
/*
		if (thread_current != NULL)
		{
			if (thread_current->privilage > t->privilage) // 激活了优先级更高的线程
			{
				list_append_tail(&run_list[thread_current->privilage],
						thread_current);

				thread_schedule();
			}
		}*/
	}
	else
		sem->flag = 1;

	spin_unlock(&sem->lock);
}

void sem_init(sem_t* sem, int flag, char* name)
{
	memset(sem, 0, sizeof(sem_t));
	sem->name = name;
	sem->flag = flag;
}

