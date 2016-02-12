#include "kernel.h"
#include "object.h"
#include "printk.h"
#include "libc.h"
#include "port.h"
#include "kmalloc.h"
#include "thread.h"

typedef struct console_t
{
    ops_t* ops;

} console_t;

/*
 * These are set up by the setup-routine at boot-time:
 */

//#define ORIG_X			(*(unsigned char *)0x90000)
//#define ORIG_Y			(*(unsigned char *)0x90001)
#define ORIG_VIDEO_PAGE		(*(unsigned short *)PHY2VIR(0x90004))
//#define ORIG_VIDEO_MODE		((*(unsigned short *)0x90006) & 0xff)
//#define ORIG_VIDEO_COLS 	(((*(unsigned short *)0x90006) & 0xff00) >> 8)
#define ORIG_VIDEO_LINES	(25)
//#define ORIG_VIDEO_EGA_AX	(*(unsigned short *)0x90008)
#define ORIG_VIDEO_EGA_BX	(*(unsigned short *)PHY2VIR(0x9000a))
//#define ORIG_VIDEO_EGA_CX	(*(unsigned short *)0x9000c)

#define VIDEO_TYPE_MDA		0x10	/* Monochrome Text Display	*/
#define VIDEO_TYPE_CGA		0x11	/* CGA Display 			*/
#define VIDEO_TYPE_EGAM		0x20	/* EGA/VGA in Monochrome Mode	*/
#define VIDEO_TYPE_EGAC		0x21	/* EGA/VGA in Color Mode	*/

#define NPAR 16

/*	intr=^C		quit=^|		erase=del	kill=^U
	eof=^D		vtime=\0	vmin=\1		sxtc=\0
	start=^Q	stop=^S		susp=^Z		eol=\0
	reprint=^R	discard=^U	werase=^W	lnext=^V
	eol2=\0
*/

extern void keyboard_interrupt(void);

static u8	video_type;		/* Type of display being used	*/
static u32	video_num_columns;	/* Number of text columns	*/
static u32	video_size_row;		/* Bytes per row		*/
static u32	video_num_lines;	/* Number of test lines		*/
static u64	video_page;		/* Initial video page		*/
static u64	video_mem_start;	/* Start of video RAM		*/
static u64	video_mem_end;		/* End of video RAM (sort of)	*/
static u16	video_port_reg;		/* Video register select port	*/
static u16	video_port_val;		/* Video register value port	*/
static u16	video_erase_char;	/* Char+Attrib to erase with	*/

static u64	origin;		/* Used for EGA/VGA fast scroll	*/
static u64	scr_end;	/* Used for EGA/VGA fast scroll	*/
static u64	xypos;
static u32	x,y;
static u32	top,bottom;
static u32	state=0;
static u32	npar,par[NPAR];
static u32	ques=0;
static u8	attr=0x07;

static void sysbeep(void);

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query (= vt100 response).
 */
#define RESPONSE "\033[?1;2c"

/* NOTE! gotoxy thinks x==video_num_columns is ok */
static inline void gotoxy(unsigned int new_x,unsigned int new_y)
{
    if (new_x > video_num_columns || new_y >= video_num_lines)
        return;

    x=new_x;
    y=new_y;
    xypos=origin + y*video_size_row + (x<<1);
}

static inline void set_origin(void)
{
    //cli();
    outb(video_port_reg, 12);
    outb(video_port_val, 0xff&((origin-video_mem_start)>>9));
    outb(video_port_reg, 13);
    outb(video_port_val, 0xff&((origin-video_mem_start)>>1));
    //sti();
}

