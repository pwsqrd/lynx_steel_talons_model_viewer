; 93C46 EEPROM driver for Atari Lynx.
;
; Direct port of Bastian Schick's BLL driver (also cc65 libsrc/lynx/eeprom.s).
; Wiring:
;   CS    = A7 of cart shift register (driven via CART_STROBE on SYSCTL1)
;   CLK   = A1 of cart shift register (toggles every 2 writes to RCART0)
;   DI/DO = AUDIN pin (bidirectional; direction set via IODIR bit 4)
;
; Calling convention (LLVM-MOS):
;   u16  sys_EepromReadWord(u8 wordAddr)
;       wordAddr in A. Returns u16 in A:X (lo:hi).
;
;   void sys_EepromWriteWord(u8 wordAddr, u16 value)
;       wordAddr in A. value lo in X, value hi in __rc2 -- llvm-mos hands
;       out argument bytes A, X, __rc2, ... in order, so the u16 that follows
;       a u8 splits across X and __rc2, NOT __rc2:__rc3.
;
; On entry both routines save MIKEY.iodat and the CPU's I flag, then SEI to
; keep IRQ-driven code out of the way. On exit IODIR goes back to the ASM_CART_IODIR_INIT CONSTANT, iodat is restored, I is
; restored, and g_cartCurrentBlock is forced to 0xFF so the next sys_CartSeek
; re-selects (the protocol scrambles the cart address counter).
;
; NEVER READ IODIR. IT IS WRITE-ONLY AND READS BACK AS 0xFF ON REAL HARDWARE.
; Saving and restoring a read-back IODIR would leave every I/O pin an output
; for the rest of the session. Emulators make IODIR read back what was written,
; so this is invisible outside real hardware. sys.cpp writes ASM_CART_IODIR_INIT
; once and nothing else in the tree writes IODIR, so the constant IS the
; resting state; EE_DIR_OUT/EE_DIR_IN below are built from it.
;
; IODAT, unlike IODIR, does read back -- it returns the pin levels -- and it
; MUST be saved and restored. Bit 4 is AUDIN (mikey.h Iodat::AUDIN_BIT), which
; on a cart larger than 256 KB is the ROM chip select, and ASM_CART_IODIR_INIT
; makes AUDIN an OUTPUT, so this driver drives that line. Nothing else in the
; tree changes bit 4 after sys_Init: sys_CartSelectBlock and the stage2 loader
; mask with $FC and touch only bits 0-1. Without the restore the first EEPROM
; access permanently selects the wrong ROM half and every later cart read
; returns garbage.
;
; The bit loops likewise OR the AUDIN data bit into ee_iodat_base (= the saved
; IODAT with AUDIN masked off) rather than the reference driver's literal 0x0B,
; so bits 0-3 stay exactly as sys_Init left them. In particular RESTLESS
; (bit 3) must keep being driven high -- dropping it blanks the Lynx I LCD
; (see cart_asm.h).
;
; Read path ordering also matters: AUDIN goes back to an output BEFORE the
; closing CS-LOW strobe, the way the reference driver does it, not after.
;
; GearLynx does not emulate the 93C46 at all and serves a flat cart image, so
; it cannot reproduce any of this -- these bugs only show on real hardware.

; Addresses and the AUDIN bit come from cart_asm.h so this driver and sys.cpp
; cannot disagree; its static_assert block checks them against mikey.h.
#include "cart_asm.h"

IODIR    = ASM_MIKEY_IODIR
IODAT    = ASM_MIKEY_IODAT
SYSCTL1  = ASM_MIKEY_SYSCTL1
RCART0   = ASM_RCART0

EE_C_WRITE = 0x40
EE_C_READ  = 0x80
EE_C_ERASE = 0xC0
EE_C_EWEN  = 0x30
EE_C_EWDS  = 0x00

; The DI/DO line, and the only bit of the port this driver owns. Everything
; else in IODIR/IODAT belongs to sys.cpp and stays put.
EE_AUDIN     = ASM_CART_AUDIN
EE_NOT_AUDIN = ASM_CART_NOT_AUDIN

; The two IODIR states, both derived from the resting value sys.cpp installs --
; never from a read-back, which would be 0xFF. See the header.
EE_DIR_OUT = ASM_CART_IODIR_INIT
EE_DIR_IN  = (ASM_CART_IODIR_INIT & ASM_CART_NOT_AUDIN)

; Outer count of the write-busy poll's bounded wait (inner count is 256).
EE_BUSY_OUTER = 64

.extern g_cartCurrentBlock
.extern __rc2

.section .bss.eeprom,"aw",@nobits
ee_scratch:    .byte 0
ee_scratch1:   .byte 0
ee_iodat_save: .byte 0
; ee_iodat_save with the AUDIN level masked off. Computed once per entry point
; so the bit loops can OR the data bit straight in without disturbing bits 0-3.
; There is deliberately no ee_iodir_save -- see the header.
ee_iodat_base: .byte 0

; ---------------------------------------------------------------------------
.section .text.sys_EepromReadWord,"ax",@progbits
.global sys_EepromReadWord

sys_EepromReadWord:
    php
    sei
    pha                       ; save addr
    lda IODAT                 ; AUDIN level = cart ROM select; see header
    sta ee_iodat_save
    and #EE_NOT_AUDIN
    sta ee_iodat_base
    pla

    and #0x3F
    ora #EE_C_READ
    jsr EE_Send9Bit

    lda #EE_DIR_IN            ; AUDIN to input, bits 0-3 unchanged
    sta IODIR

    clc
    stz ee_scratch
    stz ee_scratch1
    ldy #15
