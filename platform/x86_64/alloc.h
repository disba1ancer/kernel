#ifndef ALLOC_H
#define ALLOC_H

#include "kernel/bootdata.h"

#ifdef __cplusplus
extern "C" {
#endif

KERNEL_STRUCT(PhysicalRange) {
    union {
        uint64_t start;
        int error;
    };
    uint64_t size;
};

KERNEL_STRUCT(VirtualRange) {
    union {
        void* start;
        int error;
    };
    size_t size;
};

KERNEL_STRUCT(PageMM) {
    PhysicalRange (*PAlloc)(PageMM* mm, void* helperPage, size_t size);
    VirtualRange (*VAlloc)(PageMM* mm, void* page, size_t size, int flags, uint64_t pArgs);
};

int kernel_InitAllocator(void);

#ifdef __cplusplus
}
#endif

#endif // ALLOC_H
