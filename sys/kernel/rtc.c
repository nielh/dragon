
#include "kernel.h"
#include "port.h"
#include "libc.h"
#include "rtc.h"

#define CMOS_PORT   0x70
#define CMOS_DATA   0x71

#define SECS    0x00
#define MINS    0x02
#define HOURS   0x04
#define DAY     0x07
#define MONTH   0x08
#define YEAR    0x09

#define CMOS_STATA   0x0a
#define CMOS_STATB   0x0b
#define CMOS_UIP     (1 << 7)        // RTC update in progress


static u8 cmos_read(u8 reg)
{
    outb(CMOS_PORT, reg);
    return inb(CMOS_DATA);
}

static void fill_rtcdate(rtcdate *r)
{
    r->second = cmos_read(SECS);
    r->minute = cmos_read(MINS);
    r->hour   = cmos_read(HOURS);
    r->day    = cmos_read(DAY);
    r->month  = cmos_read(MONTH);
    r->year   = cmos_read(YEAR);
}

// qemu seems to use 24-hour GWT and the values are BCD encoded
void cmostime(rtcdate *r)
{
	rtcdate t1 = {0}, t2= {0};
    int sb, bcd;

    sb = cmos_read(CMOS_STATB);
    bcd = (sb & (1 << 2)) == 0;

    // make sure CMOS doesn't modify time while we read it
    for (;;)
    {
        fill_rtcdate(&t1);
        if (cmos_read(CMOS_STATA) & CMOS_UIP)
            continue;

        fill_rtcdate(&t2);
        if (memcmp(&t1, &t2, sizeof(t1)) == 0)
            break;
    }

    // convert
    if (bcd)
    {
        #define    CONV(x)     (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))

        CONV(second);
        CONV(minute);
        CONV(hour  );
        CONV(day   );
        CONV(month );
        CONV(year  );
    }

    *r = t1;
    r->year += 2000;
}
