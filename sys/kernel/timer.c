#include "kernel.h"
#include "thread.h"
#include "printk.h"
#include "malloc.h"
#include "vmalloc.h"
#include "libc.h"
#include "isr.h"
#include "process.h"
#include "thread.h"
#include "idt.h"
#include "timer.h"
#include "port.h"
#include "atom.h"

/*
I/O port     Usage
0x40         Channel 0 data port (read/write)
0x41         Channel 1 data port (read/write)
0x42         Channel 2 data port (read/write)
0x43         Mode/Command register (write only, a read is ignored)

The Mode/Command register at I/O address 0x43 contains the following:
Bits         Usage
 6 and 7      Select channel :
                 0 0 = Channel 0
                 0 1 = Channel 1
                 1 0 = Channel 2
                 1 1 = Read-back command (8254 only)
 4 and 5      Access mode :
                 0 0 = Latch count value command
                 0 1 = Access mode: lobyte only
                 1 0 = Access mode: hibyte only
                 1 1 = Access mode: lobyte/hibyte
 1 to 3       Operating mode :
                 0 0 0 = Mode 0 (interrupt on terminal count)
                 0 0 1 = Mode 1 (hardware re-triggerable one-shot)
                 0 1 0 = Mode 2 (rate generator)
                 0 1 1 = Mode 3 (square wave generator)
                 1 0 0 = Mode 4 (software triggered strobe)
                 1 0 1 = Mode 5 (hardware triggered strobe)
                 1 1 0 = Mode 2 (rate generator, same as 010b)
                 1 1 1 = Mode 3 (square wave generator, same as 011b)
 0            BCD/Binary mode: 0 = 16-bit binary, 1 = four-digit BCD

*/

volatile u64 clock_ticks = 0;

extern DList run_list[THREAD_PRI_MAX];


typedef struct SLEEP_LIST
{
	spinlock_t lock;
	DList list;

} SLEEP_LIST;

static SLEEP_LIST sleep_list ={0};

int msleep(int ms)
{
	if(ms < 1)
		return -1;

    assert(thread_current != NULL);

    PUSIF();

    spin_lock(&sleep_list.lock);

    thread_current->msleep = ms;
    DListAddTail(&sleep_list.list, &thread_current->run_node); // 加入定时器队列
    spin_unlock(&sleep_list.lock);

    thread_schedule();
    POPIF();

    return 0;
}

static void SleepHandle()
{
	if(DListCount(&sleep_list.list) == 0)
		return;

   // 定时器处理
    spin_lock(&sleep_list.lock);

    DListNode* n = sleep_list.list.head;

    while(n)
    {
    	thread_t* t = container_of(n, thread_t, run_node);
        t->msleep -= MS;

        if(t->msleep <= 0)
        {
        	t->msleep = 0;
			DListRemove(&sleep_list.list, n);
			DListAddTail(&run_list[t->privilage], n); // 加入运行队列
        }

        n = DListGetNext(n);
    }

    spin_unlock(&sleep_list.lock);
}

static void timer_handle(void* data, registers_t* regs)
{
    __sync_add_and_fetch(&clock_ticks, MS);

    SleepHandle();

    // 当前线程处理
    if(thread_current != NULL)
    {
        thread_current->ticks -= MS;
        thread_current->ktime += MS;

        if(thread_current->ticks <= 0)
        {
            // 恢复时间片
            thread_current->ticks = THREAD_TICKS;

            // 加入到对应优先级的运行队列队尾
            DListAddTail(&run_list[thread_current->privilage], &thread_current->run_node);

            //list_append_tail(&run_list[thread_current->privilage], thread_current);
            thread_schedule();
        }
    }
}

void timer_init()
{
	ENTER();

    irq_register(IRQ_TIMER, timer_handle, 0);
    irq_enable(IRQ_TIMER);

    int divisor = 1193180 / HZ;       /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */

    LEAVE();
}


