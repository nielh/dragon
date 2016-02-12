#include "kernel.h"
#include "printk.h"
#include "libc.h"
#include "isr.h"
#include "thread.h"
#include "object.h"
#include "vsnprintf.h"
#include "hd.h"
#include "pci.h"
#include "page.h"
#include "dir.h"
#include "timer.h"
#include "gdt.h"
#include "port.h"

#define SECTORSIZE 512
#define CDSECTORSIZE 2048

#define HD_CONTROLLERS 2
#define HD_DRIVES 4
#define HD_PARTITIONS 4

#define MAX_PRDS (PAGESIZE / 8)
#define MAX_DMA_XFER_SIZE ((MAX_PRDS - 1) * PAGESIZE)

#define HDC0_IOBASE 0x01F0
#define HDC1_IOBASE 0x0170

#define HD0_DRVSEL 0x00 // was:0xA0
#define HD1_DRVSEL 0x10 // was:0xB0
#define HD_LBA 0x40

#define idedelay() delay(25)

//
// Controller registers
//

#define HDC_DATA 0x0000
#define HDC_ERR 0x0001
#define HDC_FEATURE 0x0001
#define HDC_SECTORCNT 0x0002
#define HDC_SECTOR 0x0003
#define HDC_TRACKLSB 0x0004
#define HDC_TRACKMSB 0x0005
#define HDC_DRVHD 0x0006
#define HDC_STATUS 0x0007
#define HDC_COMMAND 0x0007
#define HDC_DEVCTRL 0x0008
#define HDC_ALT_STATUS 0x0206
#define HDC_CONTROL 0x0206

//
// Drive commands
//

#define HDCMD_NULL 0x00
#define HDCMD_IDENTIFY 0xEC
#define HDCMD_RESET 0x10
#define HDCMD_DIAG 0x90
#define HDCMD_READ 0x20
#define HDCMD_WRITE 0x30
#define HDCMD_PACKET 0xA0
#define HDCMD_PIDENTIFY 0xA1
#define HDCMD_MULTREAD 0xC4
#define HDCMD_MULTWRITE 0xC5
#define HDCMD_SETMULT 0xC6
#define HDCMD_READDMA 0xC8
#define HDCMD_WRITEDMA 0xCA
#define HDCMD_SETFEATURES 0xEF
#define HDCMD_FLUSHCACHE 0xE7

//
// Controller status
//

#define HDCS_ERR 0x01  // Error
#define HDCS_IDX 0x02  // Index
#define HDCS_CORR 0x04 // Corrected data
#define HDCS_DRQ 0x08  // Data request
#define HDCS_DSC 0x10  // Drive seek complete
#define HDCS_DWF 0x20  // Drive write fault
#define HDCS_DRDY 0x40 // Drive ready
#define HDCS_BSY 0x80  // Controller busy
//
// Device control
//

#define HDDC_HD15 0x00 // Use 4 bits for head (not used, was 0x08)
#define HDDC_SRST 0x04 // Soft reset
#define HDDC_NIEN 0x02 // Disable interrupts
//
// Feature bits
//

#define HDFEAT_ENABLE_WCACHE 0x02  // Enable write caching
#define HDFEAT_XFER_MODE 0x03      // Set transfer mode
#define HDFEAT_DISABLE_RLA 0x55    // Disable read-lookahead
#define HDFEAT_DISABLE_WCACHE 0x82 // Disable write caching
#define HDFEAT_ENABLE_RLA 0xAA     // Enable read-lookahead
//
// Transfer modes
//

#define HDXFER_MODE_PIO 0x00
#define HDXFER_MODE_WDMA 0x20
#define HDXFER_MODE_UDMA 0x40

//
// Controller error conditions
//

#define HDCE_AMNF 0x01  // Address mark not found
#define HDCE_TK0NF 0x02 // Track 0 not found
#define HDCE_ABRT 0x04  // Abort
#define HDCE_MCR 0x08   // Media change requested
#define HDCE_IDNF 0x10  // Sector id not found
#define HDCE_MC 0x20    // Media change
#define HDCE_UNC 0x40   // Uncorrectable data error
#define HDCE_BBK 0x80   // Bad block
//
// Timeouts (in ms)
//

#define HDTIMEOUT_DRDY 5000
#define HDTIMEOUT_DRQ 5000
#define HDTIMEOUT_CMD 1000
#define HDTIMEOUT_BUSY 60000
#define HDTIMEOUT_XFER 10000

//
// Drive interface types
//

#define HDIF_NONE 0
#define HDIF_PRESENT 1
#define HDIF_UNKNOWN 2
#define HDIF_ATA 3
#define HDIF_ATAPI 4

//
// IDE media types
//

#define IDE_FLOPPY 0x00
#define IDE_TAPE 0x01
#define IDE_CDROM 0x05
#define IDE_OPTICAL 0x07
#define IDE_SCSI 0x21
#define IDE_DISK 0x20

//
// ATAPI commands
//

#define ATAPI_CMD_REQUESTSENSE 0x03
#define ATAPI_CMD_READCAPICITY 0x25
#define ATAPI_CMD_READ10 0x28

//
// Transfer type
//

#define HD_XFER_IDLE 0
#define HD_XFER_READ 1
#define HD_XFER_WRITE 2
#define HD_XFER_DMA 3
#define HD_XFER_IGNORE 4

//
// Bus master registers
//

#define BM_COMMAND_REG 0 // Offset to command reg
#define BM_STATUS_REG 2  // Offset to status reg
#define BM_PRD_ADDR 4    // Offset to PRD addr reg
//
// Bus master command register flags
//

#define BM_CR_MASK_READ 0x00  // Read from memory
#define BM_CR_MASK_WRITE 0x08 // Write to memory
#define BM_CR_MASK_START 0x01 // Start transfer
#define BM_CR_MASK_STOP 0x00  // Stop transfer
//
// Bus master status register flags
//

#define BM_SR_MASK_SIMPLEX 0x80 // Simplex only
#define BM_SR_MASK_DRV1 0x40    // Drive 1 can do dma
#define BM_SR_MASK_DRV0 0x20    // Drive 0 can do dma
#define BM_SR_MASK_INT 0x04     // INTRQ signal asserted
#define BM_SR_MASK_ERR 0x02     // Error
#define BM_SR_MASK_ACT 0x01     // Active
//
// Parameters returned by read drive parameters command
//

#define MBR_SIGNATURE 0xAA55
#pragma pack(1)

struct disk_partition
{
    unsigned char bootid;  // Bootable?  0=no, 128=yes
    unsigned char beghead; // Beginning head number
    unsigned char begsect; // Beginning sector number
    unsigned char begcyl;  // 10 bit nmbr, with high 2 bits put in begsect
    unsigned char systid;  // Operating System type indicator code
    unsigned char endhead; // Ending head number
    unsigned char endsect; // Ending sector number
    unsigned char endcyl;  // Also a 10 bit nmbr, with same high 2 bit trick
    unsigned int relsect;  // First sector relative to start of disk
    unsigned int numsect;  // Number of sectors in partition
};

struct master_boot_record
{
    char bootstrap[446];
    struct disk_partition parttab[4];
    unsigned short signature;
};

