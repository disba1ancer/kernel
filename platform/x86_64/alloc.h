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
    std::size_t size;
};

struct PageMM {
    PhysicalRange (*PAlloc)(PageMM* mm, void* helperPage, std::size_t size);
    VirtualRange (*VAlloc)(PageMM* mm, void* page, std::size_t size, int flags, std::uint64_t pArgs);
};

} // namespace kernel::tgtspec

#endif // ALLOC_H
