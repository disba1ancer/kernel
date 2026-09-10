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
 *   Write = 4,
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
#include "kernel/debug.h"
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
constexpr auto PageSize2M = PageSize << 9;
constexpr auto PageSize1G = PageSize2M << 9;
constexpr auto PageMask = PageSize - 1;
constexpr std::uint64_t InvalidPage = -1;

} // namespace

extern const kernel_LdrData* loaderData;

extern "C" x86_64::PageEntry __mapping_window[];
extern "C" alignas(PageSize) unsigned char __smheap_start[];
extern "C" alignas(PageSize) unsigned char __smheap_end[];

namespace {

std::uint64_t zeroPage;

struct MemoryMap {
    auto FindMemoryMap(const kernel_LdrData* data)
    {
        auto entriesCount = (size_t)data->value;
        for (size_t i = 1; i < entriesCount; ++i) {
            if (data[i].type == kernel_LdrDataType_MemoryMap) {
                auto memmap = ptr_cast<const kernel_MemoryMap*>(data[i].value);
                map = ptr_cast<const kernel_MemoryMapEntry*>(memmap->entries);
                count = memmap->count;
            }
        }
        return NULL;
    }

    enum FindMemoryFlag {
        OnlyAvailable,
        NoKernel,
        All
    };

    auto FindNextMemoryEntry(
        const kernel_MemoryMapEntry *from,
        FindMemoryFlag flag = FindMemoryFlag::OnlyAvailable
    )
    -> const kernel_MemoryMapEntry* {
        auto end = map + count;
        if (from) {
            from += 1;
        } else {
            from = map;
        }
        for (auto i = from; i != end; ++i) {
            switch (i->type) {
            case kernel_MemoryMapEntryType_SystemReclaimable:
            case kernel_MemoryMapEntryType_BootReclaimable:
            case kernel_MemoryMapEntryType_Inherited:
                if (flag == FindMemoryFlag::OnlyAvailable) {
                    break;
                }
                return i;
            case kernel_MemoryMapEntryType_Kernel:
                if (flag != FindMemoryFlag::All) {
                    break;
                }
            case kernel_MemoryMapEntryType_AvailableMemory:
                return i;
            }
        }
        return nullptr;
    }

    auto FindPrevMemoryEntry(
        const kernel_MemoryMapEntry* from,
        FindMemoryFlag flag = FindMemoryFlag::OnlyAvailable
    ) -> const kernel_MemoryMapEntry* {
        auto end = map + count;
        if (from == nullptr) {
            from = end;
        }
        for (auto i = from; i != map;) {
            --i;
            switch (i->type) {
            case kernel_MemoryMapEntryType_SystemReclaimable:
            case kernel_MemoryMapEntryType_BootReclaimable:
            case kernel_MemoryMapEntryType_Inherited:
                if (flag == FindMemoryFlag::OnlyAvailable) {
                    break;
                }
                return i;
            case kernel_MemoryMapEntryType_Kernel:
                if (flag != FindMemoryFlag::All) {
                    break;
                }
            case kernel_MemoryMapEntryType_AvailableMemory:
                return i;
            }
        }
        return nullptr;
    }

    static bool IsMemory(const kernel_MemoryMapEntry *r)
    {
        switch (r->type) {
        case kernel_MemoryMapEntryType_SystemReclaimable:
        case kernel_MemoryMapEntryType_BootReclaimable:
        case kernel_MemoryMapEntryType_Inherited:
        case kernel_MemoryMapEntryType_Kernel:
        case kernel_MemoryMapEntryType_AvailableMemory:
            return true;
        }
        return false;
    }

    struct BeginComp
    {
        bool operator()(const kernel_MemoryMapEntry& a, const kernel_MemoryMapEntry& b) const
        {
            return a.begin < b.begin;
        }
        bool operator()(const std::uint64_t& a, const kernel_MemoryMapEntry& b) const
        {
            return a < b.begin;
        }
        bool operator()(const kernel_MemoryMapEntry& a, const std::uint64_t& b) const
        {
            return a.begin < b;
        }
    } beginComp;

    struct EndComp
    {
        bool operator()(const kernel_MemoryMapEntry& a, const kernel_MemoryMapEntry& b) const
        {
            return a.end < b.end;
        }
        bool operator()(const std::uint64_t& a, const kernel_MemoryMapEntry& b) const
        {
            return a < b.end;
        }
        bool operator()(const kernel_MemoryMapEntry& a, const std::uint64_t& b) const
        {
            return a.end < b;
        }
    } endComp;

