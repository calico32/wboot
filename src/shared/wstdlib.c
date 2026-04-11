#include <__stdarg_va_arg.h>
#include <__stdarg_va_list.h>
#include <efi.h>

#include "efiglobal.h"
#include "stdlib.h"
#include "string.h"
#include "wstdlib.h"

#define _PRINTF_BUFFER_SIZE 1024
CHAR16 _printf_buffer[_PRINTF_BUFFER_SIZE];
UINTN _printf_buffer_pos = 0;

static inline UINTN _strlen(const CHAR16 *str) {
    UINTN len = 0;
    while (str[len] != L'\0') {
        len++;
    }
    return len;
}

static inline VOID _printf_flush() {
    if (_printf_buffer_pos > 0) {
        _printf_buffer[_printf_buffer_pos] = L'\0';
        ST->ConOut->OutputString(ST->ConOut, _printf_buffer);
        _printf_buffer_pos = 0;
    }
}

static inline VOID _printf_write(const CHAR16 *str) {
    UINTN len = _strlen(str);
    if (_printf_buffer_pos + len >= _PRINTF_BUFFER_SIZE) {
        _printf_flush();
    }
    for (UINTN i = 0; i < len; i++) {
        _printf_buffer[_printf_buffer_pos++] = str[i];
    }
}

static inline VOID _printf_write_char(CHAR16 c) {
    if (_printf_buffer_pos + 1 >= _PRINTF_BUFFER_SIZE) {
        _printf_flush();
    }
    _printf_buffer[_printf_buffer_pos++] = c;
}

static inline VOID _printf_write_uintn(UINTN num) {
    if (num == 0) {
        _printf_write_char(L'0');
        return;
    }
    UINTN temp = num;
    UINTN divisor = 1;
    while (temp >= 10) {
        temp /= 10;
        divisor *= 10;
    }
    while (divisor > 0) {
        UINTN digit = (num / divisor) % 10;
        _printf_write_char((CHAR16)(digit + L'0'));
        divisor /= 10;
    }
}

static inline VOID _printf_write_hex(UINTN num) {
    if (num == 0) {
        _printf_write_char(L'0');
        return;
    }
    UINTN temp = num;
    UINTN divisor = 1;
    while (temp >= 16) {
        temp /= 16;
        divisor *= 16;
    }
    while (divisor > 0) {
        UINTN digit = (num / divisor) % 16;
        _printf_write_char((CHAR16)((digit < 10) ? (digit + L'0')
                                                 : (digit - 10 + L'a')));
        divisor /= 16;
    }
}

static inline VOID _printf_write_pointer(VOID *ptr) {
    UINTN num = (UINTN)ptr;
    INT8 digit;
    _printf_write(L"0x");

    UINTN divisor = 1ULL << (((sizeof(VOID *) * 2) - 1) * 4);
    while (divisor > 0) {
        digit = (INT8)((num / divisor) % 16);
        _printf_write_char((digit < 10) ? (digit + L'0') : (digit - 10 + L'a'));
        divisor /= 16;
    }
}

