// Do not forget add #undef for new #define's
#ifdef __cplusplus
#define _KSTD_EXTERN_BEGIN extern "C" {
#define _KSTD_EXTERN_END }
#define _KSTD_RESTRICT
#define _KSTD_NORETURN [[noreturn]]
#define _KSTD_NOEXCEPT noexcept
#define _KSTD_CONSTEXPR constexpr
#else
#define _KSTD_EXTERN_BEGIN
#define _KSTD_EXTERN_END
#define _KSTD_RESTRICT restrict
#define _KSTD_NORETURN _Noreturn
#define _KSTD_NOEXCEPT
#define _KSTD_CONSTEXPR
#endif
