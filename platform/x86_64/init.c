#include <stdlib.h>
#include "kernel/bootdata.h"
#include "alloc.h"

void kernel_x86_64_EnableInterrupts(void);
_Noreturn void cpp_start(void);

const kernel_LdrData* kernel_LoaderData;

_Noreturn void c_start(const kernel_LdrData *data)
{
    kernel_LoaderData = data;
    kernel_InitAllocator();
    kernel_x86_64_EnableInterrupts();
    cpp_start();
}
