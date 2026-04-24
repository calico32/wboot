#include "efiglobal.h"
#include "wboot.h"
#include "wboot_graphics.h"
#include "wstdlib.h"

const CHAR16 logo[] = L"\r\n\r\n    |_  _  __|_ \r\n\\/\\/|_)(_)(_)|_\r\n\r\n";

EFI_STATUS efi_main(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systemTable) {
    EFI_STATUS status = EFI_SUCCESS;
    EFI_INPUT_KEY key;
    ImageHandle = handle;
    ST = systemTable;
    BS = ST->BootServices;

    printf(logo);
    printf(L"[wboot] Hello from wboot!\r\n");

    EFI_FILE_PROTOCOL *kernel_file;
    EFI_FILE_PROTOCOL *initramfs_file;
    status = wboot_locate_kernel(&kernel_file, &initramfs_file);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + (512 * sizeof(CHAR16));
    EFI_FILE_INFO *file_info = malloc(file_info_size);
    if (file_info == NULL) {
        perror(L"[wboot] error allocating pool for file info", status);
        goto exit;
    }

    status = kernel_file->GetInfo(
        kernel_file, &gEfiFileInfoGuid, &file_info_size, file_info
    );
    if (EFI_ERROR(status)) {
        perror(L"[wboot] error getting file info", status);
        goto exit;
    }

    printf(L"[wboot] kernel file found: %s\r\n", file_info->FileName);
    printf(L"[wboot] kernel file size: %u bytes\r\n", file_info->FileSize);

    file_info_size = sizeof(EFI_FILE_INFO) + (512 * sizeof(CHAR16));
    status = initramfs_file->GetInfo(
        initramfs_file, &gEfiFileInfoGuid, &file_info_size, file_info
    );
    if (EFI_ERROR(status)) {
        perror(L"[wboot] error getting initramfs file info", status);
        goto exit;
    }

    printf(L"[wboot] initramfs file found: %s\r\n", file_info->FileName);
    printf(L"[wboot] initramfs file size: %u bytes\r\n", file_info->FileSize);

    setup_header_t header;
    status = wboot_read_setup_header(kernel_file, &header);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    status = BS->FreePool(file_info);
    if (EFI_ERROR(status)) {
        perror(L"[wboot] error freeing pool for file info", status);
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

    printf(L"[wboot] decompressed kernel size: %u bytes\r\n", decompressed_kernel_size);

    VOID *initramfs;
    UINTN initramfs_size;
    status = wboot_load_initramfs(initramfs_file, &initramfs, &initramfs_size);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"[wboot] read initramfs into memory at %p\r\n", initramfs);

    printf(L"[wboot] preparing boot params structure\r\n");
    boot_params_t *params = wboot_prepare_bootparams(
        &header, initramfs, initramfs_size
    );
    if (params == NULL) {
        printf(L"[wboot] Error preparing boot params\r\n");
        goto exit;
    }

    setup_data_t *e820ext = NULL;
    UINT32 e820ext_size = 0;

    printf(L"[wboot] setting up graphics\r\n");
    status = wboot_setup_graphics(params);
    if (EFI_ERROR(status)) {
        perror(L"[wboot] error setting up graphics", status);
        goto exit;
    }

    printf(L"[wboot] allocating e820 memory map\r\n");

    status = wboot_allocate_e820(params, &e820ext, &e820ext_size);
    if (EFI_ERROR(status)) {
        perror(L"[wboot] error allocating e820ext", status);
        goto exit;
    }

    printf(L"[wboot] exiting boot services!\r\n");
    EFI_MEMORY_DESCRIPTOR *memmap = wboot_exit_bs(params);
    if (memmap == NULL) {
        printf(L"[wboot] error exiting boot services\r\n");
        goto exit;
    }

    printf(L"[wboot] exited boot services, acquired memory map at %p\r\n", memmap);
    printf(L"[wboot] translating EFI memory map to e820\r\n");
    status = wboot_translate_memmap(params, e820ext, e820ext_size);
    if (EFI_ERROR(status)) {
        perror(L"[wboot] error translating memory map", status);
        goto exit;
    }

    VOID *kernel_entry = wboot_get_kernel_entry(decompressed_kernel);
    printf(L"[wboot] handing off to kernel entry point at %p\r\n", kernel_entry);
    wboot_handoff(params, kernel_entry);

    printf(L"[wboot] returned from wboot_handoff, something went wrong!\r\n");

exit:
    printf(L"[wboot] press any key to continue...\r\n");
    read_char();
    return status;
}
