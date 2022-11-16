.intel_syntax noprefix
.text
.global _start
_start:
        push    rdi
        call    kernel_AllocateInitialStack
        test    rax, rax
        jz      0f
        mov     rsi, rax
        pop     rdi
        mov     rax, offset kernel_stackEnd
        mov     0[rax], rbx
        mov     8[rax], rsp
        mov     16[rax], rbp
        mov     24[rax], r12
        mov     32[rax], r13
        mov     40[rax], r14
        mov     48[rax], r15
        mov     rsp, rax
        jmp     c_start
0:      ret

.global kernel_EndlessLoop
.type kernel_EndlessLoop, @function
kernel_EndlessLoop:
#        hlt
        jmp     kernel_EndlessLoop

.global kernel_stackEnd
kernel_stackEnd = -0x2038
