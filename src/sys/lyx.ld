__rc0 = 0x00;
INCLUDE imag-regs.ld


/* Two-stage loader linker script

   Cart layout (created by pack_lynx_cart.py):
     Bytes 0-51:    Encrypted micro_loader (52 bytes) - loads 256 bytes to $0300
     Bytes 52-307:  Stage2 loader binary (256 bytes) - loaded to $0300, runs there
     Bytes 308-309: Code size (2 bytes, little-endian)
     Bytes 310+:    Code (this is what the linker produces) - loaded to $0400

   Stage2 loader handles:
     - Reading code size
     - Loading code to $0400 with block switching
     - Setting up stack
     - Jumping to $0400
*/

MEMORY {
    /* ZP pool: $26-$FF (zx0 uses $20-$25; stage2 loader used $20-$27 before boot) */
    zp : ORIGIN = 0x26, LENGTH = 0x100 - 0x26
    /* Reclaimed from stage2 loader region - available for .extrabss */
    extra (wxa!r) : ORIGIN = 0x200, LENGTH = 0x100
    /* Software stack at $0300-$03FF. crt0 paints it with $A5 so peak usage
     * can be measured; grow it (and shrink extra) if a game needs more. */
    stack (wxa!r) : ORIGIN = 0x300, LENGTH = 0x100
    /* General RAM up to the lower framebuffer. Both framebuffers live under
     * the Suzy/Mikey/ROM hardware shadow (see sys.h SYS_SCREEN_BUFFER_1/2),
     * so RAM runs to SYS_SCREEN_BUFFER_2. LENGTH = 0xC038 - 0x400. */
    ram (wxa!r) : ORIGIN = 0x400, LENGTH = 0xBC38
}

SECTIONS {
   .zp.bss (NOLOAD) : {
      *(.zp.bss .zp.bss.*)
      /* Overlay zero page starts here (sys/lyx_overlay.ld). */
      __zp_bss_resident_end = .;
   } > zp

   .text : {
      *(.init*)
      *(.text*)
   } > ram

   .rodata : {
      *(.rodata*)
   } > ram

   .cart_data : {
      *(.cart_data*)
   } > ram

   .data : {
      *(.data*)
   } > ram

   .bss (NOLOAD) : {
       *(.bss*)
   } > ram

   .noinit (NOLOAD) : {
       *(.noinit*)
       /* Captured INSIDE the ram region so `.` reflects the true end of
        * .bss/.noinit; after .extrabss (another region) it would bind to the
        * wrong address. */
       __ram_end = .;
       /* Overlays are decompressed here (sys/overlay.cpp). */
       __overlay_start = .;
   } > ram

   .extrabss (NOLOAD) : {
       *(.extrabss .extrabss.*)
   } > extra

   /* Software stack in dedicated region - grows down from $0400 to $0300 */
   __stack = ORIGIN(stack) + LENGTH(stack);

   /* SYS_SCREEN_BUFFER_2 = ((0xFFFA - 2*8160) & ~3) = 0xC038 (4-byte aligned
    * display base, see sys.h). RAM must end at or below it or .bss
    * scribbles on framebuffer memory. */
   ASSERT(__ram_end <= 0xC038,
          "resident code/data collides with screen buffer 2 at $C038")
}
