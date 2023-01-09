/**
 * TODO:
 * Reserve - reserves virtual memory for arbitrary mapping
 * Free - frees reserved and mapped memory
 * Map - maps arbitrary physical memory or free physical RAM to specified virtual memory
 * Unmap - unmap physical memory from specified virtual memory, frees physical RAM if it was mapped
 *
 * enum FlagsGeneric {
 * Reserve,
 * Read,
 * Write,
 * Execute
 * };
 *
 * enum FlagsArch {
 * Specified, // Map specified physical address
 * WriteBack,
 * WriteCombine,
 * WriteProtect,
 * WriteThrough,
 * NoCache,
 * };
 *
 * enum FreeFrags {
 * Reserve
 * };
 *
 * VirtualRange Alloc(void* vAddr, size_t size, enum FlagsGeneric flags, uint64_t pArgs)
 * void Free(void* vAddr, size_t size, enum FlagsGeneric flags);
 *
 * struct PageMM {
 * PhysicalRegion (*PAlloc)(PageMM* mm, size_t count);
 * VirtualRegion (*VAlloc)(PageMM* mm, void* page, size_t count, int flags, uint64_t pArgs);
 * };
 *
 * IGNORE UNDEFINED BEHAVIOR WITH POINTERS
 */

#include <cstdlib>
#include <climits>
#include <limits>
#include <exception>
#include "alloc.h"
#include "kernel/bootdata.h"
#include "processor.h"
#include <kernel/UniquePtr.hpp>
#include <kernel/util.hpp>
#include <kernel/list.h>
#include <kernel/avl_tree.h>
#include <atomic>

namespace {

constexpr auto BasicPageMask = 4095;
constexpr auto BasicPageSize = 4096;

using kernel::ptr_cast;
using kernel::As;

template <typename T>
constexpr bool ValWithinRange(auto val)
{
    using nl = std::numeric_limits<T>;
    return nl::min() <= val && val <= nl::max();
}

template <long long val>
class LeastTypeByValue {
    static constexpr bool c = ValWithinRange<signed char>(val);
    static constexpr bool s = c || ValWithinRange<short>(val);
    static constexpr bool i = s || ValWithinRange<int>(val);
    static constexpr bool l = i || ValWithinRange<long>(val);
    static constexpr bool ll = l || ValWithinRange<long long>(val);
    using t1 = std::conditional_t<ll, long long, void>;
    using t2 = std::conditional_t<l, long, t1>;
    using t3 = std::conditional_t<i, int, t2>;
    using t4 = std::conditional_t<s, short, t3>;
public:
    using Type = std::conditional_t<c, signed char, t4>;
};

template <long long val>
using LeastTypeByValueT = typename LeastTypeByValue<val>::Type;

template <unsigned long long val>
class LeastUTypeByValue {
    static constexpr bool c = ValWithinRange<unsigned char>(val);
    static constexpr bool s = c || ValWithinRange<unsigned short>(val);
    static constexpr bool i = s || ValWithinRange<unsigned int>(val);
    static constexpr bool l = i || ValWithinRange<unsigned long>(val);
    static constexpr bool ll = l || ValWithinRange<unsigned long long>(val);
    using t1 = std::conditional_t<ll, unsigned long long, void>;
    using t2 = std::conditional_t<l, unsigned long, t1>;
    using t3 = std::conditional_t<i, unsigned int, t2>;
    using t4 = std::conditional_t<s, unsigned short, t3>;
public:
    using Type = std::conditional_t<c, unsigned char, t4>;
};

template <unsigned long long val>
using LeastUTypeByValueT = typename LeastUTypeByValue<val>::Type;

template <typename T>
constexpr
auto AlignDown(T s, T align) -> T
{
    return s ^ (s & (align - 1));
}

template <typename T>
constexpr
auto AlignUp(T s, T align) -> T
{
    return AlignDown(s + align - 1, align);
}

auto AlignDownPtr(void* s, std::size_t align) -> void*
{
    auto mask = align - 1;
    auto offset = ptr_cast<std::uintptr_t>(s) & mask;
    auto bptr = As<std::byte*>(s) - offset;
    return bptr;
}

template <typename A, typename B>
struct MaxAlign {
    static constexpr
    auto Value = kernel::Max(alignof(A), alignof(B));
};

template <typename A, typename B>
constexpr auto MaxAlignV = MaxAlign<A, B>::Value;

template <typename A, typename B>
struct MaxSize {
    static constexpr
    auto Value = kernel::Max(sizeof(A), sizeof(B));
};

template <typename A, typename B>
constexpr auto MaxSizeV = MaxSize<A, B>::Value;

auto FindMemoryMap(const kernel_LdrData* data) -> const kernel_MemoryMap*
{
    auto entriesCount = (size_t)data->value;
    for (size_t i = 1; i < entriesCount; ++i) {
        if (data[i].type == kernel_LdrDataType_MemoryMap) {
            return ptr_cast<const kernel_MemoryMap*>(data[i].value);
        }
    }
    return NULL;
}

auto FindFirstAvailableEntry(const kernel_MemoryMapEntry* entries, size_t count)
    -> const kernel_MemoryMapEntry*
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

auto FindBoundaryEntry(const kernel_MemoryMapEntry* entries, uint64_t boundary)
    -> std::size_t
{
    auto entry = entries;
    while (!(boundary - entry->begin <= entry->size)) {
        ++entry;
    }
    return std::size_t(entry - entries);
}

}

