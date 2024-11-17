/*
    Linker script
*/

ENTRY(start)
SECTIONS
{
    . = 0x10000;
    .text : { 
        startup.o(.text)
        *(.text)
    }
    .data : { *(.data) }
    .bss : { *(.bss COMMON) }
    . = ALIGN(8);
    . = . + 0x10000; /* 64kB of stack memory */
    stack_top = .;
}