static void scrup(void)
{
    if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
    {
        if (!top && bottom == video_num_lines)
        {
            origin += video_size_row;
            xypos += video_size_row;
            scr_end += video_size_row;
            if (scr_end > video_mem_end)
            {
                ASM("cld\r\n"
                        "rep\r\n"
                        "movsl\r\n"
                        "movl video_num_columns(%%rip),%1\r\n"
                        "rep\r\n"
                        "stosw"
                        ::"a" (video_erase_char),
                        "c" ((video_num_lines-1)*video_num_columns>>1),
                        "D" (video_mem_start),
                        "S" (origin));
                       // :"rcx","rdi","rsi");

                scr_end -= origin-video_mem_start;
                xypos -= origin-video_mem_start;
                origin = video_mem_start;
            }
            else
            {
                ASM("cld\n\t"
                        "rep\n\t"
                        "stosw"
                        ::"a" (video_erase_char),
                        "c" (video_num_columns),
                        "D" (scr_end-video_size_row));
                       // :"cx","di");
            }
            set_origin();
        }
        else
        {
            ASM("cld\n\t"
                    "rep\n\t"
                    "movsl\n\t"
                    "movl video_num_columns(%%rip),%%ecx\n\t"
                    "rep\n\t"
                    "stosw"
                    ::"a" (video_erase_char),
                    "c" ((bottom-top-1)*video_num_columns>>1),
                    "D" (origin+video_size_row*top),
                    "S" (origin+video_size_row*(top+1)));
                   // :"cx","di","si");
        }
    }
    else		/* Not EGA/VGA */
    {
        ASM("cld\n\t"
                "rep\n\t"
                "movsl\n\t"
                "movl video_num_columns(%%rip),%%ecx\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
                "c" ((bottom-top-1)*video_num_columns>>1),
                "D" (origin+video_size_row*top),
                "S" (origin+video_size_row*(top+1)));
                //:"cx","di","si");
    }
}

static void scrdown(void)
{
    if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
    {
        __asm__("std\n\t"
                "rep\n\t"
                "movsl\n\t"
                "addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
                "movl video_num_columns(%%rip),%%ecx\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
                "c" ((bottom-top-1)*video_num_columns>>1),
                "D" (origin+video_size_row*bottom-4),
                "S" (origin+video_size_row*(bottom-1)-4));
                //:"ax","cx","di","si");
    }
    else		/* Not EGA/VGA */
    {
        __asm__("std\n\t"
                "rep\n\t"
                "movsl\n\t"
                "addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
                "movl video_num_columns(%%rip),%%ecx\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
                "c" ((bottom-top-1)*video_num_columns>>1),
                "D" (origin+video_size_row*bottom-4),
                "S" (origin+video_size_row*(bottom-1)-4));
                //:"ax","cx","di","si");
    }
}

static void lf(void)
{
    if (y+1<bottom)
    {
        y++;
        xypos += video_size_row;
        return;
    }
    scrup();
}

static void ri(void)
{
    if (y>top)
    {
        y--;
        xypos -= video_size_row;
        return;
    }
    scrdown();
}

static void cr(void)
{
    xypos -= x<<1;
    x=0;
}

static void del(void)
{
    if (x)
    {
        xypos -= 2;
        x--;
        *(unsigned short *)xypos = video_erase_char;
    }
}

static void csi_J(int par)
{
    long flag;
    long start;

    switch (par)
    {
    case 0:	/* erase from cursor to end of display */
        flag = (scr_end-xypos)>>1;
        start = xypos;
        break;
    case 1:	/* erase from start to cursor */
        flag = (xypos-origin)>>1;
        start = origin;
        break;
    case 2: /* erase whole display */
        flag = video_num_columns * video_num_lines;
        start = origin;
        break;
    default:
        return;
    }
    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t"
            ::"c" (flag),
            "D" (start),"a" (video_erase_char));
            //:"cx","di");
}

static void csi_K(int par)
{
    long flag;
    long start;

    switch (par)
    {
    case 0:	/* erase from cursor to end of line */
        if (x>=video_num_columns)
            return;
        flag = video_num_columns-x;
        start = xypos;
        break;
    case 1:	/* erase from start of line to cursor */
        start = xypos - (x<<1);
        flag = (x<video_num_columns)?x:video_num_columns;
        break;
    case 2: /* erase whole line */
        start = xypos - (x<<1);
        flag = video_num_columns;
        break;
    default:
        return;
    }
    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t"
            ::"c" (flag),
            "D" (start),"a" (video_erase_char));
            //:"cx","di");
}


