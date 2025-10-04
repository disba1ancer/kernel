.intel_syntax noprefix

.text
.global memcpy
.type memcpy, @function
memcpy:
        test    rdx, rdx
        mov     rax, rdi
        jnz     0f
        ret

0:      mov     rcx, rsi
        sub     rcx, rdi

.Lmvdir:

        cmp     rdx, 16
        lea     rdx, [rdx + rsi]
        jc      .Lmemcpy1

        and     rcx, 7
        jmp     .Lmemcpy_tbl[rcx * 8]

.section .rodata
.align 8
.Lmemcpy_tbl:
        .8byte  .Lmemcpy8
        .8byte  .Lmemcpy1
        .8byte  .Lmemcpy2
        .8byte  .Lmemcpy1
        .8byte  .Lmemcpy4
        .8byte  .Lmemcpy1
        .8byte  .Lmemcpy2
        .8byte  .Lmemcpy1

.text
.Lmemcpy1:
        mov     rcx, rdx
        sub     rcx, rsi
        rep movs [rdi], byte ptr [rsi]
        ret

.Lmemcpy2:
        test    rdi, 1
        jz      0f
        movs    [rdi], byte ptr [rsi]

0:      mov     rcx, rdx
        sub     rcx, rsi
        shr     rcx, 1
        rep movs [rdi], word ptr [rsi]

        test    rdx, 1
        jz      0f
        movs    [rdi], byte ptr [rsi]
0:      ret

.Lmemcpy4:
        lea     rcx, [rsi + 3]
        and     rcx, -4
        sub     rcx, rsi
        rep movs [rdi], byte ptr [rsi]

0:      mov     rcx, rdx
        sub     rcx, rsi
        shr     rcx, 2
        rep movs [rdi], dword ptr [rsi]
        jmp     .Lmemcpy1

.Lmemcpy8:
        lea     rcx, [rsi + 7]
        and     rcx, -8
        sub     rcx, rsi
        rep movs [rdi], byte ptr [rsi]

0:      mov     rcx, rdx
        sub     rcx, rsi
        shr     rcx, 3
        rep movs [rdi], qword ptr [rsi]
        jmp     .Lmemcpy1

.global memmove
.type memmove, @function
memmove:
        test    rdx, rdx
        mov     rax, rdi
        jnz     1f
0:      ret

1:      mov     rcx, rdi
        sub     rcx, rsi
        jz      0b
	cmp	rcx, rdx
        jnc     .Lmvdir
        std
        add     rdi, rdx
        lea     rsi, [rsi + rdx]

        cmp     rdx, 16
        jc      .Lmemmove1

        and     rcx, 7
        jmp     .Lmemmove_Tbl[rcx * 8]

.section .rodata
.align 8
.Lmemmove_Tbl:
        .8byte  .Lmemmove8
        .8byte  .Lmemmove1
        .8byte  .Lmemmove2
        .8byte  .Lmemmove1
        .8byte  .Lmemmove4
        .8byte  .Lmemmove1
        .8byte  .Lmemmove2
        .8byte  .Lmemmove1

.text
.Lmemmove1:
        sub     rsi, 1
        mov     rcx, rdi
        lea     rdi, [rdi - 1]
1:      sub     rcx, rax
        rep movs [rdi], byte ptr [rsi]
        cld
        ret

.Lmemmove2:
        test    edi, 1
        jz      0f

        sub     rsi, 1
        lea     rdi, [rdi - 1]
        movs    [rdi], byte ptr [rsi]
        lea     rdi, [rdi + 1]
        add     rsi, 1

0:      mov     rcx, rdi
        lea     rsi, [rsi - 2]
        sub     rcx, rax
        lea     rdi, [rdi - 2]
        shr     rcx, 1
        rep movs [rdi], word ptr [rsi]
        add     rdi, 1
        lea     rsi, [rsi + 1]
        cmp     rdi, rax
        jnz     0f
        movs    [rdi], byte ptr [rsi]

0:      cld
        ret

.Lmemmove4:
        mov     rcx, rdi
        lea     rsi, [rsi - 1]
        and     rcx, 3
        sub     rdi, 1

        rep movs [rdi], byte ptr [rsi]

        lea     rcx, [rdi + 1]
        sub     rsi, 3
        lea     rdi, [rdi - 3]
        sub     rcx, rax
        shr     rcx, 2
        rep movs [rdi], dword ptr [rsi]
        lea     rcx, [rdi + 4]
        add     rdi, 3
        lea     rsi, [rsi + 3]

        jmp     1b

.Lmemmove8:
        mov     rcx, rdi
        lea     rsi, [rsi - 1]
        and     rcx, 7
        sub     rdi, 1

        rep movs [rdi], byte ptr [rsi]

        lea     rcx, [rdi + 1]
        sub     rsi, 7
        lea     rdi, [rdi - 7]
        sub     rcx, rax
        shr     rcx, 3
        rep movs [rdi], qword ptr [rsi]
        lea     rcx, [rdi + 8]
        add     rdi, 7
        lea     rsi, [rsi + 7]

        jmp     1b
