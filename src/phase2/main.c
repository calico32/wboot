#include "efiglobal.h"
#include "wboot.h"
#include "wstdlib.h"

EFI_STATUS efi_main(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systemTable) {
    EFI_STATUS status = EFI_SUCCESS;
    EFI_INPUT_KEY key;
    ST = systemTable;
    BS = ST->BootServices;

    EFI_FILE_PROTOCOL *kernel_file;
    EFI_FILE_PROTOCOL *initramfs_file;
    status = wboot_locate_kernel(&kernel_file, &initramfs_file);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + (512 * sizeof(CHAR16));
    EFI_FILE_INFO *file_info = malloc(file_info_size);
    if (file_info == NULL) {
        perror(L"Error allocating pool for file info", status);
        goto exit;
    }

    status = kernel_file->GetInfo(
        kernel_file, &gEfiFileInfoGuid, &file_info_size, file_info
    );
    if (EFI_ERROR(status)) {
        perror(L"Error getting file info", status);
        goto exit;
    }

    printf(L"Kernel file found: %s\r\n", file_info->FileName);
    printf(L"Kernel file size: %u bytes\r\n", file_info->FileSize);

    file_info_size = sizeof(EFI_FILE_INFO) + (512 * sizeof(CHAR16));
    status = initramfs_file->GetInfo(
        initramfs_file, &gEfiFileInfoGuid, &file_info_size, file_info
    );
    if (EFI_ERROR(status)) {
        perror(L"Error getting initramfs file info", status);
        goto exit;
    }

    printf(L"Initramfs file found: %s\r\n", file_info->FileName);
    printf(L"Initramfs file size: %u bytes\r\n", file_info->FileSize);

    setup_header_t header;
    status = wboot_read_setup_header(kernel_file, &header);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    wboot_dump_setup_header(&header);

    status = BS->FreePool(file_info);
    if (EFI_ERROR(status)) {
        perror(L"Error freeing pool for file info", status);
        goto exit;
    }

    VOID *decompressed_kernel;
    UINTN decompressed_kernel_size;
    status = wboot_decompress_kernel(
        &header, kernel_file, &decompressed_kernel, &decompressed_kernel_size
    );
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"Decompressed kernel size: %u bytes\r\n", decompressed_kernel_size);

    VOID *initramfs;
    UINTN initramfs_size;
    status = wboot_load_initramfs(initramfs_file, &initramfs, &initramfs_size);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"Read initramfs into memory at %p\r\n", initramfs);
exit:
    printf(L"Press any key to continue...\r\n");
    read_char();
    return status;
}
