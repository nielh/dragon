
.global spin_lock_up, spin_unlock_up

	.seh_proc	spin_lock_up // use for gdb backtrace
	.seh_pushreg	%rbp
	.seh_setframe	%rbp, 0
spin_lock_up:
	pushf
	popq 8(%rcx) 		//save rflag
    cli
    ret
    .seh_endproc

	.seh_proc	spin_unlock_up // use for gdb backtrace
	.seh_pushreg	%rbp
	.seh_setframe	%rbp, 0
spin_unlock_up:
	pushq 8(%rcx) 		// pop rflag
	popf
    ret
    .seh_endproc

.global spin_lock_mp, spin_unlock_mp
spin_lock_mp:
	cli
    lock btsl $0, (%rcx)        //Attempt to acquire the lock (in case lock is uncontended)
    jc .spin_with_pause
    ret

.spin_with_pause:
    pause                    // Tell CPU we're spinning
    testl $1, (%rcx)      //Is the lock free?
    jnz .spin_with_pause     // no, wait
    jmp spin_lock_mp          // retry

spin_unlock_mp:
    movl $0,(%rcx)
    sti
    ret
