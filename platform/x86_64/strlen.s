.intel_syntax noprefix

.text
.globl strlen
.type strlen, @function
strlen:
        mov     rsi, rdi

        test    esi, 7
        jz      1f
0:      lods    byte ptr [rsi]
        test    al, al
        jz      0f
        test    esi, 7
        jnz     0b

        mov     rdx, 0x8080808080808080
1:      lods    qword ptr [rsi]
        mov     rcx, 0xfefefefefefefeff
        add     rcx, rax
        not     rax
        and     rcx, rdx
        and     rax, rcx
        jz      0b
        bsf     rax, rax
        sub     rsi, rdi
        shr     rax, 3
        sub     rsi, 8
        add     rax, rsi
        ret

0:      lea     rax, [rsi - 1]
        sub     rax, rdi
        ret
