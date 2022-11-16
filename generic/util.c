#include "kernel/util.h"
#include <stdbool.h>

extern inline void kernel_DoublyLinkedList_Add(kernel_DoublyLinkedList *, kernel_DoublyLinkedListElement *);
extern inline void kernel_DoublyLinkedList_Remove(kernel_DoublyLinkedList *, kernel_DoublyLinkedListElement *);

extern inline int  kernel_Log2U64(uint64_t val);
extern inline int  kernel_Log2U32(uint32_t val);

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

static void PutForward(char* str, size_t size, int ch, size_t* off)
{
    char* end = str + size;
    ++*off;
    if (*off <= size) {
        *(end - *off) = (char)ch;
    }
}

static void PutNumForward(char* str, size_t size, int ch, size_t* off)
{
    PutForward(str, size, ch < 10 ? ch + '0' : ch + 'A' - 10, off);
}

static void PutZero(char* str, size_t size, size_t* off)
{
    PutForward(str, size, '0', off);
}

#undef kernel_UToStr

#define GENERATE(p, t) \
static void p##ToStr8(char* str, size_t size, t num, size_t* off) {\
    while (num) {\
        PutNumForward(str, size, num & 0x7, off);\
        num = num >> 3;\
    }\
}\
\
static void p##ToStr10(char* str, size_t size, t num, size_t* off) {\
    while (num) {\
        PutNumForward(str, size, num % 10, off);\
        num = num / 10;\
    }\
}\
\
static void p##ToStr16(char* str, size_t size, t num, size_t* off) {\
    while (num) {\
        PutNumForward(str, size, num & 0xF, off);\
        num = num >> 4;\
    }\
}\
\
static void p##ToStrGen(char* str, size_t size, t num, int radix, size_t* off) {\
    while (num) {\
        PutNumForward(str, size, (int)(num % (unsigned)radix), off);\
        num = num / (unsigned)radix;\
    }\
}\
\
size_t kernel_##p##ToStr(char* str, size_t size, t num, int radix) {\
    size_t result = 0;\
    PutForward(str, size, 0, &result);\
    if (num == 0) {\
        PutZero(str, size, &result);\
    } else {\
        switch (radix) {\
            case 8:\
                p##ToStr8(str, size, num, &result);\
                break;\
            case 10:\
                p##ToStr10(str, size, num, &result);\
                break;\
            case 16:\
                p##ToStr16(str, size, num, &result);\
                break;\
            default:\
                p##ToStrGen(str, size, num, radix, &result);\
                break;\
        }\
    }\
    if (result <= size) {\
        memmove(str, str + size - result, result);\
    }\
    return result - 1;\
}

GENERATE(ULL, unsigned long long)
GENERATE(UL, unsigned long)
GENERATE(U, unsigned)
#undef GENERATE
