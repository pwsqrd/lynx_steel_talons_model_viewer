; ZX02 Decompressor for Atari Lynx - Cart Reading Version
; Based on zx02-optim.asm by DMSC (MIT license)
; Modified to read from cartridge instead of memory

.extern sys_CartReadByte

; Zero-page variables (hardcoded to avoid linker issues)
zx0_offset_hi = 0x20
; zx0_dst is the caller's own __rc2:__rc3 (the dest argument), used in place:
; nothing on the read path (sys_CartReadByte -> cart_AdvancePosHi ->
; sys_CartSelectBlock) touches an imaginary register. $21-$22 are now unused.
zx0_bitr      = 0x23
zx0_pntr      = 0x24    ; 2 bytes: $24-$25

.section .text.zx0,"ax",@progbits

; void zx0_Decompress(u8* dst)
; Input: __rc2:__rc3 = destination address (lo:hi) per LLVM-MOS calling convention
;
; sys_CartDecompress(u8* dest) has the same ABI, so it is an alias label here
; rather than a jmp trampoline in cart.S.
;
; No IRQ mask here: the IRQ path (irqBody + chipper_Tick) touches only A/X/Y
; and its own zero page, never the $02-$25 imaginary registers and zx0 state
; this routine keeps its pointers in, so nothing can disturb a decompression.
.global sys_CartDecompress
.global zx0_Decompress
sys_CartDecompress:
zx0_Decompress:
    ; Initialize state
    stz zx0_offset_hi
    lda #0x80
    sta zx0_bitr
    lda #0xff
    sta zx0_pntr            ; Initial offset-1 XOR 255 = 0

; Decode literal: Copy next N bytes from cart
decode_literal:
    ldx #1
    jsr get_elias

cop0:
    jsr sys_CartReadByte ; Read from cart (result in A)
    sta (mos8(__rc2))
    inc mos8(__rc2)
    bne 1f
    inc mos8(__rc3)
1:  dex
    bne cop0

    asl zx0_bitr
    bcs dzx0s_new_offset

; Copy from last offset (repeat N bytes from last offset)
    ldx #1
    jsr get_elias

dzx0s_copy:
    lda mos8(__rc3)
    sbc zx0_offset_hi       ; C=0 from get_elias
    sta zx0_pntr+1

cop1:
    ldy mos8(__rc2)
    lda (zx0_pntr),y
    sta (mos8(__rc2))
    inc mos8(__rc2)
    bne 2f
    inc mos8(__rc3)
    inc zx0_pntr+1
2:  dex
    bne cop1

    asl zx0_bitr
    bcc decode_literal

; Copy from new offset
dzx0s_new_offset:
    ldx #1
    jsr get_elias
    beq exit                ; Read a 0, signals the end

    ; Get low part of offset from cart (the read preserves carry, X and Y)
    jsr sys_CartReadByte
    sta zx0_pntr

    ; Decrease and divide by 2
    dex
    txa
    lsr a
    sta zx0_offset_hi

    ; Divide by 2
    lda zx0_pntr
    ror a
    eor #0xff
    sta zx0_pntr

    ; Get the copy length (start elias with bit in carry)
    ldx #1
    jsr elias_skip1

    inx
    bcc dzx0s_copy

; Read an elias-gamma interlaced code
elias_get:
    asl zx0_bitr
    rol a
    tax

get_elias:
    asl zx0_bitr
    bne elias_skip1

    ; Read new byte from cart for bit reservoir (the read preserves carry)
    pha
    jsr sys_CartReadByte
    rol a
    sta zx0_bitr
    pla

elias_skip1:
    txa
    bcs elias_get
exit:
    rts
