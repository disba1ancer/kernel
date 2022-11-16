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
    __first_free_page = ALIGN(4096);
    __mapping_window = 0xFFFFFFFFFFFFF000;
}
