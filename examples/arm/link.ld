/*
    Linker script
*/

ENTRY(_Reset)
SECTIONS
{
    . = 0x10000;
    start = 0x10000;
    .startup . : { startup.o(.text) }
    .text : { *(.text) }
  /*  .data : { *(.data) } */
   /* .bss : { *(.bss COMMON) } */
    . = ALIGN(8);
    . = . + 0x10000; /* 64kB of stack memory */
    stack_top = .;
}