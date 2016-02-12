#undef DEBUG

#include "kernel.h"
#include "printk.h"
#include "libc.h"
#include "isr.h"
#include "sem.h"
#include "thread.h"
#include "object.h"
#include "vsnprintf.h"
#include "hd.h"
#include "pci.h"
#include "page.h"
#include "page_map.h"
#include "dir.h"
#include "timer.h"
#include "idt.h"
#include "port.h"
#include "kmalloc.h"
#include "ahci.h"

#define MBR_SIGNATURE 0xAA55

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
} PACKED;

struct master_boot_record
{
    char bootstrap[446];
    struct disk_partition parttab[4];
    unsigned short signature;
} PACKED;

typedef enum
{
    FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
    FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
    FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
    FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
    FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

typedef struct tagFIS_REG_H2D
{
    // u32 0
    u8	fis_type;	// FIS_TYPE_REG_H2D

    u8	pmport:4;	// Port multiplier
    u8	rsv0:3;		// Reserved
    u8	c:1;		// 1: Command, 0: Control

    u8	command;	// Command register
    u8	featurel;	// Feature register, 7:0

    // u32 1
    u8	lba0;		// LBA low register, 7:0
    u8	lba1;		// LBA mid register, 15:8
    u8	lba2;		// LBA high register, 23:16
    u8	device;		// Device register


    // u32 2
    u8	lba3;		// LBA register, 31:24
    u8	lba4;		// LBA register, 39:32
    u8	lba5;		// LBA register, 47:40
    u8	featureh;	// Feature register, 15:8

    // u32 3
    u16	count;		// Count register, 7:0

    u8	icc;		// Isochronous command completion
    u8	control;	// Control register

    // u32 4
    u8	rsv1[4];	// Reserved
} FIS_REG_H2D;

typedef struct tagFIS_REG_D2H
{
    // u32 0
    u8	fis_type;    // FIS_TYPE_REG_D2H

    u8	pmport:4;    // Port multiplier
    u8	rsv0:2;      // Reserved
    u8	i:1;         // Interrupt bit
    u8	rsv1:1;      // Reserved

    u8	status;      // Status register
    u8	printk;       // Error register

    // u32 1
    u8	lba0;        // LBA low register, 7:0
    u8	lba1;        // LBA mid register, 15:8
    u8	lba2;        // LBA high register, 23:16
    u8	device;      // Device register

    // u32 2
    u8	lba3;        // LBA register, 31:24
    u8	lba4;        // LBA register, 39:32
    u8	lba5;        // LBA register, 47:40
    u8	rsv2;        // Reserved

    // u32 3
    u8	countl;      // Count register, 7:0
    u8	counth;      // Count register, 15:8
    u8	rsv3[2];     // Reserved

    // u32 4
    u8	rsv4[4];     // Reserved
} FIS_REG_D2H;

typedef struct tagFIS_DATA
{
    // u32 0
    u8	fis_type;	// FIS_TYPE_DATA

    u8	pmport:4;	// Port multiplier
    u8	rsv0:4;		// Reserved

    u8	rsv1[2];	// Reserved

    // u32 1 ~ N
    u32	data[1];	// Payload
} FIS_DATA;

typedef struct tagFIS_PIO_SETUP
{
    // u32 0
    u8	fis_type;	// FIS_TYPE_PIO_SETUP

    u8	pmport:4;	// Port multiplier
    u8	rsv0:1;		// Reserved
    u8	d:1;		// Data transfer direction, 1 - device to host
    u8	i:1;		// Interrupt bit
    u8	rsv1:1;

    u8	status;		// Status register
    u8	printk;		// Error register

    // u32 1
    u8	lba0;		// LBA low register, 7:0
    u8	lba1;		// LBA mid register, 15:8
    u8	lba2;		// LBA high register, 23:16
    u8	device;		// Device register

    // u32 2
    u8	lba3;		// LBA register, 31:24
    u8	lba4;		// LBA register, 39:32
    u8	lba5;		// LBA register, 47:40
    u8	rsv2;		// Reserved

    // u32 3
    u8	countl;		// Count register, 7:0
    u8	counth;		// Count register, 15:8
    u8	rsv3;		// Reserved
    u8	e_status;	// New value of status register

    // u32 4
    u16	tc;		// Transfer count
    u8	rsv4[2];	// Reserved
} FIS_PIO_SETUP;

typedef struct tagFIS_DMA_SETUP
{
    // u32 0
    u8	fis_type;	// FIS_TYPE_DMA_SETUP

    u8	pmport:4;	// Port multiplier
    u8	rsv0:1;		// Reserved
    u8	d:1;		// Data transfer direction, 1 - device to host
    u8	i:1;		// Interrupt bit
    u8	a:1;            // Auto-activate. Specifies if DMA Activate FIS is needed

    u8    rsved[2];       // Reserved

    //u32 1&2

    u16   DMAbufferID;    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory. SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

    //u32 3
    u32   rsvd;           //More reserved

    //u32 4
    u32   DMAbufOffset;   //Byte offset into buffer. First 2 bits must be 0

    //u32 5
    u32   TransferCount;  //Number of bytes to transfer. Bit 0 must be 0

    //u32 6
    u32   resvd;          //Reserved

} FIS_DMA_SETUP;

typedef struct tagHBA_CMD_HEADER
{
    // DW0
    u8	cfl:5;		// Command FIS length in u32S, 2 ~ 16
    u8	a:1;		// ATAPI
    u8	w:1;		// Write, 1: H2D, 0: D2H
    u8	p:1;		// Prefetchable

    u8	r:1;		// Reset
    u8	b:1;		// BIST
    u8	c:1;		// Clear busy upon R_OK
    u8	rsv0:1;		// Reserved
    u8	pmp:4;		// Port multiplier port

    u16	prdtl;		// Physical region descriptor table length in entries

    // DW1
    volatile
    u32	prdbc;		// Physical region descriptor byte count transferred

    // DW2, 3
    u64	ctba;		// Command table descriptor base address

    // DW4 - 7
    u32	rsv1[4];	// Reserved
} HBA_CMD_HEADER;

typedef struct tagHBA_PRDT_ENTRY
{
    u64	dba;		// Data base address
    u32	rsv0;		// Reserved

    // DW3
    u32	dbc:22;		// Byte count, 4M max
    u32	rsv1:9;		// Reserved
    u32	i:1;		// Interrupt on completion
} HBA_PRDT_ENTRY;

typedef struct tagHBA_CMD_TBL
{
    // 0x00
    u8	cfis[64];	// Command FIS

    // 0x40
    u8	acmd[16];	// ATAPI command, 12 or 16 bytes

    // 0x50
    u8	rsv[48];	// Reserved

    // 0x80
    HBA_PRDT_ENTRY	prdt_entry[1];	// Physical region descriptor table entries, 0 ~ 65535
} HBA_CMD_TBL;

typedef volatile struct tagHBA_PORT
{
    u64	clb;		// 0x00, command list base address, 1K-byte aligned
    u64	fb;		// 0x08, FIS base address, 256-byte aligned
    u32	is;		// 0x10, interrupt status
    u32	ie;		// 0x14, interrupt enable
    u32	cmd;		// 0x18, command and status
    u32	rsv0;		// 0x1C, Reserved
    u32	tfd;		// 0x20, task file data
    u32	sig;		// 0x24, signature
    u32	ssts;		// 0x28, SATA status (SCR0:SStatus)
    u32	sctl;		// 0x2C, SATA control (SCR2:SControl)
    u32	serr;		// 0x30, SATA error (SCR1:SError)
    u32	sact;		// 0x34, SATA active (SCR3:SActive)
    u32	ci;		// 0x38, command issue
    u32	sntf;		// 0x3C, SATA notification (SCR4:SNotification)
    u32	fbs;		// 0x40, FIS-based switch control
    u32	rsv1[11];	// 0x44 ~ 0x6F, Reserved
    u32	vendor[4];	// 0x70 ~ 0x7F, vendor specific
} HBA_PORT;

typedef volatile struct tagHBA_MEM
{
    // 0x00 - 0x2B, Generic Host Control
    u32	cap;		// 0x00, Host capability
    u32	ghc;		// 0x04, Global host control
    u32	is;		// 0x08, Interrupt status
    u32	pi;		// 0x0C, Port implemented
    u32	vs;		// 0x10, Version
    u32	ccc_ctl;	// 0x14, Command completion coalescing control
    u32	ccc_pts;	// 0x18, Command completion coalescing ports
    u32	em_loc;		// 0x1C, Enclosure management location
    u32	em_ctl;		// 0x20, Enclosure management control
    u32	cap2;		// 0x24, Host capabilities extended
    u32	bohc;		// 0x28, BIOS/OS handoff control and status

    // 0x2C - 0x9F, Reserved
    u8	rsv[0xA0-0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    u8	vendor[0x100-0xA0];

    // 0x100 - 0x10FF, Port control registers
    HBA_PORT	ports[1];	// 1 ~ 32
} HBA_MEM;

typedef volatile struct tagHBA_FIS
{
    // 0x00
    FIS_DMA_SETUP	dsfis;		// DMA Setup FIS
    u8		pad0[4];

    // 0x20
    FIS_PIO_SETUP	psfis;		// PIO Setup FIS
    u8		pad1[12];

    // 0x40
    FIS_REG_D2H	rfis;		// Register ¡§C Device to Host FIS
    u8		pad2[4];

    // 0x58
    u8	sdbfis[8];		// Set Device Bit FIS

    // 0x60
    u8		ufis[64];

    // 0xA0
    u8		rsv[0x100-0xA0];
} HBA_FIS;

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

enum
{
    PORT_INT_CPD	= (1 << 31),	// Cold Presence Detect Status/Enable
    PORT_INT_TFE	= (1 << 30),	// Task File Error Status/Enable
    PORT_INT_HBF	= (1 << 29),	// Host Bus Fatal Error Status/Enable
    PORT_INT_HBD	= (1 << 28),	// Host Bus Data Error Status/Enable
    PORT_INT_IF		= (1 << 27),	// Interface Fatal Error Status/Enable
    PORT_INT_INF	= (1 << 26),	// Interface Non-fatal Error Status/Enable
    PORT_INT_OF		= (1 << 24),	// Overflow Status/Enable
    PORT_INT_IPM	= (1 << 23),	// Incorrect Port Multiplier Status/Enable
    PORT_INT_PRC	= (1 << 22),	// PhyRdy Change Status/Enable
    PORT_INT_DI		= (1 << 7),		// Device Interlock Status/Enable
    PORT_INT_PC		= (1 << 6),		// Port Change Status/Enable
    PORT_INT_DP		= (1 << 5),		// Descriptor Processed Interrupt
    PORT_INT_UF		= (1 << 4),		// Unknown FIS Interrupt
    PORT_INT_SDB	= (1 << 3),		// Set Device Bits FIS Interrupt
    PORT_INT_DS		= (1 << 2),		// DMA Setup FIS Interrupt
    PORT_INT_PS		= (1 << 1),		// PIO Setup FIS Interrupt
    PORT_INT_DHR	= (1 << 0),		// Device to Host Register FIS Interrupt
};

// Serial ATA Status and control
#define HBA_PORT_IPM_MASK		0x00000f00	// Interface Power Management
#define SSTS_PORT_IPM_ACTIVE	0x00000100	// active state
#define SSTS_PORT_IPM_PARTIAL	0x00000200	// partial state
#define SSTS_PORT_IPM_SLUMBER	0x00000600	// slumber power management state
#define SSTS_PORT_IPM_DEVSLEEP	0x00000800	// devsleep power management state
#define SCTL_PORT_IPM_NORES		0x00000000	// no power restrictions
#define SCTL_PORT_IPM_NOPART	0x00000100	// no transitions to partial
#define SCTL_PORT_IPM_NOSLUM	0x00000200	// no transitions to slumber

#define HBA_PORT_SPD_MASK		0x000000f0	// Interface speed

#define HBA_PORT_DET_MASK		0x0000000f	// Device Detection
#define SSTS_PORT_DET_NODEV		0x00000000	// no device detected
#define SSTS_PORT_DET_NOPHY		0x00000001	// device present but PHY not est.
#define SSTS_PORT_DET_PRESENT	0x00000003	// device present and PHY est.
#define SSTS_PORT_DET_OFFLINE	0x00000004	// device offline due to disabled
#define SCTL_PORT_DET_NOINIT	0x00000000	// no initalization request
#define SCTL_PORT_DET_INIT		0x00000001	// perform interface initalization
#define SCTL_PORT_DET_DISABLE	0x00000004	// disable phy

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	    0x96690101	// Port multiplier

#define	AHCI_DEV_NULL       0x0
#define	AHCI_DEV_SATA       0x1
#define	AHCI_DEV_SATAPI     0x2
#define	AHCI_DEV_SEMB       0x3
#define	AHCI_DEV_PM         0x4

struct ahci_t;

typedef struct port_t
{
    ops_t* ops;

    int index;

    HBA_PORT *mmio;
    HBA_CMD_HEADER *clb;
    HBA_CMD_TBL* cmtl;
    HBA_FIS* fb;

    sem_t ready;
    sem_t complete;

} port_t;

typedef struct ahci_t
{
    volatile HBA_MEM *abar;

    int cap;
    int nr_ports;
    int nr_cmds;
    int has_ncq;
    int has_clo;
    int irq_num;

    port_t ports[32];
} ahci_t;

ahci_t ahci= {0};

static int part_create(port_t *port);


// Check device type
static int check_type(HBA_PORT *port)
{
    u32 ssts = port->ssts;

    if ((ssts & SSTS_PORT_DET_PRESENT) != SSTS_PORT_DET_PRESENT)	// Check drive status
        return AHCI_DEV_NULL;

    if ((ssts & SSTS_PORT_IPM_ACTIVE) != SSTS_PORT_IPM_ACTIVE)
        return AHCI_DEV_NULL;

    switch (port->sig)
    {
    case SATA_SIG_ATAPI:
        return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB:
        return AHCI_DEV_SEMB;
    case SATA_SIG_PM:
        return AHCI_DEV_PM;
    default:
        return AHCI_DEV_SATA;
    }
}

// Find a free command list slot
static int find_cmdslot(port_t* port)
{
    HBA_PORT *mmio = port->mmio;

    // If not set in SACT and CI, the slot is free
    u32 slots = (mmio->sact | mmio->ci);

    for (int i=0; i< ahci.nr_cmds; i++)
    {
        if ((slots&1) == 0)
            return i;

        slots >>= 1;
    }

    printk("Cannot find free command list entry\n");
    return -1;
}

#define MAX_DATA_BYTE_COUNT  (4*1024*1024)

static int port_fill_prd(HBA_PRDT_ENTRY* prd, unsigned char *user_buf, int user_buf_len)
{
    assert(((u64)user_buf & 0x3) == 0); // align to WORD
    assert(((u64)user_buf_len & 0x3) == 0); // align to WORD

    u64 last_end = 0;
    int prd_count = 1;

    while(user_buf_len > 0)
    {
        u64 phy_start = (u64)vir_to_phy(_PT4, user_buf); // 转换成物理页地址
        //debug("vir[0x%x] = phy[0x%x]", user_buf, phy_start);

        int size;

        if(phy_start & 0xFFF) // 物理地址非页对齐
            size = PAGE_ALIGN(phy_start) -phy_start; // 对齐到页边界
        else
            size = PAGE_SIZE;

        size = (user_buf_len < size) ? user_buf_len : size; // 不能超出缓冲区长度

        if(last_end == 0) // 第一块
        {
            prd->dba = phy_start;
            prd->dbc = 0x3FFFFF & (size - 1); // 0 代表1个字节
            //prd->i = 1;
        }
        else
        {
            if( (phy_start == last_end) && // 物理内存页连续 并且不超过 4M
                    (prd->dbc + PAGE_SIZE < MAX_DATA_BYTE_COUNT))
            {
                prd->dbc += PAGE_SIZE;
            }
            else
            {
                prd++; // 切换下一个sg
                prd_count ++;

                assert(prd_count < AHCI_MAX_PRDS);

                prd->dba = phy_start;
                prd->dbc = 0x3FFFFF & (size - 1); // 0 代表1个字节
                //prd->i = 1;
            }
        }

        last_end = phy_start + size;
        user_buf += size;
        user_buf_len -= size;
    }

    return prd_count;
}

static void* port_open(void* obj, char* path, s64 flag, s64 mode)
{
    return obj;
}

static s64 port_rw_block(port_t* port, void* buf, s64 len, s64 pos, int isWrite)
{
    u32 count = len >> 9; // sector count
    u64 sector = pos >> 9;//sector index
    HBA_PORT *mmio = port->mmio;

    int spin = 0; // Spin lock timeout counter
    int slot = find_cmdslot(port);
    assert(slot >= 0);

    // Fill Command table
    HBA_CMD_TBL *cmtl = port->cmtl;
    memset(cmtl, 0, sizeof(HBA_CMD_TBL)); // init to zero

    FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(cmtl->cfis);// Setup command

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;	// Command
    cmdfis->command = isWrite? ATA_CMD_WRITE_DMA_EXT: ATA_CMD_READ_DMA_EXT;
    cmdfis->lba0 = (u8)sector;
    cmdfis->lba1 = (u8)(sector>>8);
    cmdfis->lba2 = (u8)(sector>>16);
    cmdfis->device = 1<<6;	// LBA mode
    cmdfis->lba3 = (u8)(sector>>24);
    cmdfis->lba4 = (u8)(sector>>32);
    cmdfis->lba5 = (u8)(sector>>40);
    cmdfis->count = count;
    int prd_cnt = port_fill_prd(cmtl->prdt_entry, buf, len); // fill prds

    // Fill Command List
    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)port->clb;
    cmdheader += slot;
    memset(cmdheader, 0, sizeof(HBA_CMD_HEADER)); // init to zero

    cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(u32);	// Command FIS size
    cmdheader->w = isWrite;		// Read from device
    cmdheader->p = isWrite;
    cmdheader->ctba = (u64)VIR2PHY(cmtl);
    cmdheader->prdtl = prd_cnt;

    // The below loop waits until the port is no longer busy before issuing a new command
    while ((mmio->tfd & (AHCI_PORT_TFD_STS_BSY | AHCI_PORT_TFD_STS_DRQ)) && spin < 1000000)
    {
        spin++;
    }

    if (spin >= 1000000)
    {
        panic("Port is hung\n");
    }

    sem_init(&port->complete, 0, "port complete");
    mmio->ci = (1 << slot);	// Issue command

    down(&port->complete);
    // Check again
    if (mmio->is & PORT_INT_TFE)
    {
        panic("IO error\n");
        return 0;
    }

    return 0;
}

static s64 port_rw(port_t* port, void* buf, s64 len, s64 pos, int isWrite)
{
    ENTER();

    assert(((u64)buf & 3) == 0); // buf must DWORD align
    assert((len & 0x1FF) == 0); // must 512 bytes align
    //assert(len <= 4096); // must in a page
    assert((pos & 0x1FF) == 0); // must 512 bytes align

    const int MAX_LEN = (1 << 30); // 256 sector

    down(&port->ready);

    s64 ret_len = len;
    while(len > 0)
    {
        s64 rwlen = len < MAX_LEN ? len : MAX_LEN;
        port_rw_block(port, buf, rwlen, pos, isWrite);

        buf += rwlen;
        pos += rwlen;
        len -= rwlen;
    }

    up_one(&port->ready);

    LEAVE();
    return ret_len;
}

static s64 port_read(void* obj, void* buf, s64 len, s64 pos)
{
    return port_rw((port_t*)obj, buf, len, pos, FALSE);
}

static s64 port_write(void* obj, void* buf, s64 len, s64 pos)
{
    return port_rw((port_t*)obj, buf, len, pos, TRUE);
}

static ops_t port_ops =
{
    .open = port_open,
    .read = port_read,
    .write = port_write,
};

/*===========================================================================*
 *				port_hardreset				     *
 *===========================================================================*/
static void port_hardreset(HBA_PORT *base)
{
    /* Perform a port-level (hard) reset on the given port.
     */
    base->sctl = AHCI_PORT_SCTL_DET_INIT;
    mdelay(5);
    base->sctl = AHCI_PORT_SCTL_DET_NONE;
}


#define ATA_BUSY (1 << 7)


static int wait_spinup(HBA_PORT *base)
{
    //ulong start;
    u32 tf_data;

    //start = get_timer(0);
    do
    {
        tf_data = base->tfd;
        if (!(tf_data & ATA_BUSY))
            return 0;
    }
    while (1);

    return -1;
}

char mbr[512] = {0};


int port_init(int idx)
{
    ENTER();

    port_t* port = &ahci.ports[idx];
    port->ops = &port_ops;
    port->index = idx;
    sem_init(&port->ready, 1, "port ready");

    HBA_PORT *mmio = &ahci.abar->ports[idx];
    port->mmio = mmio;

    // ensure port idle
    while(mmio->cmd & (AHCI_PORT_CMD_FR| AHCI_PORT_CMD_FRE| AHCI_PORT_CMD_ST| AHCI_PORT_CMD_CR))
    {
    	LOG("port%d: running, stop it\n", idx);

        // stop device
        mmio->cmd &= ~(AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST);
        mdelay(500);
    }

    LOG("port%d: stopped\n", idx);

    // PANIC("port%d: device not idle state, status %u", idx, mmio->cmd);
    // alloc prdt memory
    s64 phy_mem = page_alloc()->address;
    assert (phy_mem);

    s64 vir_mem = PHY2VIR(phy_mem);

    // Command List 1K
    port->clb = (void*)vir_mem;
    mmio->clb = phy_mem;
    // FIS 256 byte
    port->fb = (void*)vir_mem + AHCI_CL_SIZE;
    mmio->fb = phy_mem + AHCI_CL_SIZE;

    // Command Table
    HBA_CMD_TBL* cmtl = (void*)vir_mem + AHCI_CL_SIZE + AHCI_FIS_SIZE;
    port->cmtl = cmtl;
    // Enable FIS receive.
    mmio->cmd |= AHCI_PORT_CMD_FRE;
    while((mmio->cmd & AHCI_PORT_CMD_FR) == 0);

    mmio->serr = -1;
    mmio->is = -1; // clear pending interrupt
    mmio->ie = AHCI_PORT_IE_MASK;

    //mmio->cmd |= AHCI_PORT_CMD_SUD;

    u32 status = mmio->ssts & AHCI_PORT_SSTS_DET_MASK;
    if(status != AHCI_PORT_SSTS_DET_PHY)
        panic("port%d: device not connect, status %u", idx, status);

    // If a Phy connection has been established, and the BSY and
    // DRQ flags are cleared, the device is ready.
    if (mmio->tfd & (AHCI_PORT_TFD_STS_BSY | AHCI_PORT_TFD_STS_DRQ))
        panic("port%d: device busy, status %u", idx, mmio->tfd);

    mmio->cmd |= AHCI_PORT_CMD_ST;
    while((mmio->cmd & AHCI_PORT_CMD_CR) == 0);

    LOG("port%d: started\n", idx);
/*
    int spin =0;
    // The below loop waits until the port is no longer busy before issuing a new command
    while ((mmio->tfd & (AHCI_PORT_TFD_STS_BSY | AHCI_PORT_TFD_STS_DRQ)) && spin < 1000000)
    {
        spin++;
    }

    if (spin >= 1000000)
    {
        PANIC("port%d: tfd busy, status %u", idx,     mmio->serr = -1;
    mmio->is = -1;mmio->tfd);
    }
*/

    char sd_name[32];
    snprintf(sd_name, sizeof(sd_name), "sd%d", idx); // get hd0 name
    append(HANDLE_OBJ, sd_name, port); // 注册 hd0 目录

    LOG("create sata disk [/obj/%s]\n", sd_name);
/*
    while(1)
    {
        port_rw(port, mbr, 512, 0, 0);
        DBG("read mbr 512 ok\n");
        mdelay(2000);
    }
*/
    part_create(port);

    LEAVE();
    return 0;
}



typedef struct part_t
{
    ops_t* ops;
    port_t *hd;
    unsigned int start;
    unsigned int len;
    unsigned short bootid;
    unsigned short systid;
} part_t;

static void* part_open(void* obj, char* path, s64 flag, s64 mode)
{
    return obj;
}

static s64 part_read(void* obj, void* buf, s64 len, s64 pos)
{
    part_t *part = (part_t *)obj;
    return port_read(part->hd, buf, len, pos + ((s64)part->start << 9));
}

static s64 part_write(void* obj, void* buf, s64 len, s64 pos)
{
    part_t *part = (part_t *)obj;
    return port_write(part->hd, buf, len, pos + ((s64)part->start << 9));
}

static ops_t part_ops =
{
    .open = part_open,
    .read = part_read,
    .write = part_write
};

#define SECTORSIZE 512

static int part_create(port_t *port)
{
    struct master_boot_record* mbr = kmalloc(sizeof(struct master_boot_record));

    //dev_t devno;
    int rc;
    int i;
    //char devname[64];

    // Read partition table
    rc = port_read(port, mbr, SECTORSIZE, 0);

    if (rc < 0)
    {
        printk("error %d reading partition table\n", rc);
        return rc;
    }

    // Create partition devices
    if (mbr->signature != MBR_SIGNATURE)
    {
        printk("%s: illegal boot sector signature\n", "hd0");
        return -1;
    }

    for (i = 0; i < 4; i++)
    {
        if(mbr->parttab[i].systid == 0)
            continue;

        // 建立 portN 节点
        part_t *part = (part_t *) kmalloc(sizeof(part_t));

        part->ops = &part_ops;
        part->hd = port;
        part->bootid = mbr->parttab[i].bootid;
        part->systid = mbr->parttab[i].systid;
        part->start = mbr->parttab[i].relsect;
        part->len = mbr->parttab[i].numsect;

        printk("[%d] id:%3x boot:%3x   start:%dM  len:%dM\n",
                i,
                part->systid,
                part->bootid,
                part->start >> 11,
                part->len >> 11);

        char part_name[32];
        snprintf(part_name, sizeof(part_name), "sd%dp%d", port->index, i);
        append(HANDLE_OBJ, part_name, part);

        printk("create part [/obj/%s]\n", part_name);
    }

    kmfree(mbr);

    return 0;
}



static void port_intr(int idx)
{
    ENTER();

    u32 smask, emask;
    //int success;

    volatile HBA_PORT *mmio = &ahci.abar->ports[idx];
    smask = mmio->is;
    emask = smask & mmio->ie;
    mmio->is = smask;

    LOG("port_intr: interrupt (%08x)\n", smask);

    if(emask == 0)
        return;

    if(emask & AHCI_PORT_IS_PRCS)
    {
		/* Clear the N diagnostics bit to clear this interrupt. */
		mmio->serr = AHCI_PORT_SERR_DIAG_N;
		LOG("port%d: device detached\n",idx);
    }

    if (emask & AHCI_PORT_IS_PCS)
    {
		// Clear the X diagnostics bit to clear this interrupt.
		mmio->serr = AHCI_PORT_SERR_DIAG_X;
		LOG("port%d: device attached\n",idx);
    }

    if (emask & AHCI_PORT_IS_DHRS)
    {
        if((mmio->ci & 1) == 0)
        {
            up_one(&ahci.ports[idx].complete);
            //DBG("port%d: device complete\n",idx);
        }
    }

    LEAVE();
}

static void ahci_handler(void* data, registers_t* regs)
{
    // Handle an interrupt for each port that has the interrupt bit set.
    u32 mask = ahci.abar->is;

    for (int idx = 0; idx < ahci.nr_ports; idx++)
    {
        if (mask & (1 << idx))
        {
            port_intr(idx);
        }
    }

    // Clear the bits that we processed.
    ahci.abar->is = mask;
    outb(0x20, 0x20); // EOI
}

/*===========================================================================*
 *				ahci_reset				     *
 *===========================================================================*/
static void ahci_reset(HBA_MEM* abar)
{
    /* Reset the HBA. Do not enable AHCI mode afterwards.
     */
    abar->ghc |= AHCI_HBA_GHC_AE;
    abar->ghc |= AHCI_HBA_GHC_HR;

    int spin_count = 1000000;
    while(spin_count>0)
    {
        if(abar->ghc&AHCI_HBA_GHC_HR)
            spin_count --;
        else
            break;
    }

    if (spin_count <= 0)
        panic("unable to reset HBA");
}

static void ahci_print_info(HBA_MEM* abar)
{
    ENTER();

    u32 vers, cap, cap2, impl, speed;
    const char *speed_s;
    const char *scc_s;

    vers = abar->vs;
    cap = abar->cap;
    cap2 = abar->cap2;
    impl = abar->pi;

    speed = (cap >> 20) & 0xf;
    if (speed == 1)
        speed_s = "1.5";
    else if (speed == 2)
        speed_s = "3";
    else if (speed == 3)
        speed_s = "6";
    else
        speed_s = "?";

    scc_s = "SATA";

    printk("AHCI %02x%02x.%02x%02x %u slots %u ports %s Gbps 0x%x impl %s mode\n",
            (vers >> 24) & 0xff,
            (vers >> 16) & 0xff,
            (vers >> 8) & 0xff,
            (vers & 0xff),
            ((cap >> 8) & 0x1f) + 1,
            (cap & 0x1f) + 1,
            speed_s, impl, scc_s);

    printk("FLAG: "
            "%s%s%s%s%s%s%s"
            "%s%s%s%s%s%s%s"
            "%s%s%s%s%s%s\n",
            cap & (1 << 31) ? "64bit " : "",
            cap & (1 << 30) ? "ncq " : "",
            cap & (1 << 28) ? "ilck " : "",
            cap & (1 << 27) ? "stag " : "",
            cap & (1 << 26) ? "pm " : "",
            cap & (1 << 25) ? "led " : "",
            cap & (1 << 24) ? "clo " : "",
            cap & (1 << 19) ? "nz " : "",
            cap & (1 << 18) ? "only " : "",
            cap & (1 << 17) ? "pmp " : "",
            cap & (1 << 16) ? "fbss " : "",
            cap & (1 << 15) ? "pio " : "",
            cap & (1 << 14) ? "slum " : "",
            cap & (1 << 13) ? "part " : "",
            cap & (1 << 7) ? "ccc " : "",
            cap & (1 << 6) ? "ems " : "",
            cap & (1 << 5) ? "sxs " : "",
            cap2 & (1 << 2) ? "apst " : "",
            cap2 & (1 << 1) ? "nvmp " : "",
            cap2 & (1 << 0) ? "boh " : "");

    LEAVE();
}

int ahci_init()
{
    ENTER();

    pci_dev_t* pDev = pci_find_class(PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_SATA);
    assert(pDev);

    //u32 pci_cmd = pci_read_dword(pDev, PCI_CONFIG_CMD_STAT);
    //LOG("pci_cmd: 0x%x\n", pci_cmd);

    ahci.abar = (HBA_MEM*)PHY2VIR(pci_read_dword(pDev, PCI_CONFIG_BASE_ADDR_5));
    ahci.irq_num = (u8)pci_read_dword(pDev, PCI_CONFIG_INTERRUPT_LINE);
    irq_register(ahci.irq_num, ahci_handler, NULL); //
    irq_enable(ahci.irq_num);

    ahci_print_info(ahci.abar);
    /*
        // enable bus master
    	u32 tmp = pci_read_dword(pDev, PCI_CONFIG_CMD);
    	tmp |= PCI_COMMAND_MASTER;
    	pci_write_dword(pDev, PCI_CONFIG_CMD, tmp);
    */
    /* Reset the HBA. */
    //ahci_reset(ahci.abar);
    //ahci.abar->is = (0xFFFFFFFF);
    /* configure PCS */
    //u32 tmp16 = pci_read_dword(pDev, 0x92);
    //tmp16 |= 0xf;
    // pci_write_dword(pDev, 0x92, tmp16);

    //mdelay(500);

    ahci.abar->is = -1;		// Clear pending interrupt bits
    ahci.abar->ghc |= (AHCI_HBA_GHC_AE | AHCI_HBA_GHC_IE);// Enable AHCI and interrupts.
    LOG("ahci.abar->ghc:0x%x\n", ahci.abar->ghc);

    ahci.cap = ahci.abar->cap;
    ahci.nr_ports = ((ahci.cap >> AHCI_HBA_CAP_NP_SHIFT) & AHCI_HBA_CAP_NP_MASK) + 1;
    ahci.nr_cmds = ((ahci.cap >> AHCI_HBA_CAP_NCS_SHIFT) & AHCI_HBA_CAP_NCS_MASK) + 1;
    ahci.has_ncq = (ahci.cap & AHCI_HBA_CAP_SNCQ);
    ahci.has_clo = (ahci.cap & AHCI_HBA_CAP_SCLO);

    /* Initialize each of the implemented ports. We ignore CAP.NP. */
    int mask = ahci.abar->pi;

    for (int i = 0; i < ahci.nr_ports; i++)
    {
        if (mask & (1 << i))
        {
            int dt = check_type(&ahci.abar->ports[i]);
            if (dt == AHCI_DEV_SATA)
            {
                printk("SATA drive found at port %d\n", i);
                port_init(i);
            }
            else if (dt == AHCI_DEV_SATAPI)
            {
                printk("SATAPI drive found at port %d\n", i);
            }
            else if (dt == AHCI_DEV_SEMB)
            {
                printk("SEMB drive found at port %d\n", i);
            }
            else if (dt == AHCI_DEV_PM)
            {
                printk("PM drive found at port %d\n", i);
            }
        }
    }

    LEAVE();
    return 0;
}

void ahci_test()
{
    void* file = open(HANDLE_ROOT, "obj/sd0p0", 0, 0);
    assert(file);

    s64   start = 0;
    s64   buff_len = 512;
    void* buff_rd = kmalloc(buff_len);

    for(int j =0 ; j < 10; j ++)
    {
        int read_bytes = read(file, buff_rd, buff_len, start);
        assert(read_bytes == buff_len);
    }

    kmfree(buff_rd);

    return;
}

void ahci_test_rw()
{
    void* file = open(HANDLE_ROOT, "obj/sd0p1", 0, 0);
    assert(file);

    for(int j =0 ; j < 10; j ++)
    {
        s64 buff_len = (1 << 9);
        u32* buff_rd = kmalloc(buff_len);
        u32* buff_wr = kmalloc(buff_len);
        s64 start = (s64)6*(1 << 30); // 1G

        for(int i=0; i <buff_len/4; i ++)
            buff_wr[i] = i+j;

        int write_bytes = write(file, buff_wr, buff_len, start);
        assert(write_bytes == buff_len);

        int read_bytes = read(file, buff_rd, buff_len, start);
        assert(read_bytes == buff_len);

        for(int i=0; i <buff_len/4; i ++)
            assert(buff_wr[i] == (i+j));

        kmfree(buff_rd);
        kmfree(buff_wr);
    }

    return;
}