struct hdparam
{
    unsigned short config;    // General configuration bits
    unsigned short cylinders; // Cylinders
    unsigned short reserved;
    unsigned short heads;          // Heads
    unsigned short unfbytespertrk; // Unformatted bytes/track
    unsigned short unfbytes;       // Unformatted bytes/sector
    unsigned short sectors;        // Sectors per track
    unsigned short vendorunique[3];
    char serial[20];           // Serial number
    unsigned short buffertype; // Buffer type
    unsigned short buffersize; // Buffer size, in 512-byte units
    unsigned short necc;       // ECC bytes appended
    char rev[8];               // Firmware revision
    char model[40];            // Model name
    unsigned char nsecperint;  // Sectors per interrupt
    unsigned char resv0;       // Reserved
    unsigned short usedmovsd;  // Can use double word read/write?
    unsigned short caps;       // Capabilities
    unsigned short resv1;      // Reserved
    unsigned short pio; // PIO data transfer cycle timing (0=slow, 1=medium, 2=fast)
    unsigned short dma; // DMA data transfer cycle timing (0=slow, 1=medium, 2=fast)
    unsigned short valid;      // Flag valid fields to follow
    unsigned short logcyl;     // Logical cylinders
    unsigned short loghead;    // Logical heads
    unsigned short logspt;     // Logical sector/track
    unsigned short cap0;       // Capacity in sectors (32-bit)
    unsigned short cap1;
    unsigned short multisec;    // Multiple sector values
    unsigned short totalsec0;   // Total number of user sectors
    unsigned short totalsec1;   //  (LBA; 32-bit value)
    unsigned short dmasingle;   // Single-word DMA info
    unsigned short dmamulti;    // Multi-word DMA info
    unsigned short piomode;     // Advanced PIO modes
    unsigned short minmulti;    // Minimum multiword xfer
    unsigned short multitime;   // Recommended cycle time
    unsigned short minpio;      // Min PIO without flow ctl
    unsigned short miniodry;    // Min with IORDY flow
    unsigned short resv2[6];    // Reserved
    unsigned short queue_depth; // Queue depth
    unsigned short resv3[4];    // Reserved
    unsigned short vermajor;    // Major version number
    unsigned short verminor;    // Minor version number
    unsigned short cmdset1;     // Command set supported
    unsigned short cmdset2;
    unsigned short cfsse;        // Command set-feature supported extensions
    unsigned short cfs_enable_1; // Command set-feature enabled
    unsigned short cfs_enable_2; // Command set-feature enabled
    unsigned short csf_default;  // Command set-feature default
    unsigned short dmaultra; // UltraDMA mode (0:5 = supported mode, 8:13 = selected mode)

    unsigned short word89;          // Reserved (word 89)
    unsigned short word90;          // Reserved (word 90)
    unsigned short curapmvalues;    // Current APM values
    unsigned short word92;          // Reserved (word 92)
    unsigned short hw_config;       // Hardware config
    unsigned short words94_125[32]; // Reserved words 94-125
    unsigned short last_lun;        // Reserved (word 126)
    unsigned short word127;         // Reserved (word 127)
    unsigned short dlf;             // Device lock function
    // 15:9	reserved
    // 8	security level 1:max 0:high
    // 7:6	reserved
    // 5	enhanced erase
    // 4	expire
    // 3	frozen
    // 2	locked
    // 1	en/disabled
    // 0	capability

    unsigned short csfo; // Current set features options
    // 15:4 reserved
    // 3	 auto reassign
    // 2	 reverting
    // 1	 read-look-ahead
    // 0	 write cache

    unsigned short words130_155[26]; // Reserved vendor words 130-155
    unsigned short word156;
    unsigned short words157_159[3];  // Reserved vendor words 157-159
    unsigned short words160_255[96]; // Reserved words 160-255
};

struct hd;

struct prd
{
    unsigned long addr;
    int len;
};

typedef int dev_t;
// typedef int int;
typedef int blkno_t;
#define PAGESIZE 4096

struct hdc
{
    // struct mutex lock;	               // Controller mutex
    // struct event ready;	               // Controller interrupt event
    // struct interrupt intr;               // Interrupt object
    // struct dpc xfer_dpc;                 // DPC for data transfer

    sem_t lock;
    sem_t ready;
    int status; // Controller status

    int iobase;    // I/O port registers base address
    int irq;       // IRQ for controller
    u16 bmregbase; // Busmaster register base

    char *bufp;        // Buffer pointer for next transfer
    int nsects;        // Number of sectors left to transfer
    int result;        // Result of transfer
    int dir;           // Transfer direction
    struct hd *active; // Active drive for transfer

    struct prd *prds;        // PRD list for DMA transfer
    void* prds_phys; // Physical address of PRD list
};

struct partition
{
    ops_t* ops;
    struct hd *hd;
    unsigned int start;
    unsigned int len;
    unsigned short bootid;
    unsigned short systid;
};

struct hd
{
    // proc_t proc;

    struct hdc *hdc;      // Controller
    struct hdparam param; // Drive parameter block
    int drvsel;           // Drive select on controller
    int use32bits;        // Use 32 bit transfers
    int sectbufs;         // Number of sector buffers
    int lba;              // LBA mode
    int iftype;           // IDE interface type (ATA/ATAPI)
    int media;            // Device media type (hd, cdrom, ...)
    int multsect;         // Sectors per interrupt
    int udmamode;         // UltraDMA mode
    dev_t devno;          // Device number

    // Geometry
    unsigned int blks; // Number of blocks on drive
    unsigned int size; // Size in MB

    unsigned int cyls;    // Number of cylinders
    unsigned int heads;   // Number of heads
    unsigned int sectors; // Sectors per track

    struct partition parts[HD_PARTITIONS]; // Partition info
};

int ideprobe = 0;
static struct hdc hdctab[HD_CONTROLLERS] =
{ 0 };
static struct hd hdtab[HD_DRIVES] =
{ 0 };

static void hd_fixstring(unsigned char *s, int len)
{
    unsigned char *p = s;
    unsigned char *end = s + len;

    // Convert from big-endian to host byte order
    for (p = end; p != s;)
    {
        unsigned short *pp = (unsigned short *) (p -= 2);
        *pp = ((*pp & 0x00FF) << 8) | ((*pp & 0xFF00) >> 8);
    }

    // Strip leading blanks
    while (s != end && *s == ' ')
        ++s;

    // Compress internal blanks and strip trailing blanks
    while (s != end && *s)
    {
        if (*s++ != ' ' || (s != end && *s && *s != ' '))
            *p++ = *(s - 1);
    }

    // Wipe out trailing garbage
    while (p != end)
        *p++ = '\0';
}

static void hd_error(char *func, unsigned char error)
{
    printk("%s: ", func);
    if (error & HDCE_BBK)
        printk("bad block  ");
    if (error & HDCE_UNC)
        printk("uncorrectable data  ");
    if (error & HDCE_MC)
        printk("media change  ");
    if (error & HDCE_IDNF)
        printk("id not found  ");
    if (error & HDCE_MCR)
        printk("media change requested  ");
    if (error & HDCE_ABRT)
        printk("abort  ");
    if (error & HDCE_TK0NF)
        printk("track 0 not found  ");
    if (error & HDCE_AMNF)
        printk("address mark not found  ");
    printk("\n");
}

