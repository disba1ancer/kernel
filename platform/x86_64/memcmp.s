.intel_syntax noprefix

.text
.globl memcmp
.type memcmp, @function
memcmp:
        # rdi rsi rdx
        xor     eax, eax
        test    rdx, rdx
        jz      .Lret

        mov     rax, rdi
        sub     rax, rsi
        jz      .Lret

        cmp     rdx, 128
        jc      .Lmemcmp1

        and     eax, 7
        jz      .Lmemcmp8
        lea     rcx, [rip + .Lmemcmp_Tbl]
        bsf     eax, eax
        lea     rcx, [rcx + rax * 8]
        xor     eax, eax
        jmp     [rcx]

.section .rodata
.align 8
.Lmemcmp_Tbl:
        .quad   .Lmemcmp1
        .quad   .Lmemcmp2
        .quad   .Lmemcmp4
.text

.Lmemcmp8:
        lea     ecx, [rsi + 7]
        and     ecx, -8
        sub     ecx, esi
        jz      0f
        sub     rdx, rcx
        repe cmps [rsi], byte ptr[rdi]
        jnz     .Lneq
0:      mov     rcx, rdx
        shr     rcx, 3
        and     rdx, 7
        repe cmps [rsi], qword ptr[rdi]
        jz      .Lmemcmp1
        sub     rsi, 8
        lea     rdi, [rdi - 8]
        mov     rdx, 8

.Lmemcmp1:
        xor     eax, eax
        mov     rcx, rdx
        repe cmps [rsi], byte ptr[rdi]
        je      .Lret
.Lneq:  adc     eax, 0
        lea     eax, [rax + rax - 1]
.Lret:  ret

.Lmemcmp4:
        lea     ecx, [rsi + 3]
        and     ecx, -4
        sub     ecx, esi
        jz      0f
        sub     rdx, rcx
        repe cmps [rsi], byte ptr[rdi]
        jnz     .Lneq
0:      mov     rcx, rdx
        shr     rcx, 2
        and     rdx, 3
        repe cmps [rsi], dword ptr[rdi]
        jz      .Lmemcmp1
        sub     rsi, 4
        lea     rdi, [rdi - 4]
        mov     rdx, 4
        jmp     .Lmemcmp1

.Lmemcmp2:
        test    rsi, 1
        jz      0f
        sub     rdx, 1
        cmps    [rsi], byte ptr[rdi]
        jnz     .Lneq
0:      mov     rcx, rdx
        shr     rcx, 1
        and     rdx, 1
        repe cmps [rsi], word ptr[rdi]
        jz      .Lmemcmp1
        sub     rsi, 2
        lea     rdi, [rdi - 2]
        mov     rdx, 2
        jmp     .Lmemcmp1
