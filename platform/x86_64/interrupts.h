#ifndef PLATFORM_X86_64_INTERRUPTS_H
#define PLATFORM_X86_64_INTERRUPTS_H

#include <cstdint>

namespace kernel::tgtspec {

struct InterruptFrame {
    std::uint64_t rax;
    std::uint64_t rcx;
    std::uint64_t rdx;
    std::uint64_t rbx;
    std::uint64_t local_rsp;
    std::uint64_t rbp;
    std::uint64_t rsi;
    std::uint64_t rdi;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t r10;
    std::uint64_t r11;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint64_t rip;
    std::uint64_t cs;
    std::uint64_t rflags;
    std::uint64_t rsp;
    std::uint64_t ss;
};

struct ExceptionFrame {
    std::uint64_t rax;
    std::uint64_t rcx;
    std::uint64_t rdx;
    std::uint64_t rbx;
    std::uint64_t local_rsp;
    std::uint64_t rbp;
    std::uint64_t rsi;
    std::uint64_t rdi;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t r10;
    std::uint64_t r11;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint64_t error;
    std::uint64_t rip;
    std::uint64_t cs;
    std::uint64_t rflags;
    std::uint64_t rsp;
    std::uint64_t ss;
};

extern "C" void kernel_x86_64_SendEOI(int both);
extern "C" int kernel_x86_64_IsSpuriousIRQ(int slave);

} // namespace kernel::tgtspec

#endif // PLATFORM_X86_64_INTERRUPTS_H
