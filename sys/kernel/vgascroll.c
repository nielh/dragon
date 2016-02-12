/*
Copyright (C) Magna Carta Software, 1988.  All Rights Reserved.

SMOOTH BROWSE -- A file browser to illustrate how to do smooth scrolling and
panning on the EGA and the VGA.
SYSTEM REQUIREMENTS:  EGA or VGA, Enhanced Color or Monochrome Display.
COMPILER SUPPORT: Turbo C v1.5,  MSC v5.0/5.1/Quick C,  Mix Power C v1.1
WATCOM C v6.0.
Simple compiler invocation (suggested):
    TURBO:          "TCC sb.c" (from the environment).
    MSC:            "CL /AS sb.c"
    MIX POWER C:    "PC /E sb.c"
    WATCOM C:       "WCC sb 3"
                    "WLINK FILE sb LIB ?:\watcomc\lib\clib%2, ?:\watcomc\lib\math%2
                    OPTION Map, caseexact, stack=2048"
Usage: UpArrow -- scroll up, DnArrow -- scroll down, + scroll faster,
- scroll slower, RtArrow -- smooth pan right, LtArrow -- smooth pan left
Space -- pause scrolling, PgUp, PgDn, Ctrl PgUp, Ctrl PgDn
First version: 3/5/88.  Last update 06-07-88.
*/


/* MSC SPECIFIC CODE */
#if !defined(__TURBOC__)        /* these two manifest constants are         */
#if !defined(__POWERC)          /* #defined by the respective compilers.    */
#if !defined(__WATCOMC__)           /* this one is our invention                */
                                /* Fall through... must be MSC!             */

/* MSC _dos_findfirst structure compatible with TURBOC "findfirst" function */

struct find_t {
    char ff_reserved[21];
    char ff_attrib;
    unsigned ff_ftime;
    unsigned ff_fdate;
    long ff_fsize;
    char ff_name[13];
};
#define _FIND_T_DEFINED  /* A Microsoft manifest constant to prevent redef. */

#define ffblk find_t
#define findfirst(x,y,z) _dos_findfirst(x,z,y)  /* note parm. order! */
#define bioskey(x) _bios_keybrd(x)

#define inportb(port) inp(port)
#define outportb(port,value) outp((port),(value))
#define outport(port,value) outpw((port),(value))
#define MK_FP(seg,ofs)  ((void far *)((((unsigned long)(seg)) << 16) | (ofs)))
#define peekb(a,b) (*((char far*)MK_FP((a),(b))))
#endif
#endif
#endif

/* WATCOM C SPECIFIC CODE */
#if defined(__WATCOMC__)
    #include <conio.h>
    #include <sys\types.h>
    #include <direct.h>

    #define inportb(port)           inp(port)
    #define outportb(port,value)    outp((port),(value))
    #define outport(port,value)     outpw((port),(value))
    #define peekb(a,b) (*((char far*)MK_FP((a),(b))))
    #define bioskey(x) kbhit()

    struct ffblk {
        char ff_reserved[21];
        char ff_attrib;
        unsigned ff_ftime;
        unsigned ff_fdate;
        long ff_fsize;
        char ff_name[13];
    };
    int findfirst(const char *pathname, struct ffblk *ffblk, int attrib);

#endif

/* OTHER COMPILER-SPECIFIC CODE */
#if defined(__TURBOC__)
    #include <dir.h>
#elif !defined(__TURBOC__) && !defined(__WATCOMC__)
    #include <conio.h>
    #include <direct.h>
#endif

#if !defined(__WATCOMC__)
    #include <bios.h>
#endif

#include <dos.h>
#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned char BYTE;     /* A convenient new data type */

/* MANIFEST CONSTANTS */
#define VERSION "1.0"
#define LOGICAL_WIDTH 132   /* EGA/VGA logical screen width (bytes, max. 512) */
#define MAXFILESIZE 0XFFFF  /* Maximum permisable file size to load */
#define NEWCOLOR    0X8     /* Code for new EGA color to load       */
#define BCOLOR      0       /* Screen background color              */
#define FCOLOR      15      /* Screen foreground color              */
#define TRUE        1
#define FALSE       !TRUE
#define TABSIZE     4

#define _KEYBRD_READY 1

#define KEYBOARD    0X16    /* The BIOS keyboard interrupt number */
#define VIDEO       0X10    /* The BIOS video interrupt number */

/* KEY DEFINITIONS */
#define ESC         1
#define UP_ARROW    72
#define DOWN_ARROW  80
#define LEFT_ARROW  75
#define RIGHT_ARROW 77
#define PAGE_UP     73
#define PAGE_DOWN   81
#define PLUS        78
#define MINUS       74
#define CPAGE_UP    132
#define CPAGE_DOWN  118
#define SPACEBAR    57

/* EGA AND VGA REGISTER VALUES */
#define PRESET_ROW_SCAN     8     /* Address of preset row scan reg. of CRTC */
#define START_ADDRESS_HIGH  0X0C  /* Address of start address h reg. of CRTC */
#define START_ADDRESS_LOW   0X0D  /* Address of start address l reg. of CRTC */
#define AC_INDEX            0X3C0 /* Attribute Controller Index Register     */
#define AC_HPP              0X13 | 0X20   /* Horizontal Pel Panning Register */
#define AC_MCR              0X10  /* Attribute Controller Mode Control Reg.  */
#define LINE_COMPARE        0X18  /* CRTC line compare register.             */
#define CRTC_OVERFLOW       0X07  /* CRTC overflow register.                 */
#define MAX_SCAN_LINE       0X09  /* CRTC maximum scan line register.        */

/* GLOBAL VARIABLES */
struct ffblk u_file;         /* DOS file descriptor returned by findfirst() */
FILE *p_u_file;              /* Ptr. to the user file to browse */
unsigned end_of_file, screen_size, end_of_buffer, buffer_size;
unsigned right_edge, left_edge; /* TRUE if we are at the edge of the screen */
int vpel, hpel, mode, mono;

/* The following three structures are a convenient form in which to store   */
/* video information and you can extend them with more information.         */
/* We will declare one of each type (below).                                */

