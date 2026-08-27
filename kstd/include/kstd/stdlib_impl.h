#ifndef STDLIB_IMPL_H
#define STDLIB_IMPL_H

#include "btstdbeg.h"

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
namespace kstd::stdlib {
extern "C" {
using std::size_t;
#else
#include <stddef.h>
#include <stdint.h>
#endif

_KSTD_NORETURN void abort(void);
_KSTD_NORETURN void exit(int exit_code);
void atexit(void(*func)(void)) _KSTD_NOEXCEPT;
_KSTD_NORETURN void _Exit(int exit_code);
void *malloc(size_t size);
void *realloc(void* ptr, size_t size);
void free(void *ptr);
void qsort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*));

#define GENERATE(type, namepref)\
type namepref##abs(type val);

GENERATE(int, )
GENERATE(long, l)
GENERATE(long long, ll)
#undef GENERATE

#ifdef __cplusplus
}
}
#endif

#include "btstdend.h"

#endif // STDLIB_IMPL_H