    const kernel_MemoryMapEntry* map;
    std::uint64_t count;
} memoryMap;

auto CanonizeAddr(std::uintptr_t addr) -> std::uintptr_t
{
    return ((addr & 0xFFFFFFFFFFFF) ^ 0x800000000000) - 0x800000000000;
}

bool Is1GBPagesSupported()
{
    auto r = x86_64::cpuid(0x80000000);
    if (r.eax < 1) {
        return false;
    }
    r = x86_64::cpuid(0x80000001);
    return (r.edx >> 26) & 1;
}

struct ISinglePageAlloc {
    virtual auto AllocPage() -> std::uint64_t = 0;
    virtual void FreePage(std::uint64_t) = 0;
};

struct InvalidPageAlloc : ISinglePageAlloc {
    virtual auto AllocPage() -> std::uint64_t override
    {
        return InvalidPage;
    }
    virtual void FreePage(std::uint64_t) override {}
};

struct DupPageAlloc : ISinglePageAlloc {
    virtual auto AllocPage() -> std::uint64_t override
    {
        return page;
    }
    virtual void FreePage(std::uint64_t) override {}
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
    static std::uint64_t directMapAddr;
    static std::uint64_t directMapEnd;
    static void* GetObject(std::uint64_t addr)
    {
        return ptr_cast<void*>(directMapAddr + addr);
    }
    static auto GetPhyAddr(const void* ptr) -> std::uint64_t
    {
        auto addr = ptr_cast<std::uintptr_t>(ptr);
        if (addr < directMapAddr || addr > directMapEnd) {
            return InvalidPage;
        }
        return addr - directMapAddr;
    }
    static auto GetPageDirectory(x86_64::PageEntry entry) -> x86_64::PageEntry(*)[512]
    {
        return as<x86_64::PageEntry(*)[512]>(GetObject(x86_64::PageEntry_GetAddr(entry)));
    }
    static auto GetPageEntryWithAlloc(int level, std::uintptr_t vaddr, ISinglePageAlloc& alloc) -> x86_64::PageEntry*
    {
        auto pml4 = x86_64::LoadCR3();
        x86_64::PageEntry* entry = &pml4;
        int l = 3;
        for (; l != level; --l) {
            entry = *GetPageDirectory(*entry) + ((vaddr >> (12 + l * 9)) & 0x1FF);
            if (entry->data & x86_64::PageEntryFlag_Present) {
                continue;
            }
            std::uint64_t newDir = alloc.AllocPage();
            if (newDir == InvalidPage) {
                return {};
            }
            *entry = x86_64::MakePageEntry(newDir, x86_64::PageEntryFlag_Present | x86_64::PageEntryFlag_Write);
        }
        return *GetPageDirectory(*entry) + ((vaddr >> (12 + l * 9)) & 0x1FF);
    }
    static void Init(ISinglePageAlloc& alloc)
    {
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        auto en1gb = Is1GBPagesSupported();
        auto flags = x86_64::PageEntryFlag_Present |
                     x86_64::PageEntryFlag_ExecDisable |
                     x86_64::PageEntryFlag_Write |
                     x86_64::PageEntryFlag_Global;
        for (auto i = memoryMap.FindNextMemoryEntry(nullptr, memoryMap.NoKernel); true; i = memoryMap.FindNextMemoryEntry(i, memoryMap.NoKernel)) {
            if (i and i->begin == end) {
                end = i->end;
                continue;
            }
            while (begin != end) {
                if (en1gb and (begin & (PageSize1G - 1)) == 0 and begin + PageSize1G <= end) {
                    auto entry = GetPageEntryWithAlloc(1, begin + PageTableAddr, alloc);
                    *entry = x86_64::MakePageEntry(begin, flags | x86_64::PageEntryFlag_PAT);
                    begin += PageSize1G;
                    continue;
                }
                if ((begin & (PageSize2M - 1)) == 0 and begin + PageSize2M <= end) {
                    auto entry = GetPageEntryWithAlloc(1, begin + PageTableAddr, alloc);
                    *entry = x86_64::MakePageEntry(begin, flags | x86_64::PageEntryFlag_PAT);
                    begin += PageSize2M;
                    continue;
                }
                auto entry = GetPageEntryWithAlloc(0, begin + PageTableAddr, alloc);
                *entry = x86_64::MakePageEntry(begin, flags);
                begin += PageSize;
            }
            if (i == nullptr) {
                directMapEnd = end;
                break;
            }
            begin = i->begin;
            end = i->end;
        }
        directMapAddr = PageTableAddr;
        directMapEnd += PageTableAddr;
        __asm__ volatile("xchg %bx, %bx");
    }
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
            auto p = alloc->AllocPage();
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
            alloc->FreePage(Reset(i));
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
        auto AllocPage() -> std::uint64_t {
            auto result = next;
            next += PageSize;
            return result;
        }
        void FreePage(std::uint64_t) {}
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

std::uint64_t Mapper::directMapAddr;
std::uint64_t Mapper::directMapEnd;

struct SinglePagePMM : ISinglePageAlloc
{
    using CMapEntry = const kernel_MemoryMapEntry;
    SinglePagePMM()
    {
        current = memoryMap.FindPrevMemoryEntry(nullptr);
        boundary = current->end;
    }
    auto AllocPage() -> std::uint64_t
    {
        if (lastFree != InvalidPage) {
            auto result = lastFree;
            auto next = as<std::uint64_t*>(Mapper::GetObject(lastFree));
            lastFree = *next;
            std::memset(next, 0, PageSize);
            return result;
        }
        auto cBegin = current->begin;
        if (cBegin == boundary) {
            auto next = memoryMap.FindPrevMemoryEntry(current);
            if (next == nullptr) {
                return InvalidPage;
            }
            current = next;
            boundary = next->end;
        }
        boundary -= PageSize;
        auto ptr = Mapper::GetObject(boundary);
        std::memset(ptr, 0, PageSize);
        return boundary;
    }
    void FreePage(std::uint64_t addr)
    {
        auto next = new(Mapper::GetObject(addr)) std::uint64_t{lastFree};
        Mapper::UnmapUnsafe(next);
        lastFree = addr;
    }
    CMapEntry* current;
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
        return reset_bits(addressCompressed, 7);
    }
    void set_address(std::uintptr_t addr)
    {
        addressCompressed = addr | (addressCompressed & 7);
    }
    auto get_balance() const -> int
    {
        return ((addressCompressed & 7) ^ 4) - 4;
    }
    void set_balance(int balance)
    {
        addressCompressed = reset_bits(addressCompressed, 7) | (balance & 7);
    }
};

struct size_node {
    using avl_tree_node_trait = size_node_trait;
    size_node* children[2];
    size_node* parent;
    std::ptrdiff_t sizeCompressed;
    auto get_size() const -> std::ptrdiff_t
    {
        return reset_bits(sizeCompressed, 7);
    }
    void set_size(std::ptrdiff_t size)
    {
        sizeCompressed = size | (sizeCompressed & 7);
    }
    auto get_balance() const -> int
    {
        return ((sizeCompressed & 7) ^ 4) - 4;
    }
    void set_balance(int balance)
    {
        sizeCompressed = reset_bits(sizeCompressed, 7) | (balance & 7);
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

bool IsAvailRg(
    const kernel_MemoryMapEntry *entries,
    std::ptrdiff_t count,
    std::uint64_t addr
    ) {
    while (1) {
        if (count == 0) {
            return false;
        }
        auto newCount = count / 2;
        auto entry = entries + newCount;
        if (addr < entry->begin) {
            count = newCount;
            continue;
        }
        if (addr >= entry->end) {
            entries = entry + 1;
            count -= newCount + 1;
            continue;
        }
        switch (entry->type) {
        case kernel_MemoryMapEntryType_AvailableMemory:
        case kernel_MemoryMapEntryType_BootReclaimable:
        case kernel_MemoryMapEntryType_SystemReclaimable:
            return true;
        }
        return false;
    }
}

struct PhyRange {
    std::uint64_t begin;
    std::uint64_t end;
};

struct IRangedAlloc : ISinglePageAlloc
{
    virtual auto AllocRange(std::uint64_t size) -> PhyRange = 0;
    virtual void FreeRange(const PhyRange& range) = 0; // TODO: Free result codes
};

struct IBuddyInstance : IRangedAlloc, intrusive::AVLTreeNode<>, PhyRange
{
    virtual bool AltReady() { return false; };
};

struct AllocInstanceComp
{
    bool operator()(const IBuddyInstance& a, const IBuddyInstance& b)
    {
        return a.begin < b.begin;
    }
    bool operator()(const std::uint64_t& a, const IBuddyInstance& b)
    {
        return a < b.begin;
    }
    bool operator()(const IBuddyInstance& a, const std::uint64_t& b)
    {
        return a.begin < b;
    }
};

struct BuddyAlloc final : IBuddyInstance {
    struct BlockListElem {
        std::uint64_t prev;
        std::uint64_t next;
    };

    BuddyAlloc(const BasicVMM& vmm, SinglePagePMM&& pmm)
    {
        auto firstMemRange = memoryMap.FindNextMemoryEntry(nullptr, MemoryMap::All);
        auto lastMemRange = memoryMap.FindPrevMemoryEntry(nullptr, MemoryMap::All);
        range = {firstMemRange->begin, lastMemRange->end};
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

        FreeRange({pmm.boundary, current->end});
        while ((current = memoryMap.FindNextMemoryEntry(current))) {
            FreeRange({current->begin, current->end});
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
        return as<BlockListElem*>(ptr);
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
    auto AllocRange(std::uint64_t size) -> PhyRange override
    {
        return {};
    }

    void FreeRange(const PhyRange& range) override
    {
        auto end = range.end;
        while (range.begin < end) {
            auto level = std::min(ctz64(end), Log2U64(end - range.begin));
            auto size = 1 << level;
            level -= PageBoundBits;
            end -= size;
            if (!InsertIntoList(level, end)) {
                std::terminate();
            }
        }
    }

    // ISinglePageAlloc interface
    std::uint64_t AllocPage() override
    {
        return AllocBlock(0);
    }

    void FreePage(std::uint64_t page) override
    {
        InsertBlock(0, page);
    }

    void DebugDumpLists()
    {
        namespace d = debug;
        d::println("[Buddy lists]");
        auto s = std::uint64_t(PageSize);
        char buf[17];
        for (int i = 0; i <= maxLevel; ++i, s <<= 1) {
            auto next = freeListHeads[i];
            kernel::UToStr(buf, 17, s, 16);
            d::puts(buf); d::putc(':');
            while (next != InvalidPage) {
                kernel::UToStr(buf, 17, next, 16);
                d::putc(' '); d::puts(buf);
                auto elem = MapExisting(next);
                next = elem->next;
                UnmapBlock(elem);
            }
            d::putc('\n');
        }
        d::putc('\n');
    }
private:
    PhyRange range;
    std::uint64_t* freeListHeads;
    std::uint32_t* bitmap;
    int maxLevel;
    const kernel_MemoryMapEntry* entries;
    std::ptrdiff_t count;
};

struct AVLBuddy final : IBuddyInstance
{
    static auto CalcSize(const PhyRange& range) -> size_t
    {
        return alignUp(sizeof(AVLBuddy), alignof(PagesList)) + sizeof(PagesList) * CalcMaxLevel(range);
    }

    static auto Construct(void* mem, const PhyRange& range) -> AVLBuddy*
    {
        auto maxLevel = CalcMaxLevel(range);
        auto r = new(mem) AVLBuddy(range, maxLevel);
        auto levelsPtr = ptr_cast<PagesList*>(alignUp(ptr_cast<uintptr_t>(r + 1), alignof(PagesList)));
        auto cur = levelsPtr;
        for (int i = 0; i < maxLevel; ++i) {
            cur = (new(cur) PagesList()) + 1;
        }
        r->levels = std::launder(levelsPtr);
        return r;
    }

    ~AVLBuddy()
    {
        for (int i = maxLevel; i-- > 0;) {
            levels[i].~PagesList();
        }
    }

    auto AllocPage() -> std::uint64_t override
    {
        return AllocBlock(0);
    }

    void FreePage(std::uint64_t page) override
    {
        FreeBlock(page, 0);
    }

    auto AllocRange(std::uint64_t size) -> PhyRange override
    {
        auto level = Log2U64((size - 1) * 2);
        auto startPage = AllocBlock(level - PageBoundBits);
        if (startPage == InvalidPage) {
            return {};
        }
        return { startPage, std::uint64_t(1 << level) };
    }

    void FreeRange(const PhyRange& range) override
    {
        auto end = range.end;
        while (range.begin < end) {
            auto level = std::min(ctz64(end), Log2U64(end - range.begin));
            auto size = 1 << level;
            level -= PageBoundBits;
            end -= size;
            FreeBlock(end, level);
        }
    }
private:
    static int CalcMaxLevel(const PhyRange& range)
    {
        auto maxLevel = Log2U64(range.end - range.begin);
        if (alignUp(range.begin, 1 << maxLevel) == alignDown(range.end, 1 << maxLevel)) {
            maxLevel -= 1;
        }
        return maxLevel - PageBoundBits;
    }

    AVLBuddy(const PhyRange& range, int maxLevel) : maxLevel(maxLevel)
    {
        PhyRange& r = *this;
        r = range;
    }

    auto AllocBlock(int level) -> std::uint64_t
    {
        for (int i = level; i < maxLevel; ++i) {
            if (levels[level].Empty()) {
                continue;
            }
            auto it = levels[level].Begin();
            auto block = it.operator->();
            levels[level].Erase(it);
            block->~BlockHeader();
            while (i-- != level) {
                auto leftover = new(ptr_cast<std::byte*>(block) + (1 << (PageBoundBits + level - 1))) BlockHeader();
                levels[level].Insert(*leftover);
            }
            return Mapper::GetPhyAddr(block);
        }
        return InvalidPage;
    }

    std::uint64_t GetNeighbor(std::uint64_t block, int level)
    {
        auto blk_size = 1 << (PageBoundBits + level);
        return block ^ blk_size;
    }

    void FreeBlock(std::uint64_t block, int level)
    {
        while (1) {
            auto neighbor = GetNeighbor(block, level);
            PagesList::Iterator it = levels[level].Find(neighbor);
            if (level == maxLevel || it == levels[level].End()) {
                auto free_block = new (Mapper::GetObject(block)) BlockHeader();
                levels[level].Insert(*free_block);
                return;
            }
            auto nheader = it.operator->();
            levels[level].Erase(it);
            nheader->~BlockHeader();
            block &= neighbor;
            level += 1;
        }
    }

    struct BlockHeader : intrusive::AVLTreeNode<>
    {};

    struct BlockComp
    {
        bool operator()(const BlockHeader& a, const BlockHeader& b)
        {
            return ptr_cast<std::uintptr_t>(&a) < ptr_cast<std::uintptr_t>(&b);
        }
        bool operator()(const std::uint64_t& a, const BlockHeader& b)
        {
            return a < Mapper::GetPhyAddr(&b);
        }
        bool operator()(const BlockHeader& a, const std::uint64_t& b)
        {
            return Mapper::GetPhyAddr(&a) < b;
        }
    };

    using PagesList = intrusive::AVLTree<BlockHeader, BlockComp>;
    int maxLevel;
    PagesList* levels;
};

struct PMM final : IRangedAlloc
{
    PMM()
    {
        SinglePagePMM bpmm;
        Mapper::Init(bpmm);
        auto [count, pagesTotal, maxLevel] = GetInstancesInfo();
        if (count == 0) {
            std::terminate();
        }
        levelCount = maxLevel + 1;
        std::uint64_t metaSize = sizeof(RangeInstance);
        metaSize = alignUp(metaSize, alignof(LevelTree)) + sizeof(LevelTree) * levelCount;
        auto bitmapWords = (pagesTotal + count + 31) / 32;
        metaSize = alignUp(metaSize, alignof(std::uint32_t)) + sizeof(std::uint32_t) * bitmapWords;
        metaSize = alignUp(metaSize, PageSize);
        auto memmapEntry = bpmm.current;
        void* metaPtr;
        if (bpmm.boundary - memmapEntry->begin < metaSize) {
            while (memmapEntry != nullptr and memmapEntry->end - memmapEntry->begin < metaSize) {
                memmapEntry = memoryMap.FindPrevMemoryEntry(memmapEntry);
            }
            if (memmapEntry == nullptr) {
                std::terminate();
            }
            metaPtr = Mapper::GetObject(memmapEntry->end - metaSize);
        } else {
            metaPtr = Mapper::GetObject(bpmm.boundary - metaSize);
        }

        auto instance = ptr_cast<RangeInstance*>(metaPtr);
        auto curEntry = memoryMap.FindNextMemoryEntry(nullptr, memoryMap.All);
        std::uint64_t bitmapOffset = 0;
        for (std::size_t i = 0; i < count; ++i) {
            PhyRange range = { curEntry->begin, curEntry->end };
            while ((curEntry = memoryMap.FindNextMemoryEntry(curEntry, memoryMap.All)) and range.end == curEntry->begin) {
                range.end = curEntry->end;
            }
            instance = (new(instance) RangeInstance(range, bitmapOffset, CalcMaxLevel(range))) + 1;
            bitmapOffset += ((range.end - range.begin) >> PageBoundBits) + 1;
        }
        instances = as<RangeInstance*>(metaPtr);
        instancesEnd = instance;

        metaPtr = ptr_cast<void*>(alignUp(ptr_cast<std::uintptr_t>(instance), alignof(LevelTree)));
        auto level = ptr_cast<LevelTree*>(metaPtr);
        for (int i = 0; i < levelCount; ++i) {
            level = (new(level) LevelTree()) + 1;
        }
        levels = as<LevelTree*>(metaPtr);

        metaPtr = ptr_cast<void*>(alignUp(ptr_cast<std::uintptr_t>(level), alignof(std::uint32_t)));
        auto bitmapBlockPtr = ptr_cast<std::uint32_t*>(metaPtr);
        for (std::uint64_t i = 0; i < bitmapWords; ++i) {
            bitmapBlockPtr = (new(bitmapBlockPtr) std::uint32_t(0)) + 1;
        }
        bitmap = as<std::uint32_t*>(metaPtr);

        curEntry = bpmm.current;
        PhyRange range{ curEntry->begin, bpmm.boundary };
        if (curEntry == memmapEntry) {
            range.end -= metaSize;
        }
        FreeRange(range);
        while ((curEntry = memoryMap.FindPrevMemoryEntry(curEntry))) {
            range = { curEntry->begin, bpmm.boundary };
            if (curEntry == memmapEntry) {
                range.end -= metaSize;
            }
            FreeRange(range);
        }
    }

    auto AllocPage() -> std::uint64_t override
    {
        return AllocRange(PageSize).begin;
    }

    void FreePage(std::uint64_t page) override
    {
        return FreeRange({ page, page + PageSize });
    }

    auto AllocRange(std::uint64_t size) -> PhyRange override
    {
        auto level = Log2U64((size - 1) * 2);
        auto startPage = AllocBlock(std::max(0, level - PageBoundBits));
        if (startPage == InvalidPage) {
            return {};
        }
        return { startPage, std::uint64_t(1 << level) };
    }

    void FreeRange(const PhyRange& range) override
    {
        if ((range.begin | range.end) & PageMask) {
            return;
        }
        auto& instance = GetInstanceByRange(range);
        auto end = range.end;
        while (range.begin < end) {
            auto level = std::min(ctz64(end), Log2U64(end - range.begin));
            auto size = 1 << level;
            level -= PageBoundBits;
            end -= size;
            FreeBlock(instance, level, end);
        }
    }

    static auto Instance() -> PMM&
    {
        static PMM alloc;
        return alloc;
    }
private:
    struct InstancesInfo
    {
        std::size_t count;
        std::uint64_t pagesTotal;
        int maxLevel;
    };

    static auto GetInstancesInfo() -> InstancesInfo
    {
        InstancesInfo info = {};
        const kernel_MemoryMapEntry* entry = memoryMap.FindNextMemoryEntry(nullptr, memoryMap.All);
        if (entry == nullptr) {
            return info;
        }
        std::uint64_t begin = entry->begin;
        std::uint64_t end = entry->begin;
        do {
            if (end != entry->begin) {
                info.count += 1;
                info.maxLevel = std::max(info.maxLevel, CalcMaxLevel({begin, end}));
                begin = entry->begin;
            }
            info.pagesTotal += (entry->end - entry->begin) >> PageBoundBits;
            end = entry->end;
        } while ((entry = memoryMap.FindNextMemoryEntry(entry, memoryMap.All)));
        info.count += 1;
        info.maxLevel = std::max(info.maxLevel, CalcMaxLevel({begin, end}));
        return info;
    }

    struct RangeInstance
    {
        PhyRange range;
        uint64_t bitmapOffset;
        int maxLevel;
    };

    struct InstanceEndComp
    {
        bool operator()(const RangeInstance& a, const RangeInstance& b)
        {
            return a.range.end < b.range.end;
        }
        bool operator()(const std::uint64_t& a, const RangeInstance& b)
        {
            return a < b.range.end;
        }
        bool operator()(const RangeInstance& a, const std::uint64_t& b)
        {
            return a.range.end < b;
        }
    };

    struct BlockHeader : intrusive::AVLTreeNode<>
    {
        BlockHeader(const RangeInstance& owner) : owner(owner) {}
        const RangeInstance& owner;
    };

    struct BlockComp
    {
        bool operator()(const BlockHeader& a, const BlockHeader& b)
        {
            return Mapper::GetPhyAddr(&a) > Mapper::GetPhyAddr(&b);
        }
        bool operator()(const std::uint64_t& a, const BlockHeader& b)
        {
            return a > Mapper::GetPhyAddr(&b);
        }
        bool operator()(const BlockHeader& a, const std::uint64_t& b)
        {
            return Mapper::GetPhyAddr(&a) > b;
        }
    };

    bool ToggleBit(std::uint64_t bit) const
    {
        auto index = bit / 32;
        auto bitMask = std::uint32_t(1) << (bit % 32);
        return (bitmap[index] ^= bitMask) & bitMask;
    }

    bool TogglePair(const RangeInstance& instance, int level, std::uint64_t block) const
    {
        if (level == instance.maxLevel) {
            return true;
        }
        auto pageNum = (block - instance.range.begin) >> PageBoundBits;
        auto pagesPerPairBlock = std::uint64_t(1) << level;
        return ToggleBit(instance.bitmapOffset + pageNum + pagesPerPairBlock);
    }

    void FreeBlock(const RangeInstance& instance, int level, std::uint64_t block)
    {
        while (!TogglePair(instance, level, block)) {
            auto neighbour = block ^ ((std::uint64_t)1 << (PageBoundBits + level));
            auto nblk = as<BlockHeader*>(Mapper::GetObject(neighbour));
            levels[level].Erase(levels[level].IteratorTo(*nblk));
            nblk->~BlockHeader();
            block &= neighbour;
            level += 1;
        }
        auto hblk = new(Mapper::GetObject(block)) BlockHeader(instance);
        levels[level].Insert(*hblk);
    }

    auto AllocBlock(int level) -> std::uint64_t
    {
        for (int i = level; i < levelCount; ++i) {
            if (levels[i].Empty()) {
                continue;
            }
            auto it = levels[i].Begin();
            auto hblk = it.operator->();
            auto& instance = hblk->owner;
            levels[i].Erase(it);
            hblk->~BlockHeader();
            auto blk = Mapper::GetPhyAddr(hblk);
            TogglePair(instance, i, blk);
            while (i-- > level) {
                auto hblk = new(Mapper::GetObject(blk)) BlockHeader(instance);
                TogglePair(instance, i, blk);
                levels[i].Insert(*hblk);
                blk += (std::uint64_t)1 << (PageBoundBits + i);
            }
            return blk;
        }
        return InvalidPage;
    }

    static int CalcMaxLevel(const PhyRange& range)
    {
        auto maxLevel = Log2U64(range.end - range.begin);
        if (alignUp(range.begin, 1 << maxLevel) == alignDown(range.end, 1 << maxLevel)) {
            maxLevel -= 1;
        }
        return std::max(0, maxLevel - PageBoundBits);
    }

    struct InvalidRegionException : std::exception
    {
        const char* what() const noexcept override
        {
            return "Range out of bounds";
        }
    };

    auto GetInstanceByRange(const PhyRange& range) -> RangeInstance&
    {
        InstanceEndComp comp;
        auto it = std::lower_bound(instances, instancesEnd, range.end, comp);
        if (it == instancesEnd or range.begin < it->range.begin) {
            throw InvalidRegionException();
        }
        return *it;
    }

    using InstanceTree = intrusive::AVLTree<RangeInstance, InstanceEndComp>;
    using LevelTree = intrusive::AVLTree<BlockHeader, BlockComp>;
    RangeInstance* instances;
    RangeInstance* instancesEnd;
    LevelTree* levels;
    std::uint32_t* bitmap;
    int levelCount;
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
        if (pmm.current == nullptr) {
            std::terminate();
        }
        Mapper::Init(pmm);
        return {{
            vmm,
            std::move(pmm)
        }, vmm};
    }

    Allocator(BuddyAlloc&& buddy, const BasicVMM& vmm) :
        pmm(buddy),
        vmm(pmm, vmm)
    {}

    // static auto Instance() -> Allocator&
    // {
    //     static Allocator alloc = Init();
    //     return alloc;
    // }

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
    // if (memPool.empty()) [[unlikely]] {
    //     auto& alloc = Allocator::Instance().pmm;
    //     if (!Mapper::MapWithAlloc(r.begin, PageSize, &alloc)) {
    //         std::terminate();
    //     }
    //     memPool.add_storage(kernel::ptr_cast<void*>(r.begin));
    //     r.begin += PageSize;
    //     if (r.begin == r.end) {
    //         return true;
    //     }
    // }
    return false;
}

} // namespace

static void print_memmap()
{
    namespace d = debug;
    d::println("[Memory Map]");
    char buf[17];
    const char* sTypes[] = {
        "kernel_MemoryMapEntryType_ReservedMemory",
        "kernel_MemoryMapEntryType_AvailableMemory",
        "kernel_MemoryMapEntryType_BootReclaimable",
        "kernel_MemoryMapEntryType_SystemReclaimable",
        "kernel_MemoryMapEntryType_Kernel",
        "kernel_MemoryMapEntryType_Inherited",
    };
    for (std::uint64_t i = 0; i < memoryMap.count; ++i) {
        kernel::UToStr(buf, 17, memoryMap.map[i].begin, 16);
        d::puts("0x"); d::puts(buf); d::puts(" - ");
        kernel::UToStr(buf, 17, memoryMap.map[i].end, 16);
        d::puts("0x"); d::puts(buf); d::puts(": ");
        d::puts(sTypes[memoryMap.map[i].type]);
        d::putc('\n');
    }
    d::putc('\n');
}

int InitAllocator()
{
    memoryMap.FindMemoryMap(loaderData);
    // print_memmap();
    // auto& alloc = Allocator::Instance();
    // zeroPage = alloc.pmm.AllocPage();
    // struct RgCheckDealloc : InvalidPageAlloc
    // {
    //     RgCheckDealloc(Allocator& alloc) :
    //         alloc(alloc)
    //     {
    //         auto entries = memoryMap.map;
    //         count = memoryMap.count;
    //         this->entries = new kernel_MemoryMapEntry[count];
    //         std::memcpy(this->entries, entries, sizeof(kernel_MemoryMapEntry) * count);
    //     }
    //     ~RgCheckDealloc()
    //     {
    //         delete[] entries;
    //     }
    //     void FreePage(std::uint64_t page) override
    //     {
    //         if (!IsAvailRg(entries, count, page)) {
    //             return;
    //         }
    //         alloc.pmm.FreePage(page);
    //     }
    //     Allocator& alloc;
    //     kernel_MemoryMapEntry* entries;
    //     std::ptrdiff_t count;
    // } dealloc(alloc);
    // loaderData = nullptr;
    // Mapper::UnmapWithAlloc(0, 0x100000, &dealloc, &alloc.pmm);
    auto& alloc = PMM::Instance();
    zeroPage = alloc.AllocPage();
    for (auto entry = memoryMap.map; entry != memoryMap.map + memoryMap.count; entry += 1) {
        if (entry->type != kernel_MemoryMapEntryType_BootReclaimable) {
            continue;
        }
        alloc.FreeRange({entry->begin, entry->end});
    }
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
    auto& alloc = PMM::Instance();
    auto range = alloc.AllocRange(size);
    size = range.end - range.begin;
    if (size == 0) [[unlikely]] {
        return nullptr;
    }
    auto ptr = ptr_cast<unsigned char*>(Mapper::GetObject(range.begin));
    new(ptr) std::ptrdiff_t(size);
    return ptr + HeaderReserve;
}

extern "C" void free(void* p)
{
    if (p == nullptr) {
        return;
    }
    constexpr auto HeaderReserve = alignof(max_align_t);
    auto ptr = kernel::as<std::ptrdiff_t*>(ptr_cast<unsigned char*>(p) - HeaderReserve);
    auto phyBegin = Mapper::GetPhyAddr(ptr);
    auto& alloc = PMM::Instance();
    alloc.FreeRange({phyBegin, phyBegin + *ptr});
}

extern "C" void* realloc(void* p, size_t newSize)
{
    auto oldSize = [&p]() -> std::ptrdiff_t {
        constexpr auto HeaderReserve = alignof(max_align_t);
        if (p == nullptr) {
            return 0;
        }
        auto ptr = ptr_cast<unsigned char*>(p) - HeaderReserve;
        return *kernel::as<std::ptrdiff_t*>(ptr) - HeaderReserve;
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