struct video_descriptor {
    unsigned ev_active;             /* if TRUE, an EGA or VGA is active */
    unsigned color;                 /* if TRUE, ega drives color monitor */
    unsigned ecd;                   /* if TRUE, Enhanced Color Display */
    unsigned ram;                   /* size of EGA memory (in k) */
    BYTE char_ht, char_wi; /* character height and width (EGA/VGA) */
    BYTE far *base;        /* starting address of the video buffer */
    unsigned address;               /* starting address of the active video page */
    unsigned isr;                   /* EGA/VGA input status register address */
    unsigned crtc;                  /* CRT Controller Register address */
};

struct screen_descriptor {
    unsigned rows;
    unsigned cols;
    unsigned a_start;      /* start address of screen A in split screen mode */
    unsigned logical_width;     /* screen logical width in words. max: 0x100 */
    unsigned start_addr;        /* screen start address. max: 0X4000 */
};

struct enhanced_graphics_adapter {
    unsigned present;               /* if TRUE, EGA present */
    unsigned active;                /* if TRUE, EGA active */
};

struct video_graphics_array {
    unsigned present;               /* if TRUE, VGA present */
    unsigned active;                /* if TRUE, EGA present */

};

struct video_descriptor video;
struct screen_descriptor screen;
struct enhanced_graphics_adapter ega;
struct video_graphics_array vga;


/* FUNCTION PROTOTYPES -- In order of appearance */
int             main_menu(BYTE *p_fbuf);
void            intro_to_buffer(unsigned state);
BYTE * build_line(BYTE **p_fbuf);
int             line_to_buffer(BYTE *p_fbuf);
void            smooth_scroll(int count, unsigned speed);
int             smooth_pan(int count, unsigned speed);
unsigned        set_logical_screen_width(unsigned l_width);
void            set_start_addr(unsigned start_addr);
void            set_pel_pan(int hpel);
void            set_line_compare(unsigned scan_line);
void            load_ega_color(BYTE pregister, BYTE color);
unsigned        set_video_mode(BYTE m);
int             get_v_mode(void);
void            get_v_config(void);
unsigned        get_key(void);
int             get_cursor_size(void);
void            set_cursor_size(BYTE top_line, BYTE bottom_line);
void            error(char *fs,...);


int main(int argc, char *argv[])
{

    BYTE *p_fbuf;
    int not_found, rc;
    int csize;


    /* TEST FOR A FILENAME/PATH */
    /* No filename entered -- give syntax message and exit */
    if(argc < 2) {
        fprintf(stderr,"Smooth browser version %s by\n\tMagna Carta Software\n\tP.O. Box 475594\n\tGarland, Texas 75047\n", VERSION);
        fprintf(stderr, "Usage: sb pathname\n");
        exit(1);
    }

    /* SEE IF FILE EXISTS */
    not_found = findfirst(argv[1],&u_file,0);
    if (not_found) error("\nFile %s not found.",argv[1]);

    /* get file size. If too large (> MAXFILESIZE) then tell the user and exit */
    if (u_file.ff_fsize > MAXFILESIZE)
        error("\nMaximum file size %u bytes exceeded\n",MAXFILESIZE);

    /* TEST FOR AN ACCEPTABLE VIDEO CONFIGURATION */
    mode = get_v_mode();
    get_v_config();
    if (!vga.present && !ega.present)
        error("EGA/VGA required to run SMOOTH BROWSE");
    if (!video.ev_active)
        error("The EGA/VGA must be active to use SMOOTH BROWSE");
    if (!video.ecd && video.color)
        error("Enhanced Color or Monochrome Display required to run SMOOTH BROWSE");
    if (video.ram < 128)
        error("More video RAM required to use SMOOTH BROWSE");


    /* CREATE BUFFER FOR FILE. IF ERROR, RETURN AN ERRORLEVEL TO DOS */
    p_fbuf = (BYTE *) calloc(1, (unsigned) u_file.ff_fsize);
    if (p_fbuf == NULL) error("Not enough memory available to load file");

    /* OPEN FILE. IF THERE IS AN ERROR RETURN AN ERRORLEVEL TO DOS */
    p_u_file = fopen(argv[1], "rt");
    if (p_u_file == NULL) error("Error opening file %s",u_file.ff_name);

    /* READ IN FILE. IF THERE IS A SHORT COUNT RETURN AN ERRORLEVEL TO DOS */
    if (fread(p_fbuf,1,(int) u_file.ff_fsize,p_u_file) == NULL)
        error("Short count returned when reading file");

    /* SYSTEM CHECKS OUT AND FILE EXISTS AND IS WITHIN SIZE LIMITS */
    /* NOTE: VARIABLE MONO IS SET BY THE FUNCTION GET_V_MODE() */
    if (mode != 7) set_video_mode(3);
    else set_video_mode(7);

    /* GET THE ADDRESS OF THE INPUT STATUS REGISTER AND CRT CONTROLLER */
    if (mono) video.isr = 0x03ba;
    else video.isr = 0x03da;
    video.crtc = video.isr - 6;

    /* SAVE THE CURSOR AND TURN IT OFF */
    csize = get_cursor_size();
    set_cursor_size(0,0);

    /* Load a pleasant but unusual EGA color */
    load_ega_color(0,NEWCOLOR);


    /* DO IT */
    rc = main_menu(p_fbuf);

    /* EXIT ROUTINES -- RESTORE THE CURSOR AND RESET THE VIDEO MODE. */
    set_cursor_size((BYTE) (csize >> 8), (BYTE) csize);
    if (mono) set_video_mode(7);
    else set_video_mode(3);
    return(rc);
}



