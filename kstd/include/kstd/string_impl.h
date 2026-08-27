#ifndef STRING_IMPL_H
#define STRING_IMPL_H

#include "btstdbeg.h"

#ifdef __cplusplus
#include <cstddef>
namespace kstd::string {
extern "C" {
#else
#include <stddef.h>
#endif

void* memset(void* dst, int val, size_t size);
void* memcpy(void* _KSTD_RESTRICT dst, const void* _KSTD_RESTRICT src, size_t size);
void* memmove(void* dst, const void* src, size_t size);
int memcmp(const void* dst, const void* src, size_t size);
int strcmp(const char* dst, const char* src);
size_t strlen(const char* dst);
char* strchr(const char* dst, int ch);

#ifdef __cplusplus
}
}
#endif

#include "btstdend.h"

#endif // STRING_IMPL_H