static int hd_wait(struct hdc *hdc, unsigned char mask, unsigned int timeout)
{
    return 0;

    unsigned int start;
    unsigned char status;

    start = clock_ticks;
    while (1)
    {
        status = inb(hdc->iobase + HDC_ALT_STATUS);
        if (status & HDCS_ERR)
        {
            unsigned char error;

            error = inb(hdc->iobase + HDC_ERR);
            hd_error("hdwait", error);

            return error;
        }

        if (!(status & HDCS_BSY) && ((status & mask) == mask))
            return 0;

        if (clock_ticks > start + timeout)
            return -1;

        delay(100);
    }
}

static void hd_select_drive(struct hd *hd)
{
    outb(hd->hdc->iobase + HDC_DRVHD, hd->drvsel);
}

static void hd_setup_transfer(struct hd *hd, blkno_t blkno, int nsects)
{
    unsigned int track;
    unsigned int head;
    unsigned int sector;

    if (hd->lba)
    {
        track = (blkno >> 8) & 0xFFFF;
        head = ((blkno >> 24) & 0xF) | HD_LBA;
        sector = blkno & 0xFF;
    }
    else
    {
        track = blkno / (hd->heads * hd->sectors);
        head = (blkno / hd->sectors) % hd->heads;
        sector = blkno % hd->sectors + 1;
    }

    outb(hd->hdc->iobase + HDC_SECTORCNT, nsects);
    outb(hd->hdc->iobase + HDC_SECTOR, (unsigned char) sector);
    outb(hd->hdc->iobase + HDC_TRACKLSB, (unsigned char) track);
    outb(hd->hdc->iobase + HDC_TRACKMSB, (unsigned char) (track >> 8));
    outb(hd->hdc->iobase + HDC_DRVHD, (unsigned char) (head & 0xFF | hd->drvsel));
}

static void pio_read_buffer(struct hd *hd, char *buffer, int size)
{
    struct hdc *hdc = hd->hdc;

    if (hd->use32bits)
        insd(hdc->iobase + HDC_DATA, buffer, size / 4);
    else
        insw(hdc->iobase + HDC_DATA, buffer, size / 2);
}

static void pio_write_buffer(struct hd *hd, char *buffer, int size)
{
    struct hdc *hdc = hd->hdc;

    if (hd->use32bits)
        outsd(hdc->iobase + HDC_DATA, buffer, size / 4);
    else
        outsw(hdc->iobase + HDC_DATA, buffer, size / 2);
}

static void setup_dma(struct hdc *hdc, char *buffer, int count, int cmd)
{
    int i;
    int len;
    char *next;

    i = 0;
    next = (char *) ((unsigned long) buffer & ~(PAGESIZE - 1)) + PAGESIZE;
    while (1)
    {
        if (i == MAX_PRDS)
            panic("hd dma transfer too large");

        hdc->prds[i].addr = buffer;
        len = next - buffer;
        if (count > len)
        {
            hdc->prds[i].len = len;
            count -= len;
            buffer = next;
            next += PAGESIZE;
            i++;
        }
        else
        {
            hdc->prds[i].len = count | 0x80000000;
            break;
        }
    }

    // Setup PRD table
    outd(hdc->bmregbase + BM_PRD_ADDR, hdc->prds_phys);

    // Specify read/write
    outb(hdc->bmregbase + BM_COMMAND_REG, cmd | BM_CR_MASK_STOP);

    // Clear INTR & ERROR flags
    outb(hdc->bmregbase + BM_STATUS_REG, inb(hdc->bmregbase + BM_STATUS_REG) | BM_SR_MASK_INT | BM_SR_MASK_ERR);
}

static void start_dma(struct hdc *hdc)
{
    // Start DMA operation
    outb(hdc->bmregbase + BM_COMMAND_REG, inb(hdc->bmregbase + BM_COMMAND_REG) | BM_CR_MASK_START);
}

static int stop_dma(struct hdc *hdc)
{
    int dmastat;

    // Stop DMA channel and check DMA status
    outb(hdc->bmregbase + BM_COMMAND_REG, inb(hdc->bmregbase + BM_COMMAND_REG) & ~BM_CR_MASK_START);

    // Get DMA status
    dmastat = inb(hdc->bmregbase + BM_STATUS_REG);

    // Clear INTR && ERROR flags
    outb(hdc->bmregbase + BM_STATUS_REG, dmastat | BM_SR_MASK_INT | BM_SR_MASK_ERR);

    if ((dmastat & (BM_SR_MASK_INT | BM_SR_MASK_ERR | BM_SR_MASK_ACT)) != BM_SR_MASK_INT)
    {
        error("hd: dma error %02X", dmastat);
        return -1;
    }

    return 0;
}

static int hd_identify(struct hd *hd)
{
    debug();

    // Ignore interrupt for identify command
    hd->hdc->dir = HD_XFER_IGNORE;
    // reset_event(&hd->hdc->ready);

    sem_init(&hd->hdc->ready, 0);

    // Issue read drive parameters command
    outb(hd->hdc->iobase + HDC_FEATURE, 0);
    outb(hd->hdc->iobase + HDC_DRVHD, hd->drvsel);
    outb(hd->hdc->iobase + HDC_COMMAND, hd->iftype == HDIF_ATAPI ? HDCMD_PIDENTIFY : HDCMD_IDENTIFY);

    // Wait for data ready
    wait(&hd->hdc->ready);

    // Some controllers issues the interrupt before data is ready to be read
    // Make sure data is ready by waiting for DRQ to be set
    if (hd_wait(hd->hdc, HDCS_DRQ, HDTIMEOUT_DRQ) < 0)
        return -1;

    // Read parameter data
    insw(hd->hdc->iobase + HDC_DATA, &(hd->param), SECTORSIZE / 2);

    // Fill in drive parameters
    hd->cyls = hd->param.cylinders;
    hd->heads = hd->param.heads;
    hd->sectors = hd->param.sectors;
    hd->use32bits = hd->param.usedmovsd != 0;
    hd->sectbufs = hd->param.buffersize;
    hd->multsect = hd->param.nsecperint;
    if (hd->multsect == 0)
        hd->multsect = 1;

    hd_fixstring(hd->param.model, sizeof(hd->param.model));
    hd_fixstring(hd->param.rev, sizeof(hd->param.rev));
    hd_fixstring(hd->param.serial, sizeof(hd->param.serial));

    if (hd->iftype == HDIF_ATA)
        hd->media = IDE_DISK;
    else
        hd->media = (hd->param.config >> 8) & 0x1f;

    // Determine LBA or CHS mode
    if ((hd->param.caps & 0x0200) == 0)
    {
        hd->lba = 0;
        hd->blks = hd->cyls * hd->heads * hd->sectors;
        if (hd->cyls == 0 && hd->heads == 0 && hd->sectors == 0)
            return -1;
        if (hd->cyls == 0xFFFF && hd->heads == 0xFFFF && hd->sectors == 0xFFFF)
            return -1;
    }
    else
    {
        hd->lba = 1;
        hd->blks = (hd->param.totalsec1 << 16) | hd->param.totalsec0;
        if (hd->media == IDE_DISK && (hd->blks == 0 || hd->blks == 0xFFFFFFFF))
            return -1;
    }
    hd->size = hd->blks / (1024 * 1024 / SECTORSIZE);

    return 0;
}

