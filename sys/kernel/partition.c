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
#include "kmalloc.h"

#define SECTORSIZE 512
#define CDSECTORSIZE 2048

#define HD_CONTROLLERS 2
#define HD_DRIVES 4
#define HD_PARTITIONS 4

#define MAX_PRDS (PAGESIZE / 8)
#define MAX_DMA_XFER_SIZE ((MAX_PRDS - 1) * PAGESIZE)

#define HDC0_IOBASE 0x01F0
#define HDC1_IOBASE 0x0170

#define HD0_DRVSEL 0x0 // was:0xA0
#define HD1_DRVSEL 0x1 // was:0xB0
#define HD_LBA 0x40

#define idedelay() delay(25)

#define ATADEV_UNKNOWN	0
#define ATADEV_SATAPI	1
#define ATADEV_PATAPI	2
#define ATADEV_SATA		3
#define ATADEV_PATA		4

#define HDC_DATA 		0x0000
#define HDC_ERR 		0x0001
#define HDC_FEATURE 	0x0001
#define HDC_SECTORCNT 	0x0002
#define HDC_LBA0 		0x0003
#define HDC_LBA1 		0x0004
#define HDC_LBA2 		0x0005
#define HDC_DRVHD 		0x0006
#define HDC_STATUS 		0x0007
#define HDC_COMMAND 	0x0007
#define HDC_DEVCTRL 	0x0008
#define HDC_ALT_STATUS 	0x0206
#define HDC_CONTROL 	0x0206

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
// Timeouts (in ms)
//

#define HDTIMEOUT_DRDY 5000
#define HDTIMEOUT_DRQ 5000
#define HDTIMEOUT_CMD 1000
#define HDTIMEOUT_BUSY 60000
#define HDTIMEOUT_XFER 10000

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
// Drive interface types
//

#define HDIF_NONE 0
#define HDIF_PRESENT 1
#define HDIF_UNKNOWN 2
#define HDIF_ATA 3
#define HDIF_ATAPI 4

//
// Transfer type
//

#define HD_XFER_IDLE 0
#define HD_XFER_READ 1
#define HD_XFER_WRITE 2
#define HD_XFER_DMA 3
#define HD_XFER_IGNORE 4

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

typedef struct hdparam_t
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
} hdparam_t;


typedef struct partition_t
{
    ops_t* ops;
    struct hd *hd;
    unsigned int start;
    unsigned int len;
    unsigned short bootid;
    unsigned short systid;

} partition_t;

typedef struct hd_t
{
    ops_t* ops;

    int iobase;
    int device;

    //struct hdc *hdc;      // Controller
    hdparam_t param; // Drive parameter block
    int drvsel;           // Drive select on controller
    int use32bits;        // Use 32 bit transfers
    int sectbufs;         // Number of sector buffers
    int lba;              // LBA mode
    int iftype;           // IDE interface type (ATA/ATAPI)
    int media;            // Device media type (hd, cdrom, ...)
    int multsect;         // Sectors per interrupt
    int udmamode;         // UltraDMA mode

    // Geometry
    unsigned int blks; // Number of blocks on drive
    unsigned int size; // Size in MB

    unsigned int cyls;    // Number of cylinders
    unsigned int heads;   // Number of heads
    unsigned int sectors; // Sectors per track

    partition_t parts[HD_PARTITIONS]; // Partition info
} hd_t;

