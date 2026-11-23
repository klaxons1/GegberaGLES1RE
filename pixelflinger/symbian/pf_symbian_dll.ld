/* pf_symbian_dll.ld - Symbian OS v9.x (EKA2) ARM BPABI Zero-WSD C++ DLL Linker Script
 * Places C++ vtables and const pointer tables (.data.rel.ro*) inside ER_RO
 * (relocated once at load time via E32Image CodeRelocs) and enforces strict
 * zero Writable Static Data (.data + .bss == 0).
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
    *(.data.rel.ro.local .data.rel.ro.local.*)
    *(.data.rel.ro .data.rel.ro.*)
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

  ASSERT(SIZEOF(.init_array) + SIZEOF(.preinit_array) == 0,
         "ERROR: Symbian DLL contains global C++ static constructors (.init_array)!")
  ASSERT(SIZEOF(.data) + SIZEOF(.bss) + SIZEOF(.tdata) + SIZEOF(.tbss) == 0,
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
}
