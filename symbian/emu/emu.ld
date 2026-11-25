/* Flat image for the unicorn runner: code+data at 0x10000, 64 MB heap, 1 MB stack. */
ENTRY(_start)
SECTIONS
{
    . = 0x10000;
    .text : { *(.text.start) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .ARM.exidx : { *(.ARM.exidx*) }
    .data : { *(.data .data.*) }
    .bss : { __bss_start = .; *(.bss .bss.* COMMON) . = ALIGN(4); __bss_end = .; }
    . = ALIGN(4096);
    __heap_start = .;
    . = . + 0x4000000;
    __heap_end = .;
    . = . + 0x100000;
    __stack_top = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.ARM.attributes) }
}