int main_menu(BYTE *p_fbuf)
{
    unsigned speed=1;
    int v_direction=0, old_v_direction=0;
    int h_direction=0, old_h_direction=0;
    unsigned scan;
    BYTE old_reg;
    int ret;

    /* SET LOGICAL SCREEN WIDTH */
    screen.logical_width = LOGICAL_WIDTH;
    set_logical_screen_width(screen.logical_width);

    /* SPLIT THE SCREEN */
    screen.a_start = screen.logical_width;
    set_line_compare((screen.rows - 1) * video.char_ht);


    /* CREATE 16 VIDEO PAGES IN VIDEO MODE 3 -- MANY CAVEATS TO THIS */
    outportb(0X3CE, 6);                 /* Point index at Misc. register */
    if (vga.active) old_reg = (BYTE) inportb(0X3CF); /* save contents */
    else old_reg = 0X0E;
    outportb(0X3CF, old_reg & 0XF7);    /* reset EGA ram to A0000-AFFFF */
    video.base = (BYTE far *) 0XA0000000L;

    /* SET SCREEN START ADDRESS FOR FILE */
    screen.start_addr = screen.a_start;
    set_start_addr(screen.start_addr);
    screen_size = screen.logical_width*(screen.rows-1);

    /* DISPLAY THE INTRODUCTORY MESSAGE */
    intro_to_buffer(TRUE);

    /* DISPLAY THE FILE */
    line_to_buffer(p_fbuf);             /* position file on screen */
    end_of_buffer = end_of_file/2 - screen_size;
    inportb(video.isr); /* a dummy read to initialize the Attribute Controller */


    left_edge = TRUE;   /* file display starts at left edge */

    /* THIS IS THE MAIN LOOP THAT MOVES LOGICAL SCREEN 'A' AROUND */
    do {
        do {
            if (v_direction) {
                smooth_scroll(v_direction,speed);
                old_v_direction = old_h_direction = 0;
            }
            else if (h_direction) {
                ret = smooth_pan(h_direction,speed);
                if (ret == -1) h_direction = v_direction = 0;
                old_h_direction = old_v_direction = 0;
            }
        } while (!bioskey(_KEYBRD_READY));

        scan = get_key();
        switch(scan) {
            case  ESC:      /* The user's key presses tell us what to do */
                 break;

            case UP_ARROW:
                v_direction = -2;
                h_direction = 0;
                break;

            case DOWN_ARROW:
                v_direction = 2;
                h_direction = 0;
                break;

            case LEFT_ARROW:
                intro_to_buffer(FALSE);
                h_direction = -1;
                v_direction = 0;
                break;

            case RIGHT_ARROW:
                intro_to_buffer(FALSE);
                h_direction = 1;
                v_direction = 0;
                break;

            case PAGE_UP:
                if (screen.start_addr > screen_size + screen.logical_width) screen.start_addr -= screen_size;
                else {
                    screen.start_addr = screen.a_start;
                    left_edge = TRUE;
                }
                vpel = 0;       /* Set the preset row scan register to 0 */
                /* address the preset row scan register */
                outport(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);
                set_start_addr(screen.start_addr);
                break;

            case PAGE_DOWN:                                 /* pg dn */
                if (screen.start_addr < end_of_buffer - screen_size)
                    screen.start_addr += screen_size;
                else screen.start_addr = end_of_buffer;
                vpel = 0;       /* Set the preset row scan register to 0 */
                outport(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);

                /* reset pel panning position */
                hpel = (mono || vga.active) ? 8 : 0;
                set_pel_pan(hpel);
                set_start_addr(screen.start_addr);
                break;

            case PLUS:                      /* '+' key -- scroll faster */
                if (speed < 5) speed++;
                break;

            case MINUS:                     /* '-' key -- scroll slower */
                if (speed > 1) speed--;
                break;

            case SPACEBAR:                  /* space bar -- toggle scrolling */
                if (v_direction) {          /* moving vertically, so stop */
                    old_v_direction = v_direction;
                    v_direction = 0;
                }
                else if (h_direction) {     /* moving horizontally, so stop */
                    old_h_direction = h_direction;
                    h_direction = 0;
                }
                else if (old_v_direction) {  /* stopped -- start vertical */
                    v_direction = old_v_direction;
                }
                else if (old_h_direction) {  /* stopped -- start horizontal */
                    h_direction = old_h_direction;
                }
                break;

            case CPAGE_UP:                   /* ctrl-pgup -- go to top */
                screen.start_addr = screen.a_start;
                left_edge = TRUE;

                /* reset pel panning position */
                vpel = 0;       /* Set the preset row scan register to 0 */
                outport(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);
                hpel = (mono || vga.active) ? 8 : 0;
                set_start_addr(screen.start_addr);
                set_pel_pan(hpel);
                break;

            case CPAGE_DOWN:                 /* ctrl-pgdn -- go to bottom */
                screen.start_addr = end_of_buffer;

                /* reset pel panning position */
                vpel = 0;   /* Set the preset row scan register to 0 */
                outport(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);
                hpel = (mono || vga.active) ? 8 : 0;
                set_pel_pan(hpel);

                set_start_addr(screen.start_addr);
                break;

            default:
                break;
        }
    } while (scan != 1);

    return (0);
}


/* INTRO_TO_BUFFER -- Writes the introductory messsage on screen 'B' */
void intro_to_buffer(unsigned state)
{
    BYTE far *ega_buf ;
    static BYTE a_sbuf[] = "SMOOTH BROWSE v1.0 -- by Magna Carta Software, 1988";
    BYTE *p_sbuf, att;

    ega_buf = video.base;
    att = ((FCOLOR & 0X7) << 4) + BCOLOR;
    if (state) {
        p_sbuf = a_sbuf;
        while (*p_sbuf) {
            *ega_buf++ = *p_sbuf++;
            *ega_buf++ = att;
        }
    }
    while (ega_buf < video.base + (screen.logical_width << 1) ) {
        *ega_buf++ = '\x20';
        *ega_buf++ = att;
    }
}


