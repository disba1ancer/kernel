/**
 * @file alloc.h
 * @brief Allocation functions declarations
 */

#ifndef ALLOC_H
#define ALLOC_H

#include <cstdint>

namespace kernel::tgtspec {

struct PhysicalRange {
    union {
        std::uint64_t start;
        int error;
    };
    std::uint64_t size;
};

struct VirtualRange {
    union {
        void* start;
        int error;
    };
    std::ptrdiff_t size;
};

struct PageMM {
    PhysicalRange (*PAlloc)(PageMM* mm, void* helperPage, std::size_t size);
};

enum FlagsGeneric {
    Reserve = 1,

    Alloc = 2,
    Map = 4,
    AllocMask = 6,

    Write = 8,
    Execute = 16,
    System = 32,

    NoCache = 64,
    WriteCombine = 128,
    CacheMask = 192,

    ModeMask = 248,
};

/**
 * @brief `valloc`
 * Allocates at least `size` bytes of memory or
 * maps specified physical address range.
 *
 * @param `size`
 * Size of requested region. For memory allocations,
 * this argument is automatically aligned up to
 * a multiple of the page size.
 *
 * @param `flags`
 * Memory and allocation flags.
 *
 * @param `psyaddr`
 * If is not 0, defines maximum physical address of
 * contiguous allocation end.
 *
 * @return
 * On error, returns zero size virtual range, where
 * `error` member stores error code. On success, returns
 * start virtual address of allocated region in `start`
 * member and size of the region in `size`.
 */
auto valloc(std::ptrdiff_t size, int flags, std::uint64_t phyaddr) -> VirtualRange;
void vmod(void* addr, std::ptrdiff_t size, int flags);
void vfree(void* addr, std::ptrdiff_t size, int flags);

} // namespace kernel::tgtspec

#endif // ALLOC_H