enum vga_color
{
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 0xA,
	COLOR_LIGHT_CYAN = 0xB,
	COLOR_LIGHT_RED = 0xC,
	COLOR_LIGHT_MAGENTA = 0xD,
	COLOR_LIGHT_BROWN = 0xE,
	COLOR_WHITE = 0xF
};

char colors[]=
{
    COLOR_BLACK,
    COLOR_LIGHT_RED,
    COLOR_LIGHT_GREEN,
    COLOR_LIGHT_BROWN,
    COLOR_LIGHT_BLUE,
    COLOR_LIGHT_MAGENTA,
    COLOR_LIGHT_CYAN,
    COLOR_WHITE
};

void csi_m(void)
{
    int i;

    for (i=0; i<=npar; i++)
    {
        char val = par[i];

        switch (val)
        {
        case 0:
            attr=0x07;
            break;
        case 1:
            attr=0x0f;
            break;
        case 4:
            attr=0x0f;
            break;
        case 7:
            attr=0x70;
            break;
        case 27:
            attr=0x07;
            break;
        }

        if(val >= 30 && val <= 37)
            attr |= (colors[val -30]) & 0xF;
        else if(val >= 40 && val <= 47)
            attr |= (colors[val -40] << 4);
    }

}

static inline void set_cursor(void)
{
    //cli();
    outb(video_port_reg, 14);
    outb(video_port_val, 0xff&((xypos-video_mem_start)>>9));
    outb(video_port_reg, 15);
    outb(video_port_val, 0xff&((xypos-video_mem_start)>>1));
    //sti();
}
/*
static void respond(struct tty_struct * tty)
{
	char * p = RESPONSE;

	cli();
	while (*p) {
		PUTCH(*p,tty->read_q);
		p++;
	}
	sti();
	copy_to_cooked(tty);
}
*/
static void insert_char(void)
{
    int i=x;
    unsigned short tmp, old = video_erase_char;
    unsigned short * p = (unsigned short *) xypos;

    while (i++<video_num_columns)
    {
        tmp=*p;
        *p=old;
        old=tmp;
        p++;
    }
}

static void insert_line(void)
{
    int oldtop,oldbottom;

    oldtop=top;
    oldbottom=bottom;
    top=y;
    bottom = video_num_lines;
    scrdown();
    top=oldtop;
    bottom=oldbottom;
}

static void delete_char(void)
{
    int i;
    unsigned short * p = (unsigned short *) xypos;

    if (x>=video_num_columns)
        return;
    i = x;
    while (++i < video_num_columns)
    {
        *p = *(p+1);
        p++;
    }
    *p = video_erase_char;
}

static void delete_line(void)
{
    int oldtop,oldbottom;

    oldtop=top;
    oldbottom=bottom;
    top=y;
    bottom = video_num_lines;
    scrup();
    top=oldtop;
    bottom=oldbottom;
}

static void csi_at(unsigned int nr)
{
    if (nr > video_num_columns)
        nr = video_num_columns;
    else if (!nr)
        nr = 1;
    while (nr--)
        insert_char();
}

static void csi_L(unsigned int nr)
{
    if (nr > video_num_lines)
        nr = video_num_lines;
    else if (!nr)
        nr = 1;
    while (nr--)
        insert_line();
}

static void csi_P(unsigned int nr)
{
    if (nr > video_num_columns)
        nr = video_num_columns;
    else if (!nr)
        nr = 1;
    while (nr--)
        delete_char();
}

static void csi_M(unsigned int nr)
{
    if (nr > video_num_lines)
        nr = video_num_lines;
    else if (!nr)
        nr=1;
    while (nr--)
        delete_line();
}

static int saved_x=0;
static int saved_y=0;

static void save_cur(void)
{
    saved_x=x;
    saved_y=y;
}

static void restore_cur(void)
{
    gotoxy(saved_x, saved_y);
}

