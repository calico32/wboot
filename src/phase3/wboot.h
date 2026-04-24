#pragma once

#include "linux.h"
#include <efi.h>

// Enumerates SIMPLE_FILE_SYSTEM_PROTOCOL devices and look for `vmlinuz-linux` and
// `initramfs-linux.img` in the root directory of each one. Returns the first one it
// finds, or an error if it can't find any.
EFI_STATUS
wboot_locate_kernel(
    EFI_FILE_PROTOCOL **kernel_file, EFI_FILE_PROTOCOL **initramfs_file
);

// Reads the kernel header from the given kernel file. The kernel header is
// located at offset 0x1f1 in the kernel file and contains important information
// about the kernel, such as the size of the setup code and the location of the
// kernel payload.
EFI_STATUS
wboot_read_setup_header(EFI_FILE_PROTOCOL *kernel_file, setup_header_t *header);

// Dumps the contents of the kernel header to the console for debugging purposes.
VOID wboot_dump_setup_header(const setup_header_t *header);

// Decompresses the kernel payload using zstd. The caller is responsible for
// freeing the decompressed kernel buffer using `free()`.
EFI_STATUS wboot_decompress_kernel(
    const setup_header_t *header, EFI_FILE_PROTOCOL *kernel_file,
    VOID **decompressed_kernel, UINTN *decompressed_kernel_size
);

// Loads the initramfs into memory and returns a pointer to it and its size.
EFI_STATUS wboot_load_initramfs(
    EFI_FILE_PROTOCOL *initramfs_file, VOID **initramfs, UINTN *initramfs_size
);

// Allocates and prepares the `boot_params_t` structure to be passed to the
// kernel. This includes copying the setup header from the kernel file and
// filling in important fields such as the location and size of the initramfs
// and the kernel command line. The caller is responsible for filling in the
// e820 memory map before jumping to the kernel.
boot_params_t *wboot_prepare_bootparams(
    const setup_header_t *header, VOID *initramfs, UINTN initramfs_size
);

EFI_STATUS
wboot_allocate_e820(
    boot_params_t *params, setup_data_t **e820ext, UINT32 *e820ext_size
);

// Exits UEFI boot services. No UEFI calls can be made after this point. Returns
// a pointer to the acquired EFI memory map.
EFI_MEMORY_DESCRIPTOR *wboot_exit_bs(boot_params_t *params);

// Fills in the e820 memory map in the given `boot_params_t` structure from the provided
// EFI memory map.
EFI_STATUS wboot_translate_memmap(
    boot_params_t *params, setup_data_t *e820ext, UINT32 e820ext_size
);

void *wboot_get_kernel_entry(const VOID *decompressed_kernel);

// (wboot_handoff.s) Sets up the GDT, prepares the CPU for entering the kernel, and
// jumps to the kernel entry point.
void wboot_handoff(void *boot_params, void *kernel_entry);
