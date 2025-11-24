/**
 * TODO:
 * Reserve - reserves virtual memory for arbitrary mapping
 * Free - frees reserved and mapped memory
 * Map - maps arbitrary physical memory or free physical RAM to specified virtual memory
 * Unmap - unmap physical memory from specified virtual memory, frees physical RAM if it was mapped
 *
 * enum FlagsGeneric {
 * + NoAlloc = 1,
 *   Map = 2,
 *   AllocMask = 3,
 *   WriteToggle = 4,
 *   Execute = 8,
 *   System = 16,
 *   NoCache = 32,
 *   WriteCombine = 64,
 *   CacheMask = 96,
 * };
 *
 * enum FreeFrags {
 * Reserve
 * };
 *
 * VirtualRange Alloc(size_t size, enum FlagsGeneric flags, uint64_t pAddr, void* vAddr)
 * void Free(void* vAddr, size_t size, enum FlagsGeneric flags);
 *
 * struct PageMM {
 * PhysicalRegion (*PAlloc)(PageMM* mm, size_t count);
 * VirtualRegion (*VAlloc)(PageMM* mm, void* page, size_t count, int flags, uint64_t pArgs);
 * };
 *
 * IGNORE UNDEFINED BEHAVIOR WITH POINTERS
 */

#include <exception>
#include <new>
#include "kernel/bootdata.h"
#include "kernel/util.hpp"
#include "kernel/avl_tree.hpp"
#include "kernel/list.hpp"
#include "processor.h"
#include <cstring>
#include <algorithm>
#include <memory>
#include <limits>

