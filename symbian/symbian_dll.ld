/* symbian_dll.ld - Symbian OS v9.x (EKA2) ARM BPABI Zero-WSD DLL Linker Script
 * Derived from GNU ld armsymbian -shared script with:
 *   - RO base at 0x8000 (matching Symbian E32Image KImageDll layout)
 *   - 4-byte aligned .rodata and Image$$ER_RO$$Limit at end of RO segment
 *   - Compile-time assertion that Writable Static Data (.data + .bss) == 0
 */
OUTPUT_FORMAT("elf32-littlearm-symbian", "elf32-bigarm-symbian",
              "elf32-littlearm-symbian")
OUTPUT_ARCH(arm)
ENTRY(_E32Dll)

VERSION
{
  __GNU {
    local:
      Image$$ER_RO$$Base;
      Image$$ER_RO$$Limit;
      SHT$$INIT_ARRAY$$Base;
      SHT$$INIT_ARRAY$$Limit;
      .ARM.exidx$$Base;
      .ARM.exidx$$Limit;
  };
}

SECTIONS
{
  . = SEGMENT_START("text", 0x8000);
  PROVIDE (Image$$ER_RO$$Base = .);

  .init : ALIGN(4)
  {
    KEEP (*(.init))
  } =0

  .text : ALIGN(4)
  {
    *(.text.startup .text.startup.*)
    *(.text.hot .text.hot.*)
    *(.text .stub .text.* .gnu.linkonce.t.*)
    *(.gnu.warning)
    *(.glue_7t) *(.glue_7) *(.vfp11_veneer) *(.v4_bx)
    . = ALIGN(4);
  } =0

  .fini : ALIGN(4)
  {
    KEEP (*(.fini))
  } =0

  /* Symbian OS kernel requires .plt at the end of the executable section */
  .plt : ALIGN(4) { *(.plt) }
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);

  .rodata : ALIGN(4)
  {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
    *(.rodata1)
    . = ALIGN(4);
  }

  .preinit_array : { KEEP (*(.preinit_array)) }
  .init_array :
  {
    PROVIDE (SHT$$INIT_ARRAY$$Base = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array))
    PROVIDE (SHT$$INIT_ARRAY$$Limit = .);
  }
  .fini_array :
  {
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array))
  }

  .ARM.extab : ALIGN(4) { *(.ARM.extab* .gnu.linkonce.armextab.*) }
  PROVIDE_HIDDEN (.ARM.exidx$$Base = .);
  PROVIDE_HIDDEN (__exidx_start = .);
  .ARM.exidx : ALIGN(4) { *(.ARM.exidx* .gnu.linkonce.armexidx.*) }
  PROVIDE_HIDDEN (__exidx_end = .);
  PROVIDE_HIDDEN (.ARM.exidx$$Limit = .);

  . = ALIGN(4);
  PROVIDE (Image$$ER_RO$$Limit = .);

  . = SEGMENT_START("data", ALIGN(CONSTANT (MAXPAGESIZE)) + (. & (CONSTANT (MAXPAGESIZE) - 1)));
  .tdata        : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
  .tbss         : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
  .data.rel.ro  : { *(.data.rel.ro.local) *(.data.rel.ro .data.rel.ro.*) }
  .data         : { __data_start = .; *(.data .data.* .gnu.linkonce.d.*) }
  .data1        : { *(.data1) }
  _edata = .;
  PROVIDE (edata = .);
  __bss_start = .;
  __bss_start__ = .;
  .bss          :
  {
    *(.dynbss)
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)
  }
  _bss_end__ = .; __bss_end__ = .;
  __end__ = .; _end = .; PROVIDE (end = .);

  ASSERT(SIZEOF(.data.rel.ro) + SIZEOF(.data) + SIZEOF(.bss) == 0,
         "ERROR: Symbian DLL contains Writable Static Data (.data/.bss)!")

  /* Unmapped BPABI sections */
  .dynamic      0 : { *(.dynamic) }
  .hash         0 : { *(.hash) }
  .dynsym       0 : { *(.dynsym) }
  .dynstr       0 : { *(.dynstr) }
  .gnu.version  0 : { *(.gnu.version) }
  .gnu.version_d 0: { *(.gnu.version_d) }
  .gnu.version_r 0: { *(.gnu.version_r) }
  .comment      0 : { *(.comment) }
  .note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) }
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.eh_frame*) }

  .rel.dyn      0 :
  {
    *(.rel.init)
    *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
    *(.rel.fini)
    *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
    *(.rel.data.rel.ro .rel.data.rel.ro.*)
    *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
    *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
    *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
    *(.rel.ctors)
    *(.rel.dtors)
    *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
    *(.rel.init_array)
    *(.rel.fini_array)
  }
  .rela.dyn     0 :
  {
    *(.rela.init)
    *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
    *(.rela.fini)
    *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
    *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
    *(.rela.bss .rela.bss.* .rel.gnu.linkonce.b.*)
  }
  .rel.plt      0 : { *(.rel.plt) }
  .rela.plt     0 : { *(.rela.plt) }
  .rel.other    0 : { *(.rel.*) }
  .rela.other   0 : { *(.rela.*) }
  .reli.other   0 : { *(.reli.*) }
}
