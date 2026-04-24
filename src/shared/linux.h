#include "efi.h" // IWYU pragma: keep

#pragma pack(push, 1)

// The first step in loading a Linux kernel should be to load the real-mode code
// (boot sector and setup code) and then examine the following header at offset
// 0x01f1. The real-mode code can total up to 32K, although the boot loader may
// choose to load only the first two sectors (1K) and then examine the bootup
// sector size.
typedef struct {
    // The size of the setup code in 512-byte sectors. If this field is 0, the real
    // value is 4. The real-mode code consists of the boot sector (always one 512-byte
    // sector) plus the setup code.
    UINT8 setup_sects;
    // If this field is nonzero, the root defaults to readonly. The use of this field is
    // deprecated; use the “ro” or “rw” options on the command line instead.
    UINT16 root_flags;
    // The size of the protected-mode code in units of 16-byte paragraphs. For protocol
    // versions older than 2.04 this field is only two bytes wide, and therefore cannot
    // be trusted for the size of a kernel if the LOAD_HIGH flag is set.
    UINT32 syssize;
    // This field is obsolete.
    UINT16 ram_size;
    // Please see the section on SPECIAL COMMAND LINE OPTIONS.
    UINT16 vid_mode;
    // The default root device device number. The use of this field is deprecated, use
    // the “root=” option on the command line instead.
    UINT16 root_dev;
    // Contains 0xAA55. This is the closest thing old Linux kernels have to a magic
    // number.
    UINT16 boot_flag;
    // Contains an x86 jump instruction, 0xEB followed by a signed offset relative to
    // byte 0x202. This can be used to determine the size of the header.
    UINT16 jump;
    // Contains the magic number “HdrS” (0x53726448).
    CHAR8 header[4];
    // Contains the boot protocol version, in (major << 8) + minor format, e.g. 0x0204
    // for version 2.04, and 0x0a11 for a hypothetical version 10.17.
    UINT16 version;
    // Boot loader hook (see ADVANCED BOOT LOADER HOOKS below.)
    UINT32 realmode_swtch;
    // The load low segment (0x1000). Obsolete.
    UINT16 start_sys_seg;
    // If set to a nonzero value, contains a pointer to a NUL-terminated
    // human-readable kernel version number string, less 0x200. This can be used
    // to display the kernel version to the user. This value should be less than
    // (0x200 * setup_sects).
    //
    // For example, if this value is set to 0x1c00, the kernel version number
    // string can be found at offset 0x1e00 in the kernel file. This is a valid
    // value if and only if the “setup_sects” field contains the value 15 or
    // higher, as:
    //
    // ```
    // 0x1c00  < 15 * 0x200 (= 0x1e00) but
    // 0x1c00 >= 14 * 0x200 (= 0x1c00)
    //
    // 0x1c00 >> 9 = 14, So the minimum value for setup_secs is 15.
    // ```
    UINT16 kernel_version;
    // If your boot loader has an assigned ID (see table below), enter 0xTV here, where
    // T is an identifier for the boot loader and V is a version number. Otherwise,
    // enter 0xFF here.
    UINT8 type_of_loader;
    // This field is a bitmask.
    // Bit 0 (read): LOADED_HIGH
    // - If 0, the protected-mode code is loaded at 0x10000.
    // - If 1, the protected-mode code is loaded at 0x100000.
    //
    // Bit 1 (kernel internal): KASLR_FLAG
    // - Used internally by the compressed kernel to communicate KASLR status to kernel
    // proper.
    // - If 1, KASLR enabled.
    // - If 0, KASLR disabled.
    //
    // Bit 5 (write): QUIET_FLAG
    // - If 0, print early messages.
    // - If 1, suppress early messages.
    // - This requests to the kernel (decompressor and early kernel) to not write early
    // messages that require accessing the display hardware directly.
    //
    // Bit 6 (obsolete): KEEP_SEGMENTS
    // - Protocol: 2.07+
    // - This flag is obsolete.
    //
    // Bit 7 (write): CAN_USE_HEAP
    // - Set this bit to 1 to indicate that the value entered in the heap_end_ptr is
    // valid. If this field is clear, some setup code functionality will be disabled.
    UINT8 loadflags;
    // When using protocol 2.00 or 2.01, if the real mode kernel is not loaded at
    // 0x90000, it gets moved there later in the loading sequence. Fill in this field if
    // you want additional data (such as the kernel command line) moved in addition to
    // the real-mode kernel itself.
    //
    // The unit is bytes starting with the beginning of the boot sector.
    //
    // This field is can be ignored when the protocol is 2.02 or higher, or if the
    // real-mode code is loaded at 0x90000.
    UINT16 setup_move_size;
    // The address to jump to in protected mode. This defaults to the load address of
    // the kernel, and can be used by the boot loader to determine the proper load
    // address. This field can be modified for two purposes:
    // 1. as a boot loader hook (see Advanced Boot Loader Hooks below.)
    // 2. if a bootloader which does not install a hook loads a relocatable kernel at a
    // nonstandard address it will have to modify this field to point to the load
    // address.
    UINT32 code32_start;
    // The 32-bit linear address of the initial ramdisk or ramfs. Leave at zero if there
    // is no initial ramdisk/ramfs.
    UINT32 ramdisk_image;
    // Size of the initial ramdisk or ramfs. Leave at zero if there is no initial
    // ramdisk/ramfs.
    UINT32 ramdisk_size;
    // This field is obsolete.
    UINT32 bootsect_kludge;
    // Set this field to the offset (from the beginning of the real-mode code)
    // of the end of the setup stack/heap, minus 0x0200.
    UINT16 heap_end_ptr;
    // This field is used as an extension of the version number in the type_of_loader
    // field. The total version number is considered to be (type_of_loader & 0x0f) +
    // (ext_loader_ver << 4).
    //
    // The use of this field is boot loader specific. If not written, it is zero.
    //
    // Kernels prior to 2.6.31 did not recognize this field, but it is safe to write for
    // protocol version 2.02 or higher.
    UINT8 ext_loader_ver;
    // This field is used as an extension of the type number in type_of_loader field. If
    // the type in type_of_loader is 0xE, then the actual type is (ext_loader_type +
    // 0x10).
    //
    // This field is ignored if the type in type_of_loader is not 0xE.
    //
    // Kernels prior to 2.6.31 did not recognize this field, but it is safe to write for
    // protocol version 2.02 or higher.
    UINT8 ext_loader_type;
    // Set this field to the linear address of the kernel command line. The kernel
    // command line can be located anywhere between the end of the setup heap and
    // 0xA0000; it does not have to be located in the same 64K segment as the real-mode
    // code itself.
    //
    // Fill in this field even if your boot loader does not support a command line, in
    // which case you can point this to an empty string (or better yet, to the string
    // “auto”.) If this field is left at zero, the kernel will assume that your boot
    // loader does not support the 2.02+ protocol.
    UINT32 cmd_line_ptr;
    // The maximum address that may be occupied by the initial ramdisk/ramfs
    // contents. For boot protocols 2.02 or earlier, this field is not present,
    // and the maximum address is 0x37FFFFFF. (This address is defined as the
    // address of the highest safe byte, so if your ramdisk is exactly 131072
    // bytes long and this field is 0x37FFFFFF, you can start your ramdisk at
    // 0x37FE0000.)
    UINT32 initrd_addr_max;
    // Alignment unit required by the kernel (if relocatable_kernel is true.) A
    // relocatable kernel that is loaded at an alignment incompatible with the
    // value in this field will be realigned during kernel initialization.
    //
    // Starting with protocol version 2.10, this reflects the kernel alignment
    // preferred for optimal performance; it is possible for the loader to modify
    // this field to permit a lesser alignment. See the min_alignment and
    // pref_address field below.
    UINT32 kernel_alignment;
    // If this field is nonzero, the protected-mode part of the kernel can be
    // loaded at any address that satisfies the kernel_alignment field. After
    // loading, the boot loader must set the code32_start field to point to the
    // loaded code, or to a boot loader hook.
    UINT8 relocatable_kernel;
    // This field, if nonzero, indicates as a power of two the minimum alignment
    // required, as opposed to preferred, by the kernel to boot. If a boot loader makes
    // use of this field, it should update the kernel_alignment field with the
    // alignment unit desired; typically:
    // ```
    // kernel_alignment = 1 << min_alignment;
    // ```
    // There may be a considerable performance cost with an excessively misaligned
    // kernel. Therefore, a loader should typically try each power-of-two alignment from
    // kernel_alignment down to this alignment.
    UINT8 min_alignment;
    // This field is a bitmask.
    //
    // Bit 0 (read): XLF_KERNEL_64
    // - If 1, this kernel has the legacy 64-bit entry point at 0x200.
    //
    // Bit 1 (read): XLF_CAN_BE_LOADED_ABOVE_4G
    // - If 1, kernel/boot_params/cmdline/ramdisk can be above 4G.
    //
    // Bit 2 (read): XLF_EFI_HANDOVER_32
    // - If 1, the kernel supports the 32-bit EFI handoff entry point given at
    //   handover_offset.
    //
    // Bit 3 (read): XLF_EFI_HANDOVER_64
    // - If 1, the kernel supports the 64-bit EFI handoff entry point given at
    //   handover_offset + 0x200.
    //
    // Bit 4 (read): XLF_EFI_KEXEC
    // - If 1, the kernel supports kexec EFI boot with EFI runtime support.
    UINT16 xloadflags;
    // The maximum size of the command line without the terminating zero. This
    // means that the command line can contain at most cmdline_size characters.
    // With protocol version 2.05 and earlier, the maximum size was 255.
    UINT32 cmdline_size;
    // In a paravirtualized environment the hardware low level architectural pieces such
    // as interrupt handling, page table handling, and accessing process control
    // registers needs to be done differently.
    //
    // This field allows the bootloader to inform the kernel we are in one one of those
    // environments.
    UINT32 hardware_subarch;
    // A pointer to data that is specific to hardware subarch This field is
    // currently unused for the default x86/PC environment, do not modify.
    UINT64 hardware_subarch_data;
    // If non-zero then this field contains the offset from the beginning of the
    // protected-mode code to the payload.
    //
    // The payload may be compressed. The format of both the compressed and uncompressed
    // data should be determined using the standard magic numbers. The currently
    // supported compression formats are gzip (magic numbers 1F 8B or 1F 9E), bzip2
    // (magic number 42 5A), LZMA (magic number 5D 00), XZ (magic number FD 37), LZ4
    // (magic number 02 21) and ZSTD (magic number 28 B5). The uncompressed payload is
    // currently always ELF (magic number 7F 45 4C 46).
    UINT32 payload_offset;
    // The length of the payload.
    UINT32 payload_length;
    // The 64-bit physical pointer to NULL terminated single linked list of struct
    // setup_data. This is used to define a more extensible boot parameters passing
    // mechanism.
    UINT64 setup_data;
    // This field, if nonzero, represents a preferred load address for the
    // kernel. A relocating bootloader should attempt to load at this address if
    // possible.
    //
    // A non-relocatable kernel will unconditionally move itself and to run at this
    // address. A relocatable kernel will move itself to this address if it loaded
    // below this address.
    UINT64 pref_address;
    // This field indicates the amount of linear contiguous memory starting at
    // the kernel runtime start address that the kernel needs before it is
    // capable of examining its memory map. This is not the same thing as the
    // total amount of memory the kernel needs to boot, but it can be used by a
    // relocating boot loader to help select a safe load address for the kernel.
    //
    // The kernel runtime start address is determined by the following algorithm:
    // ```
    // if (relocatable_kernel) {
    //      if (load_address < pref_address)
    //              load_address = pref_address;
    //      runtime_start = align_up(load_address, kernel_alignment);
    // } else {
    //      runtime_start = pref_address;
    // }
    // ```
    // Hence the necessary memory window location and size can be estimated by a boot
    // loader as:
    // ```
    // memory_window_start = runtime_start;
    // memory_window_size = init_size;
    // ```
    UINT32 init_size;
    // This field is the offset from the beginning of the kernel image to the EFI
    // handover protocol entry point. Boot loaders using the EFI handover protocol to
    // boot the kernel should jump to this offset.
    //
    // See EFI HANDOVER PROTOCOL below for more details.
    UINT32 handover_offset;
    // This field is the offset from the beginning of the kernel image to the
    // kernel_info. The kernel_info structure is embedded in the Linux image in
    // the uncompressed protected mode region.
    UINT32 kernel_info_offset;
} __attribute__((packed)) setup_header_t;

