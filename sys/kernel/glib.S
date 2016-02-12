


// invert16x16(ptr)

.global invert16x16

xor_val:
.quad 0x00ffffff00ffffff
.quad 0x00ffffff00ffffff

	.seh_proc		invert16x4 // use for gdb backtrace
	.seh_pushreg	%rbp
	.seh_setframe	%rbp, 0

invert16x16:
    push %rbp

    movdqu  0(%rcx),%xmm0
    movdqu 16(%rcx),%xmm1
    movdqu 32(%rcx),%xmm2
    movdqu 48(%rcx),%xmm3

    pxor xor_val(%rip), %xmm0
    pxor xor_val(%rip), %xmm1
    pxor xor_val(%rip), %xmm2
    pxor xor_val(%rip), %xmm3

    movdqu %xmm0,  0(%rcx)
    movdqu %xmm1, 16(%rcx)
    movdqu %xmm2, 32(%rcx)
    movdqu %xmm3, 48(%rcx)

    pop %rbp
    retq

.seh_endproc
