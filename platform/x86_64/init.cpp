#include <stdlib.h>

extern "C" void _init();

int kmain();

extern "C" [[noreturn]] void cpp_start(const void *data)
{
    try {
        _init();
        exit(kmain());
    } catch (...) {
        abort();
    }
}
