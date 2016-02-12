
.global setjmp, longjmp

// int setjmp(jmp_buf env)
setjmp:
    movq   %rbx, 0x00(%rcx)
    movq   %rsp, 0x08(%rcx)
    movq   %rbp, 0x10(%rcx)
    movq   %rsi, 0x18(%rcx)
    movq   %rdi, 0x20(%rcx)
    movq   %r12, 0x28(%rcx)
    movq   %r13, 0x30(%rcx)
    movq   %r14, 0x38(%rcx)
    movq   %r15, 0x40(%rcx)

    movq   (%rsp), %rax
    movq   %rax, 0x48(%rcx)       // save ret rip

    xorq   %rax, %rax             // Return 0
    retq

/*
    movdqa  %xmm6, 0x8(%rcx)
    movdqa  %xmm7, 0x8(%rcx)
    movdqa  %xmm8, 0x8(%rcx)
    movdqa  %xmm9, 0x8(%rcx)
    movdqa  %xmm10, 0x8(%rcx)
    movdqa  %xmm11, 0x8(%rcx)
    movdqa  %xmm12, 0x8(%rcx)
    movdqa  %xmm13, 0x8(%rcx)
    movdqa  %xmm14, 0x8(%rcx)
    movdqa  %xmm15, 0x8(%rcx)
*/


//    void longjmp(jmp_buf env, int value)
longjmp:
    movq   0x00(%rcx) , %rbx
    movq   0x08(%rcx) , %rsp
    movq   0x10(%rcx) , %rbp
    movq   0x18(%rcx) , %rsi
    movq   0x20(%rcx) , %rdi
    movq   0x28(%rcx) , %r12
    movq   0x30(%rcx) , %r13
    movq   0x38(%rcx) , %r14
    movq   0x40(%rcx) , %r15

    movq   0x48(%rcx) , %rax
    movq   %rax , (%rsp)           // restore ret rip

    movq   %rdx, %rax               // set return value
    retq
