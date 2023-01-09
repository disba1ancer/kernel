#ifndef _STRING_H
#define _STRING_H

#include "kstd/btstdbeg.h"

#ifdef _KSTD_CPPMODE
#include <cstddef>
namespace std {
#else
#include <stddef.h>
#endif

_KSTD_EXTERN_BEGIN

void* memset(void* dst, int val, size_t size);
void* memcpy(void* _KSTD_RESTRICT dst, const void* _KSTD_RESTRICT src, size_t size);
void* memmove(void* dst, const void* src, size_t size);
int memcmp(const void* dst, const void* src, size_t size);
int strcmp(const char* dst, const char* src);
size_t strlen(const char* dst);
char* strchr(const char* dst, int ch);

_KSTD_EXTERN_END

#ifdef _KSTD_CPPMODE
} // namespace std
#endif

#include "kstd/btstdend.h"

#endif // _STRING_H
