#include "kernel/bootdata.h"
#include "alloc.h"
#include "segment.h"

void kernel_x86_64_EnableInterrupts(void);
_Noreturn void cpp_start(void);

const kernel_LdrData* kernel_LoaderData;

struct LdrState {
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
};

_Noreturn void c_start(const kernel_LdrData *data, struct LdrState *state)
{
    kernel_LoaderData = data;
    InitGDT();
    kernel_InitAllocator();
    kernel_x86_64_EnableInterrupts();
    cpp_start();
}
