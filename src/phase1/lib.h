#pragma once

#include <efi.h>

//
// Globals
//
extern BOOLEAN LibInitialized;
extern BOOLEAN LibFwInstance;
extern EFI_HANDLE LibImageHandle;
extern SIMPLE_TEXT_OUTPUT_INTERFACE *LibRuntimeDebugOut;
extern EFI_UNICODE_COLLATION_INTERFACE *UnicodeInterface;
extern EFI_UNICODE_COLLATION_INTERFACE LibStubUnicodeInterface;
extern EFI_RAISE_TPL LibRuntimeRaiseTPL;
extern EFI_RESTORE_TPL LibRuntimeRestoreTPL;