.Lrd_loop:
    ; CLK pulse: 2 writes high, 2 writes low (A1 toggles every 2 writes)
    stz RCART0
    stz RCART0
    stz RCART0
    stz RCART0

    lda IODAT
    and #0x10                 ; mask AUDIN bit
    adc #0xF0                 ; carry := (A == 0x10)
    rol ee_scratch
    rol ee_scratch1
    dey
    bpl .Lrd_loop

    ; AUDIN back to an output BEFORE the closing strobe, matching the
    ; reference driver -- the strobe is a cart access, and AUDIN is the ROM
    ; chip select, so it must not happen with the line floating.
    lda #EE_DIR_OUT
    sta IODIR

    ; CS LOW
    ldx #0x03
    stx SYSCTL1
    dex
    stx SYSCTL1

    ; Restore the AUDIN level, then invalidate the cart cursor.
    lda ee_iodat_save
    sta IODAT
    lda #0xFF
    sta g_cartCurrentBlock

    lda ee_scratch            ; return lo in A
    ldx ee_scratch1           ; return hi in X
    plp
    rts

; ---------------------------------------------------------------------------
.section .text.sys_EepromWriteWord,"ax",@progbits
.global sys_EepromWriteWord

; void sys_EepromWriteWord(u8 wordAddr, u16 value)
;   A      = wordAddr
;   X      = value lo
;   __rc2  = value hi
sys_EepromWriteWord:
    php
    sei

    pha                       ; save addr
    lda IODAT                 ; AUDIN level = cart ROM select; see header
    sta ee_iodat_save
    and #EE_NOT_AUDIN
    sta ee_iodat_base

    ; Stash value into ee_scratch:ee_scratch1 for EE_Send16Bit
    stx ee_scratch
    lda mos8(__rc2)
    sta ee_scratch1

    lda #EE_C_EWEN
    jsr EE_Send9Bit

    pla                       ; restore addr
    and #0x3F
    ora #EE_C_WRITE
    jsr EE_Send9Bit

    jsr EE_Send16Bit

    ; Write-busy poll. Per cc65 driver: clock out a lead-in, then read
    ; AUDIN until bit 4 reads HIGH (write complete).
    ldx #63
.Lww_lead:
    stz RCART0
    stz RCART0
    dex
    bpl .Lww_lead

    lda #EE_DIR_IN
    sta IODIR

    ; The poll runs under SEI, so it must not be able to spin forever: a cart
    ; with no EEPROM (and GearLynx, which parses the LNX EEPROM byte but does
    ; not emulate the 93C46 at all) never drives AUDIN high, which would hang
    ; the machine with interrupts off. Bail out after EE_BUSY_OUTER * 256
    ; iterations (~37 ms at 4 MHz, well past the chip's 10 ms max write time)
    ; and let the caller's read-back verify decide whether it took.
    ldy #EE_BUSY_OUTER
.Lww_outer:
    ldx #0
.Lww_wait:
    lda IODAT
    and #0x10
    bne .Lww_done
    dex
    bne .Lww_wait
    dey
    bne .Lww_outer
.Lww_done:

    lda #EE_DIR_OUT           ; AUDIN back to an output
    sta IODIR

    lda #EE_C_EWDS
    jsr EE_Send9Bit

    ; Restore the AUDIN level, then invalidate the cart cursor. EE_Send9Bit
    ; just left IODAT at ee_iodat_base with AUDIN low, so this restore is what
    ; keeps the cart on the right ROM half.
    lda ee_iodat_save
    sta IODAT
    lda #0xFF
    sta g_cartCurrentBlock

    plp
    rts

; ---------------------------------------------------------------------------
.section .text.EE_Send9Bit,"ax",@progbits
EE_Send9Bit:
    ; CS LOW
    ldx #0x03
    stx SYSCTL1
    dex
    stx SYSCTL1

    ; Walk the cart shift register through (128 writes = 64 CLK cycles)
    ldx #63
.Lse9_lead:
    stz RCART0
    stz RCART0
    dex
    bpl .Lse9_lead

    ldy #8
    sec                       ; start bit -> C=1
    ror a
    ror a
    ror a
    ror a                     ; bit 8 (start) now at A position 4
.Lse9_loop:
    tax
    and #EE_AUDIN             ; AUDIN from data, bits 0-3 from the saved base
    ora ee_iodat_base
    sta IODAT
    ; CLK pulse
    stz RCART0
    stz RCART0
    stz RCART0
    stz RCART0
    txa
    rol a
    dey
    bpl .Lse9_loop

    lda ee_iodat_base         ; idle AUDIN low
    sta IODAT
    rts

; ---------------------------------------------------------------------------
.section .text.EE_Send16Bit,"ax",@progbits
EE_Send16Bit:
    ; Align ee_scratch1 (hi byte) bit 7 to A position 4 via three RORs.
    lda ee_scratch1
    ror a
    ror ee_scratch
    ror a
    ror ee_scratch
    ror a
    ror ee_scratch

    ldy #15
.Lse16_loop:
    tax
    and #EE_AUDIN
    ora ee_iodat_base
    sta IODAT
    stz RCART0
    stz RCART0
    stz RCART0
    stz RCART0
    txa
    rol ee_scratch
    rol a
    dey
    bpl .Lse16_loop

    ; CS LOW
    ldx #0x03
    stx SYSCTL1
    dex
    stx SYSCTL1

    lda ee_iodat_base
    sta IODAT
    rts