namespace kernel::tgtspec {

namespace {

using byte = unsigned char;

constexpr auto PageBoundBits = 12;
constexpr auto PageSize = 1U << PageBoundBits;
constexpr auto PageMask = PageSize - 1;
constexpr std::uint64_t InvalidPage = -1;

} // namespace

extern const kernel_LdrData* loaderData;

extern "C" x86_64::PageEntry __mapping_window[];
extern "C" alignas(PageSize) unsigned char __smheap_start[];
extern "C" alignas(PageSize) unsigned char __smheap_end[];

namespace {

std::uint64_t zeroPage;

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

auto FindNextAvailableEntry(
    const kernel_MemoryMapEntry *entries,
    std::ptrdiff_t count,
    std::ptrdiff_t from
)
-> std::ptrdiff_t {
    for (std::ptrdiff_t i = from; i < count; ++i) {
        if (entries[i].type == kernel_MemoryMapEntryType_AvailableMemory) {
            return i;
        }
    }
    return count;
}

auto CanonizeAddr(std::uintptr_t addr) -> std::uintptr_t
{
    return ((addr & 0xFFFFFFFFFFFF) ^ 0x800000000000) - 0x800000000000;
}

struct ISinglePageAlloc {
    virtual auto alloc() -> std::uint64_t = 0;
    virtual void free(std::uint64_t) = 0;
};

struct InvalidPageAlloc : ISinglePageAlloc {
    virtual auto alloc() -> std::uint64_t override
    {
        return InvalidPage;
    }
    virtual void free(std::uint64_t) override {}
};

struct DupPageAlloc : ISinglePageAlloc {
    virtual auto alloc() -> std::uint64_t override
    {
        return page;
    }
    virtual void free(std::uint64_t) override {}
    uint64_t page;
};

struct Mapper
{
    static constexpr auto IndexMask = 0xFFFFFFFFF;
    static constexpr auto PageTableAddr = -0x800000000000;
    static constexpr auto BottomLevelMask = 0777;
    static constexpr auto NonBottomLevelMask = 0777777777000;
    static constexpr auto PageDirectoriesStartIndex = 0400000000000U;
    static constexpr auto PML4StartIndex = 0400400400000U;
    static constexpr auto LevelBits = 9;
    static void Init()
    {}
    static auto Entry(std::ptrdiff_t index) -> x86_64::PageEntry&
    {//0400400400400
        return *(ptr_cast<x86_64::PageEntry*>(PageTableAddr) + index);
    }
    static auto IndexOf(void* addr) -> std::ptrdiff_t
    {
        return IndexOf(ptr_cast<std::uintptr_t>(addr));
    }
    static auto IndexOf(std::uintptr_t addr) -> std::ptrdiff_t
    {
        return (addr / PageSize) & IndexMask;
    }
    static auto EntryByAddr(void* addr) -> x86_64::PageEntry&
    {
        return Entry(IndexOf(addr));;
    }
    static auto EntryByAddr(std::uintptr_t addr) -> x86_64::PageEntry&
    {
        return Entry(IndexOf(addr));
    }
    static void InvalidateSingle(std::ptrdiff_t index)
    {
        auto addr = CanonizeAddr(std::uintptr_t(index) * PageSize);
        x86_64::FlushPageTLB(ptr_cast<void*>(addr));
    }
    static void Invalidate(std::ptrdiff_t index)
    {
        constexpr auto TopLevelMask = 0777000000000;
        while (1) {
            InvalidateSingle(index);
            if ((index & TopLevelMask) != PageDirectoriesStartIndex) {
                break;
            }
            index *= 01000;
        }
    }
    static void InvalidateByAddr(void* addr)
    {
        Invalidate(IndexOf(addr));
    }
    static void InvalidateByAddr(std::uintptr_t addr)
    {
        Invalidate(IndexOf(addr));
    }
    static void Set(std::ptrdiff_t index, uint64_t newPage)
    {
        Entry(index) = x86_64::MakePageEntry(
            newPage, x86_64::PageEntryFlag_Present | x86_64::PageEntryFlag_Write);
    }
    static auto Reset(std::ptrdiff_t index) -> uint64_t
    {
        auto& t = Entry(index);
        auto ptr = x86_64::PageEntry_GetAddr(t);
        t = {};
        Invalidate(index);
        return ptr;
    }
    static auto UnmapUnsafe(void* addr) -> std::uint64_t
    {
        return Reset(IndexOf(addr));
    }
    static void* MapUnsafe(std::uintptr_t vAddr, std::uint64_t pAddr)
    {
        EntryByAddr(vAddr) = x86_64::MakePageEntry(
            pAddr, x86_64::PageEntryFlag_Present | x86_64::PageEntryFlag_Write);
        InvalidateByAddr(vAddr);
        return ptr_cast<void*>(vAddr);
    }
    static bool FillEntries(
        std::ptrdiff_t begin, std::ptrdiff_t end,
        ISinglePageAlloc* alloc)
    {
        for (auto i = begin; i < end; ++i) {
            auto p = alloc->alloc();
            if (p == InvalidPage) {
                ClearEntries(begin, i, alloc);
                return false;
            }
            Set(i, p);
        }
        return true;
    }
    static void ClearEntries(
        std::ptrdiff_t begin, std::ptrdiff_t end,
        ISinglePageAlloc* alloc)
    {
        for (auto i = end; i > begin;) {
            --i;
            alloc->free(Reset(i));
        }
    }
    static bool EntriesPresent(std::ptrdiff_t begin, std::ptrdiff_t end)
    {
        for (auto i = begin; i != end; ++i) {
            auto &t = Entry(i);
            if (t.data & x86_64::PageEntryFlag_Present) {
                return true;
            }
        }
        return false;
    }
    struct Args {
        ISinglePageAlloc* alloc;
        std::ptrdiff_t begin;
        std::ptrdiff_t end;
        int level;
    };
    static bool FillLevels(Args* args)
    {
        if (args->level == 3) {
            return true;
        }
        auto levelID = (PML4StartIndex << (args->level * LevelBits)) & IndexMask;
        auto level = 3 - args->level;
        auto begin = (args->begin >> (level * LevelBits)) | levelID;
        auto end = args->end - 1 + (1 << (level * LevelBits));
        end = (end >> (level * LevelBits)) | levelID;
        if (Entry(begin).data & x86_64::PageEntryFlag_Present) {
            ++begin;
        }
        if (Entry(end - 1).data & x86_64::PageEntryFlag_Present) {
            --end;
        }
        if (!FillEntries(begin, end, args->alloc)) {
            return false;
        }
        args->level += 1;
        if (FillLevels(args)) [[likely]] {
            return true;
        }
        ClearEntries(begin, end, args->alloc);
        return false;
    }
    static void ClearLevels(Args* args)
    {
        auto levelID = PageDirectoriesStartIndex;
        for (int i = 2; i != 0; --i) {
            args->begin = (args->begin >> LevelBits) | levelID;
            args->end = (args->end >> LevelBits) | levelID;
            levelID |= levelID >> LevelBits;
            if (args->end <= args->begin) {
                return;
            }
            ClearEntries(args->begin, args->end, args->alloc);
            if (EntriesPresent(args->begin & NonBottomLevelMask, args->begin)) {
                args->begin += BottomLevelMask;
            }
            auto newEnd = args->end + BottomLevelMask;
            if (!EntriesPresent(args->end, newEnd & NonBottomLevelMask)) {
                args->end = newEnd;
            }
        }
    }
    struct LinearAddrGen : ISinglePageAlloc {
        LinearAddrGen(std::uint64_t paddr) : next(paddr) {}
        auto alloc() -> std::uint64_t {
            auto result = next;
            next += PageSize;
            return result;
        }
        void free(std::uint64_t) {}
        std::uint64_t next;
    };
    static bool MapWithAlloc(std::uintptr_t vaddr, std::ptrdiff_t size,
        ISinglePageAlloc *alloc, ISinglePageAlloc *ptAlloc = nullptr)
    {
        Args args = {};
        args.alloc = (ptAlloc ? ptAlloc : alloc);
        args.begin = (vaddr / PageSize) & IndexMask;
        args.end = ((vaddr + size + PageMask) / PageSize) & IndexMask;
        if (!FillLevels(&args)) {
            return false;
        }
        if (FillEntries(args.begin, args.end, alloc)) [[likely]] {
            return true;
        }
        ClearLevels(&args);
        return false;
    }
    static bool Map(std::uintptr_t vaddr, std::ptrdiff_t size,
                    std::uint64_t paddr, ISinglePageAlloc *ptAlloc)
    {
        LinearAddrGen alloc(paddr);
        return MapWithAlloc(vaddr, size, &alloc, ptAlloc);
    }
    static void UnmapWithAlloc(std::uintptr_t vaddr, std::ptrdiff_t size,
        ISinglePageAlloc *alloc, ISinglePageAlloc *ptAlloc = nullptr)
    {
        Mapper::Args args = {};
        args.alloc = (ptAlloc ? ptAlloc : alloc);
        args.begin = (vaddr / PageSize) & IndexMask;
        args.end = ((vaddr + size + PageMask) / PageSize) & IndexMask;
        ClearEntries(args.begin, args.end, alloc);
        if (EntriesPresent(args.begin & NonBottomLevelMask, args.begin)) {
            args.begin += BottomLevelMask;
        }
        auto newEnd = args.end + BottomLevelMask;
        if (!EntriesPresent(args.end, newEnd & NonBottomLevelMask)) {
            args.end = newEnd;
        }
        ClearLevels(&args);
    }
    static void Unmap(std::uintptr_t vaddr, std::ptrdiff_t size,
        ISinglePageAlloc *ptAlloc)
    {
        LinearAddrGen alloc(0);
        UnmapWithAlloc(vaddr, size, &alloc, ptAlloc);
    }
};

struct SinglePagePMM : ISinglePageAlloc
{
    using CMapEntry = const kernel_MemoryMapEntry;
    SinglePagePMM()
    {
        auto memmap = FindMemoryMap(loaderData);
        auto entries = ptr_cast<CMapEntry*>(memmap->entries);
        count = memmap->count;
        map = entries;
        current = FindNextAvailableEntry(entries, count, 0);
        boundary = entries[current].begin;
    }
    auto alloc() -> std::uint64_t
    {
        if (lastFree != InvalidPage) {
            auto result = lastFree;
            auto ptr = ptr_cast<std::uintptr_t>(&__mapping_window);
            auto next = ptr_cast<std::uint64_t*>(Mapper::MapUnsafe(ptr, result));
            lastFree = *std::launder(next);
            std::memset(next, 0, PageSize);
            Mapper::UnmapUnsafe(next);
            return result;
        }
        auto cEnd = map[current].end;
        if (cEnd == boundary) {
            auto next = FindNextAvailableEntry(map, count, current + 1);
            if (next == count) {
                return InvalidPage;
            }
            current = next;
            boundary = map[next].begin;
        }
        auto result = boundary;
        boundary += PageSize;
        auto iptr = ptr_cast<std::uintptr_t>(&__mapping_window);
        auto ptr = Mapper::MapUnsafe(iptr, result);
        std::memset(ptr, 0, PageSize);
        Mapper::UnmapUnsafe(ptr);
        return result;
    }
    void free(std::uint64_t addr)
    {
        auto ptr = ptr_cast<std::uintptr_t>(&__mapping_window);
        auto next = ptr_cast<std::uint64_t*>(Mapper::MapUnsafe(ptr, addr));
        *std::launder(next) = lastFree;
        Mapper::UnmapUnsafe(next);
        lastFree = addr;
    }
    CMapEntry* map;
    std::ptrdiff_t count;
    std::ptrdiff_t current;
    std::uint64_t boundary;
    std::uint64_t lastFree = InvalidPage;
};

template <class T, class M>
constexpr T reset_bits(T in, M bitmask)
{
    return in ^ (bitmask & in);
}

struct address_node_trait;
struct size_node_trait;

struct address_node {
    using avl_tree_node_trait = address_node_trait;
    address_node* children[2];
    address_node* parent;
    std::uintptr_t addressCompressed;
    auto get_address() const -> std::uintptr_t
    {
        return reset_bits(addressCompressed, 3);
    }
    void set_address(std::uintptr_t addr)
    {
        addressCompressed = addr | (addressCompressed & 3);
    }
    auto get_balance() const -> int
    {
        return ((addressCompressed & 3) ^ 2) - 2;
    }
    void set_balance(int balance)
    {
        addressCompressed = reset_bits(addressCompressed, 3) | (balance & 3);
    }
};

struct size_node {
    using avl_tree_node_trait = size_node_trait;
    size_node* children[2];
    size_node* parent;
    std::ptrdiff_t sizeCompressed;
    auto get_size() const -> std::ptrdiff_t
    {
        return reset_bits(sizeCompressed, 3);
    }
    void set_size(std::ptrdiff_t size)
    {
        sizeCompressed = size | (sizeCompressed & 3);
    }
    auto get_balance() const -> int
    {
        return ((sizeCompressed & 3) ^ 2) - 2;
    }
    void set_balance(int balance)
    {
        sizeCompressed = reset_bits(sizeCompressed, 3) | (balance & 3);
    }
};

struct address_node_trait
{
    static auto GetParent(const address_node& node) -> address_node*
    {
        return node.parent;
    }
    static void SetParent(address_node& node, address_node* parent)
    {
        node.parent = parent;
    }
    static auto GetChild(const address_node& node, bool right) -> address_node*
    {
        return node.children[right];
    }
    static void SetChild(address_node& node, bool right, address_node* child)
    {
        node.children[right] = child;
    }
    static  int GetBalance(const address_node& node)
    {
        return node.get_balance();
    }
    static void SetBalance(address_node& node, int balance)
    {
        node.set_balance(balance);
    }
};

struct size_node_trait
{
    static auto GetParent(const size_node& node) -> size_node*
    {
        return node.parent;
    }
    static void SetParent(size_node& node, size_node* parent)
    {
        node.parent = parent;
    }
    static auto GetChild(const size_node& node, bool right) -> size_node*
    {
        return node.children[right];
    }
    static void SetChild(size_node& node, bool right, size_node* child)
    {
        node.children[right] = child;
    }
    static  int GetBalance(const size_node& node)
    {
        return node.get_balance();
    }
    static void SetBalance(size_node& node, int balance)
    {
        node.set_balance(balance);
    }
};

struct free_range
{
    free_range* children[2];
    std::uintptr_t parent;
    std::uintptr_t address;
    std::ptrdiff_t size;
};

struct free_range_comparator
{
    bool operator()(const free_range& a, const free_range& b) const
    {
        auto av = reinterpret_cast<std::uintptr_t>(std::addressof(a));
        auto bv = reinterpret_cast<std::uintptr_t>(std::addressof(b));
        return av < bv;
    }
    bool operator()(void* const& a, const free_range& b) const
    {
        auto av = reinterpret_cast<std::uintptr_t>(a);
        auto bv = reinterpret_cast<std::uintptr_t>(std::addressof(b));
        return av < bv;
    }
    bool operator()(const free_range& a, void* const& b) const
    {
        auto av = reinterpret_cast<std::uintptr_t>(std::addressof(a));
        auto bv = reinterpret_cast<std::uintptr_t>(b);
        return av < bv;
    }
};

constexpr auto align(std::size_t size, std::size_t align) -> std::size_t
{
    auto t = (size + align - 1);
    return reset_bits(t, align - 1);
}

template <class T, std::size_t storageSize>
struct chunked_mem_pool
{
    using list_node = kernel::intrusive::ListNode<>;
    struct free_obj : list_node {};
    using list = kernel::intrusive::List<free_obj>;