static inline UINTN strlen8(const CHAR8 *str) {
    UINTN len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

EFI_STATUS printf(const CHAR16 *format, ...) {
    va_list args;
    va_start(args, format);
    UINTN pos = 0;
    for (UINTN i = 0; format[i] != L'\0'; i++) {
        if (format[i] != L'%') {
            _printf_write_char(format[i]);
            continue;
        }
        i++;
        switch (format[i]) {
            case L's': {
                CHAR16 *str = va_arg(args, CHAR16 *);
                _printf_write(str);
                break;
            }
            case L'u': {
                UINTN num = va_arg(args, UINTN);
                _printf_write_uintn(num);
                break;
            }
            case L'd': {
                INTN num = va_arg(args, INTN);
                if (num < 0) {
                    _printf_write_char(L'-');
                    num = -num;
                }
                _printf_write_uintn((UINTN)num);
                break;
            }
            case L'x': {
                UINTN num = va_arg(args, UINTN);
                _printf_write_hex(num);
                break;
            }
            case L'p': {
                VOID *ptr = va_arg(args, VOID *);
                _printf_write_pointer(ptr);
                break;
            }
            case L'c': {
                CHAR16 c = (CHAR16)va_arg(args, INTN);
                _printf_write_char(c);
                break;
            }
            case L'%': {
                _printf_write_char(L'%');
                break;
            }
            case L'-': {
                i++;
                if (format[i] == L's') {
                    CHAR8 *str = va_arg(args, CHAR8 *);
                    // Convert to CHAR16
                    CHAR16 *buffer = (CHAR16 *)malloc(
                        (strlen8(str) + 1) * sizeof(CHAR16)
                    );
                    UINTN j;
                    for (j = 0; j < 255 && str[j] != '\0'; j++) {
                        buffer[j] = (CHAR16)str[j];
                    }
                    buffer[j] = L'\0';
                    _printf_write(buffer);
                    free(buffer);
                } else {
                    _printf_write_char(L'%');
                    _printf_write_char(format[i]);
                }
                break;
            }
            default: {
                _printf_write_char(L'%');
                _printf_write_char(format[i]);
            }
        }
    }
    va_end(args);
    _printf_flush();
    return EFI_SUCCESS;
}

CHAR16 read_char() {
    EFI_INPUT_KEY key;
    BS->WaitForEvent(1, &ST->ConIn->WaitForKey, NULL);
    while (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) == EFI_NOT_READY) {}
    return key.UnicodeChar;
}

const CHAR16 *strerror(EFI_STATUS status) {
    switch (status) {
        case EFI_SUCCESS: return L"success";
        case EFI_LOAD_ERROR: return L"load error";
        case EFI_INVALID_PARAMETER: return L"invalid parameter";
        case EFI_UNSUPPORTED: return L"unsupported";
        case EFI_BAD_BUFFER_SIZE: return L"bad buffer size";
        case EFI_BUFFER_TOO_SMALL: return L"buffer too small";
        case EFI_NOT_READY: return L"not ready";
        case EFI_DEVICE_ERROR: return L"device error";
        case EFI_WRITE_PROTECTED: return L"write protected";
        case EFI_OUT_OF_RESOURCES: return L"out of resources";
        case EFI_VOLUME_CORRUPTED: return L"volume corrupted";
        case EFI_VOLUME_FULL: return L"volume full";
        case EFI_NO_MEDIA: return L"no media";
        case EFI_MEDIA_CHANGED: return L"media changed";
        case EFI_NOT_FOUND: return L"not found";
        case EFI_ACCESS_DENIED: return L"access denied";
        case EFI_NO_RESPONSE: return L"no response";
        case EFI_NO_MAPPING: return L"no mapping";
        case EFI_TIMEOUT: return L"timeout";
        case EFI_NOT_STARTED: return L"not started";
        case EFI_ALREADY_STARTED: return L"already started";
        case EFI_ABORTED: return L"aborted";
        case EFI_ICMP_ERROR: return L"ICMP error";
        case EFI_TFTP_ERROR: return L"TFTP error";
        case EFI_PROTOCOL_ERROR: return L"protocol error";
        case EFI_INCOMPATIBLE_VERSION: return L"incompatible version";
        case EFI_SECURITY_VIOLATION: return L"security violation";
        case EFI_CRC_ERROR: return L"CRC error";
        case EFI_END_OF_MEDIA: return L"end of media";
        case EFI_END_OF_FILE: return L"end of file";
        case EFI_INVALID_LANGUAGE: return L"invalid language";
        case EFI_COMPROMISED_DATA: return L"compromised data";
        case EFI_IP_ADDRESS_CONFLICT: return L"IP address conflict";
        case EFI_HTTP_ERROR: return L"HTTP error";
        default: return L"unknown error";
    }
}

static EFI_STATUS _errno;
EFI_STATUS *_errno_location() {
    return &_errno;
}

VOID perror(const CHAR16 *message, EFI_STATUS status) {
    printf(L"%s: %s\r\n", message, strerror(status));
}

INTN strcmp(const CHAR16 *str1, const CHAR16 *str2) {
    UINTN i = 0;
    while (str1[i] != L'\0' && str2[i] != L'\0') {
        if (str1[i] != str2[i]) {
            return (INTN)(str1[i]) - str2[i];
        }
        i++;
    }
    return (INTN)(str1[i]) - str2[i];
}

