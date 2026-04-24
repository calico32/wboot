#pragma once

#include <efi.h>

extern EFI_SYSTEM_TABLE *ST;
extern EFI_BOOT_SERVICES *BS;
extern EFI_HANDLE ImageHandle;

extern EFI_GUID gEfiDevicePathProtocolGuid;
extern EFI_GUID gEfiDevicePathToTextProtocolGuid;
extern EFI_GUID gEfiSimpleFileSystemProtocolGuid;
extern EFI_GUID gEfiFileInfoGuid;
extern EFI_GUID gEfiGraphicsOutputProtocolGuid;
extern EFI_GUID gEfiEdidActiveProtocolGuid;
extern EFI_GUID gEfiEdidDiscoveredProtocolGuid;
extern EFI_GUID gEfiSimpleTextOutputProtocolGuid;