    static constexpr auto max_align = std::max(alignof(T), alignof(free_obj));
    static constexpr auto max_size = std::max(sizeof(T), sizeof(free_obj));
    static constexpr auto obj_size = align(max_size, max_align);
    static constexpr auto objs_per_storage = ptrdiff_t(storageSize / obj_size);

    void add_storage(void* storage)
    {
        auto current = static_cast<unsigned char*>(storage);
        for (ptrdiff_t i = 0; i < objs_per_storage; ++i) {
            auto& obj = *new(current) free_obj;
            freeObjs.PushBack(obj);
            current += obj_size;
        }
    }

    bool empty()
    {
        return freeObjs.Empty();
    }

    template <class ... Init>
    T* alloc(Init&& ... init)
    {
        if (empty()) {
            return nullptr;
        }
        auto last = (--freeObjs.End()).operator->();
        freeObjs.PopBack();
        last->~free_obj();
        return new(last) T(std::forward<Init>(init)...);
    }

    template <class ... Init>
    void free(T* obj)
    {
        obj->~T();
        freeObjs.PushBack(*new(obj) free_obj);
    }

    list freeObjs;
};

struct BasicVMM
{
    struct mem_range
    {
        std::uintptr_t begin;
        std::uintptr_t end;
    };

    BasicVMM(mem_range* memRanges, std::ptrdiff_t count) :
        memRanges(memRanges),
        count(count)
    {}
    auto AcquireRange(std::ptrdiff_t size) const -> mem_range
    {
        size = align(size, PageSize);
        for (std::ptrdiff_t i = 0; i < count; ++i) {
            if (memRanges[i].end - memRanges[i].begin >= size) {
                return {memRanges[i].begin, memRanges[i].begin += size};
            }
        }
        return {};
    }

