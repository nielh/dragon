#ifndef PCI_H
#define PCI_H

#include "kernel.h"

#define PCI_CONFIG_VENDOR               0x00
#define PCI_CONFIG_CMD_STAT             0x04
#define PCI_CONFIG_CMD                  0x04    // 16 bits
#define PCI_CONFIG_STATUS               0x06    // 16 bits
#define PCI_CONFIG_CLASS_REV            0x08
#define PCI_CONFIG_HDR_TYPE             0x0C
#define PCI_CONFIG_CACHE_LINE_SIZE      0x0C    // 8 bits
#define PCI_CONFIG_LATENCY_TIMER        0x0D    // 8 bits
#define PCI_CONFIG_BASE_ADDR_0          0x10
#define PCI_CONFIG_BASE_ADDR_1          0x14
#define PCI_CONFIG_BASE_ADDR_2          0x18
#define PCI_CONFIG_BASE_ADDR_3          0x1C
#define PCI_CONFIG_BASE_ADDR_4          0x20
#define PCI_CONFIG_BASE_ADDR_5          0x24
#define PCI_CONFIG_CIS                  0x28
#define PCI_CONFIG_SUBSYSTEM            0x2C
#define PCI_CONFIG_ROM                  0x30
#define PCI_CONFIG_CAPABILITIES         0x34
#define PCI_CONFIG_INTR                 0x3C
#define PCI_CONFIG_INTERRUPT_LINE       0x3C    // 8 bits
#define PCI_CONFIG_INTERRUPT_PIN        0x3D    // 8 bits
#define PCI_CONFIG_MIN_GNT              0x3E    // 8 bits
#define PCI_CONFIG_MAX_LAT              0x3F    // 8 bits
#define PCI_BASE_ADDRESS_MEM_MASK	(~0x0FUL)
#define PCI_BASE_ADDRESS_IO_MASK	(~0x03UL)

#define PCI_CLASS_MASK          0xFF0000
#define PCI_SUBCLASS_MASK       0xFFFF00

#define PCI_HOST_BRIDGE         0x060000
#define PCI_BRIDGE              0x060400
#define PCI_ISA_BRIDGE          0x060100


#define PCI_CLASS_MASS_STORAGE  0x01
#define PCI_SUBCLASS_SATA       0x06

#define  PCI_CMD_MASTER	        0x4	/* Enable bus mastering */
#define  PCI_CMD_ID	        (1<<10)	/* Enable bus mastering */

typedef struct pci_dev_t
{
	struct pci_dev_t* next;
	u16 vid;
	u16 did;
	u16 class;
	u16 subclass;
    u32 addr;
} pci_dev_t;

pci_dev_t* pci_find_dev(u16 vid, u16 did);
pci_dev_t* pci_find_class(u16 class, u16 subclass);

u32 pci_read_dword(pci_dev_t* dev, u8 offset);
void pci_write_dword(pci_dev_t* dev, u8 offset, u32 value);
void pci_init();
#endif //