extern x86_64_PageEntry __mapping_window_entry;
extern byte __bss_end[];
extern byte __init_stack_start[];

extern "C" const kernel_LdrData* kernel_LoaderData;

enum PhyAllocError {
    PhyAllocError_SuccessEmpty,
    PhyAllocError_NoMem,
    PhyAllocError_OutOfAddressSpace,
};

enum FlagsGeneric {
    FlagsGeneric_Reserve = 1,
    FlagsGeneric_Read,
    FlagsGeneric_Write = 4,
    FlagsGeneric_Execute = 8
};

enum FlagsInternal {
    FlagsInternal_Present = 1,
    FlagsInternal_Kernel,
};

template <typename T, std::size_t size>
requires(sizeof(T) < size && alignof(T) < size)
class ChunkedAllocator {
    using ValueType = T;
    static constexpr
    auto StorageUnitAlign = MaxAlignV<T, std::size_t>;
    static constexpr
    auto StorageUnitSize = AlignUp(MaxSizeV<T, std::size_t>, StorageUnitAlign);
    struct ChunkHeader {
        ChunkHeader()
        {
            auto ptr = GetStorage();
            std::size_t i = 0;
            while (i != StorageUnitCount - 1) {
                new(ptr) std::size_t(++i);
                ptr += StorageUnitSize;
            }
            new(ptr) std::size_t(std::size_t(-1));
        }

        T* Alloc()
        {
            if (Full()) {
                return nullptr;
            }
            auto allocElem = GetStorageUnit(brk);
            brk = *As<std::size_t*>(allocElem);
            ++count;
            allocElem = new(allocElem) std::byte[StorageUnitSize];
            return *As<T(*)[1]>(allocElem);
        }

        void Free(T* elem)
        {
            auto freeElem = As<std::byte*>(elem);
            auto freeElemNum = GetStorageUnitNum(freeElem);
            new(freeElem) std::size_t(brk);
            brk = freeElemNum;
            --count;
        }

        auto GetStorage() -> std::byte*
        {
            return As<std::byte*>(this) + StorageOffset;
        }

        auto GetStorageUnit(std::size_t n) -> std::byte*
        {
            return GetStorage() + n * StorageUnitSize;
        }

        auto GetStorageUnitNum(std::byte* unit) -> std::size_t
        {
            return std::size_t(unit - GetStorage()) / StorageUnitSize;
        }

        bool Full()
        {
            return brk == std::size_t(-1);
        }

        bool Empty()
        {
            return count == 0;
        }

        kernel::intrusive::ListNode<> elem;
        std::size_t brk = 0;
        std::size_t count = 0;
    };
    static constexpr
    auto StorageOffset = AlignUp(sizeof(ChunkHeader), StorageUnitAlign);
    static constexpr
    auto StorageUnitCount = (size - StorageOffset) / StorageUnitSize;
    static constexpr
    auto offset = offsetof(ChunkHeader, elem);
    using CastPolicy = kernel::intrusive::OffsetCastPolicy<offset>;
    using List = kernel::intrusive::List<ChunkHeader, CastPolicy>;
public:
    ChunkedAllocator(void* initialPage) :
        lastUsed(new(initialPage) ChunkHeader{})
    {
        storageList.PushBack(*lastUsed);
    }

