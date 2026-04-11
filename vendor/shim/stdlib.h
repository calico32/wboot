#ifndef _SHIM_STDLIB_H
#define _SHIM_STDLIB_H

// Allocates size bytes of memory (via AllocatePool) and returns a pointer to
// the allocated memory. The memory is not initialized. Returns NULL if the
// allocation fails, in which case errno is set appropriately.
void *malloc(size_t size);

// Frees the memory space pointed to by ptr, which must have been returned by a
// previous call to malloc() or calloc(). If ptr is NULL, no operation is
// performed. errno may be set upon return to indicate an error.
void free(void *ptr);

// Allocates memory for an array of nmemb elements of size bytes each and
// returns a pointer to the allocated memory. The memory is set to zero. Returns
// NULL if the allocation fails, in which case errno is set appropriately.
//
// Equivalent to malloc(nmemb * size) followed by memset(ptr, 0, nmemb * size).
void *calloc(size_t nmemb, size_t size);

#endif
