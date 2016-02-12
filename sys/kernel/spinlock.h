

#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct
{
	u64 bts;
	u64 eflag;

} spinlock_t;

void spin_lock_up(spinlock_t* lock);
void spin_unlock_up(spinlock_t* lock);

void spin_lock_mp(spinlock_t* lock);
void spin_unlock_mp(spinlock_t* lock);

#if CONFIG_SMP
#define spin_lock(a) 		spin_lock_mp(a)
#define spin_unlock(a) 		spin_unlock_mp(a)
#else
#define spin_lock(a) 		spin_lock_up(a)
#define spin_unlock(a) 		spin_unlock_up(a)
#endif

#endif