    T* Alloc()
    {
        if (!lastUsed->Full()) {
            return lastUsed->Alloc();
        }
        for (auto& storage : storageList) {
            if (!storage.Full()) {
                lastUsed = &storage;
                return lastUsed->Alloc();
            }
        }
        // TODO: Add function for chunk allocation
        return nullptr;
    }

    void Free(T* elem)
    {
        if (elem == nullptr) {
            return;
        }
        auto storage = As<ChunkHeader*>(AlignDownPtr(elem, size));
        storage->Free(elem);
        if (lastUsed != storage && lastUsed->Empty()) {
            storageList.Erase(*lastUsed);
            // TODO: Add function for chunk deallocation
            lastUsed = storage;
        }
    }
private:
    List storageList;
    ChunkHeader* lastUsed;
};

struct Test {

    struct Node {
        using AVLTreeNode = kernel::intrusive::AVLTreeNode<>;
        AVLTreeNode addrNode;
        AVLTreeNode sizeNode;
        uintptr_t addr;
        size_t size;
    };

    ChunkedAllocator<Node, 4096> storage;
};

class VMMFreeList {
private:
    struct Node {
        using AVLTreeNode = kernel::intrusive::AVLTreeNode<>;
        AVLTreeNode addrNode;
        AVLTreeNode sizeNode;
        uintptr_t addr;
        size_t size;
    };

    using Allocator = ChunkedAllocator<Node, 4096>;

    template <typename ... A>
    using AVLTree = kernel::intrusive::AVLTree<A...>;
    template <auto mptr>
    struct FieldComparator {
        bool operator()(const Node& a, const Node& b) const noexcept
        {
            return (a.*mptr) < (b.*mptr);
        }
    };
    static constexpr
    auto addrOffset = offsetof(Node, addrNode);
    using AddrCastPol = kernel::intrusive::OffsetCastPolicy<addrOffset>;
    static constexpr
    auto sizeOffset = offsetof(Node, sizeNode);
    using SizeCastPol = kernel::intrusive::OffsetCastPolicy<sizeOffset>;
    using AVLAddrTree =
        kernel::intrusive::AVLTree<Node, FieldComparator<&Node::addr>, AddrCastPol>;
    using AVLSizeTree =
        kernel::intrusive::AVLTree<Node, FieldComparator<&Node::size>, SizeCastPol>;
public:
    struct MemoryRange {
        uintptr_t begin;
        size_t size;
    };

    VMMFreeList(void* initialPage) :
        alloc(initialPage)
    {}

    void ReleaseRange(MemoryRange r)
    {
        std::uintptr_t end = AlignAndCalcEnd(r);
        end = kernel::Max(r.begin, end);
        if (r.begin == end) {
            return;
        }
        if (addrList.Empty()) {
            AddMemoryRegion(r.begin, r.size);
            return;
        }
        auto crBegin = addrList.UpperBound(r.begin);
        auto crEnd = addrList.LowerBound(end);
        if (crBegin != addrList.Begin()) {
            --crBegin;
        }
        int count = CountMerges(crBegin, crEnd, r);
        if (count > 1 || IsIntersect(*crBegin, r)) {
            return;
        }
        std::uintptr_t rBeginEnd = crBegin->addr + crBegin->size;
        if (count == 0 && crEnd != addrList.End() && crEnd->addr == end) {
            sizeList.Erase(*crEnd);
            crEnd->addr = r.begin;
            crEnd->size += r.size;
            sizeList.Insert(*crEnd);
            return;
        }
        if (rBeginEnd == r.begin) {
            sizeList.Erase(*crBegin);
            crBegin->size = end - crBegin->addr;
            Node* node = nullptr;
            if (crEnd != addrList.End() && crEnd->addr == end) {
                crBegin->size += crEnd->size;
                node = crEnd.operator->();
                Erase(*node);
            }
            sizeList.Insert(*crBegin);
            alloc.Free(node);
            return;
        }
        AddMemoryRegion(r.begin, r.size);
    }

