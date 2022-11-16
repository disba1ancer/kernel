// Do not forget add #undef for new #define's
#ifdef __cplusplus
extern "C" {
#define KSTD_RESTRICT
#define KSTD_NORETURN [[noreturn]]
#define KSTD_NOEXCEPT noexcept
#else
#define KSTD_RESTRICT restrict
#define KSTD_NORETURN _Noreturn
#define KSTD_NOEXCEPT
#endif
