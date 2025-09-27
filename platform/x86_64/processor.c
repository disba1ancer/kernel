#include "processor.h"

extern inline uint64_t x86_64_PageEntry_GetAddr(x86_64_PageEntry entry);
extern inline uint64_t x86_64_PageEntry_GetFlags(x86_64_PageEntry entry);
extern inline int x86_64_PageEntry_GetProtectionKey(x86_64_PageEntry entry);
extern inline void x86_64_FlushPageTLB(volatile void* addr);
extern inline void* x86_64_FlushPageTLB2(uintptr_t addr);
extern inline x86_64_PageEntry x86_64_LoadCR3(void);
extern inline void *i686_LoadPointer(i686_RMPtr fptr);
extern inline i686_RMPtr i686_MakeRMPointer(void *ptr);
