// wstdlib.h - basic implementations of C standard library functions for UEFI
// applications. This is not a complete implementation of the C standard
// library, but it provides enough functionality for our purposes in this
// project.

#pragma once

#include "efibind.h"
#include "stdlib.h" // IWYU pragma: keep
#include "string.h" // IWYU pragma: keep
#include <efi.h>

// Call immediately after exiting boot services to prevent any further calls to
// UEFI boot services functions by wstdlib functions.
void wstdlib_bs_exited();

EFI_STATUS *_errno_location();

#define errno (*_errno_location())

// Very basic printf implementation. Supported format specifiers:
//
// - %s - string (char16 null-terminated)
// - %-s - string (char8 null-terminated)
// - %d - integer INTN
// - %u - unsigned integer UINTN
// - %p - pointer
// - %x - hexadecimal UINTN
// - %c - character
// - %% - literal percent sign
//
// All other format specifiers or modifiers are not supported and will be
// treated as literal characters.
EFI_STATUS printf(const CHAR16 *format, ...);

// Reads a single character from the console input. This is a blocking call that
// waits until a key is pressed.
CHAR16 read_char();

// Returns a string representation of the given EFI_STATUS code. Useful for
// debugging and error messages.
const CHAR16 *strerror(EFI_STATUS status);

// Prints an error message to the console, including the string representation
// of the given EFI_STATUS code.
VOID perror(const CHAR16 *message, EFI_STATUS status);

// Compares two null-terminated strings. Returns 0 if the strings are equal, a
// negative value if str1 < str2, and a positive value if str1 > str2.
INTN strcmp(const CHAR16 *str1, const CHAR16 *str2);

// Compares two null-terminated strings. Returns 0 if the strings are equal, a
// negative value if str1 < str2, and a positive value if str1 > str2.
INTN strcmp8(const CHAR8 *str1, const CHAR8 *str2);

// Compares up to n characters of two null-terminated strings. Returns 0 if the
// strings are equal, a negative value if str1 < str2, and a positive value if
// str1 > str2.
INTN strncmp(const CHAR16 *str1, const CHAR16 *str2, UINTN n);

CHAR8 *strchr(const CHAR8 *str, CHAR8 c);
CHAR8 *strtok_r(CHAR8 *str, const CHAR8 *delim, CHAR8 **saveptr);
UINTN strtoul(const CHAR8 *str, CHAR8 **endptr, int base);
UINTN strlen8(const CHAR8 *str);
CHAR8 *strdup8(const CHAR8 *src);
