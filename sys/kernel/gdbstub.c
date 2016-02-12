/*
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ***************************************************************************
 */

#include "kernel.h"
#include "serial.h"
#include "isr.h"
#include "printk.h"
#include "gdbstub.h"
#include "libc.h"

/************************************************************************
 *
 * external low-level support routines
 */

void putDebugChar(char a)
{
    serial_putc(a);
}

int getDebugChar()
{
    return serial_getc();
}

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400

static char initialized;  /* boolean flag. != 0 means we've been initialized */

int     remote_debug;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

static const char hexchars[]="0123456789abcdef";

/* Number of registers.  */
#define NUMREGS	18

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 8)

u64
gdb_get_reg(int regnum, registers_t *regs)
{
	u64 val = 0;

    switch (regnum)
    {
    case 0:
        return (regs->rax);
    case 1:
        return(regs->rbx);
    case 2:
        return(regs->rcx);
    case 3:
        return(regs->rdx);
    case 4:
        return(regs->rsi);
    case 5:
        return(regs->rdi);
    case 6:
        return(regs->rbp);
    case 7:
        return(regs->rsp);
    case 8:
        return(regs->r8);
    case 9:
        return(regs->r9);
    case 10:
        return(regs->r10);
    case 11:
        return(regs->r11);
    case 12:
        return(regs->r12);
    case 13:
        return(regs->r13);
    case 14:
        return(regs->r14);
    case 15:
        return(regs->r15);
    case 16:
        return(regs->rip);
    case 17:
        return(regs->rflags);
    case 18:
    	ASM("mov %%cs, %0":"=a"(val));
        return val;
    case 19:
    	ASM("mov %%ss, %0":"=a"(val));
        return val;
    case 20:
    	ASM("mov %%ds, %0":"=a"(val));
        return val;
    case 21:
    	ASM("mov %%es, %0":"=a"(val));
        return val;
    case 22:
    	ASM("mov %%fs, %0":"=a"(val));
        return val;
    case 23:
    	ASM("mov %%gs, %0":"=a"(val));
        return val;
    default:
        return 0;
    }
}

void
gdb_set_reg(int regnum, registers_t *regs, u64 val)
{
    switch (regnum)
    {
    case 0:
        regs->rax = val;
        break;
    case 1:
        regs->rbx = val;
        break;
    case 2:
        regs->rcx = val;
        break;
    case 3:
        regs->rdx = val;
        break;
    case 4:
        regs->rsi = val;
        break;
    case 5:
        regs->rdi = val;
        break;
    case 6:
        regs->rbp = val;
        break;
    case 7:
        regs->rsp = val;
        break;

    case 8:
        regs->r8 = val;
        break;
    case 9:
        regs->r9 = val;
        break;
    case 10:
        regs->r10 = val;
        break;
    case 11:
        regs->r11 = val;
        break;
    case 12:
        regs->r12 = val;
        break;
    case 13:
        regs->r13 = val;
        break;
    case 14:
        regs->r14 = val;
        break;
    case 15:
        regs->r15 = val;
        break;

    case 16:
        regs->rip = val;
        break;
    case 17:
        regs->rflags = val;
        break;
    /*
    case 18: regs->cs = (u16)val; break;
    case 19: regs->ss = (u16)val; break;
    case 20: regs->ds = (u16)val; break;
    case 21: regs->es = (u16)val; break;
    case 22: regs->fs = (u16)val; break;
    case 23: regs->gs = (u16)val; break;*/

    default:
        break;
    }
}


/* Put the error code here just in case the user cares.  */
int gdb_i386errcode;
/* Likewise, the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
int gdb_i386vector = -1;


int
hex (ch)
char ch;
{
    if ((ch >= 'a') && (ch <= 'f'))
        return (ch - 'a' + 10);
    if ((ch >= '0') && (ch <= '9'))
        return (ch - '0');
    if ((ch >= 'A') && (ch <= 'F'))
        return (ch - 'A' + 10);
    return (-1);
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

/* scan for the sequence $<data>#<checksum>     */

