#ifndef _SHIM_STRING_H
#define _SHIM_STRING_H

#include <stddef.h>

// Copies n bytes from src to dest. The memory areas must not overlap.
void *memcpy(void *dest, const void *src, size_t n);

// Fills the first n bytes of the memory area pointed to by s with the constant
// byte c.
void *memset(void *s, int c, size_t n);

// Copies n bytes from src to dest. The memory areas may overlap.
void *memmove(void *dest, const void *src, size_t n);

#endif
