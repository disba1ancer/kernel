#include "segment.h"

i686_Descriptor gdt[] = {
    {0},
    i686_internal_MakeSegDescriptor(0, 0xFFFFF, 3, i686_DescFlag_LimitIn4K | i686_DescFlag_OP32 | i686_DescFlag_Present | i686_SegType_ExecRead),
    i686_internal_MakeSegDescriptor(0, 0xFFFFF, 0, i686_DescFlag_LimitIn4K | i686_DescFlag_OP32 | i686_DescFlag_Present | i686_SegType_ReadWrite),
    i686_internal_MakeSegDescriptor(0, 0xFFFFF, 3, i686_DescFlag_LimitIn4K | i686_DescFlag_OP32 | i686_DescFlag_Present | i686_SegType_ReadWrite),
    i686_internal_MakeSegDescriptor(0, 0xFFFFF, 0, i686_DescFlag_LimitIn4K | i686_DescFlag_Long | i686_DescFlag_Present | i686_SegType_ExecRead),
    i686_internal_MakeSegDescriptor(0, 0xFFFFF, 3, i686_DescFlag_LimitIn4K | i686_DescFlag_Long | i686_DescFlag_Present | i686_SegType_ExecRead)
};

x86_64_GDTR gdtr = { .gdt = gdt, .limit = sizeof(gdt) - 1 };

void InitGDT(void)
{
    x86_64_LoadGDT(&gdtr);
    __asm__ volatile(
"rex.w  ljmp    *0f(%%rip)\n"
"0:     .quad   0f\n"
"       .word   0x20\n"
"0:     mov     %0, %%ds\n"
"       mov     %0, %%ss\n"
"       mov     %0, %%es\n"
"       mov     %0, %%fs\n"
"       mov     %0, %%gs\n"
::"r"(0x10));
}