static int hd_cmd(struct hd *hd, unsigned int cmd, unsigned int feat, unsigned int nsects)
{
    // Ignore interrupt for command
    hd->hdc->dir = HD_XFER_IGNORE;
    // reset_event(&hd->hdc->ready);

    // Issue command
    outb(hd->hdc->iobase + HDC_FEATURE, feat);
    outb(hd->hdc->iobase + HDC_SECTORCNT, nsects);
    outb(hd->hdc->iobase + HDC_DRVHD, hd->drvsel);
    outb(hd->hdc->iobase + HDC_COMMAND, cmd);

    // Wait for data ready
    wait(&hd->hdc->ready);

    // Check status
    if (hd->hdc->result < 0)
        return -1;

    return 0;
}

static int atapi_packet_read(struct hd *hd, unsigned char *pkt, int pktlen, void *buffer, int bufsize)
{
    struct hdc *hdc;
    int result;
    char *bufp;
    int bufleft;
    unsigned short bytes;

    // printk("atapi_read_packet(0x%x) %d bytes, buflen=%d\n", pkt[0], pktlen, bufsize);

    hdc = hd->hdc;
    // if (wait_for_object(&hdc->lock, HDTIMEOUT_BUSY) < 0) return -EBUSY;

    bufp = (char *) buffer;
    bufleft = bufsize;
    hdc->dir = HD_XFER_IGNORE;
    hdc->active = hd;
    // reset_event(&hdc->ready);

    // Setup registers
    outb(hdc->iobase + HDC_SECTORCNT, 0);
    outb(hdc->iobase + HDC_SECTOR, 0);
    outb(hdc->iobase + HDC_TRACKLSB, (unsigned char) (bufsize & 0xFF));
    outb(hdc->iobase + HDC_TRACKMSB, (unsigned char) (bufsize >> 8));
    outb(hdc->iobase + HDC_DRVHD, (unsigned char) hd->drvsel);

    // Send packet command
    outb(hdc->iobase + HDC_COMMAND, HDCMD_PACKET);

    // Wait for drive ready to receive packet
    result = hd_wait(hdc, HDCS_DRDY, HDTIMEOUT_DRDY);
    if (result != 0)
    {
        error("atapi_packet_read: busy waiting for packet ready (0x%02x)", result);

        hdc->dir = HD_XFER_IDLE;
        hdc->active = NULL;
        ////release_mutex(&hdc->lock);

        return -1;
    }

    // Command packet transfer
    pio_write_buffer(hd, pkt, pktlen);

    // Data transfer
    while (1)
    {
        // Wait until data ready
        // printk("wait for data\n");
        wait(&hdc->ready);

        // reset_event(&hdc->ready);

        // Check for errors
        if (hdc->status & HDCS_ERR)
        {
            unsigned char error;

            error = inb(hdc->iobase + HDC_ERR);
            error("hd: atapi packet read error (status=0x%02x,error=0x%02x)", hdc->status, error);

            hdc->result = -1;
            break;
        }

        // Exit the read data loop if the device indicates this is the end of the command
        // printk("stat 0x%02x\n", hdc->status);
        if ((hdc->status & (HDCS_BSY | HDCS_DRQ)) == 0)
            break;

        // Get the byte count
        bytes = (inb(hdc->iobase + HDC_TRACKMSB) << 8) | inb(hdc->iobase + HDC_TRACKLSB);
        // printk("%d bytes\n", bytes);
        if (bytes == 0)
            break;
        if (bytes > bufleft)
        {
            // printk("%s: buffer overrun\n", device(hd->devno)->name);
            // hdc->result = -4;
            break;
        }

        // Read the bytes
        pio_read_buffer(hd, bufp, bytes);
        bufp += bytes;
        bufleft -= bytes;
    }

    // Cleanup
    hdc->dir = HD_XFER_IDLE;
    hdc->active = NULL;
    result = hdc->result;
    // release_mutex(&hdc->lock);
    return result == 0 ? bufsize - bufleft : result;
}

static int atapi_read_capacity(struct hd *hd)
{
    unsigned char pkt[12];
    unsigned long buf[2];
    unsigned long blks;
    unsigned long blksize;
    int rc;

    memset(pkt, 0, 12);
    pkt[0] = ATAPI_CMD_READCAPICITY;

    rc = atapi_packet_read(hd, pkt, 12, buf, sizeof buf);
    if (rc < 0)
        return rc;
    if (rc != sizeof buf)
        return -4;

    blks = buf[0];
    blksize = buf[1];
    // if (blksize != CDSECTORSIZE) printk("%s: unexpected block size (%d)\n", device(hd->devno)->name, blksize);
    return blks;
}

static int atapi_request_sense(struct hd *hd)
{
    unsigned char pkt[12];
    unsigned char buf[18];
    int rc;

    memset(pkt, 0, 12);
    pkt[0] = ATAPI_CMD_REQUESTSENSE;
    pkt[4] = sizeof buf;

    rc = atapi_packet_read(hd, pkt, 12, buf, sizeof buf);
    if (rc < 0)
        return rc;

    return 0;
}
/*
 static int hd_ioctl(struct hd *hd, int cmd, void *args, int size)
 {
 struct hd *hd = (struct hd *) dev->privdata;
 struct geometry *geom;

 switch (cmd)
 {
 case IOCTL_GETDEVSIZE:
 return hd->blks;

 case IOCTL_GETBLKSIZE:
 return SECTORSIZE;

 case IOCTL_GETGEOMETRY:
 if (!args || size != sizeof(struct geometry)) return -2;
 geom = (struct geometry *) args;
 geom->cyls = hd->cyls;
 geom->heads = hd->heads;
 geom->spt = hd->sectors;
 geom->sectorsize = SECTORSIZE;
 geom->sectors = hd->blks;

 return 0;

 case IOCTL_REVALIDATE:
 return create_partitions(hd);
 }

 return -ENOSYS;
 }
 */
static int hd_read_pio(struct hd *hd, void *buffer, int len, int pos)
{
    // struct hd *hd;
    struct hdc *hdc;
    int sectsleft;
    int nsects;
    int result;
    char *bufp;

    // printk("hdread: block %d\n", blkno);

    if (len == 0)
        return 0;

    bufp = (char *) buffer;
    // hd = (struct hd *) dev->privdata;
    hdc = hd->hdc;
    sectsleft = len / SECTORSIZE;

    wait(&hdc->lock);

    while (sectsleft > 0)
    {
        // Select drive
        hd_select_drive(hd);

        // Wait for controller ready
        result = hd_wait(hdc, HDCS_DRDY, HDTIMEOUT_DRDY);
        if (result != 0)
        {
            error("hd_read: no drdy (0x%02x)\n", result);
            hdc->result = -1;
            break;
        }

        // Calculate maximum number of sectors we can transfer
        // printk("%d sects left\n", sectsleft);
        if (sectsleft > 256)
            nsects = 256;
        else
            nsects = sectsleft;

        // Prepare transfer
        // printk("read %d sects\n", nsects);
        hdc->bufp = bufp;
        hdc->nsects = nsects;
        hdc->result = 0;
        hdc->dir = HD_XFER_READ;
        hdc->active = hd;
        // reset_event(&hdc->ready);

        sem_init(&hd->hdc->ready, 0);

        // printk("start read, hdc->ready=%d\n", hdc->ready);

        int blkno = pos /SECTORSIZE;
        hd_setup_transfer(hd, blkno, nsects);

        outb(hdc->iobase + HDC_COMMAND,
                hd->multsect > 1 ? HDCMD_MULTREAD : HDCMD_READ);

        // printk("after read, hdc->ready=%d\n", hdc->ready);

        // Wait until data read
        wait(&hdc->ready);

        // printk("after wait, hdc->ready=%d\n", hdc->ready);

        if (hdc->result < 0)
            break;

        // Advance to next
        sectsleft -= nsects;
        bufp += nsects * SECTORSIZE;
    }

    // printk("finito\n");

    // Cleanup
    hdc->dir = HD_XFER_IDLE;
    hdc->active = NULL;
    result = hdc->result;
    signal(&hdc->lock);

    return result == 0 ? len : result;
}

