#ifndef ALLOC_H
#define ALLOC_H

#include "kernel/bootdata.h"

void kernel_InitAllocators(const kernel_MemoryMap* memmap);

#endif // ALLOC_H
