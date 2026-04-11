#pragma once

#include "linux.h"
#include <efi.h>

// Enumerates SIMPLE_FILE_SYSTEM_PROTOCOL devices and look for `vmlinuz-linux` and
// `initramfs-linux.img` in the root directory of each one. Returns the first one it
// finds, or an error if it can't find any.
EFI_STATUS
kernel_locate(EFI_FILE_PROTOCOL **kernel_file, EFI_FILE_PROTOCOL **initramfs_file);

// Reads the kernel header from the given kernel file. The kernel header is
// located at offset 0x1f1 in the kernel file and contains important information
// about the kernel, such as the size of the setup code and the location of the
// kernel payload.
EFI_STATUS kernel_read_header(EFI_FILE_PROTOCOL *kernel_file, setup_header_t *header);

// Dumps the contents of the kernel header to the console for debugging purposes.
VOID kernel_dump_header(const setup_header_t *header);

// Decompresses the kernel payload using zstd. The caller is responsible for
// freeing the decompressed kernel buffer using `free()`.
EFI_STATUS kernel_decompress(
    const setup_header_t *header, EFI_FILE_PROTOCOL *kernel_file,
    VOID **decompressed_kernel, UINTN *decompressed_kernel_size
);

// Loads the initramfs into memory and returns a pointer to it and its size.
EFI_STATUS initramfs_load(
    EFI_FILE_PROTOCOL *initramfs_file, VOID **initramfs, UINTN *initramfs_size
);

boot_params_t *prepare_boot_params(
    const setup_header_t *header, VOID *initramfs, UINTN initramfs_size
);
