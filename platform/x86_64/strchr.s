.intel_syntax noprefix

.text
.globl strchr
.type strchr, @function
strchr:
        movzx   esi, sil
        mov     rdx, 0x101010101010101
        imul    rdx, rsi
        mov     rsi, rdi

        test    esi, 7
        jz      1f
0:      lods    byte ptr [rsi]
        cmp     al, dl
        jz      0f
        test    esi, 7
        jnz     0b

1:      mov     r10, 0xfefefefefefefeff
        mov     r11, 0x8080808080808080
1:      lods    qword ptr [rsi]
        mov     rcx, r10
        mov     rdi, r11
        add     rcx, rax
        xor     rdi, rax
        xor     rax, rdx
        lea     r8, [rax + r10]
        and     rcx, r11
        and     r8, r11
        xor     rax, r11
        and     rax, r8
        jnz     1f
        and     rcx, rdi
        jz      1b
2:      xor     eax, eax
        ret

0:      mov     rax, -1
        add     rax, rsi
        ret

1:      bsf     rax, rax # found pos
        sub     rsi, 8
        and     rcx, rdi # zero pos
        jz      0f
        bsf     rcx, rcx
        cmp     ecx, eax
        jc      2b
0:      shr     eax, 3
        add     rax, rsi
        ret
