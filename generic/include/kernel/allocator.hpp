#ifndef KERNEL_ALLOCATOR_H
#define KERNEL_ALLOCATOR_H

#include "avl_tree.hpp"
#include "node.hpp"
#include "util.h"

namespace kernel::memory {

namespace allocator_impl {
using kernel::intrusive::AVLTree;
using kernel::intrusive::IdentityCastPolicy;
}

enum RegionType {
    Free,
    SmallFree,
    Allocated,
    BigAllocated
};

template <typename T>
struct AllocatorRegionTraits;

template <typename T>
class Allocator {
    using Region = T;
    using RgTr = AllocatorRegionTraits<T>;
    using FreeHeader = typename RgTr::FreeHeader;
    static constexpr auto ChunkTreshold = RgTr::ChunkTreshold;
    static constexpr auto ChunkSize = RgTr::ChunkSize;

    struct Comparator {
        constexpr
        bool operator()(const FreeHeader& a, const FreeHeader& b) const
        {
            auto aSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(a))));
            auto bSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(b))));
            return aSize < bSize;
        }
        constexpr
        bool operator()(const FreeHeader& a, std::size_t size) const
        {
            auto aSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(a))));
            return aSize < size;
        }
        constexpr
        bool operator()(std::size_t size, const FreeHeader& b) const
        {
            auto bSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(b))));
            return size < bSize;
        }
    };

    using SizeTree = allocator_impl::AVLTree<
        FreeHeader, Comparator, allocator_impl::IdentityCastPolicy<FreeHeader>
    >;

    auto AllocateChunked(std::size_t size, std::size_t align) -> Region*
    {
        auto rgnIt = freeList.LowerBound(size + align - RgTr::ChunkGranularity);
        Region* rgn;
        if (rgnIt == freeList.End()) {
            rgn = RgTr::AllocateChunk(ChunkSize, RgTr::ChunkGranularity);
            if (rgn == nullptr) {
                return nullptr;
            }
            rgn = RgTr::Retype(rgn, RegionType::SmallFree);
        } else {
            rgn = RgTr::FromFreeHeader(rgnIt.operator->());
            freeList.Erase(rgnIt++);
        }
        auto offset = ptr_cast<std::uintptr_t>(rgn) & (align - 1);
        auto allocStartOffset = align - offset - RgTr::ChunkGranularity;
        if (allocStartOffset > 0) {
            rgn = RgTr::Split(rgn, allocStartOffset);
            if (RgTr::GetType(rgn) == RegionType::Free) {
                freeList.Insert(rgnIt, *RgTr::AsFreeHeader(rgn));
            }
            rgn = RgTr::GetNext(rgn);
        }
        if (RgTr::GetSize(rgn) > size) {
            rgn = RgTr::Split(rgn, size);
            auto rgn2 = RgTr::GetNext(rgn);
            if (RgTr::GetType(rgn2) == RegionType::Free) {
                freeList.Insert(rgnIt, *RgTr::AsFreeHeader(rgn2));
            }
        }
        return RgTr::Retype(rgn, RegionType::Allocated);
    }

    void DeallocateChunked(Region* rgn)
    {
        rgn = RgTr::Retype(rgn, RegionType::Free);
        auto neightbour = RgTr::GetPrev(rgn);
        auto neightbourType = RgTr::GetType(neightbour);
        if (neightbour != rgn && neightbourType != RegionType::Allocated) {
            if (neightbourType == RegionType::Free) {
                freeList.Erase(*RgTr::AsFreeHeader(neightbour));
            }
            rgn = RgTr::MergeWithNext(neightbour);
        }
        neightbour = RgTr::GetNext(rgn);
        neightbourType = RgTr::GetType(neightbour);
        if (neightbour != rgn && neightbourType != RegionType::Allocated) {
            if (neightbourType == RegionType::Free) {
                freeList.Erase(*RgTr::AsFreeHeader(neightbour));
            }
            rgn = RgTr::MergeWithNext(rgn);
        }
        auto size = RgTr::GetSize(rgn);
        if (size >= ChunkSize) {
            RgTr::DeallocateChunk(rgn);
        } else {
            freeList.Insert(*RgTr::AsFreeHeader(rgn));
        }
    }

    bool IsPOT(std::size_t val)
    {
        return !((val - 1) & val);
    }

    void* AllocateChecked(std::size_t size, std::size_t align) {
        size += RgTr::ChunkGranularity;
        Region* rgn;
        if (size < ChunkTreshold) {
            rgn = AllocateChunked(size, align);
        } else {
            rgn = RgTr::AllocateChunk(size, align);
        }
        if (rgn == nullptr) {
            return nullptr;
        }
        auto ptr = ApplyOffset<unsigned char>(rgn, RgTr::ChunkGranularity);
        return new(ptr) unsigned char[size];
    }
public:
    Allocator()
    {}
    void* Allocate(std::size_t size, std::size_t align)
    {
        if (size == 0) {
            return nullptr;
        }
        align = Max(align, std::size_t(RgTr::ChunkGranularity));
        size += RgTr::ChunkGranularity - 1;
        size &= size ^ (RgTr::ChunkGranularity - 1);
        if (!IsPOT(align) || size & (align - 1)) {
            return nullptr;
        }
        return AllocateChecked(size, align);
    }
    void Deallocate(void* ptr)
    {
        if (ptr == nullptr) {
            return;
        }
        auto rgn = ApplyOffset<Region>(ptr, -std::ptrdiff_t(RgTr::ChunkGranularity)); // TODO: Can region be small?
        if (RgTr::GetType(rgn) == RegionType::BigAllocated) {
            RgTr::DeallocateChunk(rgn);
        } else {
            DeallocateChunked(rgn);
        }
    }
    /*void DumpList()
    {
        std::cout << "Dump\n";
        for (auto &elem : freeList) {
            auto rgn = RgTr::FromFreeHeader(AddressOf(elem));
            std::cout << "Pointer: " << rgn;
            std::cout << "\nSize: " << RgTr::GetSize(rgn);
            std::cout << "\nPrev size: " << RgTr::GetPrevSize(rgn) << "\n\n";
        }
        flush(std::cout);
    }*/
    void PushChunk(void* ptr)
    {
        auto rgn = RgTr::AllocateIdentity(ptr);
        rgn = RgTr::Retype(rgn, RegionType::Free);
        freeList.Insert(ptr_cast<FreeHeader*>(rgn));
    }
private:
    SizeTree freeList;
};

}

#endif // KERNEL_ALLOCATOR_H