#define E820_MAX_ENTRIES_ZEROPAGE 128

typedef struct {
    UINT64 addr;
    UINT64 size;
    UINT32 type;
} __attribute__((packed)) boot_e820_entry_t;

typedef struct {
    UINT8 _pad1[0x0c0];

    UINT32 ext_ramdisk_image;
    UINT32 ext_ramdisk_size;
    UINT32 ext_cmd_line_ptr;

    UINT8 _pad2[0x1e8 - 0x0c0 - 12];

    UINT8 e820_entries;

    UINT8 _pad3[0x1f1 - 0x1e8 - 1];

    setup_header_t hdr;

    UINT8 _pad4[0x2d0 - 0x1f1 - sizeof(setup_header_t)];

    boot_e820_entry_t e820_table[E820_MAX_ENTRIES_ZEROPAGE];

    UINT8 _pad5[4096 - 0x2d0 - (sizeof(boot_e820_entry_t) * E820_MAX_ENTRIES_ZEROPAGE)];
} __attribute__((packed)) boot_params_t;

_Static_assert(
    offsetof(boot_params_t, ext_ramdisk_image) == 0x0c0,
    "ext_ramdisk_image must be at offset 0x0c0 in boot_params_t"
);
_Static_assert(
    offsetof(boot_params_t, e820_entries) == 0x1e8,
    "e820_entries must be at offset 0x1e8 in boot_params_t"
);
_Static_assert(
    offsetof(boot_params_t, hdr) == 0x1f1,
    "hdr must be at offset 0x1f1 in boot_params_t"
);
_Static_assert(
    offsetof(boot_params_t, e820_table) == 0x2d0,
    "e820_table must be at offset 0x2d0 in boot_params_t"
);
_Static_assert(
    sizeof(boot_params_t) == 4096, "boot_params_t must be exactly 4096 bytes in size"
);

#pragma pack(pop)
