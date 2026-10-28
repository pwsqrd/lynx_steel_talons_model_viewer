; Stage 2 Loader (fits in 256 bytes)
; Loaded by micro_loader to $0300, executes immediately.
;
; Cart layout when this runs:
;   Bytes 0-51:    Encrypted micro_loader (already executed)
;   Bytes 52-307:  This loader (256 bytes, already loaded to $0300)
;   Bytes 308-309: Code size (2 bytes, little-endian)
;   Bytes 310+:    Actual code
;
; Cart position when we start: 308 (micro_loader read 256 bytes from offset 52)
;
; This loader:
;   1. Reads code size from bytes 308-309
;   2. Loads code to $0400 with block switching
;   3. Jumps to $0400

	.section .text,"ax",@progbits
	.globl _start

	RCART_0 = 0xFCB2
	IODAT   = 0xFD8B
	SYSCTL1 = 0xFD87
	MAPCTL  = 0xFFF9

; Zero-page variables (use safe range $20-$2F)
ZP_SIZE_LO   = 0x20
ZP_SIZE_HI   = 0x21
ZP_DEST_LO   = 0x22
ZP_DEST_HI   = 0x23
ZP_BLOCK_POS_LO = 0x24
ZP_BLOCK_POS_HI = 0x25
ZP_BLOCK_NUM = 0x26
ZP_IODAT_BASE = 0x27

_start:
	; Ensure Mikey access is enabled
	stz MAPCTL

	; Read code size (2 bytes at cart offset 308-309)
	lda RCART_0         ; size_lo (byte 308)
	sta ZP_SIZE_LO
	lda RCART_0         ; size_hi (byte 309)
	sta ZP_SIZE_HI

	; If size is 0, skip loading
	ora ZP_SIZE_LO
	beq done

	; Destination = $0400
	stz ZP_DEST_LO
	lda #0x04
	sta ZP_DEST_HI

	; Current block position = 310 (52 + 256 + 2)
	lda #<310
	sta ZP_BLOCK_POS_LO
	lda #>310
	sta ZP_BLOCK_POS_HI

	; Current block number = 0
	stz ZP_BLOCK_NUM

load_loop:
	; Read byte from cart
	lda RCART_0
	sta (ZP_DEST_LO)     ; 65C02 indirect

	; Increment dest
	inc ZP_DEST_LO
	bne 1f
	inc ZP_DEST_HI
1:
	; Decrement size
	lda ZP_SIZE_LO
	bne 2f
	dec ZP_SIZE_HI
2:	dec ZP_SIZE_LO

	; Check if done (size == 0)
	lda ZP_SIZE_LO
	ora ZP_SIZE_HI
	beq done

	; Increment block position
	inc ZP_BLOCK_POS_LO
	bne load_loop
	inc ZP_BLOCK_POS_HI

	; Check if position >= 2048 (0x0800)
	lda ZP_BLOCK_POS_HI
	cmp #0x08
	bcc load_loop       ; Still in current block

	; Block boundary crossed - switch to next block
	inc ZP_BLOCK_NUM
	lda ZP_BLOCK_NUM
	; NOTE: select_block is at offset 0x5b, loaded at $0300, so address is $035b
	jsr 0x035b

	; Reset position to 0
	stz ZP_BLOCK_POS_LO
	stz ZP_BLOCK_POS_HI

	bra load_loop

done:
	; Set up hardware stack
	ldx #0xff
	txs
	cld                 ; Clear decimal mode
	sei                 ; Disable interrupts (will be set up by main code)

	; Jump to loaded code at $0400
	jmp 0x0400

; Select cart block (block number in A)
; Shifts 8 bits MSB first into shift register via IODAT/SYSCTL1
select_block:
	pha                 ; Save block number
	lda IODAT
	and #0xFC           ; Clear bits 0-1, keep bits 2-7
	sta ZP_IODAT_BASE   ; Save base IODAT value
	pla                 ; Restore block number
	ldx #8
sb_loop:
	asl                 ; Shift MSB into carry
	tay                 ; Save block number
	lda ZP_IODAT_BASE   ; Base IODAT (bits 0-1 clear)
	bcc 3f
	ora #2              ; Set CART_ADDR_DATA bit if carry set
3:	sta IODAT
	; SYSCTL1 is write-only - can't read-modify-write!
	; Use fixed values: POWERON (0x02) must stay set
	lda #0x03           ; POWERON | CART_ADDR_STROBE
	sta SYSCTL1         ; Strobe high
	lda #0x02           ; POWERON only
	sta SYSCTL1         ; Strobe low (latches shift register bit)
	tya                 ; Restore block number
	dex
	bne sb_loop
	rts

	; Padding will be done by pack_lynx_cart.py (pads to 256 bytes)
