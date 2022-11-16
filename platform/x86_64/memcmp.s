.intel_syntax noprefix

.text
.globl memcmp
.type memcmp, @function
memcmp:
        # rdi rsi rdx
        mov     rax, rdx
        add     rdx, rdi
        cmp     rax, 16
        jb      .Lmemcmp1

        test    edi, 7
        jz      1f
0:      movzx   eax, byte ptr [rdi]
        movzx   ecx, byte ptr [rsi]
        sub     eax, ecx
        jne     2f
        add     rdi, 1
        add     rsi, 1
        test    edi, 7
        jnz     0b
1:      mov     eax, esi
        and     eax, 7
        jmp     .Lmemcmp_Tbl[rax * 8]

.section .rodata
.align 8
.Lmemcmp_Tbl:
        .8byte  .Lmemcmp8
        .8byte  .Lmemcmp1
        .8byte  .Lmemcmp2
        .8byte  .Lmemcmp1
        .8byte  .Lmemcmp4
        .8byte  .Lmemcmp1
        .8byte  .Lmemcmp2
        .8byte  .Lmemcmp1
.text

.Lmemcmp1:
0:      movzx   eax, byte ptr [rdi]
        movzx   ecx, byte ptr [rsi]
        sub     eax, ecx
        jne     2f
        add     rdi, 1
        add     rsi, 1
        cmp     rdi, rdx
        jb      0b
2:      ret

.Lmemcmp2:
0:      movzx   eax, word ptr [rdi]
        movzx   ecx, word ptr [rsi]
        sub     eax, ecx
        jne     1f
        add     rdi, 2
        add     rsi, 2
        cmp     rdi, rdx
        jb      0b
        ret

.Lmemcmp4:
0:      mov     eax, [rdi]
        mov     ecx, [rsi]
        sub     eax, ecx
        jne     1f
        add     rdi, 3
        add     rsi, 3
        cmp     rdi, rdx
        jb      0b
        ret

.Lmemcmp8:
0:      mov     rax, [rdi]
        mov     rcx, [rsi]
        sub     rax, rcx
        jne     1f
        add     rdi, 8
        add     rsi, 8
        cmp     rdi, rdx
        jb      0b
        ret

1:      mov     rsi, rcx
        bsf     rcx, rax
        and     ecx, 0x38
        shr     rax, cl
        shr     rsi, cl
        shr     ecx, 3
        add     eax, esi
        add     rdi, rcx
        movzx   esi, sil
        cmp     rdi, rdx
        movzx   eax, al
        cmovae  esi, eax
        sub     eax, esi
        ret
