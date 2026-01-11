#include "kernel/debug.h"

namespace kernel::debug {

void putc(char ch)
{
#ifndef NDEBUG
    asm volatile("out %0, $0xe9"::"a"(ch));
#endif
}

void puts(std::string_view str)
{
    for (auto&& ch : str) {
        putc(ch);
    }
}

void puts(const char* cstr)
{
    while (*cstr) {
        putc(*(cstr++));
    }
}

void println(std::string_view str)
{
    puts(str);
    putc('\n');
}

void println(const char* cstr)
{
    puts(cstr);
    putc('\n');
}

}