static int hd_write_pio(struct hd *hd, void *buffer, int count, blkno_t blkno)
{
    // struct hd *hd;
    struct hdc *hdc;
    int sectsleft;
    int nsects;
    int n;
    int result;
    char *bufp;

    // printk("hdwrite: block %d\n", blkno);

    if (count == 0)
        return 0;
    bufp = (char *) buffer;
    // hd = (struct hd *) dev->privdata;
    hdc = hd->hdc;
    sectsleft = count / SECTORSIZE;
    // if (wait_for_object(&hdc->lock, HDTIMEOUT_BUSY) < 0) return -EBUSY;

    while (sectsleft > 0)
    {
        // printk("%d sects left\n", sectsleft);
        // Select drive
        hd_select_drive(hd);

        // Wait for controller ready
        result = hd_wait(hdc, HDCS_DRDY, HDTIMEOUT_DRDY);
        if (result != 0)
        {
            error("hd_write: no drdy (0x%02x)\n", result);
            hdc->result = -1;
            break;
        }

        // Calculate maximum number of sectors we can transfer
        if (sectsleft > 256)
            nsects = 256;
        else
            nsects = sectsleft;

        // printk("write %d sects\n", nsects);
        // Prepare transfer
        hdc->bufp = bufp;
        hdc->nsects = nsects;
        hdc->result = 0;
        hdc->dir = HD_XFER_WRITE;
        hdc->active = hd;
        // reset_event(&hdc->ready);

        sem_init(&hd->hdc->ready, 0);

        hd_setup_transfer(hd, blkno, nsects);
        outb(hdc->iobase + HDC_COMMAND,
                hd->multsect > 1 ? HDCMD_MULTWRITE : HDCMD_WRITE);

        // Wait for data ready
        if (!(inb(hdc->iobase + HDC_ALT_STATUS) & HDCS_DRQ))
        {
            result = hd_wait(hdc, HDCS_DRQ, HDTIMEOUT_DRQ);
            if (result != 0)
            {
                error("hd_write: no drq (0x%02x)\n", result);
                hdc->result = -1;
                break;
            }
        }

        // Write first sector(s)
        n = hd->multsect;
        if (n > nsects)
            n = nsects;
        while (n-- > 0)
        {
            pio_write_buffer(hd, hdc->bufp, SECTORSIZE);
            hdc->bufp += SECTORSIZE;
        }

        // printk("wait\n");
        // Wait until data written
        wait(&hdc->ready);
        if (hdc->result < 0)
            break;

        // printk("ready\n");

        // Advance to next
        sectsleft -= nsects;
        bufp += nsects * SECTORSIZE;
    }

    // printk("finito\n");

    // Cleanup
    hdc->dir = HD_XFER_IDLE;
    hdc->active = NULL;
    result = hdc->result;
    // release_mutex(&hdc->lock);
    return result == 0 ? count : result;
}

static int hd_read_udma(struct hd *hd, void *buffer, int count, blkno_t blkno, int flags)
{
    debug("read %d bytes from secno %d", blkno);

    // struct hd *hd;
    struct hdc *hdc;
    int sectsleft;
    int nsects;
    int result;
    char *bufp;

    if (count == 0)
        return 0;

    bufp = (char *) buffer;

    // hd = (struct hd *) dev->privdata;
    hdc = hd->hdc;
    sectsleft = count / SECTORSIZE;
    // if (wait_for_object(&hdc->lock, HDTIMEOUT_BUSY) < 0) return -EBUSY;

    // printk("hdread block %d size %d buffer %p\n", blkno, count, buffer);

    while (sectsleft > 0)
    {
        // Select drive
        hd_select_drive(hd);

        // Wait for controller ready
        result = hd_wait(hdc, HDCS_DRDY, HDTIMEOUT_DRDY);
        if (result != 0)
        {
            error("hd_read: no drdy (0x%02x)\n", result);
            result = -1;
            break;
        }

        // Calculate maximum number of sectors we can transfer
        if (sectsleft > 256)
            nsects = 256;
        else
            nsects = sectsleft;

        if (nsects > MAX_DMA_XFER_SIZE / SECTORSIZE)
            nsects = MAX_DMA_XFER_SIZE / SECTORSIZE;

        // Prepare transfer
        result = 0;
        hdc->dir = HD_XFER_DMA;
        hdc->active = hd;
        // reset_event(&hdc->ready);
        sem_init(&hd->hdc->ready, 0);

        hd_setup_transfer(hd, blkno, nsects);

        // Setup DMA
        setup_dma(hdc, bufp, nsects * SECTORSIZE, BM_CR_MASK_WRITE);

        // Start read
        outb(hdc->iobase + HDC_COMMAND, HDCMD_READDMA);

        start_dma(hdc);
        // Wait for interrupt
        wait(&hdc->ready);

        // Stop DMA channel and check DMA status
        result = stop_dma(hdc);
        if (result < 0)
            break;

        // Check controller status
        if (hdc->status & HDCS_ERR)
        {
            unsigned char error;

            error = inb(hdc->iobase + HDC_ERR);
            hd_error("hdread", error);

            error("hd: read error (0x%02x)", hdc->status);
            result = -1;
            break;
        }

        // Advance to next
        sectsleft -= nsects;
        bufp += nsects * SECTORSIZE;
    }

    // Cleanup
    hdc->dir = HD_XFER_IDLE;
    hdc->active = NULL;
    // release_mutex(&hdc->lock);
    return result == 0 ? count : result;
}

static int hd_write_udma(struct hd *hd, void *buffer, int count, blkno_t blkno, int flags)
{
    // struct hd *hd;
    struct hdc *hdc;
    int sectsleft;
    int nsects;
    int result;
    char *bufp;

    if (count == 0)
        return 0;
    bufp = (char *) buffer;

    // hd = (struct hd *) dev->privdata;
    hdc = hd->hdc;
    sectsleft = count / SECTORSIZE;
    // if (wait_for_object(&hdc->lock, HDTIMEOUT_BUSY) < 0) return -EBUSY;

    // printk("hdwrite block %d size %d buffer %p\n", blkno, count, buffer);

    while (sectsleft > 0)
    {
        // Select drive
        hd_select_drive(hd);

        // Wait for controller ready
        result = hd_wait(hdc, HDCS_DRDY, HDTIMEOUT_DRDY);
        if (result != 0)
        {
            printk("hd_write: no drdy (0x%02x)\n", result);
            result = -1;
            break;
        }

        // Calculate maximum number of sectors we can transfer
        if (sectsleft > 256)
            nsects = 256;
        else
            nsects = sectsleft;

        if (nsects > MAX_DMA_XFER_SIZE / SECTORSIZE)
            nsects = MAX_DMA_XFER_SIZE / SECTORSIZE;

        // Prepare transfer
        result = 0;
        hdc->dir = HD_XFER_DMA;
        hdc->active = hd;
        // reset_event(&hdc->ready);

        hd_setup_transfer(hd, blkno, nsects);

        // Setup DMA
        setup_dma(hdc, bufp, nsects * SECTORSIZE, BM_CR_MASK_READ);

        // Start write
        outb(hdc->iobase + HDC_COMMAND, HDCMD_WRITEDMA);
        start_dma(hdc);

        // Wait for interrupt
        wait(&hdc->ready);

        // Stop DMA channel and check DMA status
        result = stop_dma(hdc);
        if (result < 0)
            break;

        // Check controller status
        if (hdc->status & HDCS_ERR)
        {
            unsigned char error;

            error = inb(hdc->iobase + HDC_ERR);
            hd_error("hdwrite", error);

            error("hd: write error (0x%02x)", hdc->status);
            result = -1;
            break;
        }

        // Advance to next
        sectsleft -= nsects;
        bufp += nsects * SECTORSIZE;
    }

    // Cleanup
    hdc->dir = HD_XFER_IDLE;
    hdc->active = NULL;
    // release_mutex(&hdc->lock);
    return result == 0 ? count : result;
}