/* BUILD_LINE -- This function constructs a line of text from data read in  */
/* from the file.                                                           */
BYTE *build_line(BYTE **p_fbuf)
{
    static BYTE lbuf[LOGICAL_WIDTH + 1];/* add 1 for '\0' terminator */
    BYTE *p_lbuf;
    int i;

    p_lbuf = lbuf;  /* point to the buffer to hold the line that we build */

    /* construct a line in the buffer for display until we hit a '\n' or '\t'*/
    while (**p_fbuf != '\n') {
        if (**p_fbuf == '\t') for(i=0;i<TABSIZE;i++) *p_lbuf++ = 0x20;
        else if (**p_fbuf >= 0x20 && **p_fbuf < 0x80) *p_lbuf++ = **p_fbuf;
        (*p_fbuf)++;
        if (p_lbuf >= lbuf + screen.logical_width) break;
    }
    *p_lbuf = '\0';
    (*p_fbuf)++;        /* advance past the new line character */

    return (lbuf);
}


/* LINE_TO_BUFFER -- Moves the line from memory to the video buffer */
int line_to_buffer(BYTE *p_fbuf)
{
    BYTE far *p_vbuff, far *line, far *eob;
    BYTE *p_lbuf, att;
    unsigned i;

    p_vbuff = video.base + 2*screen.logical_width;
    att = (BCOLOR << 4) + FCOLOR;
    for (i = 0; i < 0X8000 - 2*screen.logical_width; i++) {
        p_vbuff[i++] = '\0';
        p_vbuff[i] = att;
    }
    eob = video.base + (0XFFFF - screen.logical_width);
    line = p_vbuff;
    while (*p_fbuf && p_vbuff < eob ) {
        p_lbuf = build_line(&p_fbuf);
        while (*p_lbuf) {
            *p_vbuff++ = *p_lbuf++;
            *p_vbuff++ = att;
        }
        /* go to next screen line */
        line += screen.logical_width*2;
        p_vbuff = line;
    }
    end_of_file = FP_OFF(p_vbuff);
    return (0);
}


/* SMOOTH_SCROLL scrolls the EGA video buffer the number of scan lines
indicated in "count" at a speed of "speed" scan lines per vertical retrace.
One retrace occurs each 60th of a second regardless of processor speed.
*/
void smooth_scroll(int count, unsigned speed)
{

    unsigned i;

    /* GET THE START ADDRESS OF THE SCREEN BUFFER */
    outportb(video.crtc, START_ADDRESS_HIGH);       /* High byte */
    screen.start_addr = inportb(video.crtc+1) << 8;
    outportb(video.crtc, START_ADDRESS_LOW);        /* Low byte */
    screen.start_addr |= inportb(video.crtc+1);

    /* count > 0 => scroll screen upwards. */
    /* i is the number of scan lines scrolled */
    if (count>0 && (screen.start_addr < end_of_buffer))
    for(i=0;i < count;) {
        if (vpel >= (int) video.char_ht) {
            vpel = vpel - video.char_ht;
            screen.start_addr += screen.logical_width;
        }
        if (vpel < 0) {
            if (screen.start_addr > screen.logical_width) {
                vpel += video.char_ht;
                screen.start_addr -= screen.logical_width;
            }
            else vpel = 0;
        }
        for(;vpel< (int) video.char_ht && i<count;vpel+=speed,i+=speed) {

            /* wait for a vertical retrace */
            while (!(inportb(video.isr) & 8));
            /* wait for horizontal or vertical retrace */
            while (inportb(video.isr) & 1);

            /* address the preset row scan register */
            outport(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);

            /* Reset the start address */
            set_start_addr(screen.start_addr);
        }
    }

    /* count < 0  => scroll screen characters downwards */
    /* i is the number of scan lines scrolled */
    if (count < 0) {
        if (vpel >= (int) video.char_ht) vpel -= video.char_ht;

        for(i=0;i < -count && screen.start_addr;) {
        /* This loop determines whether to update the start address */
        /* It is iterated once for each video.char_ht. */
            if (vpel < 0) {
                vpel = video.char_ht + vpel;
                if (screen.start_addr > screen.logical_width)
                    screen.start_addr -= screen.logical_width;
                else {
                    screen.start_addr = screen.a_start;
                    vpel=0;
                }
            }

            /* this loop moves the screen "speed" scan lines each time through */
            for(;vpel>=0 && i< -count;vpel-=speed,i+=speed) {

                /* wait for a vertical retrace */
                while (!(inportb(video.isr) & 8));
                /* wait for horizontal or vertical retrace */
                while (inportb(video.isr) & 1);

                /* address the preset row scan register */
                outport(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);

                /* Reset the start address */
                set_start_addr(screen.start_addr);
            }
        }
    }
}


/* SMOOTH_PAN
This function invokes smooth panning on the EGA/VGA in text mode.  The
function calculates the number of scan lines per row.  The speed variable
adjusts the speed in pixels per vertical retrace and takes values in the
range 1-8 for EGA color and 1-9 for monochrome and the VGA.
*/
int smooth_pan(int count, unsigned speed)
{
    unsigned i;

    /* count greater than zero (move viewport to the right) */
    if (count>0 && !right_edge) for(i=0;i<count;) {
        /* if we have scrolled a full character, reset start address */
        if (hpel >= 8) {
            screen.start_addr++;
            set_start_addr(screen.start_addr);
            if (!left_edge) if (!(screen.start_addr % (screen.logical_width)))
                right_edge = TRUE;
            left_edge = FALSE;
            if (!right_edge) {
                if (!(mono || vga.active)) hpel %= 8;    /* reset the pel counter */
                else {
                    hpel %= 9;
                    if (hpel == 8) {
                        set_pel_pan(hpel);
                        hpel += speed;
                        hpel %= 9;
                    }
                }
            }
            else {
                hpel = (mono || vga.active) ? 8 : 0;
                set_pel_pan(hpel);
                return (-1);
            }
        }
        for(;hpel < 8 && i < count;i+=speed, hpel+=speed) set_pel_pan(hpel);
    }

    /* count less than zero (move viewport to the left) */
    else if (count < 0) {
        if (mono || vga.active) {
            if (left_edge && hpel == 8) return (-1);
            if (hpel > 8) hpel -= 9;
        }
        else {
            if (left_edge && hpel == 0) return (-1);
            if (hpel > 7) hpel = hpel - 8;
        }
        for(i=0; i < (-count); ) {
        /* IF WE HAVE SCROLLED A FULL CHARACTER, RESET START ADDRESS */
            if (hpel<0 && !left_edge) {
                if ((mono || vga.active) && hpel == -1) {
                    hpel = 8;
                    set_pel_pan(hpel);
                    hpel = -1 - speed;
                }
                screen.start_addr--;
                right_edge = FALSE;
                hpel = (mono || vga.active) ? 9 + hpel : 8 + hpel;
                set_start_addr(screen.start_addr);
                if (!(screen.start_addr % screen.logical_width)) {
                    left_edge = TRUE;
                    screen.start_addr--;
                }
            }
            else if (hpel<0 && left_edge) i = (-count);

            for(;hpel >= 0 && i < (-count);i+=speed, hpel-=speed) set_pel_pan(hpel);
            if (left_edge && hpel < speed) {
                hpel = (mono || vga.active) ? 8 : 0;
                set_pel_pan(hpel);
                return (-1);
            }
        }
    }
    return (0);
}