    mem_range *memRanges;
    std::ptrdiff_t count;
};

constexpr auto alignDown(std::uint64_t size, std::uint64_t align) -> std::uint64_t
{
    return reset_bits(size, align - 1);
}

constexpr auto alignUp(std::uint64_t size, std::uint64_t align) -> std::uint64_t
{
    return alignDown(size + align - 1, align);
}

auto FindFirstMemRegion(const kernel_MemoryMapEntry* map, std::ptrdiff_t count) -> std::ptrdiff_t
{
    for (std::ptrdiff_t i = 0; i < count; ++i) {
        switch (map[i].type) {
        case kernel_MemoryMapEntryType_AvailableMemory:
        case kernel_MemoryMapEntryType_BootReclaimable:
        case kernel_MemoryMapEntryType_SystemReclaimable:
            return i;
        }
    }
    return count;
}

auto FindLastMemRegion(const kernel_MemoryMapEntry* map, std::ptrdiff_t count, std::ptrdiff_t from) -> std::ptrdiff_t
{
    std::ptrdiff_t result = from;
    for (std::ptrdiff_t i = from + 1; i < count; ++i) {
        switch (map[i].type) {
        case kernel_MemoryMapEntryType_AvailableMemory:
        case kernel_MemoryMapEntryType_BootReclaimable:
        case kernel_MemoryMapEntryType_SystemReclaimable:
            result = i;
        }
    }
    return result;
}

struct BuddyAlloc final : ISinglePageAlloc {
    struct PhyRange {
        std::uint64_t begin;
        std::uint64_t end;
    };

