#ifndef PLATFORM_X86_64_INTERRUPTS_H
#define PLATFORM_X86_64_INTERRUPTS_H

#include "kernel/util.h"

KERNEL_STRUCT(kernel_x86_64_InterruptFrame) {
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t local_rsp;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

KERNEL_STRUCT(kernel_x86_64_ExceptionFrame) {
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t local_rsp;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t error;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void kernel_x86_64_SendEOI(int both);
int kernel_x86_64_IsSpuriousIRQ(int slave);

#endif // PLATFORM_X86_64_INTERRUPTS_H
