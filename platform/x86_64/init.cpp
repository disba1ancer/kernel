#include <cstdlib>
#include "kernel/bootdata.h"
#include "alloc.h"

int kmain();

namespace kernel::tgtspec {

extern "C" void kernel_x86_64_EnableBasicInterrupts(void) noexcept;
int InitAllocator(void);
extern "C" void kernel_x86_64_EnableIRQs(void) noexcept;
extern "C" void _init();

const kernel_LdrData* loaderData;

extern "C" [[noreturn]] void cpp_start(const kernel_LdrData *data) noexcept
{
    loaderData = data;
    kernel_x86_64_EnableBasicInterrupts();
    InitAllocator();
    kernel_x86_64_EnableIRQs();
    try {
        _init();
        std::exit(kmain());
    } catch (...) {
        std::abort();
    }
}

}
