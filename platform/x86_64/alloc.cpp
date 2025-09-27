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

#include <exception>
#include <new>
#include "kernel/bootdata.h"
#include "kernel/util.hpp"
#include "kernel/util.h"
#include "kernel/avl_tree.hpp"
#include "kernel/list.hpp"
#include "processor.h"
#include <cstring>

using kernel::ptr_cast;

namespace {

constexpr auto PageSize = 4096;
constexpr auto PageMask = PageSize - 1;
constexpr std::uint64_t InvalidPage = -1;

} // namespace

extern "C" const kernel_LdrData* kernel_LoaderData;
extern "C" x86_64_PageEntry __mapping_window[];
extern "C" alignas(PageSize) unsigned char __smheap_start[];
extern "C" alignas(PageSize) unsigned char __smheap_end[];

namespace {

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

auto FindFirstAvailableEntry(const kernel_MemoryMapEntry* entries, std::uint64_t count)
    -> const kernel_MemoryMapEntry*
{
    for (uint64_t i = 0; i < count; ++i) {
        if (entries[i].type == kernel_MemoryMapEntryType_AvailableMemory &&
            (entries[i].flags & 0xF) == 1)
        {
            return entries + i;
        }
    }
    return NULL;
}

auto FindBoundaryEntry(
        const kernel_MemoryMapEntry *entries,
        std::uint64_t count,
        std::uint64_t boundary
)
-> std::ptrdiff_t {
    auto end = entries + count;
    auto entry = FindFirstAvailableEntry(entries, count);
    while (entry != end && !(boundary - entry->begin <= entry->size)) {
        ++entry;
    }
    return entry - entries;
}

auto CanonizeAddr(std::uintptr_t addr) -> std::uintptr_t
{
    return ((addr & 0xFFFFFFFFFFFF) ^ 0x800000000000) - 0x800000000000;
}

struct ISinglePageAlloc {
    virtual auto alloc() -> std::uint64_t = 0;
    virtual void free(std::uint64_t) = 0;
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
    {
        auto entry = x86_64_LoadCR3();
        entry = x86_64_MakePageEntry(
            x86_64_PageEntry_GetAddr(entry),
            x86_64_PageEntryFlag_Present | x86_64_PageEntryFlag_Write,
            0
        );
        __mapping_window[511] = entry;
        x86_64_FlushPageTLB(&__mapping_window);
        __mapping_window[256] = entry;
        __asm__ volatile("":::"memory");
        UnmapUnsafe(&__mapping_window);
    }
    static auto Entry(std::ptrdiff_t index) -> x86_64_PageEntry&
    {//0400400400400
        return *(ptr_cast<x86_64_PageEntry*>(PageTableAddr) + index);
    }
    static auto IndexOf(void* addr) -> std::ptrdiff_t
    {
        return IndexOf(ptr_cast<std::uintptr_t>(addr));
    }
    static auto IndexOf(std::uintptr_t addr) -> std::ptrdiff_t
    {
        return (addr / PageSize) & IndexMask;
    }
    static auto EntryByAddr(void* addr) -> x86_64_PageEntry&
    {
        return Entry(IndexOf(addr));;
    }
    static auto EntryByAddr(std::uintptr_t addr) -> x86_64_PageEntry&
    {
        return Entry(IndexOf(addr));
    }
    static void InvalidateSingle(std::ptrdiff_t index)
    {
        auto addr = CanonizeAddr(std::uintptr_t(index) * PageSize);
        x86_64_FlushPageTLB(ptr_cast<void*>(addr));
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
        Entry(index) = x86_64_MakePageEntry(
            newPage, x86_64_PageEntryFlag_Present | x86_64_PageEntryFlag_Write);
    }
    static auto Reset(std::ptrdiff_t index) -> uint64_t
    {
        auto& t = Entry(index);
        auto ptr = x86_64_PageEntry_GetAddr(t);
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
        EntryByAddr(vAddr) = x86_64_MakePageEntry(
            pAddr, x86_64_PageEntryFlag_Present | x86_64_PageEntryFlag_Write);
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
        if (end > begin) {
            auto addr = CanonizeAddr(std::uintptr_t(begin) * PageSize);
            std::memset(ptr_cast<void*>(addr), 0, PageSize);
            addr = CanonizeAddr(std::uintptr_t(end - 1) * PageSize);
            std::memset(ptr_cast<void*>(addr), 0, PageSize);
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
            if (t.data & x86_64_PageEntryFlag_Present) {
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
        if (Entry(begin).data & x86_64_PageEntryFlag_Present) {
            ++begin;
        }
        if (Entry(end - 1).data & x86_64_PageEntryFlag_Present) {
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
        ISinglePageAlloc *alloc, ISinglePageAlloc *ptAlloc)
    {
        Args args = {};
        args.alloc = ptAlloc;
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
        ISinglePageAlloc *alloc, ISinglePageAlloc *ptAlloc)
    {
        Mapper::Args args = {};
        args.alloc = ptAlloc;
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
        auto memmap = FindMemoryMap(kernel_LoaderData);
        auto entries = ptr_cast<CMapEntry*>(memmap->entries);
        boundary = memmap->allocatedBoundary;
        count = memmap->count;
        map = entries;
        current = FindBoundaryEntry(map, count, boundary);
    }
    auto alloc() -> std::uint64_t
    {
        if (lastFree != InvalidPage) {
            auto result = lastFree;
            auto ptr = ptr_cast<std::uintptr_t>(&__mapping_window);
            auto next = ptr_cast<std::uint64_t*>(Mapper::MapUnsafe(ptr, result));
            lastFree = *std::launder(next);
            Mapper::UnmapUnsafe(next);
            return result;
        }
        auto cEnd = map[current].begin + map[current].size;
        if (cEnd == boundary) {
            if (current == count) {
                return InvalidPage;
            }
            boundary = map[++current].begin;
        }
        auto result = boundary;
        boundary += PageSize;
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

template <typename T, typename ... TT>
struct max_size
{
    static constexpr auto value = kernel::Max(sizeof(T), max_size<TT...>::value);
};

template <typename ... T>
constexpr auto max_size_v = max_size<T...>::value;

template <class T, class M>
constexpr T reset_bits(T in, M bitmask)
{
    return in ^ (bitmask & in);
}

struct address_node {
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

} // namespace

namespace kernel::intrusive {

template <>
struct AVLTreeNodeTraits<address_node>
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

template <>
struct AVLTreeNodeTraits<size_node>
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

} // namespace kernel::intrusive

namespace  {

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
        auto av = reinterpret_cast<std::uintptr_t>(kernel::AddressOf(a));
        auto bv = reinterpret_cast<std::uintptr_t>(kernel::AddressOf(b));
        return av < bv;
    }
    bool operator()(void* const& a, const free_range& b) const
    {
        auto av = reinterpret_cast<std::uintptr_t>(a);
        auto bv = reinterpret_cast<std::uintptr_t>(kernel::AddressOf(b));
        return av < bv;
    }
    bool operator()(const free_range& a, void* const& b) const
    {
        auto av = reinterpret_cast<std::uintptr_t>(kernel::AddressOf(a));
        auto bv = reinterpret_cast<std::uintptr_t>(b);
        return av < bv;
    }
};

} // namespace

namespace kernel::intrusive {

template <>
struct AVLTreeNodeTraits<free_range>
{
    static auto GetParent(const free_range& node) -> free_range*
    {
        return ptr_cast<free_range*>(node.parent ^ (3 & node.parent));
    }
    static void SetParent(free_range& node, free_range* parent)
    {
        node.parent = (node.parent & 3) | ptr_cast<std::uintptr_t>(parent);
    }
    static auto GetChild(const free_range& node, bool right) -> free_range*
    {
        return node.children[right];
    }
    static void SetChild(free_range& node, bool right, free_range* child)
    {
        node.children[right] = child;
    }
    static  int GetBalance(const free_range& node)
    {
        return node.parent & 3;
    }
    static void SetBalance(free_range& node, int balance)
    {
        node.parent = (node.parent ^ (3 & node.parent)) | balance;
    }
};

template <>
struct ListNodeTraits<free_range> {
    using NodeType = free_range;
    using SentinelType = free_range;
    static auto GetNext(NodeType& node) -> NodeType* {
        return node.children[1];
    }
    static void SetNext(NodeType& node, NodeType* next) {
        node.children[1] = next;
    }
    static auto GetPrev(NodeType& node) -> NodeType* {
        return node.children[0];
    }
    static void SetPrev(NodeType& node, NodeType* prev) {
        node.children[0] = prev;
    }
};

} // namespace kernel::intrusive

namespace  {

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

    static constexpr auto max_align = kernel::Max(alignof(T), alignof(free_obj));
    static constexpr auto max_size = kernel::Max(sizeof(T), sizeof(free_obj));
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

struct VMM
{
    struct mem_range
    {
        std::uintptr_t begin;
        std::uintptr_t end;
    };
    struct free_range : address_node, size_node {};

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
            sizeTree.Erase(prevNode);
            if (crBegin != addressTree.End() && r.end == crBegin->get_address()) {
                prevNode.set_size(r.end - prev->get_address() + crBegin->get_size());
                Erase(*crBegin);
            } else {
                prevNode.set_size(r.end - prev->get_address());
            }
            sizeTree.Insert(prevNode);
            return;
        }
        if (crBegin != addressTree.End() && r.end == crBegin->get_address()) {
            auto& prevNode = *crBegin;
            sizeTree.Erase(prevNode);
            prevNode.set_size(r.end + prevNode.get_size() - r.begin);
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
        r.end = kernel::Max(r.begin, r.end);
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
        if (memPool.empty()) [[unlikely]] {
            if (!Mapper::MapWithAlloc(r.begin, PageSize, &pageAlloc, &pageAlloc)) {
                std::terminate();
            }
            memPool.add_storage(kernel::ptr_cast<void*>(r.begin));
            r.begin += PageSize;
            if (r.begin == r.end) {
                return;
            }
        }
        auto node = memPool.alloc();
        node->set_address(r.begin);
        node->set_size(r.end - r.begin);
        Insert(*node);
    }

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
private:
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

public:
    SinglePagePMM pageAlloc;
private:
    chunked_mem_pool<free_range, PageSize> memPool;
    using address_tree_t = kernel::intrusive::AVLTree<free_range, address_comp, kernel::intrusive::BaseClassCastPolicy<address_node, free_range>>;
    using size_tree_t = kernel::intrusive::AVLTree<free_range, size_comp, kernel::intrusive::BaseClassCastPolicy<size_node, free_range>>;
    address_tree_t addressTree;
    size_tree_t sizeTree;
};

auto AllocInstance() -> VMM&
{
    static VMM alloc;
    return alloc;
}

} // namespace

extern "C" int kernel_InitAllocator()
{
    Mapper::Init();
    auto& alloc = AllocInstance();
    auto begin = ptr_cast<std::uintptr_t>(__smheap_start);
    auto end = ptr_cast<std::uintptr_t>(__smheap_end);
    alloc.ReleaseRange({begin, end});
    alloc.ReleaseRange({-(std::uintptr_t)0x7FF000000000, -(std::uintptr_t)0x80000000});
    return 0;
}

extern "C" void* malloc(size_t size)
{
    constexpr auto HeaderReserve = alignof(max_align_t);
    static_assert(sizeof(std::uintptr_t) <= HeaderReserve);
    auto& alloc = AllocInstance();
    size += HeaderReserve;
    auto range = alloc.AcquireRange(size);
    if (range.begin == range.end) [[unlikely]] {
        return nullptr;
    }
    if (!Mapper::MapWithAlloc(range.begin, range.end - range.begin, &alloc.pageAlloc, &alloc.pageAlloc)) [[unlikely]] {
        return nullptr;
    }
    auto ptr = ptr_cast<unsigned char*>(range.begin);
    new(ptr) std::uintptr_t(range.end);
    return ptr + HeaderReserve;
}

extern "C" void* realloc(void* p, size_t newSize)
{
    constexpr auto HeaderReserve = alignof(max_align_t);
    auto ptr = ptr_cast<unsigned char*>(p) - HeaderReserve;
    VMM::mem_range range{ptr_cast<std::uintptr_t>(ptr), *kernel::As<std::uintptr_t*>(ptr)};
    auto oldSize = range.end - range.begin - HeaderReserve;
    auto newPtr = malloc(newSize);
    memcpy(newPtr, p, kernel::Min(oldSize, newSize));
    free(p);
    return newPtr;
}

extern "C" void free(void* p)
{
    constexpr auto HeaderReserve = alignof(max_align_t);
    auto ptr = ptr_cast<unsigned char*>(p) - HeaderReserve;
    VMM::mem_range range{ptr_cast<std::uintptr_t>(ptr), *kernel::As<std::uintptr_t*>(ptr)};
    auto& alloc = AllocInstance();
    Mapper::UnmapWithAlloc(range.begin, range.end - range.begin, &alloc.pageAlloc, &alloc.pageAlloc);
    alloc.ReleaseRange(range);
}