char *
getpacket (void)
{
    char *buffer = &remcomInBuffer[0];
    unsigned char checksum;
    unsigned char xmitcsum;
    int flag;
    char ch;

    while (1)
    {
        /* wait around for the start character, ignore all other characters */
        while ((ch = getDebugChar ()) != '$')
            ;

retry:
        checksum = 0;
        xmitcsum = -1;
        flag = 0;

        /* now, read until a # or end of buffer is found */
        while (flag < BUFMAX - 1)
        {
            ch = getDebugChar ();
            if (ch == '$')
                goto retry;
            if (ch == '#')
                break;
            checksum = checksum + ch;
            buffer[flag] = ch;
            flag = flag + 1;
        }
        buffer[flag] = 0;

        if (ch == '#')
        {
            ch = getDebugChar ();
            xmitcsum = hex (ch) << 4;
            ch = getDebugChar ();
            xmitcsum += hex (ch);

            if (checksum != xmitcsum)
            {
                if (remote_debug)
                {
                    //fprintf (stderr,
                    //       "bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
                    //        checksum, xmitcsum, buffer);
                }
                putDebugChar ('-');	/* failed checksum */
            }
            else
            {
                putDebugChar ('+');	/* successful transfer */

                /* if a sequence char is present, reply the sequence ID */
                if (buffer[2] == ':')
                {
                    putDebugChar (buffer[0]);
                    putDebugChar (buffer[1]);

                    return &buffer[3];
                }

                return &buffer[0];
            }
        }
    }
}

/* send the packet in buffer.  */

void
putpacket (char *buffer)
{
    unsigned char checksum;
    int flag;
    char ch;

    /*  $<packet info>#<checksum>.  */
    do
    {
        putDebugChar ('$');
        checksum = 0;
        flag = 0;

        while ((ch = buffer[flag]))
        {
            putDebugChar (ch);
            checksum += ch;
            flag += 1;
        }

        putDebugChar ('#');
        putDebugChar (hexchars[checksum >> 4]);
        putDebugChar (hexchars[checksum % 16]);

    }
    while (getDebugChar () != '+');
}



