#include <zstd.h>

#include "efiglobal.h"
#include "wboot.h"
#include "wstdlib.h" // IWYU pragma: keep

EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *_device_path_to_text;
EFI_STATUS _get_device_path_to_text() {
    EFI_STATUS status = BS->LocateProtocol(
        &gEfiDevicePathToTextProtocolGuid, NULL, (VOID **)&_device_path_to_text
    );
    if (EFI_ERROR(status)) {
        printf(L"Error locating DEVICE_PATH_TO_TEXT_PROTOCOL: %u\r\n", status);
    }
    return status;
}

EFI_STATUS
wboot_locate_kernel(
    EFI_FILE_PROTOCOL **kernel_file, EFI_FILE_PROTOCOL **initramfs_file
) {
    EFI_STATUS status = EFI_SUCCESS;

    printf(L"Locating kernel...\r\n");

    status = _get_device_path_to_text();
    if (EFI_ERROR(status)) {
        return status;
    }

    EFI_HANDLE handles[64];
    UINTN handles_size = sizeof(handles);

    status = BS->LocateHandle(
        ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &handles_size, handles
    );
    if (EFI_ERROR(status)) {
        perror(L"Error locating handles", status);
        return status;
    }

    printf(
        L"Enumerating SIMPLE_FILE_SYSTEM_PROTOCOL devices\r\n",
        handles_size / sizeof(EFI_HANDLE)
    );
    EFI_DEVICE_PATH_PROTOCOL *device_path;
    for (UINTN i = 0; i < handles_size / sizeof(EFI_HANDLE); i++) {
        status = BS->HandleProtocol(
            handles[i], &gEfiDevicePathProtocolGuid, (VOID **)&device_path
        );
        if (!EFI_ERROR(status)) {
            CHAR16 *device_path_str = _device_path_to_text->ConvertDevicePathToText(
                device_path, FALSE, FALSE
            );
            if (device_path_str == NULL) {
                printf(L"  Device path: unknown error\r\n");
            }
            printf(L"  Device path: %s\r\n", device_path_str);
            BS->FreePool(device_path_str);
        } else {
            perror(L"  Device path", status);
        }

        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
        status = BS->HandleProtocol(
            handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&fs
        );
        if (EFI_ERROR(status)) {
            perror(L"  Error getting file system protocol", status);
            continue;
        }

        EFI_FILE_PROTOCOL *root;
        status = fs->OpenVolume(fs, &root);
        if (EFI_ERROR(status)) {
            perror(L"  Error opening volume", status);
            continue;
        }

        EFI_FILE_PROTOCOL *kernel;
        status = root->Open(root, &kernel, L"vmlinuz-linux", EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status)) {
            perror(L"  Error opening vmlinuz-linux", status);
            continue;
        }
        printf(L"  Found vmlinuz-linux!\r\n");
        *kernel_file = kernel;

        EFI_FILE_PROTOCOL *initramfs;
        status = root->Open(
            root, &initramfs, L"initramfs-linux.img", EFI_FILE_MODE_READ, 0
        );
        if (EFI_ERROR(status)) {
            perror(L"  Error opening initramfs-linux.img", status);
            continue;
        }
        printf(L"  Found initramfs-linux.img!\r\n");
        *initramfs_file = initramfs;

        return EFI_SUCCESS;
    }

    printf(
        L"Could not find vmlinuz-linux and initramfs-linux.img together on any "
        L"device\r\n"
    );
    return EFI_NOT_FOUND;
}

