

#ifndef RTC_H
#define RTC_H

#include "kernel.h"

typedef struct rtcdate
{
  u8 second;
  u8 minute;
  u8 hour;
  u8 day;
  u16 month;
  u16 year;

} rtcdate;

void cmostime(rtcdate *r);

#endif //



