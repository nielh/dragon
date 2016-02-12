#ifndef SETJMP_H
#define SETJMP_H

#include <sys/types.h>


typedef struct _SETJMP_FLOAT128
{
    unsigned __int64 Part[2];
} SETJMP_FLOAT128;

typedef struct jmp_buf
{
    //unsigned __int64 Frame;
    unsigned __int64 Rbx;
    unsigned __int64 Rsp;
    unsigned __int64 Rbp;
    unsigned __int64 Rsi;
    unsigned __int64 Rdi;
    unsigned __int64 R12;
    unsigned __int64 R13;
    unsigned __int64 R14;
    unsigned __int64 R15;
    unsigned __int64 Rip;
    //unsigned __int64 Spare;
    /*
    SETJMP_FLOAT128 Xmm6;
    SETJMP_FLOAT128 Xmm7;
    SETJMP_FLOAT128 Xmm8;
    SETJMP_FLOAT128 Xmm9;
    SETJMP_FLOAT128 Xmm10;
    SETJMP_FLOAT128 Xmm11;
    SETJMP_FLOAT128 Xmm12;
    SETJMP_FLOAT128 Xmm13;
    SETJMP_FLOAT128 Xmm14;
    SETJMP_FLOAT128 Xmm15;*/

} jmp_buf[1];
/*
typedef struct {
  unsigned long ebp;
  unsigned long ebx;
  unsigned long edi;
  unsigned long esi;
  unsigned long esp;
  unsigned long eip;
} jmp_buf[1];
*/
typedef struct {
  jmp_buf env;
  sigset_t sigmask;
} sigjmp_buf[1];

#define _setjmp setjmp
#define _longjmp longjmp


#ifdef  __cplusplus
extern "C" {
#endif

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int value);

int sigsetjmp(sigjmp_buf env, int savesigs);
void siglongjmp(sigjmp_buf env, int value);

#ifdef  __cplusplus
}
#endif

#endif
