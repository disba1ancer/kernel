.intel_syntax noprefix

.text
.set i, 0
SizeOfIDT = 4096

.section .data.kinterrupts, "aw"
.align 64
idt:

.rept 256
.text
0:      push    i
        jmp     universal_handler
.section .data.kinterrupts, "aw"
        .word   0x8E00
        .word   0x28
        .quad   0b
        .long   0
.set i, i + 1
.endr

.text
universal_handler:
        sub     rsp, 0x78
        mov       0[rsp], rax
        lea     rax, [rsp + 0x80]
        mov       8[rsp], rcx
        mov      16[rsp], rdx
        mov      24[rsp], rbx
        mov      32[rsp], rax
        mov      40[rsp], rbp
        mov      48[rsp], rsi
        mov      56[rsp], rdi
        mov     rdi, 120[rsp]
        mov      64[rsp], r8
        mov      72[rsp], r9
        mov      80[rsp], r10
        mov      88[rsp], r11
        mov      96[rsp], r12
        mov     104[rsp], r13
        mov     112[rsp], r14
        mov     120[rsp], r15
        mov     rsi, rsp
        mov     rbp, rsp
        cld
        and     rsp, -16
        call    kernel_x86_64_UniversalInterruptHandler
        mov     rsp, rbp
        mov     rax,   0[rsp]
        mov     rcx,   8[rsp]
        mov     rdx,  16[rsp]
        mov     rbx,  24[rsp]
        mov     rbp,  40[rsp]
        mov     rsi,  48[rsp]
        mov     rdi,  56[rsp]
        mov     r8 ,  64[rsp]
        mov     r9 ,  72[rsp]
        mov     r10,  80[rsp]
        mov     r11,  88[rsp]
        mov     r12,  96[rsp]
        mov     r13, 104[rsp]
        mov     r14, 112[rsp]
        mov     r15, 120[rsp]
        mov     rsp,  32[rsp]
        iretq

.global kernel_x86_64_EnableInterrupts
.type kernel_x86_64_EnableInterrupts, @function
kernel_x86_64_EnableInterrupts:
        movabs  rcx, offset idt
0:      mov     eax, 0[rcx]
        mov     edx, 4[rcx]
        mov     esi, 16[rcx]
        mov     edi, 20[rcx]
        mov     r8d, 32[rcx]
        mov     r9d, 36[rcx]
        mov     r10d, 48[rcx]
        mov     r11d, 52[rcx]
        mov     0[rcx], dx
        mov     4[rcx], ax
        mov     16[rcx], di
        mov     20[rcx], si
        mov     32[rcx], r9w
        mov     36[rcx], r8w
        mov     48[rcx], r11w
        mov     52[rcx], r10w
        add     rcx, 64
        cmp     rcx, offset idt + SizeOfIDT
        jb      0b
        lidt    idtr

        in      al, 0x70
        and     al, 0x7f
        out     0x70, al
        in      al, 0x71

        sti

        ret

.data
idtr:
        .word   SizeOfIDT
        .quad   idt
