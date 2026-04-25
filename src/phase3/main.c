#include "efiglobal.h"
#include "wboot.h"
#include "wboot_config.h"
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
    printf(L"[wboot] hello from wboot!\r\n");

    printf(L"[wboot] locating config...\r\n");

    wboot_config_t *config;
    status = wboot_locate_config(&config);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"[wboot] config kernel = %s\r\n", config->kernel_path);
    printf(L"[wboot] config initrd = %s\r\n", config->initrd_path);
    printf(L"[wboot] config cmdline = %-s\r\n", config->cmdline);
    printf(
        L"[wboot] config mode = %ux%u, format=%d, depth=%u\r\n", config->mode_width,
        config->mode_height, (INTN)config->mode_format, config->mode_depth
    );

    printf(L"[wboot] opening kernel file\r\n");
    EFI_FILE_PROTOCOL *kernel_file;
    status = wboot_open_kernel(config, &kernel_file);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"[wboot] reading kernel setup header\r\n");
    setup_header_t header;
    status = wboot_read_setup_header(kernel_file, &header);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"[wboot] decompressing kernel\r\n");
    VOID *decompressed_kernel;
    UINTN decompressed_kernel_size;
    status = wboot_decompress_kernel(
        &header, kernel_file, &decompressed_kernel, &decompressed_kernel_size
    );
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"[wboot] reading initramfs into memory\r\n");
    VOID *initramfs;
    UINTN initramfs_size;
    status = wboot_load_initramfs(config, &initramfs, &initramfs_size);
    if (EFI_ERROR(status)) {
        goto exit;
    }

    printf(L"[wboot] preparing boot params\r\n");
    boot_params_t *params = wboot_prepare_bootparams(
        config, &header, initramfs, initramfs_size
    );
    if (params == NULL) {
        printf(L"[wboot] error preparing boot params\r\n");
        goto exit;
    }

    setup_data_t *e820ext = NULL;
    UINT32 e820ext_size = 0;

    printf(L"[wboot] setting up graphics\r\n");
    status = wboot_setup_graphics(config, params);
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
