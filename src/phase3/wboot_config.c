#include "wboot_config.h"
#include "efiglobal.h"
#include "wstdlib.h"

typedef struct {
    UINT8 has_kernel;
    UINT8 has_initrd;
    UINT8 has_cmdline;
    UINT8 has_mode;
} config_parse_state_t;

static UINT8 is_ws(CHAR8 c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static VOID trim_trailing_ws(CHAR8 *str) {
    UINTN len = strlen8(str);
    while (len > 0 && is_ws(str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}

static CHAR16 *strdup8_to16(const CHAR8 *src) {
    UINTN len = strlen8(src);
    CHAR16 *dst = malloc((len + 1) * sizeof(CHAR16));
    if (dst == NULL) {
        return NULL;
    }

    for (UINTN i = 0; i <= len; i++) {
        dst[i] = (CHAR16)src[i];
    }
    return dst;
}

static BOOLEAN parse_u32(const CHAR8 *value, UINT32 *out) {
    if (value == NULL || *value == '\0') {
        return FALSE;
    }

    CHAR8 *end;
    UINTN parsed = strtoul(value, &end, 10);
    if (*end != '\0' || parsed > 0xFFFFFFFFU) {
        return FALSE;
    }

    *out = (UINT32)parsed;
    return TRUE;
}

static BOOLEAN parse_mode_format(const CHAR8 *value, INT32 *out) {
    if (value == NULL || *value == '\0') {
        return FALSE;
    }

    if (strcmp8(value, (CHAR8 *)"-1") == 0) {
        *out = -1;
        return TRUE;
    }

    UINT32 parsed;
    if (!parse_u32(value, &parsed) || parsed > 0x7FFFFFFFU) {
        return FALSE;
    }

    *out = (INT32)parsed;
    return TRUE;
}

static VOID free_config_fields(wboot_config_t *config) {
    if (config == NULL) {
        return;
    }

    free(config->kernel_path);
    free(config->initrd_path);
    free(config->cmdline);
    config->kernel_path = NULL;
    config->initrd_path = NULL;
    config->cmdline = NULL;
}

static BOOLEAN parse_config_mode(wboot_config_t *config, CHAR8 *value, UINTN line_no) {
    CHAR8 *saveptr = NULL;
    CHAR8 *width_str = strtok_r(value, (CHAR8 *)" \t", &saveptr);
    CHAR8 *height_str = strtok_r(NULL, (CHAR8 *)" \t", &saveptr);
    CHAR8 *format_str = strtok_r(NULL, (CHAR8 *)" \t", &saveptr);
    CHAR8 *depth_str = strtok_r(NULL, (CHAR8 *)" \t", &saveptr);
    CHAR8 *extra = strtok_r(NULL, (CHAR8 *)" \t", &saveptr);

    if (width_str == NULL || height_str == NULL || extra != NULL) {
        printf(
            L"error parsing wboot.conf line %u: mode must be 'mode <width> <height> "
            L"[format] [depth]'\r\n",
            line_no
        );
        return FALSE;
    }

    UINT32 width;
    UINT32 height;
    if (!parse_u32(width_str, &width) || !parse_u32(height_str, &height)) {
        printf(
            L"error parsing wboot.conf line %u: invalid mode width/height\r\n", line_no
        );
        return FALSE;
    }

    INT32 format = -1;
    if (format_str != NULL && !parse_mode_format(format_str, &format)) {
        printf(
            L"error parsing wboot.conf line %u: invalid mode format '%-s'\r\n", line_no,
            format_str
        );
        return FALSE;
    }

    UINT32 depth = 0;
    if (depth_str != NULL && !parse_u32(depth_str, &depth)) {
        printf(
            L"error parsing wboot.conf line %u: invalid mode depth '%-s'\r\n", line_no,
            depth_str
        );
        return FALSE;
    }

    config->mode_width = width;
    config->mode_height = height;
    config->mode_format = format;
    config->mode_depth = depth;

    return TRUE;
}

static BOOLEAN parse_config_line(
    wboot_config_t *config, config_parse_state_t *state, CHAR8 *line, UINTN line_no
) {
    while (is_ws(*line)) {
        line++;
    }

    if (*line == '\0' || *line == '#') {
        return TRUE;
    }

    CHAR8 *value = line;
    while (*value != '\0' && !is_ws(*value)) {
        value++;
    }

    if (*value == '\0') {
        printf(
            L"error parsing wboot.conf line %u: missing value for key '%-s'\r\n",
            line_no, line
        );
        return FALSE;
    }

    *value = '\0';
    value++;
    while (is_ws(*value)) {
        value++;
    }
    trim_trailing_ws(value);

    if (*value == '\0') {
        printf(
            L"error parsing wboot.conf line %u: missing value for key '%-s'\r\n",
            line_no, line
        );
        return FALSE;
    }

    if (strcmp8(line, (CHAR8 *)"kernel") == 0) {
        CHAR16 *kernel = strdup8_to16(value);
        if (kernel == NULL) {
            perror(L"error allocating kernel path", EFI_OUT_OF_RESOURCES);
            return FALSE;
        }
        free(config->kernel_path);
        config->kernel_path = kernel;
        state->has_kernel = TRUE;
        return TRUE;
    }

    if (strcmp8(line, (CHAR8 *)"initrd") == 0) {
        CHAR16 *initrd = strdup8_to16(value);
        if (initrd == NULL) {
            perror(L"error allocating initrd path", EFI_OUT_OF_RESOURCES);
            return FALSE;
        }
        free(config->initrd_path);
        config->initrd_path = initrd;
        state->has_initrd = TRUE;
        return TRUE;
    }

    if (strcmp8(line, (CHAR8 *)"cmdline") == 0) {
        CHAR8 *cmdline = strdup8(value);
        if (cmdline == NULL) {
            perror(L"error allocating cmdline", EFI_OUT_OF_RESOURCES);
            return FALSE;
        }
        free(config->cmdline);
        config->cmdline = cmdline;
        state->has_cmdline = TRUE;
        return TRUE;
    }

    if (strcmp8(line, (CHAR8 *)"mode") == 0) {
        if (!parse_config_mode(config, value, line_no)) {
            return FALSE;
        }
        state->has_mode = TRUE;
        return TRUE;
    }

    printf(L"error parsing wboot.conf line %u: unknown key '%-s'\r\n", line_no, line);
    return FALSE;
}

wboot_config_t *wboot_load_config(EFI_FILE_PROTOCOL *root) {
    wboot_config_t *config = malloc(sizeof(wboot_config_t));
    if (!config) {
        return NULL;
    }

    *config = (wboot_config_t){
        .kernel_path = NULL,
        .initrd_path = NULL,
        .cmdline = NULL,
        .mode_width = 0,
        .mode_height = 0,
        .mode_format = -1,
        .mode_depth = 0,
        .root = root,
    };

    EFI_FILE_PROTOCOL *config_file;
    EFI_STATUS status = root->Open(
        root, &config_file, L"wboot.conf", EFI_FILE_MODE_READ, 0
    );
    if (EFI_ERROR(status)) {
        free(config);
        return NULL;
    }

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + (512 * sizeof(CHAR16));
    EFI_FILE_INFO *file_info = malloc(file_info_size);
    if (!file_info) {
        perror(L"error allocating memory for config file info", errno);
        config_file->Close(config_file);
        free(config);
        return NULL;
    }
    status = config_file->GetInfo(
        config_file, &gEfiFileInfoGuid, &file_info_size, file_info
    );
    if (EFI_ERROR(status)) {
        perror(L"error getting config file info", status);
        free(file_info);
        config_file->Close(config_file);
        free(config);
        return NULL;
    }

    UINTN config_size = file_info->FileSize;
    free(file_info);

    CHAR8 *config_data = malloc(config_size + 1);
    if (!config_data) {
        perror(L"error allocating memory for config data", errno);
        config_file->Close(config_file);
        free(config);
        return NULL;
    }

    UINTN bytes_read = config_size;
    status = config_file->Read(config_file, &bytes_read, config_data);
    if (EFI_ERROR(status)) {
        perror(L"error reading config file", status);
        free(config_data);
        config_file->Close(config_file);
        free(config);
        return NULL;
    }
    if (bytes_read != config_size) {
        printf(
            L"error: expected to read %u bytes but read %u bytes\n", config_size,
            bytes_read
        );
        free(config_data);
        config_file->Close(config_file);
        free(config);
        return NULL;
    }
    config_data[bytes_read] = '\0';

    config_parse_state_t state = {0};
    CHAR8 *saveptr = NULL;
    CHAR8 *line = strtok_r(config_data, (CHAR8 *)"\n", &saveptr);
    UINTN line_no = 1;

    while (line != NULL) {
        if (!parse_config_line(config, &state, line, line_no)) {
            free(config_data);
            config_file->Close(config_file);
            free_config_fields(config);
            free(config);
            return NULL;
        }
        line = strtok_r(NULL, (CHAR8 *)"\n", &saveptr);
        line_no++;
    }

    if (!state.has_kernel ||
        !state.has_initrd ||
        !state.has_cmdline ||
        !state.has_mode) {
        if (!state.has_kernel) {
            printf(L"error parsing wboot.conf: missing required line 'kernel'\r\n");
        }
        if (!state.has_initrd) {
            printf(L"error parsing wboot.conf: missing required line 'initrd'\r\n");
        }
        if (!state.has_cmdline) {
            printf(L"error parsing wboot.conf: missing required line 'cmdline'\r\n");
        }
        if (!state.has_mode) {
            printf(L"error parsing wboot.conf: missing required line 'mode'\r\n");
        }
        free(config_data);
        config_file->Close(config_file);
        free_config_fields(config);
        free(config);
        return NULL;
    }

    free(config_data);
    config_file->Close(config_file);

    return config;
}

EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *_device_path_to_text;
EFI_STATUS _get_device_path_to_text() {
    EFI_STATUS status = BS->LocateProtocol(
        &gEfiDevicePathToTextProtocolGuid, NULL, (VOID **)&_device_path_to_text
    );
    if (EFI_ERROR(status)) {
        printf(L"error locating DEVICE_PATH_TO_TEXT_PROTOCOL: %u\r\n", status);
    }
    return status;
}

EFI_STATUS wboot_locate_config(wboot_config_t **config) {
    EFI_STATUS status = EFI_SUCCESS;

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
        return status;
    }

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
                printf(L"[locate_config] device path: unknown error\r\n");
            }
            printf(L"[locate_config] device path: %s\r\n", device_path_str);
            BS->FreePool(device_path_str);
        } else {
            perror(L"[locate_config] error getting device path", status);
        }

        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
        status = BS->HandleProtocol(
            handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&fs
        );
        if (EFI_ERROR(status)) {
            perror(L"[locate_config] error getting file system protocol", status);
            continue;
        }

        EFI_FILE_PROTOCOL *root;
        status = fs->OpenVolume(fs, &root);
        if (EFI_ERROR(status)) {
            perror(L" [locate_config] error opening volume", status);
            continue;
        }

        wboot_config_t *cfg = wboot_load_config(root);
        if (cfg) {
            *config = cfg;
            return EFI_SUCCESS;
        }
    }

    printf(L"[locate_config] could not find config file on any device\r\n");
    return EFI_NOT_FOUND;
}
