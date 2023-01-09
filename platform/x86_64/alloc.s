.intel_syntax noprefix
.text
#.global kernel_FlushPageTLB
#.type kernel_FlushPageTLB, @function
#kernel_FlushPageTLB:
#        invlpg  [rdi]
#        ret

#.global kernel_LoadCR3
#.type kernel_LoadCR3, @function
#kernel_LoadCR3:
#        mov     rax, cr3
#        ret
