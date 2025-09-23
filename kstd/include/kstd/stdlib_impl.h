#ifndef STDLIB_IMPL_H
#define STDLIB_IMPL_H

_KSTD_EXTERN_BEGIN

#define GENERATE(type, namepref)\
_KSTD_CONSTEXPR inline type namepref##abs(type val)\
{\
    return val < 0 ? -val : val;\
}

GENERATE(int, )
GENERATE(long, l)
GENERATE(long long, ll)
GENERATE(intmax_t, imax)
#undef GENERATE

_KSTD_EXTERN_END

#ifdef _KSTD_CPPMODE
#define GENERATE(type, namepref)\
_KSTD_CONSTEXPR inline type abs(type val)\
{\
    return namepref##abs(val);\
}

GENERATE(long, l)
GENERATE(long long, ll)
#undef GENERATE
#endif

#endif // STDLIB_IMPL_H