INTN strncmp(const CHAR16 *str1, const CHAR16 *str2, UINTN n) {
    UINTN i = 0;
    while (i < n && str1[i] != L'\0' && str2[i] != L'\0') {
        if (str1[i] != str2[i]) {
            return (INTN)(str1[i]) - str2[i];
        }
        i++;
    }
    if (i == n) {
        return 0;
    }
    return (INTN)(str1[i]) - str2[i];
}

static const CHAR16 *efi_memory_type_to_string(UINT32 type) {
    switch (type) {
        case EfiReservedMemoryType: return L"Reserved    ";
        case EfiLoaderCode: return L"LoaderCode  ";
        case EfiLoaderData: return L"LoaderData  ";
        case EfiBootServicesCode: return L"BootCode    ";
        case EfiBootServicesData: return L"BootData    ";
        case EfiRuntimeServicesCode: return L"RuntimeCode ";
        case EfiRuntimeServicesData: return L"RuntimeData ";
        case EfiConventionalMemory: return L"Conventional";
        case EfiUnusableMemory: return L"Unusable    ";
        case EfiACPIReclaimMemory: return L"ACPIReclaim ";
        case EfiACPIMemoryNVS: return L"ACPINVS     ";
        case EfiMemoryMappedIO: return L"MMIO        ";
        case EfiMemoryMappedIOPortSpace: return L"MMIOPort    ";
        case EfiPalCode: return L"PalCode     ";
        case EfiPersistentMemory: return L"Persistent  ";
        default: return L"Unknown     ";
    }
}

VOID dump_memory_map() {
#ifdef DEBUG
    UINTN memory_map_size = 0;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;

    EFI_STATUS status = BS->GetMemoryMap(
        &memory_map_size, memory_map, &map_key, &descriptor_size, &descriptor_version
    );
    if (status != EFI_BUFFER_TOO_SMALL) {
        perror(L"Failed to get memory map size", status);
        return;
    }

    // Add extra space to avoid edge-cases where memory allocations change the map size
    memory_map_size += descriptor_size * 2;
    memory_map = (EFI_MEMORY_DESCRIPTOR *)malloc(memory_map_size);
    if (memory_map == NULL) {
        printf(L"Failed to allocate memory for memory map\r\n");
        return;
    }

    status = BS->GetMemoryMap(
        &memory_map_size, memory_map, &map_key, &descriptor_size, &descriptor_version
    );
    if (EFI_ERROR(status)) {
        perror(L"Failed to get memory map", status);
        free(memory_map);
        return;
    }

    printf(L"EFI Memory Map:\r\n");
    printf(L"Type               PhysicalStart         PhysicalEnd          Pages\r\n");

    UINTN num_entries = memory_map_size / descriptor_size;
    EFI_MEMORY_DESCRIPTOR *desc = memory_map;
    UINTN last_physical_end = 0;

    for (UINTN i = 0; i < num_entries; i++) {
        UINTN empty_space = desc->PhysicalStart - last_physical_end;
        if (empty_space > 0) {
            printf(
                L"                   %p    %p   %u pages\r\n", last_physical_end,
                desc->PhysicalStart, empty_space / EFI_PAGE_SIZE
            );
        }
        UINTN physical_end = desc->PhysicalStart +
                             (desc->NumberOfPages * EFI_PAGE_SIZE);
        printf(
            L"[%x] %s   %p    %p   %u pages\r\n", desc->Type,
            efi_memory_type_to_string(desc->Type), (UINTN)desc->PhysicalStart,
            physical_end, (UINTN)desc->NumberOfPages
        );
        last_physical_end = physical_end;
        desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)desc + descriptor_size);
    }

    free(memory_map);
#endif
}

void *malloc(size_t size) {
    void *ptr = NULL;
    EFI_STATUS status = BS->AllocatePool(EfiLoaderData, size, &ptr);
    if (EFI_ERROR(status)) {
        _errno = status;
        return NULL;
    }
    return ptr;
}

void free(void *ptr) {
    if (ptr) {
        _errno = BS->FreePool(ptr);
    }
}

void *calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}
void *memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (char)c;
    }
    return s;
}
void *memmove(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}
