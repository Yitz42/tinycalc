/* Overlay layout: everything in one block from address 0, entry first. */
ENTRY(ovl_main)
SECTIONS {
  . = 0;
  .image : {
    KEEP(*(.text.ovl_main))
    *(.text*) *(.rodata*) *(.data*)
    . = ALIGN(4);
    *(.bss*) *(COMMON)
    . = ALIGN(4);
  }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