static int cd_read(struct hd *hd, void *buffer, int count, blkno_t blkno, int flags)
{
    // struct hd *hd = (struct hd *) dev->privdata;
    unsigned char pkt[12];
    unsigned int blks;

    // printk("cd_read: blk %d %d bytes\n", blkno, count);

    blks = count / CDSECTORSIZE;
    if (blks > 0xFFFF)
        return -2;

    memset(pkt, 0, 12);
    pkt[0] = ATAPI_CMD_READ10;
    pkt[2] = blkno >> 24;
    pkt[3] = (blkno >> 16) & 0xFF;
    pkt[4] = (blkno >> 8) & 0xFF;
    pkt[5] = blkno & 0xFF;
    pkt[7] = (blks >> 8) & 0xFF;
    pkt[8] = blks & 0xFF;

    return atapi_packet_read(hd, pkt, 12, buffer, count);
}

static int cd_write(struct hd *hd, void *buffer, int count, blkno_t blkno, int flags)
{
    return -3;
}
/*
 static int cd_ioctl(struct hd *hd, int cmd, void *args, int size)
 {
 struct hd *hd = (struct hd *) dev->privdata;
 int rc;

 switch (cmd)
 {
 case IOCTL_GETDEVSIZE:
 if (hd->blks <= 0) hd->blks = atapi_read_capacity(hd);
 return hd->blks;

 case IOCTL_GETBLKSIZE:
 return CDSECTORSIZE;

 case IOCTL_REVALIDATE:
 rc = atapi_request_sense(hd);
 if (rc < 0) return rc;

 rc = hd->blks = atapi_read_capacity(hd);
 if (rc < 0) return rc;

 return 0;
 }

 return -ENOSYS;
 }
 */

void hd_dpc(void *arg)
{
    //debug();

    struct hdc *hdc = (struct hdc *) arg;
    int nsects;
    int n;

    // printk("hd_dpc: dir=%x nsects =%d \n", hdc->dir, hdc->nsects);
    hdc->status = inb(hdc->iobase + HDC_STATUS);

    switch (hdc->dir)
    {
    case HD_XFER_READ:
        // Check status
        if (hdc->status & HDCS_ERR)
        {
            unsigned char error;

            error = inb(hdc->iobase + HDC_ERR);
            hd_error("hdread", error);

            error("hd: read error (0x%02x)", hdc->status);
            hdc->result = -1;
            signal(&hdc->ready);
        }
        else
        {
            // Read sector data
            nsects = hdc->active->multsect;
            if (nsects > hdc->nsects)
                nsects = hdc->nsects;

            pio_read_buffer(hdc->active, hdc->bufp, nsects * SECTORSIZE);
            hdc->bufp += nsects * SECTORSIZE;

            // printk("read %d sectors complete\n", nsects);

            // Signal event if we have read all sectors
            hdc->nsects -= nsects;

            if (hdc->nsects == 0)
            {
                signal(&hdc->ready);
                // printk("read all sectors complete. signal client thread,hdc->ready=%d \n", hdc->ready);
            }
        }

        break;

    case HD_XFER_WRITE:
        // Check status
        if (hdc->status & HDCS_ERR)
        {
            unsigned char error;

            error = inb(hdc->iobase + HDC_ERR);
            hd_error("hdwrite", error);

            error("hd: write error (0x%02x)", hdc->status);
            hdc->result = -1;
            signal(&hdc->ready);
        }
        else
        {
            // Transfer next sector(s) or signal end of transfer
            nsects = hdc->active->multsect;
            if (nsects > hdc->nsects)
                nsects = hdc->nsects;

            pio_write_buffer(hdc->active, hdc->bufp, nsects * SECTORSIZE);

            hdc->nsects -= nsects;
            if (hdc->nsects == 0)
                signal(&hdc->ready);
        }

        break;

    case HD_XFER_DMA:
        debug("hd_dpc:HD_XFER_DMA");

        //outb(hdc->bmregbase + BM_STATUS_REG, inb(hdc->bmregbase + BM_STATUS_REG));
        //hdc->status = inb(hdc->iobase + HDC_STATUS);
        signal(&hdc->ready);
        break;

    case HD_XFER_IGNORE:
        debug("hd_dpc:HD_XFER_IGNORE");

        // Read status to acknowledge interrupt
        signal(&hdc->ready);
        break;

    case HD_XFER_IDLE:
        debug("hd_dpc:HD_XFER_IDLE");
        break;

    default:
        // Read status to acknowledge interrupt
        error("unexpected intr from hdc[%d]", hdc->dir);
        break;
    }
}

static void hdc_handler(void *data, registers_t* regs)
{
    hd_dpc((struct hdc *) data);
}

/*
 int hdc_handler(struct context *ctxt, void *arg)
 {
 struct hdc *hdc = (struct hdc *) arg;

 if (hdc->xfer_dpc.flags & DPC_QUEUED) printk("hd: intr lost\n");
 queue_irq_dpc(&hdc->xfer_dpc, hd_dpc, hdc);
 eoi(hdc->irq);
 return 0;
 }

 static int part_ioctl(struct hd *hd, int cmd, void *args, int size)
 {
 struct partition *part = (struct partition *) dev->privdata;

 switch (cmd)
 {
 case IOCTL_GETDEVSIZE:
 return part->len;

 case IOCTL_GETBLKSIZE:
 return dev_ioctl(part->dev, IOCTL_GETBLKSIZE, NULL, 0);
 }

 return -ENOSYS;
 }



static int part_read(struct partition *part, void *buffer, int count, blkno_t blkno)
{
    if (blkno + count / SECTORSIZE > part->len)
        return -1;

    return hd_read_pio(part->hd, buffer, count, blkno + part->start);
    //return hd_read_udma(part->hd, buffer, count, blkno + part->start, 0);
}

static int part_write(struct partition *part, void *buffer, int count, blkno_t blkno)
{
    if (blkno + count / SECTORSIZE > part->len)
        return -1;

    return hd_write_pio(part->hd, buffer, count, blkno + part->start);
}

 struct driver harddisk_udma_driver =
 {
 "idedisk/udma",
 //DEV_TYPE_BLOCK,
 hd_ioctl,
 hd_read_udma,
 hd_write_udma
 };

 struct driver harddisk_pio_driver =
 {
 "idedisk/pio",
 //DEV_TYPE_BLOCK,
 hd_ioctl,
 hd_read_pio,
 hd_write_pio
 };

 struct driver cdrom_pio_driver =
 {
 "idecd/pio",
 DEV_TYPE_BLOCK,
 cd_ioctl,
 cd_read,
 cd_write
 };

 struct driver partition_driver =
 {
 "partition",
 DEV_TYPE_BLOCK,
 part_ioctl,
 part_read,
 part_write
 };
 */


