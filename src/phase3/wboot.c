#include <zstd.h>

#include "efiglobal.h"
#include "efiprot.h"
#include "elf.h"
#include "wboot.h"
#include "wboot_config.h"
#include "wstdlib.h"

static EFI_STATUS
open_file(EFI_FILE_PROTOCOL *root, CHAR16 *path, EFI_FILE_PROTOCOL **file) {
    EFI_STATUS status = root->Open(root, file, path, EFI_FILE_MODE_READ, 0);
    return status;
}

EFI_STATUS
wboot_open_kernel(const wboot_config_t *config, EFI_FILE_PROTOCOL **kernel_file) {
    return open_file(config->root, config->kernel_path, kernel_file);
}

EFI_STATUS
wboot_read_setup_header(EFI_FILE_PROTOCOL *kernel_file, setup_header_t *header) {
    EFI_STATUS status;
    UINTN bytes_read = sizeof(setup_header_t);

    status = kernel_file->SetPosition(kernel_file, 0x1f1);
    if (EFI_ERROR(status)) {
        perror(L"[read_setup_header] error seeking to kernel header", status);
        return status;
    }

    status = kernel_file->Read(kernel_file, &bytes_read, header);
    if (EFI_ERROR(status)) {
        perror(L"[read_setup_header] error reading kernel header", status);
        return status;
    }

    if (bytes_read != sizeof(setup_header_t)) {
        printf(
            L"[read_setup_header] error: expected to read %u bytes for kernel "
            L"header, but only read %u\r\n",
            sizeof(setup_header_t), bytes_read
        );
        return EFI_INVALID_PARAMETER;
    }

    if (header->boot_flag != 0xAA55) {
        printf(
            L"[read_setup_header] error: invalid kernel header magic (expected "
            L"0xAA55, got 0x%x)\r\n",
            header->boot_flag
        );
        return EFI_INVALID_PARAMETER;
    }

    if (header->header[0] != 'H' ||
        header->header[1] != 'd' ||
        header->header[2] != 'r' ||
        header->header[3] != 'S') {
        printf(
            L"[read_setup_header] error: invalid kernel header magic (expected "
            L"'HdrS', got '%c%c%c%c')\r\n",
            header->header[0], header->header[1], header->header[2], header->header[3]
        );
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

static ZSTD_DCtx *dctx;

EFI_STATUS wboot_decompress_kernel(
    const setup_header_t *header, EFI_FILE_PROTOCOL *kernel_file,
    VOID **decompressed_kernel, UINTN *decompressed_kernel_size
) {
    if (dctx == NULL) {
        dctx = ZSTD_createDCtx();
        if (dctx == NULL) {
            printf(
                L"[decompress_kernel] error creating ZSTD decompression context\r\n"
            );
            return EFI_OUT_OF_RESOURCES;
        }
    } else {
        ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
    }

    UINTN compressed_kernel_size = header->payload_length;
    VOID *compressed_kernel = malloc(compressed_kernel_size);
    if (compressed_kernel == NULL) {
        perror(
            L"[decompress_kernel] error allocating memory for compressed kernel\r\n",
            errno
        );
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN sects = header->setup_sects ? header->setup_sects : 4;
    UINTN payload_start = ((1 + sects) * 512) + header->payload_offset;

    UINTN bytes_read = compressed_kernel_size;
    EFI_STATUS status = kernel_file->SetPosition(kernel_file, payload_start);
    if (EFI_ERROR(status)) {
        perror(L"[decompress_kernel] error setting kernel file position\r\n", status);
        free(compressed_kernel);
        return status;
    }
    status = kernel_file->Read(kernel_file, &bytes_read, compressed_kernel);
    if (EFI_ERROR(status)) {
        perror(L"[decompress_kernel] error reading compressed kernel\r\n", status);
        free(compressed_kernel);
        return status;
    }

    if (bytes_read != compressed_kernel_size) {
        printf(
            L"[decompress_kernel] error: expected to read %u bytes for compressed "
            L"kernel, but only read %u\r\n",
            compressed_kernel_size, bytes_read
        );
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }

    // check zstd magic number 0xFD2FB528 (LE)
    if (compressed_kernel_size < 4 || *((UINT32 *)compressed_kernel) != 0xFD2FB528) {
        printf(
            L"[decompress_kernel] error: compressed kernel does not have zstd magic "
            L"(different compression format or not compressed at all?) expected "
            L"0xFD2FB528, got 0x%x\r\n",
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
            L"[decompress_kernel] error finding zstd compressed frame size: %-s\r\n",
            ZSTD_getErrorName(exact_compressed_size)
        );
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }

    // allocate pages for decompressed kernel
    UINTN decompressed_kernel_capacity = header->init_size;
    UINTN pages = (decompressed_kernel_capacity + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;

    // align up to kernel_alignment
    UINTN alignment = (UINTN)header->kernel_alignment;
    UINTN aligned_size = (decompressed_kernel_capacity + alignment - 1) &
                         ~(alignment - 1);

    EFI_PHYSICAL_ADDRESS decompressed_kernel_addr;
    if (header->pref_address != 0) {
        // try to allocate at preferred address
        decompressed_kernel_addr = header->pref_address;
        status = BS->AllocatePages(
            AllocateAddress, EfiLoaderData, pages, &decompressed_kernel_addr
        );
        if (EFI_ERROR(status)) {
            printf(
                L"warning: couldn't allocate pages for decompressed kernel at "
                L"preferred address 0x%x: %s\r\n",
                header->pref_address, strerror(status)
            );
        }
    }
    if (header->pref_address == 0 || EFI_ERROR(status)) {
        // fallback to any address
        status = BS->AllocatePages(
            AllocateAnyPages, EfiLoaderData, pages, &decompressed_kernel_addr
        );
        if (EFI_ERROR(status)) {
            printf(
                L"[decompress_kernel] error allocating pages for decompressed kernel: "
                L"%s\r\n",
                strerror(status)
            );
            free(compressed_kernel);
            return status;
        }
    }

    VOID *decompressed_kernel_ptr = (VOID *)decompressed_kernel_addr;
    size_t decompressed_size = ZSTD_decompressDCtx(
        dctx, decompressed_kernel_ptr, decompressed_kernel_capacity, compressed_kernel,
        exact_compressed_size
    );
    if (ZSTD_isError(decompressed_size)) {
        printf(
            L"[decompress_kernel] error decompressing kernel: %-s\r\n",
            ZSTD_getErrorName(decompressed_size)
        );
        BS->FreePages(decompressed_kernel_addr, pages);
        free(compressed_kernel);
        return EFI_INVALID_PARAMETER;
    }

    // check ELF magic number 0x7F454C46 (".ELF" in ASCII)
    if (decompressed_size < 4 || *((UINT32 *)decompressed_kernel_ptr) != 0x464C457F) {
        printf(
            L"[decompress_kernel] error: decompressed kernel does not have ELF "
            L"magic (invalid kernel format?) expected 0x464C457F, got 0x%x\r\n",
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
    const wboot_config_t *config, VOID **initramfs, UINTN *initramfs_size
) {
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *initramfs_file;
    status = open_file(config->root, config->initrd_path, &initramfs_file);
    if (EFI_ERROR(status)) {
        printf(
            L"[load_initramfs] error opening initramfs file '%s': %s\r\n",
            config->initrd_path, strerror(status)
        );
        return status;
    }

    // get file info size
    UINTN size = 0;
    status = initramfs_file->GetInfo(initramfs_file, &gEfiFileInfoGuid, &size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        printf(
            L"[load_initramfs] error getting initramfs file info size: %s\r\n",
            strerror(status)
        );
        return status;
    }

    // get file info
    EFI_FILE_INFO *file_info = malloc(size);
    if (file_info == NULL) {
        printf(
            L"[load_initramfs] error allocating memory for initramfs file info: "
            L"%s\r\n",
            strerror(errno)
        );
        return errno;
    }
    status = initramfs_file->GetInfo(
        initramfs_file, &gEfiFileInfoGuid, &size, file_info
    );
    if (EFI_ERROR(status)) {
        printf(
            L"[load_initramfs] error getting initramfs file info: %s\r\n",
            strerror(status)
        );
        free(file_info);
        return status;
    }

    UINTN file_size = file_info->FileSize;
    free(file_info);

    UINTN pages = (file_size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS buffer_addr = 0xFFFFFFFF;
    status = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, pages, &buffer_addr);
    if (EFI_ERROR(status)) {
        buffer_addr = 0;
        status = BS->AllocatePages(
            AllocateAnyPages, EfiLoaderData, pages, &buffer_addr
        );
        if (EFI_ERROR(status)) {
            printf(
                L"[load_initramfs] error allocating pages for initramfs: %s\r\n",
                strerror(status)
            );
            return status;
        }
    }

    CHAR8 *buffer = (CHAR8 *)(UINTN)buffer_addr;
    memset(buffer, 0, pages * EFI_PAGE_SIZE);

    // read initramfs file
    UINTN bytes_read = file_size;
    status = initramfs_file->Read(initramfs_file, &bytes_read, buffer);
    if (EFI_ERROR(status)) {
        printf(
            L"[load_initramfs] error reading initramfs file: %s\r\n", strerror(status)
        );
        BS->FreePages(buffer_addr, pages);
        return status;
    }

    if (bytes_read != file_size) {
        printf(
            L"[load_initramfs] error: expected to read %u bytes but read %u bytes\r\n",
            file_size, bytes_read
        );
        BS->FreePages(buffer_addr, pages);
        return EFI_BAD_BUFFER_SIZE;
    }

    *initramfs = buffer;
    *initramfs_size = file_size;
    return EFI_SUCCESS;
}

boot_params_t *wboot_prepare_bootparams(
    const wboot_config_t *config, const setup_header_t *header, VOID *initramfs,
    UINTN initramfs_size
) {
    boot_params_t *params = malloc(sizeof(boot_params_t));
    if (params == NULL) {
        printf(
            L"[prepare_bootparams] error allocating memory for boot params: %s\r\n",
            strerror(errno)
        );
        return NULL;
    }

    // zero out the params struct
    memset(params, 0, sizeof(boot_params_t));

    // copy setup header to params
    memcpy(&params->hdr, header, sizeof(setup_header_t));

    // set important fields in boot params
    params->hdr.ramdisk_image = (UINT32)(UINTN)initramfs;
    params->ext_ramdisk_image = (UINT32)((UINTN)initramfs >> 32);
    params->hdr.ramdisk_size = (UINT32)initramfs_size;
    params->ext_ramdisk_size = (UINT32)(initramfs_size >> 32);
    params->hdr.type_of_loader = 0xFF; // custom loader
    params->hdr.initrd_addr_max = 0xFFFFFFFF;
    params->hdr.cmd_line_ptr = (UINT32)(UINTN)config->cmdline;
    params->ext_cmd_line_ptr = (UINT32)((UINTN)config->cmdline >> 32);
    params->hdr.cmdline_size = strlen8(config->cmdline);

    return params;
}

static void populate_bootparams_efi(
    boot_params_t *params, EFI_MEMORY_DESCRIPTOR *memmap, UINTN map_size,
    UINTN desc_size, UINT32 desc_ver
) {
    const char *signature = "EL64";

    memcpy(&params->efi_info.efi_loader_signature, signature, sizeof(UINT32));

    params->efi_info.efi_systab = (UINT32)(UINTN)ST;
    params->efi_info.efi_systab_hi = (UINT32)((UINTN)ST >> 32);
    params->efi_info.efi_memdesc_size = desc_size;
    params->efi_info.efi_memdesc_version = desc_ver;
    params->efi_info.efi_memmap = (UINT32)(UINTN)memmap;
    params->efi_info.efi_memmap_hi = (UINT32)((UINTN)memmap >> 32);
    params->efi_info.efi_memmap_size = map_size;
}

EFI_STATUS wboot_allocate_e820(
    boot_params_t *params, setup_data_t **e820ext, UINT32 *e820ext_size
) {
    EFI_MEMORY_DESCRIPTOR *memmap;
    UINTN map_size = 0;
    UINTN desc_size;

    EFI_STATUS status = BS->GetMemoryMap(&map_size, NULL, NULL, &desc_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        printf(
            L"[allocate_e820] error getting memory map size: %s\r\n", strerror(status)
        );
        return status;
    }
    UINTN buffer_size = map_size + (desc_size * EFI_MMAP_NR_SLACK_SLOTS);

    memmap = malloc(buffer_size);
    if (memmap == NULL) {
        printf(
            L"[allocate_e820] error allocating memory for memory map: %s\r\n",
            strerror(errno)
        );
        return EFI_OUT_OF_RESOURCES;
    }
    map_size = buffer_size;
    status = BS->GetMemoryMap(&map_size, memmap, NULL, &desc_size, NULL);
    if (EFI_ERROR(status)) {
        printf(L"[allocate_e820] error getting memory map: %s\r\n", strerror(status));
        free(memmap);
        return status;
    }

    UINT32 nr_desc = map_size / desc_size;
    if (nr_desc > E820_MAX_ENTRIES_ZEROPAGE - EFI_MMAP_NR_SLACK_SLOTS) {
        UINT32 nr_e820ext = nr_desc -
                            E820_MAX_ENTRIES_ZEROPAGE +
                            EFI_MMAP_NR_SLACK_SLOTS;
        *e820ext_size = sizeof(setup_data_t) + (sizeof(boot_e820_entry_t) * nr_desc);
        *e820ext = malloc(*e820ext_size);
        if (*e820ext == NULL) {
            printf(
                L"[allocate_e820] error allocating memory for e820ext: %s\r\n",
                strerror(errno)
            );
            free(memmap);
            return EFI_OUT_OF_RESOURCES;
        }
    }

    free(memmap);
    return EFI_SUCCESS;
}

EFI_MEMORY_DESCRIPTOR *wboot_exit_bs(boot_params_t *params) {
    EFI_MEMORY_DESCRIPTOR *memmap = NULL;
    UINTN map_key = 0;
    UINT32 desc_ver = 0;
    UINTN map_size = 0;
    UINTN desc_size = 0;

    EFI_STATUS status = BS->GetMemoryMap(
        &map_size, NULL, &map_key, &desc_size, &desc_ver
    );
    if (status != EFI_BUFFER_TOO_SMALL) {
        perror(L"[exit_bs] error getting memory map size", status);
        return NULL;
    }

    // add some slack in case the memory map changes
    UINTN buffer_size = map_size + (desc_size * 32);

    memmap = malloc(buffer_size);
    if (memmap == NULL) {
        perror(L"[exit_bs] error allocating memory for memory map", errno);
        return NULL;
    }

    status = BS->GetMemoryMap(&map_size, memmap, &map_key, &desc_size, &desc_ver);
    if (EFI_ERROR(status)) {
        perror(L"[exit_bs] error getting memory map", status);
        free(memmap);
        return NULL;
    }

    status = BS->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        perror(L"[exit_bs] warning: error exiting boot services", status);
        printf(L"[exit_bs] trying again!\r\n");

        map_size = buffer_size;
        status = BS->GetMemoryMap(&map_size, memmap, &map_key, &desc_size, &desc_ver);
        if (EFI_ERROR(status)) {
            perror(L"[exit_bs] error getting memory map on second attempt", status);
            free(memmap);
            return NULL;
        }

        status = BS->ExitBootServices(ImageHandle, map_key);
        if (EFI_ERROR(status)) {
            perror(L"[exit_bs] error exiting boot services on second attempt", status);
            free(memmap);
            return NULL;
        }
    }

    // no more EFI calls after this point!
    wstdlib_bs_exited();

    // populate the boot params EFI info so the kernel can use it
    populate_bootparams_efi(params, memmap, map_size, desc_size, desc_ver);

    return memmap;
}

static void
add_e820ext(boot_params_t *params, setup_data_t *e820ext, UINT32 nr_entries) {
    setup_data_t *data;

    e820ext->type = SETUP_E820_EXT;
    e820ext->len = nr_entries * sizeof(boot_e820_entry_t);
    e820ext->next = 0;

    data = (setup_data_t *)params->hdr.setup_data;

    while (data && data->next) {
        data = (setup_data_t *)data->next;
    }

    if (data) {
        data->next = (UINT64)e820ext;
    } else {
        params->hdr.setup_data = (UINT64)e820ext;
    }
}

EFI_STATUS wboot_translate_memmap(
    boot_params_t *params, setup_data_t *e820ext, UINT32 e820ext_size
) {
    boot_e820_entry_t *entry = params->e820_table;
    efi_info_t *efi = &params->efi_info;
    boot_e820_entry_t *prev = NULL;
    UINT32 nr_entries;
    UINT32 nr_desc;

    nr_entries = 0;
    nr_desc = efi->efi_memmap_size / efi->efi_memdesc_size;

    for (UINTN i = 0; i < nr_desc; i++) {
        e820_type_t e820_type = 0;
        UINTN map = efi->efi_memmap | ((UINT64)efi->efi_memmap_hi << 32);
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)((void *)map +
                                                             (i *
                                                              efi->efi_memdesc_size));

        switch (d->Type) {
            case EfiReservedMemoryType:
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
            case EfiMemoryMappedIO:
            case EfiMemoryMappedIOPortSpace:
            case EfiPalCode: e820_type = E820_TYPE_RESERVED; break;

            case EfiUnusableMemory: e820_type = E820_TYPE_UNUSABLE; break;

            case EfiACPIReclaimMemory: e820_type = E820_TYPE_ACPI; break;

            case EfiLoaderCode:
            case EfiLoaderData:
            case EfiBootServicesCode:
            case EfiBootServicesData:
            case EfiConventionalMemory: e820_type = E820_TYPE_RAM; break;

            case EfiACPIMemoryNVS: e820_type = E820_TYPE_NVS; break;

            case EfiPersistentMemory: e820_type = E820_TYPE_PMEM; break;

            default: continue;
        }

        // Merge adjacent mappings
        if (prev &&
            prev->type == e820_type &&
            (prev->addr + prev->size) == d->PhysicalStart) {
            prev->size += d->NumberOfPages << 12;
            continue;
        }

        if (nr_entries == E820_MAX_ENTRIES_ZEROPAGE) {
            UINT32 need = ((nr_desc - i) * sizeof(boot_e820_entry_t)) +
                          sizeof(setup_header_t);

            if (!e820ext || e820ext_size < need) {
                return EFI_BUFFER_TOO_SMALL;
            }

            // boot_params map full, switch to e820 extended
            entry = (boot_e820_entry_t *)e820ext->data;
        }

        entry->addr = d->PhysicalStart;
        entry->size = d->NumberOfPages << 12;
        entry->type = e820_type;
        prev = entry++;
        nr_entries++;
    }

    if (nr_entries > E820_MAX_ENTRIES_ZEROPAGE) {
        UINT32 nr_e820ext = nr_entries - E820_MAX_ENTRIES_ZEROPAGE;

        add_e820ext(params, e820ext, nr_e820ext);
        nr_entries -= nr_e820ext;
    }

    params->e820_entries = (UINT8)nr_entries;

    return EFI_SUCCESS;
}

VOID *wboot_get_kernel_entry(const VOID *decompressed_kernel) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)decompressed_kernel;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)((UINT8 *)decompressed_kernel + ehdr->e_phoff);

    for (UINTN i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == 1) { // PT_LOAD
            UINT8 *dest = (UINT8 *)phdrs[i].p_paddr;
            UINT8 *src = (UINT8 *)decompressed_kernel + phdrs[i].p_offset;
            memmove(dest, src, phdrs[i].p_filesz);
            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                memset(
                    dest + phdrs[i].p_filesz, 0, phdrs[i].p_memsz - phdrs[i].p_filesz
                );
            }
        }
    }

    return (void *)ehdr->e_entry;
}
