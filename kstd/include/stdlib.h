#ifndef _STDLIB_H
#define _STDLIB_H

#include "kstd/btstdbeg.h"

#ifdef _KSTD_CPPMODE
#include <cstddef>
#include <cstdint>
namespace std {
#else
#include <stddef.h>
#include <stdint.h>
#endif

_KSTD_EXTERN_BEGIN

_KSTD_NORETURN void abort(void);
_KSTD_NORETURN void exit(int exit_code);
void atexit(void(*func)(void)) _KSTD_NOEXCEPT;
_KSTD_NORETURN void _Exit(int exit_code);
void *malloc(size_t size);
void *realloc(void* ptr, size_t size);
void free(void *ptr);
void qsort(void* ptr, size_t count, size_t size,
    int (*comp)(const void*, const void*));

#define GENERATE(type, namepref)\
_KSTD_CONSTEXPR inline type namepref##abs(type val);

GENERATE(int, )
GENERATE(long, l)
GENERATE(long long, ll)
GENERATE(intmax_t, imax)
#undef GENERATE

_KSTD_EXTERN_END

#include "kstd/stdlib_impl.h"

#ifdef _KSTD_CPPMODE
} // namespace std
#endif

#include "kstd/btstdend.h"

#endif // _STDLIB_H
