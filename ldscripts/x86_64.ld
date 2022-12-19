ENTRY(_start)

__binary_load_address = 0xFFFFFFFFC0000000;

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
        __bss_end = ALIGN(16);
    }
    __first_free_page = ALIGN(CONSTANT(MAXPAGESIZE));
    .stack -CONSTANT(MAXPAGESIZE) * 3 : {
        . = . + CONSTANT(MAXPAGESIZE);
        __stack_end = .;
    }
    __mapping_window = -CONSTANT(MAXPAGESIZE);
}
