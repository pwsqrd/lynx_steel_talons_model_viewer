; =============================================================================
; cart.S — resident SDK cart I/O wrappers.
;
; Hand-asm of the cart cluster. The tight per-byte inner loops
; (cart_read_bytes / cart_skip_bytes) live in sys/cart_asm.S and are called
; from here. sys_CartSeek (u24 offset) and the allocating sys_CartRead stay
; C++ in sys.cpp.
;
; MOS C calling convention (verified against the baseline t.elf listings):
;   sys_CartSelectBlock(block):                 block in A         (extern "C")
;   sys_CartInit:                          no args
;   sys_CartSeekBlockPos(block, pos):     block A, pos X(lo):__rc2(hi)
;   sys_CartReadByte:                      returns data in A; preserves
;                                               X and Y (zx0.S relies on it)
;   sys_CartReadWord:                      returns A(lo):X(hi)
;   sys_CartReadAsset(dest, &a):   dest __rc2:3, asset __rc4:5
;   sys_CartDecompress(dest):             dest __rc2:3 (an alias label on
;                                               zx0_Decompress, defined in zx0.S)
;   sys_CartReadRaw(dest,block,pos,size): dest __rc2:3, block A,
;                                               pos X(lo):__rc4(hi),
;                                               size __rc5:6
;   Mangled names are exported verbatim so C++ and hand-asm callers (zx0.S)
;   link against them; changing a C++ signature in sys.h breaks the link.
;
; Internal clobber contract (this file only):
;   sys_CartSelectBlock clobbers A only (X, Y and every __rcN survive), so
;   the byte readers need no saves around a block boundary.
;   sys_CartSeekBlockPos clobbers A, X, Y only — __rc2..__rc6 SURVIVE, which
;   the two sys_CartRead bodies rely on (dest.hi in __rc3, size in __rc5:6).
;   Both sets are subsets of what the old C++ bodies clobbered, so no
;   external caller can regress. __rc20/__rc21 are never touched.
;
; The 8-bit cart address shift in sys_CartSelectBlock is ordering-sensitive
; hardware code: per bit, IODAT carries the data bit, then SYSCTL1 strobes
; high then low. MSB first, 8 bits, IODAT restored to base after the loop —
; byte-faithful to the C++ reference.
; =============================================================================

#include "cart_asm.h"

.extern g_cartCurrentBlock      ; ZP u8  (sys.cpp)
.extern g_cartBlockPosition     ; ZP u16 (sys.cpp)
.extern cart_read_bytes         ; sys/cart_asm.S
.extern cart_skip_bytes         ; sys/cart_asm.S

.section .text.sys_CartSelectBlock,"ax",@progbits
.global sys_CartInit
sys_CartInit:
    lda #ASM_CART_STROBE_ON
    sta ASM_MIKEY_SYSCTL1
    lda #ASM_CART_IODIR_INIT
    sta ASM_MIKEY_IODIR
    lda #$FF
    sta mos8(g_cartCurrentBlock)
    ; g_cartBlockPosition is not cleared here: SelectBlock(0) below cannot
    ; early-out (0 != 0xFF) and unconditionally zeroes it.
    lda #0                          ; falls through into sys_CartSelectBlock

.global sys_CartSelectBlock
sys_CartSelectBlock:
    cmp mos8(g_cartCurrentBlock)
    beq .Lsel_done
    sta mos8(g_cartCurrentBlock)    ; shifted out by 9-bit rotation below
    phx
    phy
    lda ASM_MIKEY_IODAT
    and #$FC
    tax                             ; iodat_base
    ldy #8
.Lsel_loop:
    txa
    rol mos8(g_cartCurrentBlock)    ; MSB -> carry
    bcc .Lsel_bit_clear
    ora #ASM_CART_ADDR_DATA
.Lsel_bit_clear:
    sta ASM_MIKEY_IODAT
    lda #ASM_CART_STROBE_ON
    sta ASM_MIKEY_SYSCTL1
    lda #ASM_CART_STROBE_OFF
    sta ASM_MIKEY_SYSCTL1
    dey
    bne .Lsel_loop
    ; ninth rol of the 9-bit (C:block) rotation restores the block byte
    rol mos8(g_cartCurrentBlock)
    stx ASM_MIKEY_IODAT
    stz mos8(g_cartBlockPosition)
    stz mos8(g_cartBlockPosition+1)
    ply
    plx
