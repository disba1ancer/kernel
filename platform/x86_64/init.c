#include <stdlib.h>
#include "kernel/bootdata.h"

void kernel_x86_64_EnableInterrupts(void);
_Noreturn void cpp_start(const void *data);

void c_start(const kernel_LdrData *data)
{
    kernel_x86_64_EnableInterrupts();
    cpp_start(data);
}
