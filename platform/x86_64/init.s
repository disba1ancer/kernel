.intel_syntax noprefix
.text
.global _start
.type _start, @function
_start:
        lea     rax, __init_stack_end[rip]
        mov     rsp, rax
        lea     rcx, 0f[rip]
        lgdt    6 + gdtr[rip]
        pushq   16
        push    rax
        pushfq
        pushq   8
        push    rcx
        iretq
0:      mov     eax, ss
        mov     ds, eax
        mov     es, eax
        mov     fs, eax
        mov     gs, eax
        call    cpp_start

.global kernel_EndlessLoop
.type kernel_EndlessLoop, @function
kernel_EndlessLoop:
        hlt
        jmp     kernel_EndlessLoop