    struct BlockListElem {
        std::uint64_t prev;
        std::uint64_t next;
    };

    BuddyAlloc(const BasicVMM& vmm, SinglePagePMM&& pmm)
    {
        auto firstMemRange = FindFirstMemRegion(pmm.map, pmm.count);
        auto lastMemRange = FindLastMemRegion(pmm.map, pmm.count, firstMemRange);
        range = {pmm.map[firstMemRange].begin, pmm.map[lastMemRange].end};
        auto size = range.end - range.begin;
        maxLevel = Log2U64(size / PageSize);
        auto maxAlign = (std::uint64_t(1) << maxLevel) * PageSize;
        auto levelCount = maxLevel + 1;
        range.begin = alignDown(range.begin, maxAlign);
        range.end = alignUp(range.end, maxAlign);
        auto bitmapSize = (range.end - range.begin) / PageSize;
        std::ptrdiff_t bitmapElemCount = (bitmapSize + 31) / 32;
        std::ptrdiff_t listsOffset = alignUp(sizeof(std::uint32_t) * bitmapElemCount, alignof(std::uint64_t));
        std::ptrdiff_t arraysSize = listsOffset + levelCount * sizeof(std::uint64_t);
        auto hdrRange = vmm.AcquireRange(arraysSize);
        if (hdrRange.begin == hdrRange.end) {
            std::terminate();
        }
        if (!Mapper::MapWithAlloc(hdrRange.begin, hdrRange.end - hdrRange.begin, &pmm)) {
            std::terminate();
        }
        auto storage = ptr_cast<byte*>(hdrRange.begin);
        bitmap = new(storage) std::uint32_t[bitmapElemCount];
        freeListHeads = new(storage + listsOffset) std::uint64_t[levelCount];
        for (int i = 0; i <= maxLevel; ++i) {
            freeListHeads[i] = InvalidPage;
        }
        auto current = pmm.current;

        ReleaseRange({pmm.boundary, pmm.map[current].end});
        while ((current = FindNextAvailableEntry(pmm.map, pmm.count, ++current)) != pmm.count) {
            ReleaseRange({pmm.map[current].begin, pmm.map[current].end});
        }
    }
private:
    bool ToggleBit(std::uint64_t bit) const
    {
        auto index = bit / 32;
        auto bitMask = std::uint32_t(1) << (bit % 32);
        return (bitmap[index] ^= bitMask) & bitMask;
    }

    bool TogglePair(int level, std::uint64_t pairNum) const
    {
        auto pagesPerPairBlock = std::uint64_t(1) << level;
        return ToggleBit((pairNum << (level + 1)) + pagesPerPairBlock);
    }

    auto MapExisting(std::uint64_t block) const -> BlockListElem*
    {
        auto ptr = Mapper::MapUnsafe(ptr_cast<std::uintptr_t>(&__mapping_window), block);
        return std::launder(ptr_cast<BlockListElem*>(ptr));
    }

    void UnmapBlock(BlockListElem* elem) const
    {
        Mapper::UnmapUnsafe(elem);
    }

    void CreateBlock(std::uint64_t block, std::uint64_t prev, std::uint64_t next) const
    {
        auto ptr = Mapper::MapUnsafe(ptr_cast<std::uintptr_t>(&__mapping_window), block);
        Mapper::UnmapUnsafe(new(ptr) BlockListElem{ prev, next });
    }

    auto DestroyBlock(std::uint64_t block) const -> BlockListElem
    {
        auto elem = MapExisting(block);
        auto result = *elem;
        elem->~BlockListElem();
        std::memset(elem, 0, PageSize);
        UnmapBlock(elem);
        return result;
    }

