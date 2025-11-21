#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <cstdint>

namespace i686 {

enum SegTypeFlag {
    SegType_TSS16 = 1 << 8,
    SegType_TSS32 = 9 << 8,
    SegType_TSSCommon = 1 << 8,
    SegType_TSSMask = 0x15 << 8,
    SegType_BusyTSS = 2 << 8,

    SegType_LDT = 2 << 8,

    SegType_ReadWrite = 0x12 << 8,
    SegType_Read = 0x10 << 8,
    SegType_Expandown = 4 << 8,

    SegType_ExecRead = 0x1A << 8,
    SegType_Exec = 0x18 << 8,
    SegType_Conforming = 4 << 8,

    SegType_Accessed = 1 << 8,
};

enum GateType {
    GateType_G32 = 8 << 8,
    GateType_Call = 0x14 << 8,
    GateType_Task = 0x15 << 8,
    GateType_Int = 0x16 << 8,
    GateType_Trap = 0x17 << 8,
    GateType_Mask = 0x14 << 8,
    GateType_Common = 0x4 << 8,
};

enum DescFlag {
    DescFlag_UserFlag = 1 << 20,
    DescFlag_Long = 2 << 20,
    DescFlag_OP32 = 4 << 20,
    DescFlag_LimitIn4K = 8 << 20,

    DescFlag_Present = 1 << 15,
};

enum Interrupt {
    Interrupt_DE = 0,
    Interrupt_DB = 1,
    Interrupt_NMI = 2,
    Interrupt_BP = 3,
    Interrupt_OF = 4,
    Interrupt_BR = 5,
    Interrupt_UD = 6,
    Interrupt_NM = 7,
    Interrupt_DF = 8,
    //  Interrupt_?? = 9,
    Interrupt_TS = 10,
    Interrupt_NP = 11,
    Interrupt_SS = 12,
    Interrupt_GP = 13,
    Interrupt_PF = 14,
    //  Interrupt_?? = 15,
    Interrupt_MF = 16,
    Interrupt_AC = 17,
    Interrupt_MC = 18,
    Interrupt_XM = 19,
    Interrupt_VE = 20,
};

struct Descriptor {
    uint32_t low;
    uint32_t high;
};

#define i686_internal_MakeSegDescriptor(base, limit, dpl, typeflags) {\
    .low = (((uint32_t)(base) & 0xFFFFU) << 16U) | ((uint32_t)(limit) & 0xFFFFU),\
    .high = (((uint32_t)(base) >> 16U) & 0xFFU) | ((uint32_t)(base) & 0xFF000000U) | ((uint32_t)(limit) & 0xF0000U) | \
    ((uint32_t)(typeflags) & 0xF09F00) | (((uint32_t)(dpl) & 3) << 13) }

#define i686_internal_MakeGateDescriptor(offset, segsel, dpl, typeflags) {\
    .low = ((uint32_t)(offset) & 0xFFFFU) | (((uint32_t)(segsel) & 0xFFFFU) << 16U),\
    .high = ((uint32_t)(offset) & 0xFFFF0000U) | \
    ((uint32_t)(typeflags) & 0x8B00) | GateType_Common | (((uint32_t)(dpl) & 3) << 13) }

constexpr inline Descriptor MakeSegDescriptor(uint32_t base, uint32_t limit, uint32_t dpl, uint32_t typeflags)
{
    return i686_internal_MakeSegDescriptor(base, limit, dpl, typeflags);
}
constexpr inline Descriptor MakeGateDescriptor(uint32_t offset, uint32_t segsel, uint32_t dpl, uint32_t typeflags)
{
    return i686_internal_MakeGateDescriptor(offset, segsel, dpl, typeflags);
}
#undef i686_internal_MakeSegDescriptor
#undef i686_internal_MakeGateDescriptor

