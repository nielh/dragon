#ifndef FAT_H
#define FAT_H

#include "kernel.h"

/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_APPEND 0x0008
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_EXCL 0x0400
#define O_TEXT 0x4000
#define O_BINARY 0x8000
#define O_WTEXT 0x10000
#define O_U16TEXT 0x20000
#define O_U8TEXT 0x40000
#define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR)

struct FAT_DOSTIME
{
    unsigned int twosecs :5; // 2-second increments
    unsigned int minutes :6; // minutes
    unsigned int hours :5; // hours (0-23)
} PACKED;

struct FAT_DOSDATE
{
    unsigned int date :5; // date (1-31)
    unsigned int month :4; // month (1-12)
    unsigned int year :7; // year - 1980
} PACKED;

struct FAT_ATTRIBUTE
{
    int readonly :1;
    int hidden :1;
    int system :1;
    int volumelabel :1;
    int directory :1;
    int archive :1;
    int reserved :2;
} PACKED;

#define FAT_NAMESIZE		8
#define FAT_EXTENSIONSIZE	3


struct FAT_ENTRY
{
    u8 name[FAT_NAMESIZE];
    u8 extention[FAT_EXTENSIONSIZE];
    struct FAT_ATTRIBUTE attribute;
    u8 reserved[2];
    struct FAT_DOSTIME ctime;
    struct FAT_DOSDATE cdate;
    struct FAT_DOSDATE adate;
    u16 start_clusterHI;
    struct FAT_DOSTIME wtime;
    struct FAT_DOSDATE wdate;
    u16 start_clusterLO;
    u32 file_size;

} PACKED;

void fat_init();

#endif //
