#include <stdlib.h>
#include "kernel/bootdata.h"
#include "processor.h"

extern x86_64_PageEntry __mapping_window[512];
extern byte __first_free_page[];

static const kernel_MemoryMap *FindMemoryMap(const kernel_LdrData* data)
{
    size_t entriesCount = (size_t)data->value;
    for (size_t i = 1; i < entriesCount; ++i) {
        if (data[i].type == kernel_LdrDataType_MemoryMap) {
            return (void*)(uintptr_t)data[i].value;
        }
    }
    return NULL;
}

static const kernel_MemoryMapEntry* FindFirstAvailableEntry(
    const kernel_MemoryMapEntry* entries, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].type == kernel_MemoryMapEntryType_AvailableMemory &&
            (entries[i].flags & 0xF) == 1)
        {
            return entries + i;
        }
    }
    return NULL;
}

const kernel_MemoryMap* kernel_AllocateInitialStack(const kernel_LdrData* data)
{
    const kernel_MemoryMap* memmap = FindMemoryMap(data);
    if (memmap == NULL || memmap->allocatedBoundary == 0) {
        return NULL;
    }
    const kernel_MemoryMapEntry
        *entries = (void*)(uintptr_t)memmap->entries,
        *entry = FindFirstAvailableEntry(entries, (size_t)memmap->count);
    if (entry == NULL) {
        return NULL;
    }
    for (; entry != entries + (size_t)memmap->count; ++entry)
    {
        uint64_t diff = memmap->allocatedBoundary - entry->begin;
        if (entry->begin <= memmap->allocatedBoundary && diff < entry->size) {
            __mapping_window[509] = (x86_64_PageEntry)x86_64_MakePageEntry(
                memmap->allocatedBoundary,
                i686_PageEntryFlag_Present | i686_PageEntryFlag_Write);
            return memmap;
        }
    }
    return NULL;
}

void* malloc(size_t size)
{
    (void)size;
    return NULL;
}

void free(void* ptr)
{
    (void)ptr;
}

void* realloc(void* ptr, size_t size)
{
    (void)ptr;
    (void)size;
    return NULL;
}
