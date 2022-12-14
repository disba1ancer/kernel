#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include "btstdbeg.h"

void* memset(void* dst, int val, size_t size);
void* memcpy(void* KSTD_RESTRICT dst, const void* KSTD_RESTRICT src, size_t size);
void* memmove(void* dst, const void* src, size_t size);
int memcmp(const void* dst, const void* src, size_t size);
int strcmp(const char* dst, const char* src);
size_t strlen(const char* dst);
char* strchr(const char* dst, int ch);

#include "btstdend.h"

#endif // STRING_H
