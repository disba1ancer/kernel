#ifndef KERNEL_UTIL_H
#define KERNEL_UTIL_H

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef unsigned char byte;

#define KERNEL_STRUCT(name) \
    typedef struct name name; \
    struct name

#define KERNEL_TYPEDEF_STRUCT(name) \
    typedef struct name name; \
    typedef struct name

#define KERNEL_UNION(name) \
    typedef union name name; \
    union name

#define KERNEL_UNION_STRUCT(name) \
    typedef union name name; \
    typedef union name

KERNEL_STRUCT(kernel_DoublyLinkedListElement) {
    kernel_DoublyLinkedListElement *next;
    kernel_DoublyLinkedListElement *prev;
};

KERNEL_STRUCT(kernel_DoublyLinkedList) {
    kernel_DoublyLinkedListElement *begin;
};

#ifdef __cplusplus
extern "C" {
#endif

inline void kernel_DoublyLinkedList_Add(kernel_DoublyLinkedList *list, kernel_DoublyLinkedListElement *elem)
{
    elem->prev = NULL;
    elem->next = list->begin;
    list->begin = elem;
    if (elem->next != NULL) {
        elem->next->prev = elem;
    }
}

inline void kernel_DoublyLinkedList_Remove(kernel_DoublyLinkedList *list, kernel_DoublyLinkedListElement *elem)
{
    if (elem->prev != NULL) {
        elem->prev->next = elem->next;
    } else {
        list->begin = elem->next;
    }
    if (elem->next != NULL) {
        elem->next->prev = elem->prev;
    }
}

inline int kernel_Log2U64(uint64_t val)
{
#if defined(__clang__) || defined(__GNUC__)
    return __builtin_clzll(val) ^ 63;
#else
    int result = 0;
    while (val != 1) {
        val >>= 1;
        ++result;
    }
    return result;
#endif
}

inline int kernel_Log2U32(uint32_t val)
{
    return kernel_Log2U64(val);
}

#define TMINMAX(s, t)\
    inline t kernel_Min ## s(t a, t b)\
    { return (a < b) ? a : b; }\
    inline t kernel_Max ## s(t a, t b)\
    { return (a > b) ? a : b; }\
    inline unsigned t kernel_MinU ## s(unsigned t a, unsigned t b)\
    { return (a < b) ? a : b; }\
    inline unsigned t kernel_MaxU ## s(unsigned t a, unsigned t b)\
    { return (a > b) ? a : b; }

TMINMAX(, int)
TMINMAX(L, long)
TMINMAX(LL, long long)

#undef TMINMAX

#ifndef __cplusplus

#define kernel_MinG(s, t)\
    t : kernel_Min ## s,\
    unsigned t : kernel_MinU ## s

#define kernel_Min(a, b) _Generic((a)+(b),\
    kernel_MinG(, int),\
    kernel_MinG(L, long),\
    kernel_MinG(LL, long long)\
    )(a, b)

#define kernel_MaxG(s, t)\
    t : kernel_Min ## s,\
    unsigned t : kernel_MinU ## s

#define kernel_Max(a, b) _Generic((a)+(b),\
    kernel_MaxG(, int),\
    kernel_MaxG(L, long),\
    kernel_MaxG(LL, long long)\
    )(a, b)

#endif

#define TMINMAX(bits)\
    inline uint##bits##_t kernel_MinU##bits(uint##bits##_t a, uint##bits##_t b)\
    { return (a < b) ? a : b; }\
    inline uint##bits##_t kernel_MaxU##bits(uint##bits##_t a, uint##bits##_t b)\
    { return (a > b) ? a : b; }\
    inline int##bits##_t kernel_MinS##bits(int##bits##_t a, int##bits##_t b)\
    { return (a < b) ? a : b; }\
    inline int##bits##_t kernel_MaxS##bits(int##bits##_t a, int##bits##_t b)\
    { return (a > b) ? a : b; }

TMINMAX(8)
TMINMAX(16)
TMINMAX(32)
TMINMAX(64)

#undef TMINMAX

size_t kernel_ULLToStr(char* str, size_t size, unsigned long long num, int radix);
size_t kernel_ULToStr(char* str, size_t size, unsigned long num, int radix);
size_t kernel_UToStr(char* str, size_t size, unsigned num, int radix);

#ifndef __cplusplus
#define kernel_UToStr(str, size, num, radix) _Generic(num,\
    unsigned long long : kernel_ULLToStr(str, size, num, radix),\
    unsigned long : kernel_ULToStr(str, size, num, radix),\
    unsigned : kernel_UToStr(str, size, num, radix))
#endif

#ifdef __cplusplus
} // extern "C"

#include <concepts>

namespace kernel {

#define NS_FUNC(prefix, name) inline constexpr auto& name = :: prefix ## name;

NS_FUNC(kernel_, DoublyLinkedList_Add)
NS_FUNC(kernel_, DoublyLinkedList_Remove)

template <typename A, typename B>
requires (requires(A a, B b) { { a < b } -> std::convertible_to<bool>; })
constexpr auto Min(const A& a, const B& b) -> decltype(a + b) {
    using T = decltype(a + b);
    return (T(a) < T(b)) ? a : b;
}

template <typename A, typename B>
requires (requires(A a, B b) { { a > b } -> std::convertible_to<bool>; } )
constexpr auto Max(const A& a, const B& b) -> decltype(a + b) {
    using T = decltype(a + b);
    return (T(a) > T(b)) ? a : b;
}

#define TMINMAX(bits)\
    NS_FUNC(kernel_, MinU##bits)\
    NS_FUNC(kernel_, MaxU##bits)\
    NS_FUNC(kernel_, MinS##bits)\
    NS_FUNC(kernel_, MaxS##bits)

TMINMAX(8)
TMINMAX(16)
TMINMAX(32)
TMINMAX(64)

#undef TMINMAX

NS_FUNC(kernel_, ULLToStr)
NS_FUNC(kernel_, ULToStr)

inline size_t UToStr(char* str, size_t size, unsigned num, int radix) {
    return kernel_UToStr(str, size, num, radix);
}

inline size_t UToStr(char* str, size_t size, unsigned long num, int radix) {
    return kernel_ULToStr(str, size, num, radix);
}

inline size_t UToStr(char* str, size_t size, unsigned long long num, int radix) {
    return kernel_ULLToStr(str, size, num, radix);
}

#undef NS_FUNC

}

#endif

#endif // KERNEL_UTIL_H