/* SET_LOGICAL_SCREEN_WIDTH defines an EGA/VGA screen width for smooth panning
and sets the the global variable "screen.cols."  screen.cols must be restored
when smooth panning is finished or subsequent screen writes will address the
wrong screen locations.
l_width is the width of the logical screen in bytes. Max 512.
*/
unsigned set_logical_screen_width(unsigned l_width)
{
    l_width = (l_width > 512) ? 512 : l_width;

    /* set screen_cols */
    screen.cols = l_width;

    /* set logical screen width */
    l_width >>= 1;                                  /* convert to words */
    outport(video.crtc, (l_width << 8) | 0x013);

    return (l_width << 1);
}


/* SET_START_ADDRESS -- Sets the start address of the screen. I.e. the  */
/* address that occupies the top left of the screen.                    */
void set_start_addr(unsigned start_addr)
{
    /* address the start address high register */
    outport(video.crtc, (start_addr & 0XFF00) | START_ADDRESS_HIGH);

    /* address the start address low register */
    outport(video.crtc, (start_addr << 8) | START_ADDRESS_LOW);
}


/* SET_PEL_PAN -- Sets the horizontal pel panning register to the value     */
/* specified in "hpel".  Called by smooth_pan()                             */
void set_pel_pan(int hpel)
{
    /* first wait for end of vertical retrace */
    while (inportb(video.isr) & 8);

    /* wait for next vertical retrace */
    while (!(inportb(video.isr) & 8));

    /* address the horizontal pel paning register */
    outportb(AC_INDEX, AC_HPP);
    outportb(AC_INDEX, hpel);
}


/* SET_LINE_COMPARE -- Sets the scan line at which the screen is split.     */
void set_line_compare(unsigned scan_line)
{
    BYTE old_reg;
    if (scan_line > 256) {          /* set bit 4 of the overflow register */
        if (ega.active) {
            if ((mode >= 4 && mode <= 7) || (mode == 0X0D || mode == 0X0E))
                outport(video.crtc, (0X11 << 8) | CRTC_OVERFLOW);
            else outport(video.crtc, (0X1F << 8) | CRTC_OVERFLOW);
            if (mode < 4 && !video.ecd) outport(video.crtc, (0X11 << 8) | CRTC_OVERFLOW);
        }
        else if (vga.active) {      /* ahhh... readable registers */
            /* set bit 8 of line compare reg., which is bit 4 of the CRTC overflow reg. */
            outportb(video.crtc,CRTC_OVERFLOW);
            old_reg = (BYTE) inportb(video.crtc+1);
            outportb(video.crtc+1,old_reg | 0X10);
            /* set bit 9 of line compare reg., which is bit 6 of
            the CRTC max. scan line register */

            outportb(video.crtc,MAX_SCAN_LINE);
            old_reg = (BYTE) inportb(video.crtc+1);
            if (old_reg | 0X40) outportb(video.crtc+1,old_reg & 0XBF);
        }
        scan_line -= 256;
    }
    else {
        if (ega.active) {
            /* clear bit 8 of the line compare register (which is stored as bit */
            /* 4 of the CRTC overflow register).  This register is write only.  */
            if (!video.ecd && mode < 4) outport(video.crtc, (1 << 8) | CRTC_OVERFLOW);
            else if (video.ecd && mode < 4) outport(video.crtc, (0XF << 8) | CRTC_OVERFLOW);
            if ((mode >= 4 && mode < 7) || (mode == 0XD || mode == 0XE) ) outport(video.crtc, (1 << 8) | CRTC_OVERFLOW);
            if (mode == 7 || mode > 0X0E) outport(video.crtc, (0XF << 8) | CRTC_OVERFLOW);
        }
        if (vga.active) {
            outportb(video.crtc,CRTC_OVERFLOW); /* clear bit 8 of line compare reg. */
            old_reg = (BYTE) inportb(video.crtc+1);
            outportb(video.crtc,((old_reg & 0XEF) << 8) | CRTC_OVERFLOW);
            outportb(video.crtc,MAX_SCAN_LINE);  /* clear bit 9 of line compare reg. */
            old_reg = (BYTE) inportb(video.crtc+1);
            outportb(video.crtc,((old_reg & 0XBF) << 8) | MAX_SCAN_LINE);
        }
    }
    outport(video.crtc, (scan_line << 8) | LINE_COMPARE);
}


/************************** UTILITY FUNCTIONS *********************************/


/* LOAD_EGA_COLOR -- Load an EGA or VGA palette register with a selected color */
void load_ega_color(BYTE pregister, BYTE color)
{
    union REGS regs;

    regs.x.ax = 0x1000;     /* Service 0. Set an individual palette register */
    regs.h.bl = pregister;  /* BL holds the register to be set (0-15) */
    regs.h.bh = color;      /* BH hols the new color */
    int86(VIDEO, &regs, &regs);
}


/* SET_VIDEO_MODE -- sets the active video mode. */
unsigned set_video_mode(BYTE m)
{
    union REGS regs;

    regs.h.ah = 0;          /* service 0 - set current video mode */
    regs.h.al = m;          /* requested mode # */
    int86(VIDEO, &regs, &regs);

    return m;
}


