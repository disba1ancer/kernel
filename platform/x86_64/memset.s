.intel_syntax noprefix

.text
.global memset
.type memset, @function
memset:
        movzx   eax, sil
        mov     rsi, 0x101010101010101
        imul    rax, rsi
        mov     rsi, rdi
        cmp     rdx, 16
        mov     rcx, rdx
        jc      0f
        add     rdx, rdi

        mov     rcx, rdi
        neg     rcx
        and     rcx, 7
        rep stos [rdi], al

        mov     rcx, rdx
        sub     rcx, rdi
        shr     rcx, 3
        rep stos [rdi], rax

        mov     rcx, rdx
        and     rcx, 7
0:      rep stos [rdi], al
        mov     rax, rsi
        ret
