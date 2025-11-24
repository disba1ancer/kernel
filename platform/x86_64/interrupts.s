.intel_syntax noprefix

.text
.set i, 0
IDTEntries = 256
SizeOfIDT = 16 * IDTEntries
LimitOfIDT = SizeOfIDT - 1

.section .data.kinterrupts, "aw"
.align 64
idt:

.text
.global idt_handlers
idt_handlers:
.rept IDTEntries
.text
0:
.cfi_startproc
.if !(i == 8 || i - 10 < 5 || i == 17)
        sub     rsp, 8
.endif
.cfi_adjust_cfa_offset 8
        push    (i ^ 0x80) - 0x80
        jmp     universal_handler
.cfi_endproc
.section .data.kinterrupts, "aw"
        .word   0x8E00
        .word   8
        .quad   0b
        .long   0
.set i, i + 1
.endr

.text
universal_handler:
.cfi_startproc
.cfi_adjust_cfa_offset 16
        sub     rsp, 0x70
.cfi_adjust_cfa_offset 0x70
        mov       0[rsp], rax
        mov       8[rsp], rcx
        mov      16[rsp], rdx
        mov      24[rsp], rbx
        mov      32[rsp], rbp
.cfi_rel_offset rbp, 32
        mov      40[rsp], rsi
        mov      48[rsp], rdi
        movzx   edi, byte ptr 112[rsp]
        mov      56[rsp], r8
        mov      64[rsp], r9
        mov      72[rsp], r10
        mov      80[rsp], r11
        mov      88[rsp], r12
        mov      96[rsp], r13
        mov     104[rsp], r14
        mov     112[rsp], r15
        mov     rsi, rsp
        mov     rbp, rsp
.cfi_def_cfa_register rbp
        cld
        and     rsp, -16
        call    kernel_x86_64_SystemInterruptHandler
        mov     rsp, rbp
.cfi_def_cfa_register rsp
        mov     rax,   0[rsp]
        mov     rcx,   8[rsp]
        mov     rdx,  16[rsp]
        mov     rbx,  24[rsp]
        mov     rbp,  32[rsp]
.cfi_restore rbp
        mov     rsi,  40[rsp]
        mov     rdi,  48[rsp]
        mov     r8 ,  56[rsp]
        mov     r9 ,  64[rsp]
        mov     r10,  72[rsp]
        mov     r11,  80[rsp]
        mov     r12,  88[rsp]
        mov     r13,  96[rsp]
        mov     r14, 104[rsp]
        mov     r15, 112[rsp]
        add     rsp,  0x80
.cfi_adjust_cfa_offset -0x80
        iretq
.cfi_endproc

.global kernel_x86_64_IDTFixupAndLoad
.type kernel_x86_64_IDTFixupAndLoad, @function
kernel_x86_64_IDTFixupAndLoad:
        mov     r8, rbx
        xor     ecx, ecx
        lea     rbx, idt[rip]
0:      mov     eax, 0[rbx + rcx]
        mov     edx, 4[rbx + rcx]
        mov     esi, 16[rbx + rcx]
        mov     edi, 20[rbx + rcx]
        mov     0[rbx + rcx], dx
        mov     4[rbx + rcx], ax
        mov     16[rbx + rcx], di
        mov     20[rbx + rcx], si
        add     rcx, 32
        cmp     rcx, SizeOfIDT
        jb      0b
        mov     rbx, r8
        lidt    idtr[rip]
        ret

.global kernel_x86_64_EnableInterrupts
.type kernel_x86_64_EnableInterrupts, @function
kernel_x86_64_EnableInterrupts:
        in      al, 0x21
        movzx   esi, al
        in      al, 0xA1
        movzx   edi, al

        mov     eax, 0x11
        out     0x20, al
        out     0x80, al
        out     0xA0, al
        out     0x80, al

        mov     eax, 0x40
        out     0x21, al
        out     0x80, al
        mov     eax, 0x48
        out     0xA1, al
        out     0x80, al

        mov     eax, 4
        out     0x21, al
        out     0x80, al
        mov     eax, 2
        out     0xA1, al
        out     0x80, al

        mov     eax, 1
        out     0x21, al
        out     0x80, al
        out     0xA1, al
        out     0x80, al

        mov     eax, esi
        out     0x21, al
        out     0x80, al
        mov     eax, edi
        out     0xA1, al
        out     0x80, al

        in      al, 0x70
        and     al, 0x7f
        out     0x70, al
        in      al, 0x71

        sti

        ret

.global kernel_x86_64_SendEOI
.type kernel_x86_64_SendEOI, @function
kernel_x86_64_SendEOI:
        test    edi, edi
        mov     eax, 0x20
        jz      0f
        out     0xA0, al
0:      out     0x20, al
        ret

.global kernel_x86_64_IsSpuriousIRQ
.type kernel_x86_64_IsSpuriousIRQ, @function
kernel_x86_64_IsSpuriousIRQ:
        test    edi, edi
        mov     eax, 0xB
        jz      0f
        out     0xA0, al
        out     0x80, al
        in      al, 0xA0
        and     eax, 0x80
        ret
0:      out     0x20, al
        out     0x80, al
        in      al, 0x20
        and     eax, 0x80
        ret

.global kernel_x86_64_KeyboardIRQ
.type kernel_x86_64_KeyboardIRQ, @function
kernel_x86_64_KeyboardIRQ:
        in      al, 0x60
        mov     [0xb8000], al
        ret

.section .rodata
.align	8
        .skip   6
idtr:
        .word   LimitOfIDT
        .quad   idt
