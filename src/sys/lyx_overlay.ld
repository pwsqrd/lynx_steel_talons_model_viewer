/* Overlay linker script

   Each overlay is linked on its own against the resident ELF
   (--just-symbols=t.elf), so the __rc imaginary registers and every resident
   function and global resolve to their resident addresses. The Makefile
   passes the overlay base addresses taken from t.elf:
     OVERLAY_BASE     = __overlay_start        (end of resident RAM)
     OVERLAY_ZP_BASE  = __zp_bss_resident_end  (end of resident zero page)

   The image is the 6-byte header followed by .text/.rodata/.data. sys/overlay.cpp
   decompresses it to OVERLAY_BASE, zeroes [bss_start, bss_end) and the overlay
   zero page, then calls the entry point.
*/

MEMORY {
    zp : ORIGIN = OVERLAY_ZP_BASE, LENGTH = 0x100 - OVERLAY_ZP_BASE
    /* Ceiling is the lower framebuffer (SYS_SCREEN_BUFFER_2 = 0xC038), see lyx.ld */
    overlay (wxa!r) : ORIGIN = OVERLAY_BASE, LENGTH = 0xC038 - OVERLAY_BASE
}

ENTRY(ovl_Main)

SECTIONS {
   .zp.bss (NOLOAD) : {
      *(.zp.bss .zp.bss.*)
      *(.zp.noinit .zp.noinit.*)
   } > zp

   /* Overlays have no loader for initialised zero page; this must stay empty. */
   .zp.data : {
      *(.zp.data .zp.data.* .zp.rodata .zp.rodata.*)
   } > zp

   .overlay_header : {
      SHORT(ovl_Main)
      SHORT(__overlay_bss_start)
      SHORT(__overlay_bss_end)
   } > overlay

   .text : {
      *(.text*)
   } > overlay

   .rodata : {
      *(.rodata*)
   } > overlay

   .data : {
      *(.data*)
   } > overlay

   .bss (NOLOAD) : {
      __overlay_bss_start = .;
      *(.bss .bss.*)
      *(.noinit .noinit.*)
      __overlay_bss_end = .;
   } > overlay

   /* Overlays have no access to the resident's .extrabss page. */
   .extrabss (NOLOAD) : {
      *(.extrabss .extrabss.*)
   } > overlay

   ASSERT(SIZEOF(.zp.data) == 0,
          "overlay has initialised zero-page data, which overlays cannot load")
   ASSERT(SIZEOF(.extrabss) == 0,
          "overlay uses .extrabss, which belongs to the resident")
}
