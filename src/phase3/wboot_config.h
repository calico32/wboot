#pragma once

#include <efi.h>

typedef struct {
    CHAR16 *kernel_path;
    CHAR16 *initrd_path;
    CHAR8 *cmdline;
    UINT32 mode_width;
    UINT32 mode_height;
    INT32 mode_format;
    UINT32 mode_depth;
    EFI_FILE_PROTOCOL *root;
} wboot_config_t;

wboot_config_t *wboot_load_config(EFI_FILE_PROTOCOL *root);

// Enumerates SIMPLE_FILE_SYSTEM_PROTOCOL devices and look for `wboot.conf` in
// the root directory of each one. Returns the parsed config if found, or an
// error if it can't find any.
EFI_STATUS wboot_locate_config(wboot_config_t **config);