/* GET_V_MODE -- Returns the video mode and sets the global variables       */
/* screen.cols (# of screen columns), screen.rows (# of screen rows)        */
/* video.base (screen buffer starting address), and mono.                   */
int get_v_mode(void)
{
    union REGS regs;

    regs.h.ah = 0xf;            /* get current video mode */
    int86(VIDEO, &regs, &regs);
    mode = regs.h.al;           /* current mode # */
    screen.cols = regs.h.ah;
    regs.h.dl = 0x18;           /* dummy #rows for CGA compatibility */
    regs.h.bh = 0;
    regs.x.ax = 0x1130;         /* BIOS function 0x11, sub-function 0x30 */
    int86(VIDEO,&regs,&regs);
    screen.rows = regs.h.dl + 1;/* screen rows returned in DL */
    if (mode == 7) {
        video.base = (BYTE far *) 0XB0000000L;
        mono = TRUE;
    }
    else {
        video.base = (BYTE far *) 0XB8000000L;
        mono = FALSE;
    }

    return (mode);
}


/* GET_V_CONFIG -- A portmanteau routine to determine if an EGA, VGA or     */
/* neither is present and, if an EGA or VGA is present, to set the          */
/* following video parameters:                                              */
/* vga.present = TRUE if a VGA is present in the system.                    */
/* vga.active = TRUE if the VGA is the active adapter.                      */
/* ega.present = TRUE if an EGA is present in the system.                   */
/* ega.active = TRUE if an EGA is the active adapter.                       */
/* video.ram = amount of video RAM installed on the adapter.                */
/* video.color = TRUE if the EGA/VGA drives a color display.                */
/* video.ecd = TRUE if the EGA/VGA drives an enhanced display.              */
/* video.ev_active = TRUE if the EGA or the VGA is active.                  */
/* This function should be preceeded by a call to get_v_mode().             */
void get_v_config(void)
{

    union REGS regs;
    BYTE e_byte;           /* EGA information byte in ROM data area */

    /* First, check for the presence of an VGA */
    regs.x.ax = 0x1a00;     /* Function call 0x1a -- Tech. Ref./2 p. 3-24 */
    int86(VIDEO, &regs, &regs);         /* Function supported => VGA */
    if (regs.h.al == 0x1a) {
        vga.present = TRUE;
        if (regs.h.bl < 7) vga.active = FALSE;
        else vga.active = TRUE;
    }
    else {
        vga.present = FALSE;
        vga.active = FALSE;
    }
    /* Check for the presence of an EGA and set relevant EGA/VGA parameters */
    regs.h.ah = 0x12;       /* EGA BIOS alternate select. Tech. Ref. p106. */
    regs.h.bl = 0x10;       /* return EGA information.              */
    int86(VIDEO, &regs, &regs);
    if (regs.h.bl == 0x10 || vga.present) ega.present = FALSE;
    else ega.present = TRUE;

    /* If an EGA or VGA is present then set some important parameters */
    if (ega.present || vga.present) {   /* EGA/VGA is present -- is it active? */
        video.ram = regs.h.bl*64 + 64;  /* EGA/VGA installed memory */
        if (regs.h.bh) video.color = FALSE; /* EGA/VGA drives a mono monitor */
        else video.color = TRUE;            /* EGA/VGA drives a color monitor */

        /* See if EGA/VGA drives an Enhanced Color Display */
        if (video.color) {
            if (regs.h.cl == 3 || regs.h.cl == 9) video.ecd = TRUE;
            else video.ecd = FALSE;
        }
        e_byte = peekb(0,0x487);                    /* EGA info. byte */
        if (e_byte & 8) video.ev_active = FALSE;    /* EGA not active */
        else {
            video.ev_active = TRUE;                 /* EGA is active */
            if (!vga.active) ega.active = TRUE;
        }
    }

    if (vga.active) {
        regs.x.ax = 0X1202;
        regs.h.bl = 0X30;
        int86(VIDEO, &regs, &regs);
    }
    /* check # of screen rows for font size */
    if (vga.active) video.char_ht = (BYTE) (400/screen.rows);
    else if (ega.active) if (video.ecd || mono) video.char_ht = (BYTE) (350/screen.rows);
    else video.char_ht = (BYTE) (200/screen.rows);
}


/* GET_KEY -- Returns the scan code of the next key in the keyboard buffer. */
unsigned get_key(void)
{
    union REGS regs;

    regs.h.ah = 0;     /* read next keyboard character */
    int86(KEYBOARD, &regs, &regs);
    return ((unsigned) regs.h.ah);
}


/* GET_CURSOR_ SIZE -- Get the cursor size on the active page.         */
int get_cursor_size(void)
{
    union REGS regs;
    int hi,lo;

    regs.h.ah = 3;                 /* request cursor size */
    regs.h.bh = 0;                 /* from page 0 */
    int86(VIDEO, &regs, &regs);    /* ROM BIOS video service */
    hi = regs.h.ch;                /* top scan line of cursor */
    lo = regs.h.cl;                /* bottom scan line of cursor */
    return (hi << 8) | lo;         /* combine for return */
}


/* SET CURSOR SIZE sets the cursor size. top_line=bottom_line=0 turns it off           */
void set_cursor_size(BYTE top_line, BYTE bottom_line)
{
    union REGS regs;

    regs.h.ah = 1;                      /* BIOS function 1, set cursor size */
    if (top_line == 0 && bottom_line == 0) regs.h.ch = 32; /* request cursor off */
    else {
        regs.h.ch = top_line;               /* row */
        regs.h.cl = bottom_line;            /* column */
    }
    int86(VIDEO, &regs, &regs);
}


/* ERROR -- A simple error-handler */
void error(char *fs,...)
{
    va_list argptr;
    va_start(argptr,fs);
    vfprintf(stderr,fs,argptr);
    va_end(argptr);
    exit(1);
}


#if defined(__WATCOMC__)
/*
WATCOM C does not access the DOS file system in a fashion similar to the other
compilers so we write our own "findfirst()" function.  WATCOM C uses a
function called opendir(), which could be used instead.
*/