    auto AcquireRange(std::size_t size) -> MemoryRange
    {
        size = AlignUp<std::size_t>(size, BasicPageSize);
        auto crBegin = sizeList.LowerBound(size);
        auto crEnd = sizeList.UpperBound(size);
        if (crBegin != crEnd) {
            auto node = crBegin.operator->();
            return AcquireIdealMatch(node);
        }
        if (crEnd == sizeList.End()) {
            return { 0, 0 };
        }
        auto& node = *crEnd;
        MemoryRange result = {node.addr, size};
        node.addr += size;
        sizeList.Erase(node);
        node.size -= size;
        sizeList.Insert(node);
        return result;
    }

    bool AcquireRangeAt(MemoryRange r)
    {
        std::uintptr_t rEnd = AlignAndCalcEnd(r);
        auto cr = addrList.UpperBound(r.size);
        if (cr == addrList.Begin()) {
            return false;
        }
        --cr;
        if (!Contains(*cr, r)) {
            return false;
        }
        std::uintptr_t crEnd = cr->addr + cr->size;
        auto t = (cr->addr == r.begin) * 2 + (crEnd == rEnd);
        switch (t) {
            case 0b00:
                sizeList.Erase(*cr);
                cr->size = r.begin - cr->addr;
                sizeList.Insert(*cr);
                AddMemoryRegion(rEnd, crEnd - rEnd);
                break;
            case 0b01:
                sizeList.Erase(*cr);
                cr->size -= r.size;
                sizeList.Insert(*cr);
                [[fallthrough]];
            case 0b10:
                cr->addr += r.size;
                break;
            case 0b11:
                AcquireIdealMatch(cr.operator->());
                break;
        }
        return true;
    }

    auto AlignAndCalcEnd(MemoryRange& r) -> std::uintptr_t
    {
        std::uintptr_t end = r.begin + r.size;
        r.begin = AlignDown<std::uintptr_t>(r.begin, BasicPageSize);
        end = AlignUp<std::uintptr_t>(end, BasicPageSize);
        r.size = end - r.begin;
        return end;
    }
private:
    auto AcquireIdealMatch(Node* node) -> MemoryRange
    {
        MemoryRange result = {node->addr, node->size};
        Erase(*node);
        alloc.Free(node);
        return result;
    }

    void AddMemoryRegion(std::uintptr_t addr, std::size_t size)
    {
        auto node = alloc.Alloc();
        node->addr = addr;
        node->size = size;
        Insert(*node);
    }

    int CountMerges(
        AVLAddrTree::Iterator rBegin, AVLAddrTree::Iterator rEnd, MemoryRange r)
    {
        std::uintptr_t end = r.begin + r.size;
        end = kernel::Max(r.begin, end);
        auto listBegin = addrList.Begin();
        if (rBegin != listBegin) {
            --rBegin;
        }
        int count = 0;
        for (auto it = rBegin; it != rEnd; ++it) {
            auto itEnd = it->addr + it->size;
            count += (r.begin <= itEnd && end >= it->addr);
            if (count > 1) {
                return count;
            }
        }
        return count;
    }

    static bool IsIntersect(Node& node, MemoryRange r)
    {
        std::uintptr_t nodeEnd = node.addr + node.size;
        std::uintptr_t rEnd = r.begin + r.size;
        return node.addr < rEnd && nodeEnd > r.begin;
    }

    static bool Contains(Node& node, MemoryRange r)
    {
        std::uintptr_t nodeEnd = node.addr + node.size;
        std::uintptr_t rEnd = r.begin + r.size;
        return node.addr <= r.begin && rEnd <= nodeEnd;
    }

    void Insert(Node& node)
    {
        addrList.Insert(node);
        sizeList.Insert(node);
    }

    void Erase(Node& node)
    {
        addrList.Erase(node);
        sizeList.Erase(node);
    }

    Allocator alloc;
    AVLAddrTree addrList;
    AVLSizeTree sizeList;
};

class BuddyMemoryManager {

};