    bool InsertIntoList(int level, std::uint64_t block) const
    {
        if (level < maxLevel && !TogglePair(level, GetPairNum(level, block))) {
            return false;
        }
        if (freeListHeads[level] != InvalidPage) {
            auto elem = MapExisting(freeListHeads[level]);
            elem->prev = block;
            UnmapBlock(elem);
        }
        CreateBlock(block, std::uint64_t(InvalidPage), freeListHeads[level]);
        freeListHeads[level] = block;
        return true;
    }

    void EraseFromList(int level, std::uint64_t block) const
    {
        auto copy = DestroyBlock(block);
        if (copy.next != InvalidPage) {
            auto elem = MapExisting(copy.next);
            elem->prev = copy.prev;
            UnmapBlock(elem);
        }
        if (copy.prev != InvalidPage) {
            auto elem = MapExisting(copy.prev);
            elem->next = copy.next;
            UnmapBlock(elem);
        } else {
            freeListHeads[level] = copy.next;
        }
    }

    auto GetNeighbor(int level, std::uint64_t block) const -> std::uint64_t
    {
        auto mask = std::uint64_t(1) << (level + PageBoundBits);
        return block ^ mask;
    }

    auto GetPairNum(int level, std::uint64_t block) const -> std::uint64_t
    {
        return (block - range.begin) >> (level + PageBoundBits + 1);
    }

    auto GetUpper(int level, std::uint64_t block) const -> std::uint64_t
    {
        return GetNeighbor(level, block) & block;
    }

    void InsertBlock(int level, std::uint64_t block) const
    {
        while (level < maxLevel) {
            if (InsertIntoList(level, block)) {
                break;
            }
            EraseFromList(level, GetNeighbor(level, block));
            block = GetUpper(level, block);
            ++level;
        }
    }

    auto ExtractBlock(int level) const -> std::uint64_t
    {
        auto block = freeListHeads[level];
        if (block == InvalidPage) {
            return block;
        }
        auto copy = DestroyBlock(block);
        freeListHeads[level] = copy.next;
        if (TogglePair(level, GetPairNum(level, block))) {
            std::terminate();
        }
        if (copy.next == InvalidPage) {
            return block;
        }
        auto elem = MapExisting(copy.next);
        elem->prev = InvalidPage;
        UnmapBlock(elem);
        return block;
    }

    auto AllocBlock(int level) const -> std::uint64_t
    {
        int currentLevel = level;
        std::uint64_t block = InvalidPage;
        while (currentLevel <= maxLevel) {
            block = ExtractBlock(currentLevel);
            if (block != InvalidPage) {
                break;
            }
            currentLevel += 1;
        }
        if (block == InvalidPage) {
            return block;
        }
        while (currentLevel > level) {
            currentLevel -= 1;
            if (!InsertIntoList(currentLevel, GetNeighbor(currentLevel, block))) {
                std::terminate();
            }
        }
        return block;
    }
public:
    void ReleaseRange(PhyRange&& range) const
    {
        while (range.begin < range.end) {
            auto level = std::min(ctz64(range.end), Log2U64(range.end - range.begin));
            auto size = 1 << level;
            level -= PageBoundBits;
            range.end -= size;
            if (!InsertIntoList(level, range.end)) {
                std::terminate();
            }
        }
    }

    // ISinglePageAlloc interface
    std::uint64_t alloc() override
    {
        return AllocBlock(0);
    }

    void free(std::uint64_t page) override
    {
        InsertBlock(0, page);
    }

    void DebugDumpLists()
    {
        auto putStr = [](const char* str) {
            while (*str) {
                // __asm__ volatile ("outb %0, $0xe9"::"a"(*str));
                ++str;
            }
        };
        putStr("\n[Buddy lists]\n");
        auto s = std::uint64_t(PageSize);
        char buf[17];
        for (int i = 0; i <= maxLevel; ++i, s <<= 1) {
            auto next = freeListHeads[i];
            kernel::UToStr(buf, 17, s, 16);
            putStr(buf); putStr(":");
            while (next != InvalidPage) {
                kernel::UToStr(buf, 17, next, 16);
                putStr(" "); putStr(buf);
                auto elem = MapExisting(next);
                next = elem->next;
                UnmapBlock(elem);
            }
            putStr("\n");
        }
    }
private:
    PhyRange range;
    std::uint64_t* freeListHeads;
    std::uint32_t* bitmap;
    int maxLevel;
};

struct VMM
{
    using mem_range = BasicVMM::mem_range;
    struct free_range : address_node, size_node {};