/* NOTE: THE FOLLOWING IMPLEMENTATION IS VALID ONLY IN SMALL DATA MODELS */
int findfirst(const char *pathname, struct ffblk *ffblk, int attrib)
{
    union REGS regs;
    struct SREGS sregs;

    segread(&sregs);
    regs.h.ah = 0X1A;               /* DOS set DTA function         */
    regs.x.dx = (unsigned) ffblk;   /* address of new DTA address   */
    intdosx(&regs, &regs, &sregs);  /* this reads the registers...  */

    regs.h.ah = 0X4E;               /* DOS find first matching file */
    regs.x.cx = attrib;             /* search attribute             */
    regs.x.dx = (unsigned) pathname;/* DS:DX is address of pathname */
    intdosx(&regs, &regs, &sregs);
    return (regs.x.ax);
}
#endif


/*
SMOOTHLIB--A library of C language routines to do smooth scrolling and panning on the EGA and the VGA.
by Andrew J. Chalk.
SYSTEM REQUIREMENTS: EGA or VGA. Enhanced Color, Monochrome or Analog Display.
COMPILER SUPPORT: MSC 5.0+/Quick C
Simple compiler invocation (suggested):
    MSC:            "CL /AS sb.c"
First version: 3/5/88.  Last update 09-01-88.
*/

#include <conio.h>
#include <stdio.h>



typedef unsigned char BYTE;     /* A convenient new data type */



/* MANIFEST CONSTANTS */
#define LOGICAL_WIDTH 132   /* EGA/VGA logical screen width (bytes, max. 512) */
#define TRUE            1
#define FALSE       !TRUE

/* Macros to make a far pointer and examine at a byte in memory */
#define MK_FP(seg,ofs)  ((void far *)((((unsigned long)(seg)) << 16) | (ofs)))
#define peekb(a,b)      (*((char far*)MK_FP((a),(b))))



/* EGA AND VGA REGISTER VALUES */
#define PRESET_ROW_SCAN     8     /* Address of preset row scan reg. of CRTC */
#define START_ADDRESS_HIGH  0X0C  /* Address of start address h reg. of CRTC */
#define START_ADDRESS_LOW   0X0D  /* Address of start address l reg. of CRTC */
#define AC_INDEX            0X3C0 /* Attribute Controller Index Register     */
#define AC_HPP              0X13 | 0X20   /* Horizontal Pel Panning Register */
#define AC_MCR              0X10  /* Attribute Controller Mode Control Reg.  */
#define LINE_COMPARE        0X18  /* CRTC line compare register.             */
#define CRTC_OVERFLOW       0X07  /* CRTC overflow register.                 */
#define MAX_SCAN_LINE       0X09  /* CRTC maximum scan line register.        */



/* GLOBAL VARIABLES */
unsigned end_of_buffer;         /* address of the end of the video buffer   */
unsigned right_edge, left_edge; /* TRUE if we are at the edge of the screen */
int vpel, hpel;                 /* vertical pel height, etc.                */
int mode, mono;                 /* video mode, monochrome (TRUE or FALSE)   */



/* STRUCTURE DEFINITIONS
The following four structures define convenients form in which to store
video information and can be extended with additional members.
We will declare one of each type (below).
*/
struct video_descriptor {
    unsigned ev_active;             /* if TRUE, an EGA or VGA is active     */
    unsigned color;                 /* if TRUE, ega drives color monitor    */
    unsigned ecd;                   /* if TRUE, Enhanced Color Display      */
    unsigned ram;                   /* size of EGA memory (in k)            */
    BYTE char_ht, char_wi;          /* character height and width (EGA/VGA) */
    BYTE far *base;                 /* start address of the video buffer    */
    unsigned address;               /* start addr. of the active video page */
    unsigned isr;                   /* EGA/VGA input status register address*/
    unsigned crtc;                  /* CRT Controller Register address      */
};


/* We place all the information related to the screen in one structure */
struct screen_descriptor {
    unsigned rows;
    unsigned cols;
    unsigned a_start;      /* start address of screen A in split screen mode */
    unsigned logical_width;     /* screen logical width in words. max: 0x100 */
    unsigned start_addr;        /* screen start address. max: 0X4000 */
};


/* This structure contains information specific to the EGA */
struct enhanced_graphics_adapter {
    unsigned present;               /* if TRUE, EGA present */
    unsigned active;                /* if TRUE, EGA active */
};


/* This structure contains information specific to the VGA */
struct video_graphics_array {
    unsigned present;               /* if TRUE, VGA present */
    unsigned active;                /* if TRUE, EGA present */

};


/* STRUCTURE DECLARATIONS */
struct video_descriptor video;
struct screen_descriptor screen;
struct enhanced_graphics_adapter ega;
struct video_graphics_array vga;



/* FUNCTION PROTOTYPES -- In order of appearance */
void        smooth_scroll(int count, unsigned speed);
int         smooth_pan(int count, unsigned speed);
unsigned    set_logical_screen_width(unsigned l_width);
void        set_start_addr(unsigned start_addr);
void        set_pel_pan(int hpel);



/*** FUNCTION DEFINITIONS BEGIN HERE ***/

