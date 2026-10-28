; cart_asm.S - Tight cart-bus helpers ported from cc65 libsrc/lynx/lynx-cart.s.
;
; cc65's lynx-cart.s holds a (count XOR $FFFF) pair in X:Y so the loop
; tail is just `inx; bne; iny; beq exit` - no compare against an end
; sentinel. We adopt the same trick here, but track the global
; g_cartBlockPosition (used elsewhere - notably zx0.S for in-decompress
; boundary handling) and auto-advance at the 2048-byte block boundary.
;
; Block size is hard-coded to 2048 ($0800) since CART_BLOCK_SIZE is the
; only value actually shipped by this codebase.


RCART_0 = 0xFCB2

.extern g_cartCurrentBlock
.extern g_cartBlockPosition
.extern cart_AdvancePosHi

.section .text.cart_asm,"ax",@progbits

; ---------------------------------------------------------------------------
; void cart_skip_bytes(u16 count)
;   LLVM-MOS calling convention for a single 16-bit arg:
;     A = count_lo, X = count_hi  (verified by disasm)
; Reads `count` bytes from RCART_0 and discards them. Updates
; g_cartBlockPosition and auto-advances the block at the 2048-byte
; boundary.
; ---------------------------------------------------------------------------
.global cart_skip_bytes
cart_skip_bytes:
    ; X:Y := (count XOR $FFFF) so the loop tail is just `inx; bne; iny; beq`.
    eor #0xFF
    pha                  ; lo XOR $FF
    txa                  ; A = count_hi
    eor #0xFF
    tay                  ; Y = hi XOR $FF
    plx                  ; X = lo XOR $FF

cart_skip_loop:
    inx
    bne cart_skip_one
    iny
    beq cart_skip_done
cart_skip_one:
    lda RCART_0
    inc mos8(g_cartBlockPosition)
    bne cart_skip_loop
    jsr cart_AdvancePosHi    ; block boundary (preserves X:Y)
    bra cart_skip_loop

cart_skip_done:
    rts

; ---------------------------------------------------------------------------
; void cart_read_bytes(u8* dest, u16 count)
;   LLVM-MOS calling convention:
;     __rc2:__rc3 = dest
;     A = count_lo, X = count_hi  (last 16-bit arg goes in A:X, NOT __rc4/5)
; Reads `count` bytes from RCART_0 into `dest`. Updates
; g_cartBlockPosition and auto-advances at the block boundary.
; Uses 65C02 (zp) indirect store so Y is free for the count tail.
; ---------------------------------------------------------------------------
.global cart_read_bytes
cart_read_bytes:
    eor #0xFF
    pha                  ; lo XOR $FF
    txa                  ; A = count_hi
    eor #0xFF
    tay                  ; Y = hi XOR $FF
    plx                  ; X = lo XOR $FF

cart_read_loop:
    inx
    bne cart_read_one
    iny
    beq cart_read_done
cart_read_one:
    lda RCART_0
    sta (__rc2)
    inc mos8(__rc2)
    bne cart_read_pos
    inc mos8(__rc3)
cart_read_pos:
    inc mos8(g_cartBlockPosition)
    bne cart_read_loop
    ; Block boundary. cart_AdvancePosHi (cart.S) preserves X:Y (the
    ; count-down counter) and __rc2:__rc3 (the destination pointer).
    jsr cart_AdvancePosHi
    bra cart_read_loop

cart_read_done:
    rts
