ENTRY(_start)

__binary_load_address = -0x80000000;

SECTIONS {
    . = __binary_load_address + SIZEOF_HEADERS;
    .text : ALIGN(16) {
        *(.init*)
        *(.fini*)
        *(.text*)
    }
    .rodata : {
        *(.rodata*)
    }
    .eh_frame_hdr : {
        *(.eh_frame_hdr*)
    }
    .eh_frame : {
        *(.eh_frame)
    }
    .gcc_except_table : {
        *(.gcc_except_table*)
    }
    . = . + CONSTANT(MAXPAGESIZE);
    .ctors : ALIGN(16) {
        *(.ctors)
    }
    .dtors : ALIGN(16) {
        *(.dtors)
    }
    .data : ALIGN(16) {
        *(.data*)
    }
    .bss : ALIGN(16) {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __mapping_window = .;
    . += CONSTANT(MAXPAGESIZE);
    __stack_size = 64K;
    .stack : {
        __init_stack_start = .;
        . += __stack_size;
        __init_stack_end = .;
    }
    magic_handler = .;
    . += CONSTANT(MAXPAGESIZE);
    __smheap_start = .;
    __smheap_end = -CONSTANT(MAXPAGESIZE);
}