/*
SMOOTH_SCROLL scrolls the EGA/VGA video buffer in text mode the number of
scan lines indicated in "count" at a speed of "speed" scan lines per
vertical retrace.
*/
void smooth_scroll(int count, unsigned speed)
{

    unsigned i;

    /* GET THE START ADDRESS OF THE SCREEN BUFFER */
    outp(video.crtc, START_ADDRESS_HIGH);       /* High byte */
    screen.start_addr = inp(video.crtc+1) << 8;
    outp(video.crtc, START_ADDRESS_LOW);        /* Low byte */
    screen.start_addr |= inp(video.crtc+1);

    /* COUNT > 0 => SCROLL SCREEN UPWARDS. */
    /* i is the number of scan lines scrolled */
    if (count>0 && (screen.start_addr < end_of_buffer))
    for(i=0;i < count;) {
        if (vpel >= (int) video.char_ht) {
            vpel = vpel - video.char_ht;
            screen.start_addr += screen.logical_width;
        }
        if (vpel < 0) {
            if (screen.start_addr > screen.logical_width) {
                vpel += video.char_ht;
                screen.start_addr -= screen.logical_width;
            }
            else vpel = 0;
        }
        for(;vpel< (int) video.char_ht && i<count;vpel+=speed,i+=speed) {

            /* wait for a vertical retrace */
            while (!(inp(video.isr) & 8));
            /* wait for horizontal or vertical retrace */
            while (inp(video.isr) & 1);

            /* address the preset row scan register */
            outpw(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);

            /* Reset the start address */
            set_start_addr(screen.start_addr);
        }
    }

    /* COUNT < 0  => SCROLL SCREEN CHARACTERS DOWNWARDS */
    /* i is the number of scan lines scrolled */
    if (count < 0) {
        if (vpel >= (int) video.char_ht) vpel -= video.char_ht;

        for(i=0;i < -count && screen.start_addr;) {
        /* This loop determines whether to update the start address */
        /* It is iterated once for each video.char_ht. */
            if (vpel < 0) {
                vpel = video.char_ht + vpel;
                if (screen.start_addr > screen.logical_width)
                    screen.start_addr -= screen.logical_width;
                else {
                    screen.start_addr = screen.a_start;
                    vpel=0;
                }
            }

            /* this loop moves the screen "speed" scan lines each time through */
            for(;vpel>=0 && i< -count;vpel-=speed,i+=speed) {

                /* wait for a vertical retrace */
                while (!(inp(video.isr) & 8));
                /* wait for horizontal or vertical retrace */
                while (inp(video.isr) & 1);

                /* address the preset row scan register */
                outpw(video.crtc, (vpel << 8) | PRESET_ROW_SCAN);

                /* Reset the start address */
                set_start_addr(screen.start_addr);
            }
        }
    }
}



/*
SMOOTH_PAN -- This function invokes smooth panning on the EGA/VGA in
text mode.  The function calculates the number of scan lines per row.
The speed variable adjusts the speed in pixels per vertical retrace and
takes values in the range 1-8 for EGA color and 1-9 for monochrome and
the VGA.
*/
int smooth_pan(int count, unsigned speed)
{
    unsigned i;

    /* count greater than zero (move viewport to the right) */
    if (count>0 && !right_edge) for(i=0;i<count;) {
        /* if we have scrolled a full character, reset start address */
        if (hpel >= 8) {
            screen.start_addr++;
            set_start_addr(screen.start_addr);
            if (!left_edge) if (!(screen.start_addr % (screen.logical_width)))
                right_edge = TRUE;
            left_edge = FALSE;
            if (!right_edge) {
                if (!(mono || vga.active)) hpel %= 8;    /* reset the pel counter */
                else {
                    hpel %= 9;
                    if (hpel == 8) {
                        set_pel_pan(hpel);
                        hpel += speed;
                        hpel %= 9;
                    }
                }
            }
            else {
                hpel = (mono || vga.active) ? 8 : 0;
                set_pel_pan(hpel);
                return (-1);
            }
        }
        for(;hpel < 8 && i < count;i+=speed, hpel+=speed) set_pel_pan(hpel);
    }

    /* count less than zero (move viewport to the left) */
    else if (count < 0) {
        if (mono || vga.active) {
            if (left_edge && hpel == 8) return (-1);
            if (hpel > 8) hpel -= 9;
        }
        else {
            if (left_edge && hpel == 0) return (-1);
            if (hpel > 7) hpel = hpel - 8;
        }
        for(i=0; i < (-count); ) {
        /* IF WE HAVE SCROLLED A FULL CHARACTER, RESET START ADDRESS */
            if (hpel<0 && !left_edge) {
                if ((mono || vga.active) && hpel == -1) {
                    hpel = 8;
                    set_pel_pan(hpel);
                    hpel = -1 - speed;
                }
                screen.start_addr--;
                right_edge = FALSE;
                hpel = (mono || vga.active) ? 9 + hpel : 8 + hpel;
                set_start_addr(screen.start_addr);
                if (!(screen.start_addr % screen.logical_width)) {
                    left_edge = TRUE;
                    screen.start_addr--;
                }
            }
            else if (hpel<0 && left_edge) i = (-count);

            for(;hpel >= 0 && i < (-count);i+=speed, hpel-=speed) set_pel_pan(hpel);
            if (left_edge && hpel < speed) {
                hpel = (mono || vga.active) ? 8 : 0;
                set_pel_pan(hpel);
                return (-1);
            }
        }
    }
    return (0);
}



/*
SET_LOGICAL_SCREEN_WIDTH defines an EGA/VGA screen width (in bytes) for
smooth panning and sets the the global variable "screen.cols."
screen.cols is the number of screen columns and must be restored when
smooth panning is finished or subsequent screen writes will address the
wrong screen locations.
l_width is the width of the logical screen in bytes. Max 512.
*/
unsigned set_logical_screen_width(unsigned l_width)
{
    l_width = (l_width > 512) ? 512 : l_width;

    /* set screen_columns */
    screen.cols = l_width;

    /* set logical screen width */
    l_width >>= 1;                                  /* convert to words */
    outpw(video.crtc, (l_width << 8) | 0x013);

    return (l_width << 1);
}



/*
SET_START_ADDRESS -- Sets the start address of the screen. I.e. the
address that occupies the top left of the screen.
*/
void set_start_addr(unsigned start_addr)
{
    /* address the start address high register */
    outpw(video.crtc, (start_addr & 0XFF00) | START_ADDRESS_HIGH);

    /* address the start address low register */
    outpw(video.crtc, (start_addr << 8) | START_ADDRESS_LOW);
}



/*
SET_PEL_PAN -- Sets the horizontal pel panning register to the value
specified in "hpel".  Called by smooth_pan()
*/
void set_pel_pan(int hpel)
{
    /* first wait for end of vertical retrace */
    while (inp(video.isr) & 8);

    /* wait for next vertical retrace */
    while (!(inp(video.isr) & 8));

    /* address the horizontal pel paning register */
    outp(AC_INDEX, AC_HPP);
    outp(AC_INDEX, hpel);
}
