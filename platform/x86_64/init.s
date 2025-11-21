.intel_syntax noprefix
.text
.global _start
.type _start, @function
_start:
        lea     rsp, __init_stack_end - 0x18[rip]
        lea     rcx, kernel_EndlessLoop[rip]
        mov     16[rsp], rcx
        mov     dword ptr 8[rsp], 0x10
        lea     rcx, cpp_start[rip]
        mov     [rsp], rcx
        lgdt    6 + gdtr[rip]
        mov     eax, 0x20
        mov     ds, eax
        mov     ss, eax
        mov     es, eax
        mov     fs, eax
        mov     gs, eax
        retfq

.global kernel_EndlessLoop
.type kernel_EndlessLoop, @function
kernel_EndlessLoop:
        hlt
        jmp     kernel_EndlessLoop
