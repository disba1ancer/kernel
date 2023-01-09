#ifndef KERNEL_BOOTDATA_H
#define KERNEL_BOOTDATA_H

#include "util.h"
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

KERNEL_STRUCT(kernel_LdrData) {
    alignas(8)
    uint64_t type;
    uint64_t value;
};

enum kernel_LdrDataType {
    kernel_LdrDataType_EntriesCount = 0,
    kernel_LdrDataType_MemoryMap = 1,
    kernel_LdrDataType_VideoMode = 2,
    kernel_LdrDataType_DriveParams = 3,
};

enum kernel_MemoryMapEntryType {
    kernel_MemoryMapEntryType_AvailableMemory = 1,
};

KERNEL_STRUCT(kernel_MemoryMapEntry) {
    alignas(8)
    uint64_t begin;
    uint64_t size;
    uint32_t type;
    uint32_t flags;
};

KERNEL_STRUCT(kernel_MemoryMap) {
    alignas(8)
    uint64_t entries;
    uint64_t count;
    uint64_t allocatedBoundary;
};

#ifdef __cplusplus
}
#endif

#endif // KERNEL_BOOTDATA_H
