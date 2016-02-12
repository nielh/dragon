#ifndef MULTIBOOT_HEADER
#define MULTIBOOT_HEADER 1

/* How many bytes from the start of the file we search for the header.  */
#define MULTIBOOT_SEARCH			32768
#define MULTIBOOT_HEADER_ALIGN			8

/* The magic field should contain this.  */
#define MULTIBOOT2_HEADER_MAGIC			0xe85250d6

/* This should be in %eax.  */
#define MULTIBOOT2_BOOTLOADER_MAGIC		0x36d76289

/* Alignment of multiboot modules.  */
#define MULTIBOOT_MOD_ALIGN			0x00001000

/* Alignment of the multiboot info structure.  */
#define MULTIBOOT_INFO_ALIGN			0x00000008

/* Flags set in the 'flags' member of the multiboot header.  */

#define MULTIBOOT_TAG_ALIGN                  8
#define MULTIBOOT_TAG_TYPE_END               0
#define MULTIBOOT_TAG_TYPE_CMDLINE           1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME  2
#define MULTIBOOT_TAG_TYPE_MODULE            3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO     4
#define MULTIBOOT_TAG_TYPE_BOOTDEV           5
#define MULTIBOOT_TAG_TYPE_MMAP              6
#define MULTIBOOT_TAG_TYPE_VBE               7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER       8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS      9
#define MULTIBOOT_TAG_TYPE_APM               10
#define MULTIBOOT_TAG_TYPE_EFI32             11
#define MULTIBOOT_TAG_TYPE_EFI64             12
#define MULTIBOOT_TAG_TYPE_SMBIOS            13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD          14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW          15
#define MULTIBOOT_TAG_TYPE_NETWORK           16
#define MULTIBOOT_TAG_TYPE_EFI_MMAP          17
#define MULTIBOOT_TAG_TYPE_EFI_BS            18

#define MULTIBOOT_HEADER_TAG_END  0
#define MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST  1
#define MULTIBOOT_HEADER_TAG_ADDRESS  2
#define MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS  3
#define MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS  4
#define MULTIBOOT_HEADER_TAG_FRAMEBUFFER  5
#define MULTIBOOT_HEADER_TAG_MODULE_ALIGN  6
#define MULTIBOOT_HEADER_TAG_EFI_BS        7

#define MULTIBOOT_ARCHITECTURE_I386  0
#define MULTIBOOT_ARCHITECTURE_MIPS32  4
#define MULTIBOOT_HEADER_TAG_OPTIONAL 0

#define MULTIBOOT_CONSOLE_FLAGS_CONSOLE_REQUIRED 1
#define MULTIBOOT_CONSOLE_FLAGS_EGA_TEXT_SUPPORTED 2

#ifndef ASM_FILE

#include "kernel.h"

struct multiboot_header
{
  /* Must be MULTIBOOT_MAGIC - see above.  */
  u32 magic;

  /* ISA */
  u32 architecture;

  /* Total header length.  */
  u32 header_length;

  /* The above fields plus this one must equal 0 mod 2^32. */
  u32 checksum;
};

struct multiboot_header_tag
{
  u16 type;
  u16 flags;
  u32 size;
};

struct multiboot_header_tag_information_request
{
  u16 type;
  u16 flags;
  u32 size;
  u32 requests[0];
};

struct multiboot_header_tag_address
{
  u16 type;
  u16 flags;
  u32 size;
  u32 header_addr;
  u32 load_addr;
  u32 load_end_addr;
  u32 bss_end_addr;
};

struct multiboot_header_tag_entry_address
{
  u16 type;
  u16 flags;
  u32 size;
  u32 entry_addr;
};

struct multiboot_header_tag_console_flags
{
  u16 type;
  u16 flags;
  u32 size;
  u32 console_flags;
};

struct multiboot_header_tag_framebuffer
{
  u16 type;
  u16 flags;
  u32 size;
  u32 width;
  u32 height;
  u32 depth;
};

struct multiboot_header_tag_module_align
{
  u16 type;
  u16 flags;
  u32 size;
};

struct multiboot_color
{
  u8 red;
  u8 green;
  u8 blue;
};