static void* part_open(void* obj, char* path, s64 mode)
{
    return obj;
}

static s64 part_read(void* obj, void* buf, s64 len, s64 pos)
{
    struct partition *part = (struct partition *)obj;
    return hd_read_pio(part->hd, buf, len, pos + (part->start << 9));
}

static s64 part_write(void* obj, void* buf, s64 len, s64 pos)
{
    struct partition *part = (struct partition *)obj;
    return hd_write_pio(part->hd, buf, len, pos + (part->start << 9));
}

static ops_t part_ops =
{
    .open = part_open,
    .read = part_read,
    .write = part_write
};


static int create_partitions(struct SATA *hd)
{
    struct master_boot_record mbr =
    { 0 };

    dev_t devno;
    int rc;
    int i;
    char devname[64];

    // Read partition table
    rc = hd_read_pio(hd, &mbr, SECTORSIZE, 0);
    //rc = hd_read_udma(hd, &mbr, SECTORSIZE, 0, 0);

    if (rc < 0)
    {
        error("error %d reading partition table", rc);
        return rc;
    }

    // Create partition devices
    if (mbr.signature != MBR_SIGNATURE)
    {
        error("%s: illegal boot sector signature", "hd0");
        return -1;
    }

    void* hd0_dir = dir_create();
    append(HANDLE_OBJ, "hd0", hd0_dir); // 注册 hd0 目录

    for (i = 0; i < HD_PARTITIONS; i++)
    {
        hd->parts[i].ops = &part_ops;
        hd->parts[i].hd = hd;
        hd->parts[i].bootid = mbr.parttab[i].bootid;
        hd->parts[i].systid = mbr.parttab[i].systid;
        hd->parts[i].start = mbr.parttab[i].relsect;
        hd->parts[i].len = mbr.parttab[i].numsect;

        int StartSector = mbr.parttab[i].begsect & 0x3F;
        int StartCylinder = ((((int) mbr.parttab[i].begsect) >> 6) << 8) + mbr.parttab[i].begcyl;

        int EndSector = mbr.parttab[i].endsect & 0x3F;
        int EndCylinder = ((((int) mbr.parttab[i].endsect) >> 6) << 8) + mbr.parttab[i].endcyl;

        trace("[%d] %d (%d %d %d) %d - (%d %d %d) (%dM %dM)", i, mbr.parttab[i].bootid, mbr.parttab[i].beghead, StartSector, StartCylinder, mbr.parttab[i].systid, mbr.parttab[i].endhead, EndSector, EndCylinder, mbr.parttab[i].relsect >> 11, mbr.parttab[i].numsect >> 11);

        char part_name[4];
        snprintf(part_name, sizeof(part_name), "p%d", i);
        append(hd0_dir, part_name, &hdtab[0].parts[i]);
    }

    return 0;
}

static int probe_device(struct hdc *hdc, int drvsel)
{
    unsigned char sc, sn;

    // Probe for device on controller
    outb(hdc->iobase + HDC_DRVHD, drvsel);
    idedelay();

    outb(hdc->iobase + HDC_SECTORCNT, 0x55);
    outb(hdc->iobase + HDC_SECTOR, 0xAA);

    outb(hdc->iobase + HDC_SECTORCNT, 0xAA);
    outb(hdc->iobase + HDC_SECTOR, 0x55);

    outb(hdc->iobase + HDC_SECTORCNT, 0x55);
    outb(hdc->iobase + HDC_SECTOR, 0xAA);

    sc = inb(hdc->iobase + HDC_SECTORCNT);
    sn = inb(hdc->iobase + HDC_SECTOR);

    if (sc == 0x55 && sn == 0xAA)
        return 1;
    else
        return -1;
}

static int wait_reset_done(struct hdc *hdc, int drvsel)
{
    unsigned int tmo;

    outb(hdc->iobase + HDC_DRVHD, drvsel);
    idedelay();

    tmo = clock_ticks + 5 * HZ;
    while (clock_ticks < tmo)
    {
        hdc->status = inb(hdc->iobase + HDC_STATUS);
        if ((hdc->status & HDCS_BSY) == 0)
            return 0;

        delay(100);
    }

    return -1;
}

static int get_interface_type(struct hdc *hdc, int drvsel)
{
    unsigned char sc, sn, cl, ch, st;

    outb(hdc->iobase + HDC_DRVHD, 0xA0 | (drvsel << 4));
    idedelay();

    sc = inb(hdc->iobase + HDC_SECTORCNT);
    sn = inb(hdc->iobase + HDC_SECTOR);
    // printk("%x: sc=0x%02x sn=0x%02x\n", hdc->iobase, sc, sn);

    if (sc == 0x01 && sn == 0x01)
    {
        cl = inb(hdc->iobase + HDC_TRACKLSB);
        ch = inb(hdc->iobase + HDC_TRACKMSB);
        st = inb(hdc->iobase + HDC_STATUS);

        // printk("%x: cl=0x%02x ch=0x%02x st=0x%02x\n", hdc->iobase, cl, ch, st);

        if (cl == 0x14 && ch == 0xeb)
            return HDIF_ATAPI;
        if (cl == 0x00 && ch == 0x00 && st != 0x00)
            return HDIF_ATA;
    }

    return HDIF_UNKNOWN;
}

static int setup_hdc(struct hdc *hdc, int iobase, int irq, int bmregbase, int *masterif, int *slaveif)
{
    debug();

    memset(hdc, 0, sizeof(struct hdc));
    hdc->iobase = iobase;
    hdc->irq = irq;
    hdc->bmregbase = bmregbase;
    hdc->dir = HD_XFER_IGNORE;

    if (hdc->bmregbase)
    {
        // Allocate one page for PRD list
        hdc->prds_phys = page_alloc();
        hdc->prds = (struct prd *)hdc->prds_phys;
    }

    sem_init(&hdc->lock, 1);
    // init_dpc(&hdc->xfer_dpc);
    // init_mutex(&hdc->lock, 0);
    // init_event(&hdc->ready, 0, 0);

    ideprobe = 1;
    if (ideprobe)
    {
        // Assume no devices connected to controller
        *masterif = HDIF_NONE;
        *slaveif = HDIF_NONE;

        // Setup device control register
        outb(hdc->iobase + HDC_CONTROL, HDDC_HD15 | HDDC_NIEN);

        // Probe for master and slave device on controller
        if (probe_device(hdc, HD0_DRVSEL) >= 0)
            *masterif = HDIF_PRESENT;

        if (probe_device(hdc, HD1_DRVSEL) >= 0)
            *slaveif = HDIF_PRESENT;

        // Reset controller
        outb(hdc->iobase + HDC_CONTROL, HDDC_HD15 | HDDC_SRST | HDDC_NIEN);
        idedelay();
        outb(hdc->iobase + HDC_CONTROL, HDDC_HD15 | HDDC_NIEN);
        idedelay();

        // Wait for reset to finish on all present devices
        if (*masterif != HDIF_NONE)
        {
            int rc = wait_reset_done(hdc, HD0_DRVSEL);
            if (rc < 0)
            {
                error("hd: error %d waiting for reset to complete on master device");
                *masterif = HDIF_NONE;
            }
        }

        if (*slaveif != HDIF_NONE)
        {
            int rc = wait_reset_done(hdc, HD1_DRVSEL);
            if (rc < 0)
            {
                error("hd: error %d waiting for reset to complete on slave device");
                *slaveif = HDIF_NONE;
            }
        }

        // Determine interface types
        if (*masterif != HDIF_NONE)
            *masterif = get_interface_type(hdc, HD0_DRVSEL);
        if (*slaveif != HDIF_NONE)
            *slaveif = get_interface_type(hdc, HD1_DRVSEL);
    }
    else
    {
        // No IDE probing, assume both devices connected to force selection by BIOS settings
        *masterif = HDIF_ATA;
        *slaveif = HDIF_ATA;
    }

    // Enable interrupts
    irq_register(irq, hdc_handler, hdc);
    enable_irq(irq);

    outb(hdc->iobase + HDC_CONTROL, HDDC_HD15);
    idedelay();

    return 0;
}

