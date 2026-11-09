OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 0x1800
    RAM (rwx) : ORIGIN = 0x00001800, LENGTH = 0x0800
}

SECTIONS
{
    .text 0x00000000 :
    {
        build/crt0.o(.text .text.*)
        *(.text .text.*)
        *(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(8);
    } > ROM

    .data 0x00001800 :
    {
        *(.sdata .sdata.* .data .data.*)
        . = ALIGN(8);
    } > RAM

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        _bss_start = .;
        *(.sbss .sbss.* .bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM

    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
    ASSERT(_bss_end <= 0x1F00, "Error: RAM overflow into stack space (0x1800-0x2000)")
}
