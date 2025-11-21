#ifndef KERNEL_UTIL_H
#define KERNEL_UTIL_H

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "multilang.h"

typedef unsigned char byte;

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

#define TMINMAX(s, t)\
    inline t kernel_Min ## s(t a, t b)\
    { return (a < b) ? a : b; }\
    inline t kernel_Max ## s(t a, t b)\
    { return (a > b) ? a : b; }

TMINMAX(, int)
TMINMAX(L, long)
TMINMAX(LL, long long)
TMINMAX(U, unsigned int)
TMINMAX(UL, unsigned long)
TMINMAX(ULL, unsigned long long)

#undef TMINMAX

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

#ifdef __cplusplus
}
#endif

#endif // KERNEL_UTIL_H
