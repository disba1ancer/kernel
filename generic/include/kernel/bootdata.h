#ifndef KERNEL_BOOTDATA_H
#define KERNEL_BOOTDATA_H

#include "multilang.h"
#include <stdalign.h>
#include <stdint.h>

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
    kernel_MemoryMapEntryType_AvailableMemory,
    kernel_MemoryMapEntryType_BootReclaimable,
    kernel_MemoryMapEntryType_SystemReclaimable,
    kernel_MemoryMapEntryType_ReservedMemory,
};

KERNEL_STRUCT(kernel_MemoryMapEntry) {
    alignas(8)
    uint64_t begin;
    uint64_t end;
    uint32_t type;
};

KERNEL_STRUCT(kernel_MemoryMap) {
    alignas(8)
    uint64_t entries;
    uint64_t count;
};

#ifdef __cplusplus
}
#endif

#endif // KERNEL_BOOTDATA_H
