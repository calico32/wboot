#include <efi.h>

EFI_STATUS efi_main(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systemTable) {
    EFI_STATUS status = EFI_SUCCESS;
    EFI_INPUT_KEY key;
    EFI_SYSTEM_TABLE *ST = systemTable;

    // Print "Hello World" to the console. UEFI follows Windows-style
    // conventions, so we need to use wide strings (CHAR16*) and CRLF line
    // endings (\r\n).
    status = ST->ConOut->OutputString(ST->ConOut, L"Hello World\r\n");
    if (EFI_ERROR(status)) {
        return status;
    }

    // Reset the console input buffer to make sure there are no pending key
    // presses that we might accidentally read later.
    status = ST->ConIn->Reset(ST->ConIn, FALSE);
    if (EFI_ERROR(status)) {
        return status;
    }

    // Poll the console input until a key is pressed. Simple but very
    // inefficient. In a real application, you'd probably want to use events and
    // wait for them instead of busy-waiting like this.
    while ((status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key)) == EFI_NOT_READY) {}

    return status;
}