static void setup_hd(struct hd *hd, struct hdc *hdc, char *devname, int drvsel, int udmasel, int iftype)
{
    debug();

    static int udma_speed[] =
    { 16, 25, 33, 44, 66, 100 };

    int rc;

    // Initialize drive block
    memset(hd, 0, sizeof(struct hd));
    hd->hdc = hdc;
    hd->drvsel = drvsel;
    hd->iftype = iftype;

    // Get info block from device
    rc = hd_identify(hd);

    debug("hd_identify:%d", rc);

    if (rc < 0)
    {
        // Try other interface type
        if (hd->iftype == HDIF_ATA)
            hd->iftype = HDIF_ATAPI;
        else if (hd->iftype == HDIF_ATAPI)
            hd->iftype = HDIF_ATA;

        rc = hd_identify(hd);
        if (rc < 0)
        {
            error("hd: device %s not responding, ignored.", devname);
            return;
        }
    }

    // Determine UDMA mode
    if (!hdc->bmregbase)
        hd->udmamode = -1;
//	else if(memcmp(hd->param.model, "VMware", 6) == 0)
//	{
//		// UltraDMA does not seem to work with VMware emulator, disable it
//		hd->udmamode = -1;
//	}
    else if ((hd->param.valid & 4) && (hd->param.dmaultra & (hd->param.dmaultra >> 8) & 0x3F))
    {
        if ((hd->param.dmaultra >> 13) & 1)
            hd->udmamode = 5; // UDMA 100
        else if ((hd->param.dmaultra >> 12) & 1)
            hd->udmamode = 4; // UDMA 66
        else if ((hd->param.dmaultra >> 11) & 1)
            hd->udmamode = 3; // UDMA 44
        else if ((hd->param.dmaultra >> 10) & 1)
            hd->udmamode = 2; // UDMA 33
        else if ((hd->param.dmaultra >> 9) & 1)
            hd->udmamode = 1; // UDMA 25
        else
            hd->udmamode = 0; // UDMA 16
    }
    else
        hd->udmamode = -1;

    // Set multi-sector mode if drive supports it
    if (hd->multsect > 1)
    {
        rc = hd_cmd(hd, HDCMD_SETMULT, 0, hd->multsect);
        if (rc < 0)
        {
            error("hd: unable to set multi sector mode");
            hd->multsect = 1;
        }
    }

    // Enable UDMA for drive if it supports it.
    if (hd->udmamode != -1)
    {
        // Enable drive in bus master status register
        int dmastat = inb(hdc->bmregbase + BM_STATUS_REG);
        outb(hdc->bmregbase + BM_STATUS_REG, dmastat | udmasel);

        // Set feature in IDE controller
        rc = hd_cmd(hd, HDCMD_SETFEATURES, HDFEAT_XFER_MODE,
                HDXFER_MODE_UDMA | hd->udmamode);
        if (rc < 0)
            error("hd: unable to enable UDMA mode");
    }

    // Enable read ahead and write caching if supported
    if (hd->param.csfo & 2)
        hd_cmd(hd, HDCMD_SETFEATURES, HDFEAT_ENABLE_RLA, 0);
    if (hd->param.csfo & 1)
        hd_cmd(hd, HDCMD_SETFEATURES, HDFEAT_ENABLE_WCACHE, 0);

    // Make new device
    if (hd->media == IDE_DISK)
    {
        /*
         if (hd->udmamode != -1)
         printk("harddisk_udma_driver");
         //hd->devno = dev_make(devname, &harddisk_udma_driver, NULL, hd);
         else
         printk("harddisk_pio_driver");
         //hd->devno = dev_make(devname, &harddisk_pio_driver, NULL, hd);*/
    }
    else if (hd->media == IDE_CDROM)
    {
        // hd->devno = dev_make("cd#", &cdrom_pio_driver, NULL, hd);
    }
    else
    {
        error("%s: unknown media type 0x%02x (iftype %d, config 0x%04x)", devname, hd->media, hd->iftype, hd->param.config);
        return;
    }

    printk("%s", hd->param.model);
    if (hd->size > 0)
        printk(" (%d MB)", hd->size);
    if (hd->lba)
        printk(", LBA");
    if (hd->udmamode != -1)
        printk(", UDMA%d", udma_speed[hd->udmamode]);
    if (hd->param.csfo & 2)
        printk(", read ahead");
    if (hd->param.csfo & 1)
        printk(", write cache");
    if (hd->udmamode == -1 && hd->multsect > 1)
        printk(", %d sects/intr", hd->multsect);
    if (!hd->use32bits)
        printk(", word I/O");
    if (hd->hdc->bmregbase)
        printk(", bmregbase=0x%x", hd->hdc->bmregbase);
    printk("\n");

    if (hd->media == IDE_DISK)
    {
        create_partitions(hd);
    }
}

/*
 #define SANOS_BOOT_PARTITION_ID  0xCC

 //#pragma pack(push)





 struct boot_sector
 {
 char prolog[4];
 char label[8];
 unsigned short ldrsize;
 unsigned long ldrstart;
 char bootstrap[492];
 unsigned short signature;
 };

 //#pragma pack(pop)


 */


char buf[SECTORSIZE * 256] =
{ 0 };

u32 EnableBusMaster()
{
   // u32 pci_ide = pci_find_class(PCI_CLASS_STORAGE_IDE);
    u32 pci_ide = 0;
    assert(pci_ide != 0);

    // enable pci bus master
    u32 value = pci_read_dword(pci_ide + PCI_CONFIG_CMD_STAT);
    value |= 0x00000004;
    pci_write_dword(pci_ide + PCI_CONFIG_CMD_STAT, value);

    u32 bm_base = pci_read_dword(pci_ide + PCI_CONFIG_BASE_ADDR_4) & 0xFFF0;

    return bm_base;
}

void hd_init()
{
    debug();

    int masterif;
    int slaveif;

    u32 bm_base = EnableBusMaster();
    assert(bm_base != 0);

    setup_hdc(&hdctab[0], HDC0_IOBASE, IRQ_HDC0, 0, &masterif, &slaveif);
    setup_hd(&hdtab[0], &hdctab[0], "hd0", HD0_DRVSEL, BM_SR_MASK_DRV0, masterif);
}