/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */
char *
mem2hex (mem, buf, flag)
char *mem;
char *buf;
int flag;
{
    int i;
    unsigned char ch;

    for (i = 0; i < flag; i++)
    {
        ch = *mem++;

        *buf++ = hexchars[ch >> 4];
        *buf++ = hexchars[ch % 16];
    }
    *buf = 0;

    return (buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
char *
hex2mem (buf, mem, flag)
char *buf;
char *mem;
int flag;
{
    int i;
    unsigned char ch;

    for (i = 0; i < flag; i++)
    {
        ch = hex (*buf++) << 4;
        ch = ch + hex (*buf++);
        *mem++ = ch;

    }

    return (mem);
}

/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
int
computeSignal (int exceptionVector)
{
    int sigval;
    switch (exceptionVector)
    {
    case 0:
        sigval = 8;
        break;			/* divide by zero */
    case 1:
        sigval = 5;
        break;			/* debug exception */
    case 3:
        sigval = 5;
        break;			/* breakpoint */
    case 4:
        sigval = 16;
        break;			/* into instruction (overflow) */
    case 5:
        sigval = 16;
        break;			/* bound instruction */
    case 6:
        sigval = 4;
        break;			/* Invalid opcode */
    case 7:
        sigval = 8;
        break;			/* coprocessor not available */
    case 8:
        sigval = 7;
        break;			/* double fault */
    case 9:
        sigval = 11;
        break;			/* coprocessor segment overrun */
    case 10:
        sigval = 11;
        break;			/* Invalid TSS */
    case 11:
        sigval = 11;
        break;			/* Segment not present */
    case 12:
        sigval = 11;
        break;			/* stack exception */
    case 13:
        sigval = 11;
        break;			/* general protection */
    case 14:
        sigval = 11;
        break;			/* page fault */
    case 16:
        sigval = 7;
        break;			/* coprocessor error */
    default:
        sigval = 7;		/* "software generated" */
    }
    return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
int
hexToInt (char **ptr, u64 *intValue)
{
    int numChars = 0;
    int hexValue;

    *intValue = 0;

    while (**ptr)
    {
        hexValue = hex (**ptr);
        if (hexValue >= 0)
        {
            *intValue = (*intValue << 4) | hexValue;
            numChars++;
        }
        else
            break;

        (*ptr)++;
    }

    return (numChars);
}

u64 gdb_all_reg[NUMREGS];

/*
 * This function does all command procesing for interfacing to gdb.
 */
void
handle_exception (void* data, registers_t* regs)
{
	//ASM("cli");
    int exceptionVector = regs->int_no;

    int sigval, stepping;
    u64 addr, length;
    char *ptr;
    //u64 newPC;

    gdb_i386vector = exceptionVector;
/*
      if (remote_debug)
        {
          printk ("vector=%d, sr=0x%x, pc=0x%x\n",
    	      exceptionVector, registers[PS], registers[PC]);
        }
*/
    /* reply to host that an exception has occurred */
    sigval = computeSignal (exceptionVector);

    ptr = remcomOutBuffer;

    *ptr++ = 'T';			/* notify gdb with signo, PC, FP and SP */
    *ptr++ = hexchars[sigval >> 4];
    *ptr++ = hexchars[sigval & 0xf];

    *ptr++ = hexchars[7];//rsp
    *ptr++ = ':';
    ptr = mem2hex((char *)&regs->rsp, ptr, 8, 0);	/* SP */
    *ptr++ = ';';

    *ptr++ = hexchars[6];
    *ptr++ = ':';
    ptr = mem2hex((char *)&regs->rbp, ptr, 8, 0); 	/* FP */
    *ptr++ = ';';

    *ptr++ = '1';
    *ptr++ = '0';
    *ptr++ = ':';
    ptr = mem2hex((char *)&regs->rip, ptr, 8, 0); 	/* PC */
    *ptr++ = ';';

    *ptr = '\0';

    putpacket (remcomOutBuffer);

    stepping = 0;

    while (1 == 1)
    {
        remcomOutBuffer[0] = 0;
        ptr = getpacket ();

        switch (*ptr++)
        {
        case '?':
            remcomOutBuffer[0] = 'S';
            remcomOutBuffer[1] = hexchars[sigval >> 4];
            remcomOutBuffer[2] = hexchars[sigval % 16];
            remcomOutBuffer[3] = 0;
            break;
        case 'd':
            remote_debug = !(remote_debug);	/* toggle debug flag */
            break;
        case 'g':		/* return the value of the CPU registers */
            for(int i=0; i < NUMREGS; i ++)
                gdb_all_reg[i] = gdb_get_reg(i, regs);

            mem2hex ((char *) gdb_all_reg, remcomOutBuffer, sizeof(gdb_all_reg));
            break;
        case 'G':		/* set the value of the CPU registers - return OK */
            hex2mem (ptr, (char *) &gdb_all_reg, sizeof(gdb_all_reg));

            for(int i=0; i < NUMREGS; i ++)
                gdb_set_reg(i, regs,  gdb_all_reg[i]);

            strcpy (remcomOutBuffer, "OK");
            break;
        case 'P':		/* set the value of a single CPU register - return OK */
        {
            u64 regno, regval;

            if (hexToInt (&ptr, &regno) && *ptr++ == '=')
            {
                if (regno >= 0 && regno < NUMREGS)
                {
                    hex2mem (ptr, (char *) &regval, 8, 0);
                    gdb_set_reg(regno, regs, regval);
                    strcpy (remcomOutBuffer, "OK");
                    break;
                }
            }

            strcpy (remcomOutBuffer, "E01");
            break;
        }

        /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
        case 'm':
            /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
            if (hexToInt (&ptr, &addr))
                if (*(ptr++) == ',')
                    if (hexToInt (&ptr, &length))
                    {
                        ptr = 0;
                        mem2hex ((char *) addr, remcomOutBuffer, length);
                    }

            if (ptr)
            {
                strcpy (remcomOutBuffer, "E01");
            }
            break;

        /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
        case 'M':
            /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
            if (hexToInt (&ptr, &addr))
                if (*(ptr++) == ',')
                    if (hexToInt (&ptr, &length))
                        if (*(ptr++) == ':')
                        {
                            hex2mem (ptr, (char *) addr, length);

                            strcpy (remcomOutBuffer, "OK");
                            ptr = 0;
                        }
            if (ptr)
            {
                strcpy (remcomOutBuffer, "E02");
            }
            break;

        /* cAA..AA    Continue at address AA..AA(optional) */
        /* sAA..AA   Step one instruction from AA..AA(optional) */
        case 's':
            stepping = 1;
        case 'c':
            /* try to read optional parameter, pc unchanged if no parm */
            if (hexToInt (&ptr, &addr))
                regs->rip = addr;

            //newPC = regs->rip;

            /* clear the trace bit */
            regs->rflags &= 0xfffffffffffffeff;

            /* set the trace bit if we're stepping */
            if (stepping)
                regs->rflags |= 0x100;

            return;	/* this is a jump */
            break;

        /* kill the program */
        case 'k':		/* do nothing */
#if 0
            /* Huh? This doesn't look like "nothing".
               m68k-stub.c and sparc-stub.c don't have it.  */
            BREAKPOINT ();
#endif
            ASM("hlt");
            break;
        }			/* switch */

        /* reply to the request */
        putpacket (remcomOutBuffer);
    }
}



/* this function is used to set up exception handlers for tracing and
   breakpoints */
void set_debug_traps (void)
{
	ENTER();

    for(int i=0; i < 13; i ++)
        isr_register(i, handle_exception, NULL);

    initialized = 1;

    LEAVE();
}


void breakpoint (void)
{
   // if (initialized)
    //    BREAKPOINT();
}