class MemoryMapper {
public:
    static constexpr
    auto EntriesPerPageLog = 9;
    static constexpr
    auto EntriesPerPage = 1 << EntriesPerPageLog;
    static constexpr
    auto PageDirAddr = uintptr_t(-0x800000000000);
    static constexpr
    auto PageTableSize =
        std::uintptr_t(EntriesPerPage) *
        EntriesPerPage *
        EntriesPerPage *
        EntriesPerPage;
    static constexpr
    auto PageTableMask = PageTableSize - 1;
    static constexpr
    auto PageTableLevels = 4;
    static constexpr
    auto SpecialIndex = 0400400400400u;
    static constexpr
    auto FlagsAllAccess = x86_64_PageEntryFlag_Present |
            x86_64_PageEntryFlag_Write;
    static void Init()
    {
        constexpr auto windowAddress = std::uintptr_t(-0x200000);
        x86_64_PageEntry entry = x86_64_LoadCR3();
        entry = x86_64_MakePageEntry(
            x86_64_PageEntry_GetAddr(entry),
            x86_64_PageEntryFlag_Present | x86_64_PageEntryFlag_Write,
            0
        );
        __mapping_window_entry = entry;
        __asm__ volatile("":::"memory");
        auto localPagetable =
            std::launder(ptr_cast<x86_64_PageEntry*>(windowAddress));
        localPagetable[128] = entry;
        __asm__ volatile("":::"memory");
        __mapping_window_entry = {0};
        x86_64_FlushPageTLB(localPagetable);
        RemapPageUnsafe(&__mapping_window_entry);
    }

    static void* GetPageByIndex(uintptr_t index)
    {
        return ptr_cast<void*>(index * BasicPageSize);
    }

    static auto GetPageEntry(size_t index) -> x86_64_PageEntry*
    {
        return *As<x86_64_PageEntry(*)[]>(
            ptr_cast<void*>(PageDirAddr)) + index;
    }

    static auto GetPageEntryByPtr(const void* ptr) -> x86_64_PageEntry*
    {
        return GetPageEntry((ptr_cast<uintptr_t>(ptr) / BasicPageSize) &
            PageTableMask);
    }

    static int ToPlatformFlags(int flags, int intFlags)
    {
        bool read = flags & FlagsGeneric_Read;
        bool write = flags & FlagsGeneric_Write;
        bool execute = flags & FlagsGeneric_Execute;
        bool present = intFlags & FlagsInternal_Present;
        bool kernel = intFlags & FlagsInternal_Kernel;
        int result =
            x86_64_PageEntryFlag_Write * write |
            x86_64_PageEntryFlag_ExecDisable * !execute |
            x86_64_PageEntryFlag_User * !kernel |
            x86_64_PageEntryFlag_Present * present;
        return result;
    }

    static
    void MapPageUnsafe(void* vAddr, int flags, std::uint64_t pAddr)
    {
        x86_64_PageEntry* pe = GetPageEntryByPtr(vAddr);
        *pe = x86_64_MakePageEntry(pAddr, flags, 0);
        __asm__ volatile("":::"memory");
    }

    static
    auto RemapPageUnsafe(void* vAddr, int flags = 0, std::uint64_t pAddr = 0)
        -> std::uint64_t
    {
        x86_64_PageEntry* pe = GetPageEntryByPtr(vAddr);
        auto result = x86_64_PageEntry_GetAddr(*pe);
        *pe = x86_64_MakePageEntry(pAddr, flags, 0);
        x86_64_FlushPageTLB(vAddr);
        return result;
    }

    static auto SwapPages(void* vAddr, x86_64_PageEntry n) -> x86_64_PageEntry
    {
        x86_64_PageEntry* pe = GetPageEntryByPtr(vAddr);
        auto result = *pe;
        *pe = n;
        x86_64_FlushPageTLB(vAddr);
        return result;
    }

    static void MapPageTablePage(void* vAddr, std::uint64_t pAddr)
    {
        int flags =
            x86_64_PageEntryFlag_Present |
            x86_64_PageEntryFlag_Write;
        MapPageUnsafe(vAddr, flags, pAddr);
        x86_64_FlushPageTLB(vAddr);
    }

    static void* MapPage(void* vAddr, int flags, std::uint64_t pAddr)
    {
        auto pe = GetPageEntryByPtr(vAddr);
        auto pde = GetPageEntryByPtr(pe);
        auto pdptre = GetPageEntryByPtr(pde);
        auto pml4e = GetPageEntryByPtr(pdptre);
        if (!(x86_64_PageEntry_GetFlags(*pml4e) & x86_64_PageEntryFlag_Present)) {
            return pdptre;
        }
        if (!(x86_64_PageEntry_GetFlags(*pdptre) & x86_64_PageEntryFlag_Present)) {
            return pde;
        }
        if (!(x86_64_PageEntry_GetFlags(*pde) & x86_64_PageEntryFlag_Present)) {
            return pe;
        }
        *pe = x86_64_MakePageEntry(pAddr, flags, 0);
        x86_64_FlushPageTLB(vAddr);
        return vAddr;
    }

