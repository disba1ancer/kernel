.intel_syntax noprefix

.globl strcmp
.type strcmp, @function
strcmp: # (edi, esi)
        test    edi, 7
        jz      1f
0:      movzx   eax, byte ptr [rdi]
        movzx   ecx, byte ptr [rsi]
        add     rdi, 1
        add     rsi, 1
        sub     eax, ecx
        jnz     0f
        test    ecx, ecx
        jz      0f
        test    edi, 7
        jnz     0b

1:      mov     eax, esi
        mov     r10, 0xFEFEFEFEFEFEFEFF
        lea     r8, .Lstrcmp_Tbl[rip]
        and     eax, 7
        mov     r11, 0x8080808080808080
        jmp     [r8 + rax * 8]

0:      ret

.section .rodata
.align 8
.Lstrcmp_Tbl:
        .8byte  .Lstrcmp8
        .8byte  .Lstrcmp1
        .8byte  .Lstrcmp2
        .8byte  .Lstrcmp1
        .8byte  .Lstrcmp4
        .8byte  .Lstrcmp1
        .8byte  .Lstrcmp2
        .8byte  .Lstrcmp1
.text

.Lstrcmp1:
0:      movzx   eax, byte ptr [rdi]
        movzx   ecx, byte ptr [rsi]
        add     rdi, 1
        add     rsi, 1
        sub     eax, ecx
        jnz     0f
        test    ecx, ecx
        jnz     0b
0:      ret

.Lstrcmp2:
        movzx   r11d, r11w
0:      movzx   eax, word ptr [rdi]
        movzx   edx, word ptr [rsi]
        mov     ecx, r11d
        add     rdi, 2
        and     ecx, edx
        lea     r8d, [rdx + r10]
        add     rsi, 2
        xor     ecx, r11d
        sub     eax, edx
        jnz     1f
        and     ecx, r8d
        jz      0b
        ret

.Lstrcmp4:
0:      mov     eax, [rdi]
        mov     edx, [rsi]
        mov     ecx, r11d
        add     rdi, 4
        and     ecx, edx
        lea     r8d, [rdx + r10]
        add     rsi, 4
        xor     ecx, r11d
        sub     eax, edx
        jnz     1f
        and     ecx, r8d
        jz      0b
        ret

.Lstrcmp8:
0:      mov     rax, [rdi]
        mov     rdx, [rsi]
        mov     rcx, r11
        add     rdi, 8
        and     rcx, rdx
        lea     r8, [rdx + r10]
        add     rsi, 8
        xor     rcx, r11
        sub     rax, rdx
        jnz     1f
        and     rcx, r8
        jz      0b
        ret

1:      and     r8, rcx
        bsf     rcx, rax
        bsf     rsi, r8
        jz      0f
        cmp     esi, ecx
        cmovc   ecx, esi
0:      and     ecx, 0x38
        shr     rax, cl
        shr     rdx, cl
        add     eax, edx
        movzx   eax, al
        movzx   edx, dl
        sub     eax, edx
        ret