    VMM() {}
    VMM(BuddyAlloc& pmm, const BasicVMM& vmm)
    {
        auto range = vmm.AcquireRange(PageSize);
        if (range.begin == range.end) {
            std::terminate();
        }
        if (!Mapper::MapWithAlloc(range.begin, PageSize, &pmm)) {
            std::terminate();
        }
        memPool.add_storage(ptr_cast<void*>(range.begin));
        for (std::ptrdiff_t i = 0; i < vmm.count; ++i) {
            ReleaseRange(vmm.memRanges[i]);
        }
    }

    void ReleaseRange(const mem_range& range)
    {
        if (range.begin == range.end) {
            return;
        }
        auto r = range;
        AdjustRange(r);
        if (addressTree.Empty()) {
            AddMemoryRegion(r);
            return;
        }
        auto crBegin = addressTree.UpperBound(by_end(r.begin));
        auto crEnd = addressTree.LowerBound(r.end);
        if (crBegin != crEnd) {
            std::terminate();
        }
        if (crBegin != addressTree.Begin()) {
            auto prev = crBegin; --prev;
            auto& prevNode = *prev;
            if (prevNode.get_address() + prevNode.get_size() == r.begin) {
                sizeTree.Erase(prevNode);
                if (crBegin != addressTree.End() && r.end == crBegin->get_address()) {
                    prevNode.set_size(r.end - prevNode.get_address() + crBegin->get_size());
                    Erase(*crBegin);
                } else {
                    prevNode.set_size(r.end - prevNode.get_address());
                }
                sizeTree.Insert(prevNode);
                return;
            }
        }
        if (crBegin != addressTree.End() && r.end == crBegin->get_address()) {
            auto& prevNode = *crBegin;
            sizeTree.Erase(prevNode);
            prevNode.set_address(r.begin);
            prevNode.set_size(r.end - r.begin + prevNode.get_size());
            sizeTree.Insert(prevNode);
            return;
        }
        AddMemoryRegion(r);
    }

    auto AcquireRange(std::size_t size) -> mem_range
    {
        if (size == 0) {
            return {};
        }
        size = align(size, PageSize);
        auto crBegin = sizeTree.LowerBound(size);
        auto crEnd = sizeTree.UpperBound(size);
        if (crBegin != crEnd) {
            auto node = crBegin.operator->();
            return AcquireIdealMatch(node);
        }
        if (crEnd == sizeTree.End()) {
            return {};
        }
        auto& node = *crEnd;
        mem_range result = {node.get_address(), node.get_address() + size};
        node.set_address(result.end);
        sizeTree.Erase(node);
        node.set_size(node.get_size() - size);
        sizeTree.Insert(node);
        return result;
    }

    void AdjustRange(mem_range& r)
    {
        r.begin = reset_bits(r.begin, PageMask);
        r.end = align(r.end, PageSize);
        if (r.end == 0) {
            return;
        }
        r.end = std::max(r.begin, r.end);
    }
private:
    auto AcquireIdealMatch(free_range* node) -> mem_range
    {
        mem_range result = {node->get_address(), node->get_address() + node->get_size()};
        Erase(*node);
        memPool.free(node);
        return result;
    }

    void AddMemoryRegion(mem_range& r)
    {
        if (AutoExtendStorage(r)) {
            return;
        }
        auto node = memPool.alloc();
        node->set_address(r.begin);
        node->set_size(r.end - r.begin);
        Insert(*node);
    }

    bool AutoExtendStorage(mem_range& r);

    void Insert(free_range& node)
    {
        addressTree.Insert(node);
        sizeTree.Insert(node);
    }

    void Erase(free_range& node)
    {
        addressTree.Erase(node);
        sizeTree.Erase(node);
    }

    enum class by_end : std::uintptr_t {};

    struct address_comp
    {
        bool operator()(const free_range& a, const free_range& b)
        {
            return a.get_address() < b.get_address();
        }
        bool operator()(const free_range& a, std::uintptr_t b)
        {
            return a.get_address() < b;
        }
        bool operator()(std::uintptr_t a, const free_range& b)
        {
            return a < b.get_address();
        }
        bool operator()(const free_range& a, by_end b)
        {
            return a.get_address() + a.get_size() < static_cast<std::uintptr_t>(b);
        }
        bool operator()(by_end a, const free_range& b)
        {
            return static_cast<std::uintptr_t>(a) < b.get_address() + b.get_size();
        }
    };

    struct size_comp
    {
        bool operator()(const free_range& a, const free_range& b)
        {
            return a.get_size() < b.get_size();
        }
        bool operator()(const free_range& a, std::ptrdiff_t b)
        {
            return a.get_size() < b;
        }
        bool operator()(std::ptrdiff_t a, const free_range& b)
        {
            return a < b.get_size();
        }
    };

    chunked_mem_pool<free_range, PageSize> memPool;
    using address_tree_t = kernel::intrusive::AVLTree<free_range, address_comp, kernel::intrusive::BaseClassCastPolicy<address_node, free_range>>;
    using size_tree_t = kernel::intrusive::AVLTree<free_range, size_comp, kernel::intrusive::BaseClassCastPolicy<size_node, free_range>>;
    address_tree_t addressTree;
    size_tree_t sizeTree;
};

struct memory_range
{
    void* begin;
    ptrdiff_t size;
};

struct SystemAllocator {