.Lsel_done:
    rts

; Shared block-boundary tail for every byte reader: called after the low byte
; of g_cartBlockPosition wrapped to 0. Advances the high byte and selects the
; next block at 2048. Clobbers A only; preserves X, Y, carry and every __rcN.
.global cart_AdvancePosHi
cart_AdvancePosHi:
    php
    inc mos8(g_cartBlockPosition+1)
    lda mos8(g_cartBlockPosition+1)
    cmp #ASM_CART_BLOCK_SIZE_HI
    bcc .Ladv_done
    lda mos8(g_cartCurrentBlock)
    inc a
    jsr sys_CartSelectBlock
.Ladv_done:
    plp
    rts

.section .text.sys_CartSeekBlockPos,"ax",@progbits
.global sys_CartSeekBlockPos
sys_CartSeekBlockPos:
    tay                             ; block (SelectBlock preserves X/__rc2)
    ; if (block != g_cartCurrentBlock || pos < g_cartBlockPosition)
    ;     g_cartCurrentBlock = 0xFF;
    cmp mos8(g_cartCurrentBlock)
    bne .Lseek_invalidate
    cpx mos8(g_cartBlockPosition)
    lda mos8(__rc2)
    sbc mos8(g_cartBlockPosition+1)
    bcs .Lseek_select               ; pos >= position
.Lseek_invalidate:
    lda #$FF
    sta mos8(g_cartCurrentBlock)
.Lseek_select:
    tya
    jsr sys_CartSelectBlock
    ; if (pos > position) cart_skip_bytes(pos - position)
    ; A zero diff falls through to cart_skip_bytes(0), a no-op by its
    ; count-XOR-$FFFF loop shape; the bcc keeps the pos < position case
    ; a no-call exactly like the C++ strict >.
    txa
    sec
    sbc mos8(g_cartBlockPosition)
    tay
    lda mos8(__rc2)
    sbc mos8(g_cartBlockPosition+1)
    bcc .Lseek_done
    tax                             ; diff.hi
    tya                             ; diff.lo
    jmp cart_skip_bytes
.Lseek_done:
    rts

.section .text.sys_CartReadByte,"ax",@progbits
.global sys_CartReadByte
sys_CartReadByte:
    lda ASM_RCART0
    inc mos8(g_cartBlockPosition)
    bne .Lrb_done
    pha
    jsr cart_AdvancePosHi
    pla
.Lrb_done:
    rts

.section .text.sys_CartReadWord,"ax",@progbits
.global sys_CartReadWord
sys_CartReadWord:
    jsr sys_CartReadByte
    pha
    jsr sys_CartReadByte
    tax
    pla
    rts

.section .text.sys_CartRead_asset,"ax",@progbits
.global sys_CartReadAsset
sys_CartReadAsset:
    ldy #ASM_CARTASSET_POS_OFF+1
    lda (mos8(__rc4)),y             ; asset->pos.hi
    ldx mos8(__rc2)                 ; dest.lo: __rc2 is SeekBlockPos's pos.hi
    phx
    sta mos8(__rc2)
    dey
    lda (mos8(__rc4)),y             ; asset->pos.lo
    tax
    lda (mos8(__rc4))               ; asset->block
    jsr sys_CartSeekBlockPos  ; dest.hi survives in __rc3
    jsr sys_CartReadWord       ; discard the zx0 uncompressed-size word
    pla
    sta mos8(__rc2)
    jmp sys_CartDecompress

.section .text.sys_CartRead_raw,"ax",@progbits
.global sys_CartReadRaw
sys_CartReadRaw:
    ldy mos8(__rc2)                 ; dest.lo (__rc3, size __rc5:6 survive)
    phy
    ldy mos8(__rc4)                 ; pos.hi -> __rc2 (SeekBlockPos ABI)
    sty mos8(__rc2)
    jsr sys_CartSeekBlockPos
    pla
    sta mos8(__rc2)
    lda mos8(__rc5)                 ; size -> A:X (cart_read_bytes ABI)
    ldx mos8(__rc6)
    jmp cart_read_bytes

