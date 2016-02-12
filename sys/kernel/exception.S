
.data
	error_code: .quad 0

.text

.irp num,0,1,2,3,4,5,6,7,9,15,16,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
	.global 	    isr\num
isr\num:
	push    %rbp
	mov     %rsp, %rbp
	push 	$\num			// push int num
	jmp 	isr_common
.endr

// contain error code
.irp num,8,10,11,12,13,14,17
	.global 	    isr\num
isr\num:
	pop     error_code(%rip)
	push    %rbp
	mov     %rsp, %rbp
	push 	$\num			// push int num
	jmp 	isr_common
.endr

	.seh_proc		isr_common // use for gdb backtrace
	.seh_pushreg	%rbp
	.seh_setframe	%rbp, 0

isr_common:
    // ss rsp eflag cs rip
    // rbp
    // int_num
    push 	%r15
    push 	%r14
    push 	%r13
    push 	%r12
    push 	%r11
    push 	%r10
    push 	%r9
    push 	%r8
    push 	%rdi
    push 	%rsi
    push 	%rdx
    push 	%rcx
    push 	%rbx
    push 	%rax

    mov %rsp, %rcx // ECX 第一个参数
    sub $32, %rsp

	call isr_handler
	add $32, %rsp

    pop 	%rax
    pop 	%rbx
    pop 	%rcx
    pop 	%rdx
    pop 	%rsi
    pop 	%rdi
    pop 	%r8
    pop 	%r9
    pop 	%r10
    pop 	%r11
    pop 	%r12
    pop 	%r13
    pop 	%r14
    pop 	%r15

    mov  %rbp, %rsp	 	// clean up : int_num
	pop  %rbp

	iretq					// pop : rip, cs, rflag, rsp, ss
.seh_endproc

.global gdt_flush
gdt_flush:
	lgdt (%rcx)        		// Load the IDT pointer.

	jmp 1f
1:
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	ret

.global idt_flush
idt_flush:
   lidt (%rcx)        		// Load the IDT pointer.
   ret

.global tss_flush

tss_flush:
    movw $0x2b, %ax       	// Load the index of our TSS structure - The index is
							// 0x28, as it is the 5th selector and each is 8 bytes
							// long, but we set the bottom two bits (making 0x2B)
							// so that it has an RPL of 3, not zero.
    ltr %ax           		// Load 0x2B into the task state register.
    ret

.global syscall_asm, return_to_user

	.seh_proc	syscall_asm // use for gdb backtrace
	.seh_pushreg	%rbp
	.seh_setframe	%rbp, 0
syscall_asm:
/*
    pushq %rcx
    pushq %r11
    mov   %rsp, %rcx
    mov   thread_current(%rip), %r11
    mov   (%r11), %rsp

    pop    %rax
*/
    pushq %rcx
    pushq %r11
    movq  %r10, %rcx    // set a0
    sti

	sub   $32, %rsp
	call  syscall
	add   $32, %rsp

	popq %r11
	popq %rcx
    sysretq
    .seh_endproc

// void jmp_to_user_thread(user_entry, user_param, vma_stack_top)
.global jmp_to_user
jmp_to_user:
    push	$(0x20 +3) 	// ss
    push	%r8 		// user rsp
    push	$(1 << 9) 	// rflag[IF]
    push	$(0x18 +3) 	// cs
    push	%rcx 		// rip : user_entry
	mov		%rdx, %rcx	// set user_param
	iretq				// pop : rip, cs, rflag, rsp, ss



// RCX,RDX,R8,R9，RAX、R10、R11、XMM4 和 XMM5也是易变化的(volatile)寄存器。
// RBX, RBP, RDI, RSI, R12, R13, R14, and R15寄存器则必须在使用时进行保护
// %rcx : target_thread
// %rdx : target_cr3
.global switch_context

	.seh_proc	switch_context // use for gdb backtrace
	.seh_pushreg	%rbp
	.seh_setframe	%rbp, 0

switch_context:

    mov  thread_current(%rip), %rax
    cmp   $0, %rax       // current_thread == 0 ?
    je    switch_cr3

    push    %rbp
    mov     %rsp, %rbp

    pushf
    push 	%r15
    push 	%r14
    push 	%r13
    push 	%r12
    push 	%r11
    push 	%r10
    push 	%r9
    push 	%r8
    push 	%rdi
    push 	%rsi
    push 	%rdx
    push 	%rcx
    push 	%rbx
    push 	%rax

    movq %rsp, (%rax)

switch_cr3:
    cmp  $0, %rdx        // target_cr3 == 0 ?
    je  restore

    mov   %cr3, %rax     // cr3 == target_cr3
    cmp   %rax, %rdx
    je   restore

    mov   %rdx, %cr3     // cr3 = target_cr3
    jmp  restore

restore:
    mov %rcx, thread_current(%rip)
    movq (%rcx), %rsp

    pop 	%rax
    pop 	%rbx
    pop 	%rcx
    pop 	%rdx
    pop 	%rsi
    pop 	%rdi
    pop 	%r8
    pop 	%r9
    pop 	%r10
    pop 	%r11
    pop 	%r12
    pop 	%r13
    pop 	%r14
    pop 	%r15
    popf

    pop     %rbp
return:
    retq
    .seh_endproc

