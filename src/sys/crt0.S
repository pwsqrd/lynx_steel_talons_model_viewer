; Minimal crt0 - stage2_loader already handled:
;   - Cart reading and block switching
;   - Hardware stack setup (SP = $FF)
;   - Jumping to $0400
;
; This code is loaded to $0400 and just needs to:
;   - Set up LLVM-MOS software stack
;   - Jump to main

	.section .init.100,"axR",@progbits
	.globl _start
	.globl main

	MAPCTL  = 0xFFF9
	SERCTL  = 0xFD8C

_start:
	; Hardware stack already set by stage2_loader
	; Decimal mode already cleared, interrupts disabled

	lda #0x04
	sta MAPCTL          ; Ensure Suzy/Mikey access; hide boot ROM (ROMSPACE=1).
	                    ; sys_Init sets VECTOR_SPACE before installing the IRQ
	                    ; vector.
	sta SERCTL          ; Serial control - disable external I/O (A still 0x04)

	; Paint software stack with sentinel for peak-usage measurement
	; Fills $0300-$03FF with $A5
	ldx #0
paint_loop:
	lda #0xA5
	sta 0x0300,x
	inx
	bne paint_loop

	; Software stack setup for LLVM-MOS
	lda #mos16lo(__stack)
	sta mos8(__rc0)
	lda #mos16hi(__stack)
	sta mos8(__rc1)
	jmp main