enum PageEntryFlag {
    PageEntryFlag_Present = 1 << 0,
    PageEntryFlag_Write = 1 << 1,
    PageEntryFlag_User = 1 << 2,
    PageEntryFlag_PWT = 1 << 3, // Cache write through
    PageEntryFlag_PCD = 1 << 4, // Disable cache
    PageEntryFlag_Accessed = 1 << 5,
    PageEntryFlag_Dirty = 1 << 6,
    PageEntryFlag_PAT = 1 << 7,
    PageEntryFlag_Global = 1 << 8,
};

struct PageEntry {
    uint32_t data;
};

#define i686_internal_MakePageEntry(phyPage, flags) { ((phyPage) & 0xFFFFF000U) | ((flags) & 0xFFFU) }

constexpr inline PageEntry MakePageEntry(uint32_t phyPage, uint32_t flags)
{
    return i686_internal_MakePageEntry(phyPage, flags);
}
#undef i686_internal_MakePageEntry

struct InterruptFrame {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
};

struct tss {
    uint32_t prevTask;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtSelector;
    uint16_t flags;
    uint16_t ioMap;
};

struct RMPtr {
    uint16_t ptr;
    uint16_t seg;
};

inline void *LoadPointer(RMPtr fptr) {
    uintptr_t ptr = fptr.seg;
    ptr = (ptr << 4) + fptr.ptr;
    return (void*)ptr;
}

inline RMPtr MakeRMPointer(void *ptr) {
    uintptr_t fptr = (uintptr_t)ptr;
    RMPtr rslt = { .ptr = (uint16_t)(fptr & 0xF), .seg = (uint16_t)(fptr >> 4) };
    return rslt;
}

template <typename T>
T* LoadPointer(RMPtr fptr) {
    return static_cast<T*>(LoadPointer(fptr));
}

} // namespace i686

namespace x86_64 {

enum PageEntryFlag {
    PageEntryFlag_Present = 1,
    PageEntryFlag_Write = 2,
    PageEntryFlag_User = 4,
    PageEntryFlag_PWT = 8, // Cache write through
    PageEntryFlag_PCD = 16, // Disable cache
    PageEntryFlag_Accessed = 32,
    PageEntryFlag_Dirty = 64,
    PageEntryFlag_PAT = 128,
    PageEntryFlag_Global = 256,
    PageEntryFlag_ExecDisable = 0x8000000000000000U
};

struct alignas(8) PageEntry {
    uint64_t data;
};

#define x86_64_internal_MakePageEntry(phyPage, flags, pk) { \
    ((phyPage) & 0x07FFFFFFFFFFF000U) | \
    (flags & 0x8000000000000FFF) | \
    ((uint64_t)(pk) & 0xF) << 59 }

constexpr inline PageEntry MakePageEntry(uint64_t phyPage, uint64_t flags, int pk = 0)
{
    return x86_64_internal_MakePageEntry(phyPage, flags, pk);
}
#undef x86_64_internal_MakePageEntry

inline uint64_t PageEntry_GetAddr(PageEntry entry)
{
    return entry.data & 0x07FFFFFFFFFFF000U;
}

inline uint64_t PageEntry_GetFlags(PageEntry entry)
{
    return entry.data & 0x8000000000000FFFU;
}

inline int PageEntry_GetProtectionKey(PageEntry entry)
{
    return (entry.data >> 59) & 0xFU;
}

inline void FlushPageTLB(volatile void* addr)
{
    __asm__ volatile("invlpg (%0)"::"r"(addr):"memory");
}

inline void* FlushPageTLB2(uintptr_t addr)
{
    void* result;
    __asm__ volatile(
        "invlpg (%1)\n"
        "movq %1, %0"
        :"=r"(result):"r"(addr):"memory");
    return result;
}

inline PageEntry LoadCR3(void)
{
    PageEntry r;
    __asm__("mov %%cr3, %0":"=r"(r.data));
    return r;
}

struct GDTR {
    uint32_t rsv0;
    uint16_t rsv1;
    uint16_t limit;
    i686::Descriptor* gdt;
};

inline void LoadGDT(GDTR *ptr) {
    __asm__ volatile("lgdt 6(%0)"::"r"(ptr));
}

} // namespace x86_64

#endif // PROCESSOR_H
