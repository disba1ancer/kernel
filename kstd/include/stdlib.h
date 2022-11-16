#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#include "btstdbeg.h"

KSTD_NORETURN void abort(void);
KSTD_NORETURN void exit(int exit_code);
void atexit(void(*func)(void)) KSTD_NOEXCEPT;
KSTD_NORETURN void _Exit(int exit_code);
void *malloc(size_t size);
void *realloc(void* ptr, size_t size);
void free(void *ptr);
void qsort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*));

#include "btstdend.h"

#endif // STDLIB_H