struct multiboot_mmap_entry
{
  u64 addr;
  u64 len;
#define MULTIBOOT_MEMORY_AVAILABLE		1
#define MULTIBOOT_MEMORY_RESERVED		2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5
  u32 type;
  u32 zero;
};

struct multiboot_tag
{
  u32 type;
  u32 size;
};

struct multiboot_tag_string
{
  u32 type;
  u32 size;
  char string[0];
};

struct multiboot_tag_module
{
  u32 type;
  u32 size;
  u32 mod_start;
  u32 mod_end;
  char cmdline[0];
};

struct multiboot_tag_basic_meminfo
{
  u32 type;
  u32 size;
  u32 mem_lower;
  u32 mem_upper;
};

struct multiboot_tag_bootdev
{
  u32 type;
  u32 size;
  u32 biosdev;
  u32 slice;
  u32 part;
};

struct multiboot_tag_mmap
{
  u32 type;
  u32 size;
  u32 entry_size;
  u32 entry_version;
  struct multiboot_mmap_entry entries[0];
};

struct multiboot_vbe_info_block
{
  u8 external_specification[512];
};

struct multiboot_vbe_mode_info_block
{
  u8 external_specification[256];
};

struct multiboot_tag_vbe
{
  u32 type;
  u32 size;

  u16 vbe_mode;
  u16 vbe_interface_seg;
  u16 vbe_interface_off;
  u16 vbe_interface_len;

  struct multiboot_vbe_info_block vbe_control_info;
  struct multiboot_vbe_mode_info_block vbe_mode_info;
};

struct multiboot_tag_framebuffer_common
{
  u32 type;
  u32 size;

  u64 framebuffer_addr;
  u32 framebuffer_pitch;
  u32 framebuffer_width;
  u32 framebuffer_height;
  u8 framebuffer_bpp;
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT	2
  u8 framebuffer_type;
  u16 reserved;
};

struct multiboot_tag_framebuffer
{
  struct multiboot_tag_framebuffer_common common;

  union
  {
    struct
    {
      u16 framebuffer_palette_num_colors;
      struct multiboot_color framebuffer_palette[0];
    };
    struct
    {
      u8 framebuffer_red_field_position;
      u8 framebuffer_red_mask_size;
      u8 framebuffer_green_field_position;
      u8 framebuffer_green_mask_size;
      u8 framebuffer_blue_field_position;
      u8 framebuffer_blue_mask_size;
    };
  };
};

struct multiboot_tag_elf_sections
{
  u32 type;
  u32 size;
  u32 num;
  u32 entsize;
  u32 shndx;
  char sections[0];
};

struct multiboot_tag_apm
{
  u32 type;
  u32 size;
  u16 version;
  u16 cseg;
  u32 offset;
  u16 cseg_16;
  u16 dseg;
  u16 flags;
  u16 cseg_len;
  u16 cseg_16_len;
  u16 dseg_len;
};

struct multiboot_tag_efi32
{
  u32 type;
  u32 size;
  u32 pointer;
};

struct multiboot_tag_efi64
{
  u32 type;
  u32 size;
  u64 pointer;
};

struct multiboot_tag_smbios
{
  u32 type;
  u32 size;
  u8 major;
  u8 minor;
  u8 reserved[6];
  u8 tables[0];
};

struct multiboot_tag_old_acpi
{
  u32 type;
  u32 size;
  u8 rsdp[0];
};

struct multiboot_tag_new_acpi
{
  u32 type;
  u32 size;
  u8 rsdp[0];
};

struct multiboot_tag_network
{
  u32 type;
  u32 size;
  u8 dhcpack[0];
};

struct multiboot_tag_efi_mmap
{
  u32 type;
  u32 size;
  u32 descr_size;
  u32 descr_vers;
  u8 efi_mmap[0];
};

void multiboot(s64 magic, s64 addr);
extern struct multiboot_tag_mmap* multiboot_mmap;
extern struct multiboot_tag_framebuffer* multiboot_framebuffer;
extern s64 phy_mem_size;
extern s64 phy_space_end;

#endif /* ! ASM_FILE */
#endif /* ! MULTIBOOT_HEADER */