void console_write_char(char c)
{
    switch(state)
    {
    case 0:
        if (c>31 && c<127)
        {
            if (x>=video_num_columns)
            {
                x -= video_num_columns;
                xypos -= video_size_row;
                lf();
            }

            *(short*)xypos = (attr << 8) | c;
            xypos += 2;
            x++;
        }
        else if (c==27)
            state=1;
        else if (c==10 || c==11 || c==12)
		{
			lf();cr();
		}
        //else if (c==13)
            //cr();
        //else if (c==ERASE_CHAR(tty))
        //	del();
        else if (c==8)
        {
            if (x)
            {
                x--;
                xypos -= 2;
            }
        }
        else if (c==9)
        {
            c=8-(x&7);
            x += c;
            xypos += c<<1;
            if (x>video_num_columns)
            {
                x -= video_num_columns;
                xypos -= video_size_row;
                lf();
            }
            c=9;
        }
        else if (c==7)
            sysbeep();
        break;
    case 1:
        state=0;
        if (c=='[')
            state=2;
        else if (c=='E')
            gotoxy(0,y+1);
        else if (c=='M')
            ri();
        else if (c=='D')
            lf();
        //else if (c=='Z')
        //respond(tty);
        else if (x=='7')
            save_cur();
        else if (x=='8')
            restore_cur();
        break;
    case 2:
        for(npar=0; npar<NPAR; npar++)
            par[npar]=0;
        npar=0;
        state=3;
        if ((ques= (c=='?')))
            break;
    case 3:
        if (c==';' && npar<NPAR-1)
        {
            npar++;
            break;
        }
        else if (c>='0' && c<='9')
        {
            par[npar]=10*par[npar]+c-'0';
            break;
        }
        else state=4;
    case 4:
        state=0;
        switch(c)
        {
        case 'G':
        case '`':
            if (par[0]) par[0]--;
            gotoxy(par[0],y);
            break;
        case 'A':
            if (!par[0]) par[0]++;
            gotoxy(x,y-par[0]);
            break;
        case 'B':
        case 'e':
            if (!par[0]) par[0]++;
            gotoxy(x,y+par[0]);
            break;
        case 'C':
        case 'a':
            if (!par[0]) par[0]++;
            gotoxy(x+par[0],y);
            break;
        case 'D':
            if (!par[0]) par[0]++;
            gotoxy(x-par[0],y);
            break;
        case 'E':
            if (!par[0]) par[0]++;
            gotoxy(0,y+par[0]);
            break;
        case 'F':
            if (!par[0]) par[0]++;
            gotoxy(0,y-par[0]);
            break;
        case 'd':
            if (par[0]) par[0]--;
            gotoxy(x,par[0]);
            break;
        case 'H':
        case 'f':
            if (par[0]) par[0]--;
            if (par[1]) par[1]--;
            gotoxy(par[1],par[0]);
            break;
        case 'J':
            csi_J(par[0]);
            break;
        case 'K':
            csi_K(par[0]);
            break;
        case 'L':
            csi_L(par[0]);
            break;
        case 'M':
            csi_M(par[0]);
            break;
        case 'P':
            csi_P(par[0]);
            break;
        case '@':
            csi_at(par[0]);
            break;
        case 'm':
            csi_m();
            break;
        case 'r':
            if (par[0]) par[0]--;
            if (!par[1]) par[1] = video_num_lines;
            if (par[0] < par[1] &&
                    par[1] <= video_num_lines)
            {
                top=par[0];
                bottom=par[1];
            }
            break;
        case 's':
            save_cur();
            break;
        case 'u':
            restore_cur();
            break;
        }
    }
}

/* from bsd-net-2: */
void sysbeepstop(void)
{
    /* disable counter 2 */
    outb(0x61, inb(0x61)&0xFC);
}

int beepcount = 0;
#define HZ 100

static void sysbeep(void)
{
    /* enable counter 2 */
    outb(0x61, inb(0x61)|3);
    /* set command for counter 2, 2 byte write */
    outb(0x43, 0xB6);
    /* send 0x637 for 750 HZ */
    outb(0x42, 0x37);
    outb(0x42, 0x06);
    /* 1/8 second */
    beepcount = HZ/8;
}