    static bool IsPagePresent(x86_64_PageEntry* entry)
    {
        return x86_64_PageEntry_GetFlags(*entry) & x86_64_PageEntryFlag_Present;
    }

    static bool IsLeastOnePresent(x86_64_PageEntry* eBeg, x86_64_PageEntry* eEnd)
    {
        for (auto i = eBeg; i != eEnd; ++i) {
            if (IsPagePresent(i)) {
                return true;
            }
        }
        return false;
    }

    static unsigned IsPageNotPresent(x86_64_PageEntry* entry,
        x86_64_PageEntry* entryCache[])
    {
        entryCache[0] = entry;
        for (int i = 1; i < PageTableLevels; ++i) {
            auto entry  = GetPageEntryByPtr(entryCache[i - 1]);
            if (entryCache[i] == entry) {
                break;
            }
            entryCache[i] = entry;
        }
        for (unsigned i = PageTableLevels; i > 0;) {
            --i;
            if (!IsPagePresent(entryCache[i])) {
                return i + 1;
            }
        }
        return 0;
    }

    static bool IsFullyUnmapped(VirtualRange r)
    {
        auto peBegin = ptr_cast<std::uintptr_t>(r.start);
        auto peEnd = peBegin + r.size;
        peBegin = (peBegin / BasicPageSize) & PageTableMask;
        peEnd = (peEnd / BasicPageSize) & PageTableMask;
        auto pageTable = GetPageEntry(0);
        x86_64_PageEntry* entryCache[4] = { nullptr };
        auto lEdge = IsPageNotPresent(pageTable + peBegin, entryCache);
        if (lEdge == 0) {
            return false;
        }
        auto nextAlign = std::uintptr_t(1) << ((lEdge - 1) * EntriesPerPageLog);
        for (auto i = AlignUp(peBegin + 1, nextAlign); i < peEnd;) {
            auto lEdge = IsPageNotPresent(pageTable + i, entryCache);
            if (lEdge == 0) {
                return false;
            }
            i += std::uintptr_t(1) << ((lEdge - 1) * EntriesPerPageLog);
        }
        return true;
    }

    static auto CountPageTablePages(VirtualRange r)
        -> std::size_t
    {
        if (!IsFullyUnmapped(r)) {
            return std::size_t(-1);
        }
        auto peBegin = ptr_cast<std::uintptr_t>(r.start);
        auto peEnd = peBegin + r.size;
        peBegin = (peBegin / BasicPageSize) & PageTableMask;
        peEnd = (peEnd / BasicPageSize) & PageTableMask;
        auto begin = peBegin;
        auto end = peEnd;
        auto result = std::size_t(0);
        for (int i = 0; i < PageTableLevels - 1; ++i) {
            begin = (begin + EntriesPerPage + 1) / EntriesPerPage;
            end = end / EntriesPerPage;
            result += end - begin;
        }
        auto pageTable = GetPageEntry(0);
        x86_64_PageEntry* entryCache[4] = { nullptr };
        result += IsPageNotPresent(pageTable + peBegin, entryCache) - 1;
        result += IsPageNotPresent(pageTable + peEnd - 1, entryCache) - 1;
        return result;
    }

    static auto MapRegion(void* vAddr, int flags, PhysicalRange r)
        -> std::size_t
    {
        VirtualRange virtRange = { .start = vAddr, .size = r.size };
        CountPageTablePages(virtRange);
        return virtRange.size;
    }

    static void UnmapPage(void* vAddr)
    {
        MapPage(vAddr, 0, 0);
    }
};

class LinearPMM {
public:
    LinearPMM(const kernel_LdrData* data) :
        memmap(FindMemoryMap(data)),
        brk(memmap->allocatedBoundary),
        brkRegion(
            FindBoundaryEntry(
                FindFirstAvailableEntry(
                    ptr_cast<const kernel_MemoryMapEntry*>(memmap->entries),
                    memmap->count
                ), brk)),
        lastFree(std::uint64_t(-1))
    {}

