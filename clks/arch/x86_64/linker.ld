OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(7);
}

SECTIONS {
    . = 0xffffffff80000000;

    . = ALIGN(0x1000);
    .text : ALIGN(0x1000) {
        *(.text .text.*)
    } :text

    . = ALIGN(0x1000);
    .rodata : ALIGN(0x1000) {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(0x1000);
    .data : ALIGN(0x1000) {
        *(.data .data.*)
    } :data

    . = ALIGN(0x1000);
    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
