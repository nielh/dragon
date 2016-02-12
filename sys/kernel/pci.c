
#include "kernel.h"
#include "libc.h"
#include "pci.h"
#include "port.h"
#include "kmalloc.h"
#include "printk.h"

// PCI
#define PCI_REG_ADDR      0xCF8
#define PCI_REG_DATA      0xCFC
#define PCI_CFG_EN			0x80000000

// Ìî³äPCI_REG_ADDR
#define PCI_ADDR(bus,dev,func, offset) (0x80000000L | bus << 16 | dev << 11 |  func << 8 | offset)

static u32 read_config_dword(u32 addr)
{
	outd(PCI_REG_ADDR, addr);
	return ind(PCI_REG_DATA);
}

static void write_config_dword(u32 addr, u32 value)
{
	outd(PCI_REG_ADDR, addr);
	outd(PCI_REG_DATA, value);
}

u32 pci_read_dword(pci_dev_t* dev, u8 offset)
{
	return read_config_dword(dev->addr | offset);
}

void pci_write_dword(pci_dev_t* dev, u8 offset, u32 value)
{
	return write_config_dword(dev->addr | offset, value);
}

static pci_dev_t* pci_list = 0;

pci_dev_t* pci_find_dev(u16 vid, u16 did)
{
	pci_dev_t* dev = pci_list;

	while(dev)
	{
		if((vid == dev->vid) && (did == dev->did))
			return dev;

		dev = dev->next;
	}

	return NULL;
}

pci_dev_t* pci_find_class(u16 class, u16 subclass)
{
	pci_dev_t* dev = pci_list;

	while(dev)
	{
		if((class == dev->class) && (subclass == dev->subclass))
			return dev;

		dev = dev->next;
	}

	return NULL;
}

void pci_init()
{
	// 枚举PCI设备
	for(int bus = 0; bus < 8; ++bus)
	{
		for(int dev = 0; dev < 32; ++dev)
		{
			for(int func = 0; func < 8; ++func)
			{
				// 计算地址
				u32 pci_addr = PCI_ADDR(bus, dev, func, 0);

				// read vender and device id
				u32 data = read_config_dword(pci_addr);

				u16 vid = (u16)data;
				u16 did = (u16)(data >> 16);

				// 判断设备是否存在。FFFFh是非法厂商ID
				if (vid == 0xFFFF)
					continue;

                // alloc memory
				pci_dev_t* dev = (pci_dev_t*)kmalloc(sizeof(pci_dev_t));
				dev->addr = pci_addr;

				dev->vid = vid;
				dev->did = did;

				// read class and subclass
				data = read_config_dword(pci_addr + PCI_CONFIG_CLASS_REV);
				dev->class = (u8)(data >> 24);
				dev->subclass =(u8)(data >> 16);

				dev->next = pci_list; //  insert to list head
				pci_list = dev;
			}
		}
	}

}