    auto Alloc(std::size_t size, void* tempPage) -> PhysicalRange
    {
        if (size == 0) {
            return {
                .error = PhyAllocError_SuccessEmpty,
                .size = 0
            };
        }
        size = AlignUp<std::size_t>(size, BasicPageSize);
        if (lastFree != std::uint64_t(-1)) {
            return AllocFromStack(size, tempPage);
        }
        using Entry = const kernel_MemoryMapEntry*;
        auto regions = ptr_cast<Entry>(uintptr_t(memmap->entries));
        auto region = regions + brkRegion;
        auto regionStart = region->begin;
        auto regionEnd = regionStart + region->size;
        regionStart = AlignUp<std::uint64_t>(regionStart, BasicPageSize);
        if (regionStart > UINTPTR_MAX) {
            return {
                .error = PhyAllocError_OutOfAddressSpace,
                .size = 0
            };
        }
        regionEnd = AlignDown<std::uint64_t>(regionEnd, BasicPageSize);
        size_t allocatedSize = kernel::Min(regionEnd - brk, std::uint64_t(size));
        if (allocatedSize == 0) {
            return {
                .error = PhyAllocError_NoMem,
                .size = 0
            };
        }
        uint64_t allocated = brk;
        brk += allocatedSize;
        if (brk == regionEnd && brkRegion + 1 != memmap->count) {
            size_t newRegion = brkRegion + 1;
            region = regions + newRegion;
            regionStart = AlignUp<std::size_t>(region->begin, BasicPageSize);
            if (regionStart <= UINTPTR_MAX) {
                brkRegion = newRegion;
                brk = regionStart;
            }
        }
        return {
            .start = allocated,
            .size = allocatedSize
        };
    }

    void Free(PhysicalRange range, void* tempPage)
    {
        using MM = MemoryMapper;
        if (range.size == 0) {
            return;
        }
        MM::MapPageUnsafe(tempPage, MM::FlagsAllAccess, range.start);
        auto last = range.start;
        range.start = lastFree;
        new(tempPage) PhysicalRange{range};
        lastFree = last;
        MM::RemapPageUnsafe(tempPage);
    }
private:
    auto AllocFromStack(std::size_t size, void* tempPage) -> PhysicalRange
    {
        using MM = MemoryMapper;
        MM::MapPageUnsafe(tempPage, MM::FlagsAllAccess, lastFree);
        auto rgInf = As<PhysicalRange*>(tempPage);
        auto next = rgInf->start;
        PhysicalRange range = {
            .start = lastFree,
            .size = rgInf->size
        };
        lastFree = next;
        rgInf->~PhysicalRange();
        MM::RemapPageUnsafe(tempPage);
        if (size < range.size) {
            PhysicalRange t = {
                .start = range.start + size,
                .size = range.size - size
            };
            range.size = size;
            Free(t, tempPage);
        }
        return range;
    }

    const kernel_MemoryMap* memmap;
    uint64_t brk;
    std::size_t brkRegion;
    uint64_t lastFree;
};

class MemoryManager {
public:
    static MemoryManager& Instance();
    VirtualRange Alloc(void* vAddr, size_t size, int flags, uint64_t pArgs);
    void Free(void* vAddr, size_t size, enum FlagsGeneric flags);
private:
    friend int ::kernel_InitAllocator();
    static MemoryManager& Init();
    MemoryManager(const kernel_LdrData* data, void* tempPage);

    static std::byte storage[];

    LinearPMM lPMM;
    VMMFreeList kernelFreeRegions;
};

alignas(MemoryManager) std::byte MemoryManager::storage[sizeof(MemoryManager)];

MemoryManager& MemoryManager::Instance()
{
    return *As<MemoryManager*>(storage);
}

MemoryManager& MemoryManager::Init()
{
    return *new(storage) MemoryManager(kernel_LoaderData, nullptr);
}

MemoryManager::MemoryManager(const kernel_LdrData* data, void* tempPage) :
    lPMM(data),
    kernelFreeRegions(tempPage)
{}

KERNEL_STRUCT(LinearMM) {
    PageMM base;
    const kernel_MemoryMap* memmap;
    uint64_t pBrk;
    size_t pBrkRegion;
    uintptr_t vStart;
    uintptr_t vEnd;
};

