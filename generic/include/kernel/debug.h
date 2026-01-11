#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include <string_view>

namespace kernel::debug {

void putc(char ch);
void puts(std::string_view str);
void puts(const char* cstr);
void println(std::string_view str);
void println(const char* cstr);

}

#endif // KERNEL_DEBUG_H
