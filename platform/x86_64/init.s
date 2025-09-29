.intel_syntax noprefix
.text
.global _start
_start:
        mov     rsi, offset __init_stack_end - 0x40
        mov     0[rsi], rbx
        mov     8[rsi], rsp
        mov     16[rsi], rbp
        mov     24[rsi], r12
        mov     32[rsi], r13
        mov     40[rsi], r14
        mov     48[rsi], r15
        mov     rsp, rsi
        call    c_start

.global kernel_EndlessLoop
.type kernel_EndlessLoop, @function
kernel_EndlessLoop:
        hlt
        jmp     kernel_EndlessLoop