KERNEL_STRUCT(LinearPhyAllocator) {
    const kernel_MemoryMap* memmap;
    uint64_t brk;
    size_t brkRegion;
};

KERNEL_STRUCT(LinearAllocator) {
    LinearPhyAllocator* phyAlloc;
    uintptr_t start;
    uintptr_t end;
};

static PhysicalRange AllocatePhyMemLinear(LinearPhyAllocator* alloc, size_t size);

static LinearPhyAllocator* linearPhyAllocator = NULL;

int kernel_InitAllocator()
{
}

PhysicalRange AllocatePhyMemLinear(LinearPhyAllocator* alloc, size_t size)
{
    if (size == 0) {
        return {
            .error = PhyAllocError_SuccessEmpty,
            .size = 0
        };
    }
    size = (size + BasicPageMask) & (~(size_t)BasicPageMask);
    using Entry = const kernel_MemoryMapEntry*;
    auto regions = ptr_cast<Entry>((uintptr_t)alloc->memmap->entries);
    auto region = ptr_cast<Entry>(regions + alloc->brkRegion);
    uint64_t regionStart = region->begin;
    uint64_t regionEnd = regionStart + region->size;
    regionStart = (regionStart + BasicPageMask) & (~(uint64_t)BasicPageMask);
    if (regionStart > UINTPTR_MAX) {
        return {
            .error = PhyAllocError_OutOfAddressSpace,
            .size = 0
        };
    }
    regionEnd = (uintptr_t)(regionEnd & (~(uint64_t)BasicPageMask));
    size_t allocatedSize = kernel_MinU64(regionEnd - alloc->brk, size);
    if (allocatedSize == 0) {
        return {
            .error = PhyAllocError_NoMem,
            .size = 0
        };
    }
    uint64_t allocated = alloc->brk;
    alloc->brk += allocatedSize;
    if (alloc->brk == regionEnd && alloc->brkRegion + 1 != alloc->memmap->count) {
        size_t newRegion = alloc->brkRegion + 1;
        region = regions + newRegion;
        regionStart = (region->begin + BasicPageMask) & (~(uint64_t)BasicPageMask);
        if (regionStart <= UINTPTR_MAX) {
            alloc->brkRegion = newRegion;
            alloc->brk = regionStart;
        }
    }
    return {
        .start = allocated,
        .size = allocatedSize
    };
}

static VirtualRange ReserveVirtualMemLinear(LinearAllocator *alloc, size_t size) {
    if (size == 0) {
        return { 0 };
    }
    size = (size + BasicPageMask) & (~(size_t)BasicPageMask);
    if (alloc->start == alloc->end || alloc->end - alloc->start < size) {
        return { {0} };
    }
    VirtualRange result = { { (void*)alloc->start }, size };
    alloc->start += size;
    return result;
}

static VirtualRange AllocVirtualMemLinear(LinearAllocator *alloc, void* page, size_t size)
{
    uintptr_t oldStart = alloc->start;
    uint64_t oldBrk = alloc->phyAlloc->brk;
    size_t oldBrkRegion = alloc->phyAlloc->brkRegion;
    if (page == NULL) {
        if (size == 0) {
            return { {0}, 0 };
        }
        VirtualRange range = ReserveVirtualMemLinear(alloc, size);
        if (range.size == 0) {
            return { {0}, 0 };
        }
        page = range.start;
        size = range.size;
    }
    uintptr_t begin = (uintptr_t)page;
    uintptr_t end = begin + size;
    begin = begin & ~(uintptr_t)BasicPageMask;
    end = (end + BasicPageMask) & ~(uintptr_t)BasicPageMask;
    for (uintptr_t cur = begin; cur < end; cur += BasicPageSize) {
        void* result = MapKernelPage(kernelPageTable, (void*)cur, 0, 0);
        if (result) {
            continue;
        }
        for(; cur != begin; cur -= BasicPageSize) {
            // TODO: UnmapKernelPage((void*)(cur - BasicPageSize));
        }
        alloc->start = oldStart;
        alloc->phyAlloc->brk = oldBrk;
        alloc->phyAlloc->brkRegion = oldBrkRegion;
        return { {0}, 0 };
    }
    return { { (void*)begin }, (size_t)(end - begin) };
}

extern "C" {

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

}