EFI_STATUS
wboot_read_setup_header(EFI_FILE_PROTOCOL *kernel_file, setup_header_t *header) {
    EFI_STATUS status;
    UINTN bytes_read = sizeof(setup_header_t);

    status = kernel_file->SetPosition(kernel_file, 0x1f1);
    if (EFI_ERROR(status)) {
        perror(L"Error seeking to kernel header", status);
        return status;
    }

    status = kernel_file->Read(kernel_file, &bytes_read, header);
    if (EFI_ERROR(status)) {
        perror(L"Error reading kernel header", status);
        return status;
    }

    if (bytes_read != sizeof(setup_header_t)) {
        printf(
            L"Error: expected to read %u bytes for kernel header, but only read %u\r\n",
            sizeof(setup_header_t), bytes_read
        );
        return EFI_INVALID_PARAMETER;
    }

    if (header->boot_flag != 0xAA55) {
        printf(
            L"Error: invalid kernel header magic (expected 0xAA55, got 0x%x)\r\n",
            header->boot_flag
        );
        return EFI_INVALID_PARAMETER;
    }

    if (header->header[0] != 'H' ||
        header->header[1] != 'd' ||
        header->header[2] != 'r' ||
        header->header[3] != 'S') {
        printf(
            L"Error: invalid kernel header magic (expected 'HdrS', got '%c%c%c%c')\r\n",
            header->header[0], header->header[1], header->header[2], header->header[3]
        );
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

VOID wboot_dump_setup_header(const setup_header_t *header) {
    printf(L"Kernel header:\r\n");
    printf(L"  setup_sects: %u\r\n", (UINTN)header->setup_sects);
    printf(L"  root_flags: 0x%x\r\n", (UINTN)header->root_flags);
    printf(L"  syssize: %u\r\n", (UINTN)header->syssize);
    printf(L"  ram_size: %u\r\n", (UINTN)header->ram_size);
    printf(L"  vid_mode: 0x%x\r\n", (UINTN)header->vid_mode);
    printf(L"  root_dev: 0x%x\r\n", (UINTN)header->root_dev);
    printf(L"  boot_flag: 0x%x\r\n", (UINTN)header->boot_flag);
    printf(L"  jump: 0x%x\r\n", (UINTN)header->jump);
    printf(
        L"  header: %c%c%c%c\r\n", header->header[0], header->header[1],
        header->header[2], header->header[3]
    );
    printf(L"  version: 0x%x\r\n", (UINTN)header->version);
    printf(L"  realmode_swtch: 0x%x\r\n", (UINTN)header->realmode_swtch);
    printf(L"  start_sys_seg: 0x%x\r\n", (UINTN)header->start_sys_seg);
    printf(L"  kernel_version: %u\r\n", (UINTN)header->kernel_version);
    printf(L"  type_of_loader: %u\r\n", (UINTN)header->type_of_loader);
    printf(L"  loadflags: 0x%x\r\n", (UINTN)header->loadflags);
    printf(L"  setup_move_size: %u\r\n", (UINTN)header->setup_move_size);
    printf(L"  code32_start: 0x%x\r\n", (UINTN)header->code32_start);
    printf(L"  ramdisk_image: 0x%x\r\n", (UINTN)header->ramdisk_image);
    printf(L"  ramdisk_size: %u\r\n", (UINTN)header->ramdisk_size);
    printf(L"  bootsect_kludge: 0x%x\r\n", (UINTN)header->bootsect_kludge);
    printf(L"  heap_end_ptr: 0x%x\r\n", (UINTN)header->heap_end_ptr);
    printf(L"  ext_loader_ver: %u\r\n", (UINTN)header->ext_loader_ver);
    printf(L"  ext_loader_type: %u\r\n", (UINTN)header->ext_loader_type);
    printf(L"  cmd_line_ptr: 0x%x\r\n", (UINTN)header->cmd_line_ptr);
    printf(L"  initrd_addr_max: 0x%x\r\n", (UINTN)header->initrd_addr_max);
    printf(L"  kernel_alignment: 0x%x\r\n", (UINTN)header->kernel_alignment);
    printf(L"  relocatable_kernel: %u\r\n", (UINTN)header->relocatable_kernel);
    printf(L"  min_alignment: %u\r\n", (UINTN)header->min_alignment);
    printf(L"  xloadflags: 0x%x\r\n", (UINTN)header->xloadflags);
    printf(L"  cmdline_size: %u\r\n", (UINTN)header->cmdline_size);
    printf(L"  hardware_subarch: 0x%x\r\n", (UINTN)header->hardware_subarch);
    printf(L"  hardware_subarch_data: 0x%x\r\n", header->hardware_subarch_data);
    printf(L"  payload_offset: 0x%x\r\n", (UINTN)header->payload_offset);
    printf(L"  payload_length: %u\r\n", (UINTN)header->payload_length);
    printf(L"  setup_data: 0x%x\r\n", (UINTN)header->setup_data);
    printf(L"  pref_address: 0x%x\r\n", (UINTN)header->pref_address);
    printf(L"  init_size: 0x%x\r\n", (UINTN)header->init_size);
    printf(L"  handover_offset: 0x%x\r\n", (UINTN)header->handover_offset);
    printf(L"  kernel_info_offset: 0x%x\r\n", (UINTN)header->kernel_info_offset);
}

static ZSTD_DCtx *dctx;

EFI_STATUS wboot_decompress_kernel(
    const setup_header_t *header, EFI_FILE_PROTOCOL *kernel_file,
    VOID **decompressed_kernel, UINTN *decompressed_kernel_size
) {
    if (dctx == NULL) {
        dctx = ZSTD_createDCtx();
        if (dctx == NULL) {
            printf(L"Error creating ZSTD decompression context\r\n");
            return EFI_OUT_OF_RESOURCES;
        }
    } else {
        ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
    }

    UINTN compressed_kernel_size = header->payload_length;
    VOID *compressed_kernel = malloc(compressed_kernel_size);
    if (compressed_kernel == NULL) {
        printf(L"Error allocating memory for compressed kernel\r\n");
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN sects = header->setup_sects ? header->setup_sects : 4;
    printf(L"Kernel setup size: %u bytes (%u setup sectors)\r\n", sects * 512, sects);
    UINTN payload_start = ((1 + sects) * 512) + header->payload_offset;

    printf(
        L"Seeking to kernel payload at offset 0x%x and reading %u bytes of compressed "
        L"kernel\r\n",
        payload_start, compressed_kernel_size
    );

    UINTN bytes_read = compressed_kernel_size;
    EFI_STATUS status = kernel_file->SetPosition(kernel_file, payload_start);
    if (EFI_ERROR(status)) {
        printf(L"Error setting kernel file position\r\n");
        free(compressed_kernel);
        return status;
    }
    status = kernel_file->Read(kernel_file, &bytes_read, compressed_kernel);
    if (EFI_ERROR(status)) {
        printf(L"Error reading compressed kernel\r\n");
        free(compressed_kernel);
        return status;
    }

    if (bytes_read != compressed_kernel_size) {
        printf(
            L"Error: expected to read %u bytes for compressed kernel, but only read "
            L"%u\r\n",
            compressed_kernel_size, bytes_read
        );
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }

    // check zstd magic number 0xFD2FB528 (LE)
    if (compressed_kernel_size < 4 || *((UINT32 *)compressed_kernel) != 0xFD2FB528) {
        printf(
            L"Error: compressed kernel does not have zstd magic (different compression "
            L"format or not compressed at all?) expected 0xFD2FB528, got 0x%x\r\n",
            (UINTN)(*((UINT32 *)compressed_kernel))
        );
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }

    size_t exact_compressed_size = ZSTD_findFrameCompressedSize(
        compressed_kernel, compressed_kernel_size
    );
    if (ZSTD_isError(exact_compressed_size)) {
        printf(
            L"Error finding zstd compressed frame size: %-s\r\n",
            ZSTD_getErrorName(exact_compressed_size)
        );
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }
    printf(L"Exact zstd compressed size: %u bytes\r\n", (UINTN)exact_compressed_size);

    // allocate pages for decompressed kernel
    UINTN decompressed_kernel_capacity = header->init_size;
    UINTN pages = (decompressed_kernel_capacity + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;

    printf(
        L"Decompressed kernel size: %u bytes, which requires %u pages\r\n",
        decompressed_kernel_capacity, pages
    );

    // align up to kernel_alignment
    UINTN alignment = (UINTN)header->kernel_alignment;
    UINTN aligned_size = (decompressed_kernel_capacity + alignment - 1) &
                         ~(alignment - 1);

    EFI_PHYSICAL_ADDRESS decompressed_kernel_addr;
    if (header->pref_address != 0) {
        // try to allocate at preferred address
        decompressed_kernel_addr = header->pref_address;
        printf(
            L"Asking for %u bytes for decompressed kernel at preferred address "
            L"0x%x\r\n",
            pages * EFI_PAGE_SIZE, header->pref_address
        );
        status = BS->AllocatePages(
            AllocateAddress, EfiLoaderData, pages, &decompressed_kernel_addr
        );
        if (EFI_ERROR(status)) {
            printf(
                L"Error allocating pages for decompressed kernel at preferred address "
                L"0x%x: %s\r\n",
                header->pref_address, strerror(status)
            );
        }
    }
    if (header->pref_address == 0 || EFI_ERROR(status)) {
        // fallback to any address
        printf(
            L"Asking for %u bytes for decompressed kernel at any address since "
            L"preferred address allocation failed or was not specified\r\n",
            pages * EFI_PAGE_SIZE, header->pref_address
        );
        status = BS->AllocatePages(
            AllocateAnyPages, EfiLoaderData, pages, &decompressed_kernel_addr
        );
        if (EFI_ERROR(status)) {
            printf(
                L"Error allocating pages for decompressed kernel: %s\r\n",
                strerror(status)
            );
            free(compressed_kernel);
            return status;
        }
    }

    printf(L"Decompressing kernel to address %p...\r\n", decompressed_kernel_addr);
    VOID *decompressed_kernel_ptr = (VOID *)decompressed_kernel_addr;
    size_t decompressed_size = ZSTD_decompressDCtx(
        dctx, decompressed_kernel_ptr, decompressed_kernel_capacity, compressed_kernel,
        exact_compressed_size
    );
    if (ZSTD_isError(decompressed_size)) {
        printf(
            L"Error decompressing kernel: %-s\r\n", ZSTD_getErrorName(decompressed_size)
        );
        BS->FreePages(decompressed_kernel_addr, pages);
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }

    // check ELF magic number 0x7F454C46 (".ELF" in ASCII)
    if (decompressed_size < 4 || *((UINT32 *)decompressed_kernel_ptr) != 0x464C457F) {
        printf(
            L"Error: decompressed kernel does not have ELF magic (invalid kernel "
            L"format?) expected 0x464C457F, got 0x%x\r\n",
            (UINTN)(*((UINT32 *)decompressed_kernel_ptr))
        );
        BS->FreePages(decompressed_kernel_addr, pages);
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }

    *decompressed_kernel = decompressed_kernel_ptr;
    *decompressed_kernel_size = decompressed_size;
    free(compressed_kernel);
    return EFI_SUCCESS;
}

EFI_STATUS wboot_load_initramfs(
    EFI_FILE_PROTOCOL *initramfs_file, VOID **initramfs, UINTN *initramfs_size
) {
    EFI_STATUS status;

    // get file info size
    UINTN size = 0;
    status = initramfs_file->GetInfo(initramfs_file, &gEfiFileInfoGuid, &size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        printf(L"Error getting initramfs file info size: %s\r\n", strerror(status));
        return status;
    }

    printf(L"Allocating %u bytes for initramfs file info\r\n", size);

    // get file info
    EFI_FILE_INFO *file_info = malloc(size);
    if (file_info == NULL) {
        printf(
            L"Error allocating memory for initramfs file info: %s\r\n", strerror(errno)
        );
        return errno;
    }
    status = initramfs_file->GetInfo(
        initramfs_file, &gEfiFileInfoGuid, &size, file_info
    );
    if (EFI_ERROR(status)) {
        printf(L"Error getting initramfs file info: %s\r\n", strerror(status));
        free(file_info);
        return status;
    }

    UINTN file_size = file_info->FileSize;
    free(file_info);

    // allocate memory for initramfs
    CHAR8 *buffer = malloc(file_size);
    if (buffer == NULL) {
        printf(L"Error allocating memory for initramfs: %s\r\n", strerror(errno));
        return errno;
    }

    // read initramfs file
    UINTN bytes_read = file_size;
    status = initramfs_file->Read(initramfs_file, &bytes_read, buffer);
    if (EFI_ERROR(status)) {
        printf(L"Error reading initramfs file: %s\r\n", strerror(status));
        free(buffer);
        return status;
    }

    *initramfs = buffer;
    *initramfs_size = file_size;
    return EFI_SUCCESS;
}