    VMM addressSpaceAlloc;
    SinglePagePMM physSpaceAlloc;
};

struct Allocator {
    using PhyRange = BuddyAlloc::PhyRange;

    static auto Init() -> Allocator
    {
        VMM::mem_range memRanges[2] = {
            {ptr_cast<std::uintptr_t>(__smheap_start), ptr_cast<std::uintptr_t>(__smheap_end)},
            {-(std::uintptr_t)0x7FF000000000, -(std::uintptr_t)0x80000000}
        };
        BasicVMM vmm(memRanges, 2);
        SinglePagePMM pmm;
        if (pmm.current == pmm.count) {
            std::terminate();
        }
        return {{
            vmm,
            std::move(pmm)
        }, vmm};
    }

    Allocator(BuddyAlloc buddy, BasicVMM vmm) :
        pmm(buddy),
        vmm(pmm, vmm)
    {}

    static auto Instance() -> Allocator&
    {
        static Allocator alloc = Init();
        return alloc;
    }

    auto AllocMemoryRange(std::ptrdiff_t s) -> memory_range
    {
        auto range = vmm.AcquireRange(s);
        if (range.begin == range.end) [[unlikely]] {
            return { nullptr, 0 };
        }
        ptrdiff_t size = range.end - range.begin;
        if (!Mapper::MapWithAlloc(range.begin, size, &pmm)) [[unlikely]] {
            vmm.ReleaseRange(range);
            return { nullptr, 0 };
        }
        pmm.DebugDumpLists();
        return { ptr_cast<void*>(range.begin), size };
    }

    void FreeMemoryRange(const memory_range& r)
    {
        VMM::mem_range range{ ptr_cast<std::uintptr_t>(r.begin), ptr_cast<std::uintptr_t>(r.begin) + r.size };
        auto& valloc = vmm;
        Mapper::UnmapWithAlloc(range.begin, r.size, &pmm);
        valloc.ReleaseRange(range);
        pmm.DebugDumpLists();
    }

    BuddyAlloc pmm;
    VMM vmm;
};

bool VMM::AutoExtendStorage(mem_range& r)
{
    if (memPool.empty()) [[unlikely]] {
        auto& alloc = Allocator::Instance().pmm;
        if (!Mapper::MapWithAlloc(r.begin, PageSize, &alloc)) {
            std::terminate();
        }
        memPool.add_storage(kernel::ptr_cast<void*>(r.begin));
        r.begin += PageSize;
        if (r.begin == r.end) {
            return true;
        }
    }
    return false;
}

} // namespace

int InitAllocator()
{
    Mapper::Init();
    auto& alloc = Allocator::Instance();
    zeroPage = alloc.pmm.alloc();
    return 0;
}

extern "C" void* malloc(size_t s)
{
    constexpr auto HeaderReserve = alignof(max_align_t);
    static_assert(sizeof(std::ptrdiff_t) <= HeaderReserve);
    s += HeaderReserve;
    if (s > std::numeric_limits<std::ptrdiff_t>::max()) {
        return nullptr;
    }
    std::ptrdiff_t size = s;
    auto& alloc = Allocator::Instance();
    auto range = alloc.AllocMemoryRange(size);
    if (range.size == 0) [[unlikely]] {
        return nullptr;
    }
    auto ptr = ptr_cast<unsigned char*>(range.begin);
    new(range.begin) std::ptrdiff_t(range.size);
    return ptr + HeaderReserve;
}

extern "C" void free(void* p)
{
    if (p == nullptr) {
        return;
    }
    constexpr auto HeaderReserve = alignof(max_align_t);
    auto ptr = ptr_cast<unsigned char*>(p) - HeaderReserve;
    memory_range range{ptr, *kernel::As<std::ptrdiff_t*>(ptr)};
    auto& alloc = Allocator::Instance();
    alloc.FreeMemoryRange(range);
}

extern "C" void* realloc(void* p, size_t newSize)
{
    auto oldSize = [&p]() -> std::ptrdiff_t {
        constexpr auto HeaderReserve = alignof(max_align_t);
        if (p == nullptr) {
            return 0;
        }
        auto ptr = ptr_cast<unsigned char*>(p) - HeaderReserve;
        return *kernel::As<std::ptrdiff_t*>(ptr) - HeaderReserve;
    }();
    if (std::size_t(oldSize) == newSize) {
        return p;
    }
    auto newPtr = malloc(newSize);
    if (newPtr == nullptr) {
        return nullptr;
    }
    auto minSize = std::min<std::ptrdiff_t>(oldSize, newSize);
    if (minSize != 0) {
        std::memcpy(newPtr, p, minSize);
    }
    free(p);
    return newPtr;
}

}