static void* console_open(void* obj, char* path, s64 flag, s64 mode)
{
    return obj;
}

static s64 console_write(void* obj, void* buf, s64 len, s64 pos)
{
    for(int i= 0; i < len; i++)
    {
        console_write_char(((char*)buf)[i]);
    }

    set_cursor();
    return 0;
}

#define INPUT_BUF_SIZE 256

static u8 fifo[INPUT_BUF_SIZE];
static int fifo_r, fifo_w;
static sem_t data_ready = {0};

static int fifo_count()
{
	int flag = fifo_w -fifo_r;
	if(flag < 0)
		flag += INPUT_BUF_SIZE;

	return flag;
}

static int fifo_read()
{
	if(fifo_count() <= 0) // empty
		return -1;

	u8 ch = fifo[fifo_r ++];
	if(fifo_r >= INPUT_BUF_SIZE)
		fifo_r = 0;

	return ch;
}

int console_fifo_write(char ch)
{
	if(fifo_count() >= INPUT_BUF_SIZE) // full
		return -1;

	fifo[fifo_w ++] = ch;
	if(fifo_w >= INPUT_BUF_SIZE)
		fifo_w = 0;

	up_one(&data_ready);
	return 0;
}

static s64 console_read(void* obj, void* buf, s64 len, s64 pos)
{
	down(&data_ready);

	int count = 0;
	while(len)
	{
		int ch = fifo_read();
		if(ch < 0)
			break;

		*(char*)buf++ = (char)ch;
		count ++;
		len --;
	}

	return count;
}

static ops_t console_ops =
{
    .open = console_open,
    .read = console_read,
    .write = console_write
};

void console_init()
{
    register unsigned char a;
    char *display_desc = "????";
    char *display_ptr;

    video_num_columns = 80;//ORIG_VIDEO_COLS;
    video_size_row = video_num_columns * 2;
    video_num_lines = ORIG_VIDEO_LINES;
    video_page = ORIG_VIDEO_PAGE;
    video_erase_char = 0x0720;

	video_mem_start = PHY2VIR(0xb8000);
	video_port_reg	= 0x3d4;
	video_port_val	= 0x3d5;
	if((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
	{
		video_type = VIDEO_TYPE_EGAC;
		video_mem_end = PHY2VIR(0xbc000);
		display_desc = "EGAc";
	}
	else
	{
		video_type = VIDEO_TYPE_CGA;
		video_mem_end = PHY2VIR(0xba000);
		display_desc = "*CGA";
	}

    /* Let the user known what kind of display driver we are using */

    display_ptr = (char *)(video_mem_start) + video_size_row - 8;

    while (*display_desc)
    {
        *display_ptr++ = *display_desc++;
        display_ptr++;
    }

    /* Initialize the variables used for scrolling (mostly EGA/VGA)	*/

    origin	= video_mem_start;
    scr_end	= video_mem_start + video_num_lines * video_size_row;
    top	= 0;
    bottom	= video_num_lines;

    gotoxy(0,0);
    //set_trap_gate(0x21,&keyboard_interrupt);
    outb(0x21, inb(0x21)&0xfd);
    a=inb(0x61);
    outb(0x61, a|0x80);
    outb(0x61, a);
/*
    char* str = "\033[2J";// clear
    console_write(NULL, str, strlen(str), 0);

    str = "\033[20;10H";// clear
    console_write(NULL, str, strlen(str), 0);

    str = "\033[37;41m something here \033[0m";//"01234567890\n";
    console_write(NULL, str, strlen(str), 0);
*/
    console_t* cons = (console_t*)kmalloc(sizeof(console_t));
    memset(cons, 0, sizeof(console_t));
    cons->ops = &console_ops;

    //console_write(cons, "123\n234\n345\n\n456\n", 17, 0)

    append(HANDLE_OBJ, "console", cons);
}