static void hd_fixstring(unsigned char *s, int len)
{
	return ;

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

static int hd_wait_idle(int iobase)
{
    while (1)
    {
        char status = inb(iobase + HDC_STATUS);
        if ((status & HDCS_BSY)== 0)
			break;

        ASM("pause");
    }
}

static int hd_wait(int iobase, unsigned char mask, unsigned int timeout)
{
    unsigned int  start;
    unsigned char status;

    start = clock_ticks;
    while (1)
    {
        status = inb(iobase + HDC_ALT_STATUS);

        if (status & HDCS_ERR)
        {
            unsigned char error;

            error = inb(iobase + HDC_ERR);
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


static int soft_reset(int iobase)
{
	// Reset controller
	outb(iobase + HDC_CONTROL, HDDC_HD15 | HDDC_SRST | HDDC_NIEN);
	idedelay();
	outb(iobase + HDC_CONTROL, HDDC_HD15 | HDDC_NIEN);
	idedelay();
}


static sem_t intr_flag;
//static int  intr_dir;

static int hd_identify(hd_t *hd)
{
    debug();

    // Ignore interrupt for identify command
    //intr_dir = HD_XFER_IGNORE;
    // reset_event(&hd->hdc->ready);

    //sem_init(&intr_flag, 0);

    //soft_reset(hd->iobase);
    hd_wait_idle(hd->iobase);

    // Issue read drive parameters command
    outb(hd->iobase + HDC_FEATURE, 0);
    outb(hd->iobase + HDC_DRVHD, hd->device);
	delay(1); // delay 1ms
    outb(hd->iobase + HDC_COMMAND, HDCMD_IDENTIFY);

    // Wait for data ready
    //wait(&intr_flag);

    // Some controllers issues the interrupt before data is ready to be read
    // Make sure data is ready by waiting for DRQ to be set
    if (hd_wait(hd->iobase, HDCS_DRQ, HDTIMEOUT_DRQ) != 0)
	{
		while(1){};
	}
   //     return -1;


    // Read parameter data
    insw(hd->iobase + HDC_DATA, &(hd->param), SECTORSIZE / 2);

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

	printk("disk serial:%s\n", hd->param.serial);
	printk("disk model :%s\n", hd->param.model);


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

static int create_hd(int iobase, int device)
{
	hd_t * hd = (hd_t *) kmalloc(sizeof(struct hd_t));
    assert(hd != NULL)

    hd->iobase = iobase;
    hd->device = device;

    hd_identify(hd);
}

static int probe_device_type(int iobase, int dev_no)
{
	int device = 0xA0 | (dev_no<<4);
	soft_reset(iobase);		/* waits until master drive is ready again */
	outb(iobase + HDC_DRVHD, device);
	inb(iobase);			/* wait 400ns for drive select to work */
	inb(iobase);
	inb(iobase);
	inb(iobase);
	unsigned cl=inb(iobase + HDC_LBA1);	/* get the "signature bytes" */
	unsigned ch=inb(iobase + HDC_LBA2);

	if (cl==0x14 && ch==0xEB)
	{
		debug("detect patapi device at 0x%x slot %d", iobase, dev_no);
		return ATADEV_PATAPI;
	}

	if (cl==0x69 && ch==0x96)
	{
		debug("detect satapi device at 0x%x slot %d", iobase, dev_no);
		return ATADEV_SATAPI;
	}

	if (cl==0 && ch == 0)
	{
		debug("detect pata device at 0x%x slot %d", iobase, dev_no);

		create_hd(iobase, device);
		return ATADEV_PATA;
	}

	if (cl==0x3c && ch==0xc3)
	{
		debug("detect sata device at 0x%x slot %d", iobase, dev_no);
		return ATADEV_SATA;
	}

	return ATADEV_UNKNOWN;
}

static void hdc_handler(void *data, registers_t* regs)
{
    debug("interrupt received");
    signal(&intr_flag);
}

void ata_init()
{
	debug();

    outb(HDC0_IOBASE + HDC_CONTROL, HDDC_HD15);
    idedelay();

    // Enable interrupts
    irq_register(IRQ_HDC0, hdc_handler, NULL);
    enable_irq(IRQ_HDC0);

	probe_device_type(HDC0_IOBASE, HD0_DRVSEL);
	probe_device_type(HDC0_IOBASE, HD1_DRVSEL);
	probe_device_type(HDC1_IOBASE, HD0_DRVSEL);
	probe_device_type(HDC1_IOBASE, HD1_DRVSEL);
}
