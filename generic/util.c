#include "kernel/util.h"
#include <stdbool.h>

extern inline void kernel_DoublyLinkedList_Add(kernel_DoublyLinkedList *, kernel_DoublyLinkedListElement *);
extern inline void kernel_DoublyLinkedList_Remove(kernel_DoublyLinkedList *, kernel_DoublyLinkedListElement *);

#undef kernel_Min
#undef kernel_Max

#define TMINMAX(s, t)\
    extern inline t kernel_Min ## s(t a, t b);\
    extern inline t kernel_Max ## s(t a, t b);\
    extern inline unsigned t kernel_MinU ## s(unsigned t a, unsigned t b);\
    extern inline unsigned t kernel_MaxU ## s(unsigned t a, unsigned t b);

TMINMAX(, int)
TMINMAX(L, long)
TMINMAX(LL, long long)

#undef TMINMAX

#define TMINMAX(bits)\
    extern inline uint ## bits ## _t kernel_MinU ## bits(uint ## bits ## _t a, uint ## bits ## _t b);\
    extern inline uint ## bits ## _t kernel_MaxU ## bits(uint ## bits ## _t a, uint ## bits ## _t b);\
    extern inline  int ## bits ## _t kernel_MinS ## bits( int ## bits ## _t a,  int ## bits ## _t b);\
    extern inline  int ## bits ## _t kernel_MaxS ## bits( int ## bits ## _t a,  int ## bits ## _t b);

TMINMAX(8)
TMINMAX(16)
TMINMAX(32)
TMINMAX(64)

#undef TMINMAX
