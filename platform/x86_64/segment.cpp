#include "segment.h"

namespace kernel::tgtspec {

namespace {
using enum i686::DescFlag;
using enum i686::SegTypeFlag;
using i686::MakeSegDescriptor;
}

i686::Descriptor gdt[] = {
    {},
    MakeSegDescriptor(0, 0xFFFFF, 0, DescFlag_LimitIn4K | DescFlag_Long | DescFlag_Present | SegType_ExecRead),
    MakeSegDescriptor(0, 0xFFFFF, 0, DescFlag_LimitIn4K | DescFlag_OP32 | DescFlag_Present | SegType_ReadWrite),
    MakeSegDescriptor(0, 0xFFFFF, 3, DescFlag_LimitIn4K | DescFlag_Long | DescFlag_Present | SegType_ExecRead),
    MakeSegDescriptor(0, 0xFFFFF, 3, DescFlag_LimitIn4K | DescFlag_OP32 | DescFlag_Present | SegType_ReadWrite),
    MakeSegDescriptor(0, 0xFFFFF, 3, DescFlag_LimitIn4K | DescFlag_OP32 | DescFlag_Present | SegType_ExecRead),
};

extern "C" x86_64::GDTR gdtr = { .limit = sizeof(gdt) - 1, .gdt = gdt };

} // namespace kernel::tgtspec
