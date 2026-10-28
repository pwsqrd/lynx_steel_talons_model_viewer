/*
 * CHIPPER song macros for llvm-mos
 * Converted from 8bit-Unity/cc65 format
 */

.macro CHAN_STOP
    .byte 0
.endm

.macro PLAY note, delay
    .byte \note, \delay
.endm

.macro DO count
    .byte 0x80, \count
.endm

.macro LOOP
    .byte 0x81
.endm

.macro DELAY delay
    .byte 0x82, \delay
.endm

.macro STOP
    .byte 0x83
.endm

.macro SET_INSTR p1, p2, p3
    .byte 0x84, 0, 0, \p1, \p2, \p3
.endm

.macro INSTR2 p1, p2, p3, p4, p5
    .byte 0x84, \p1, \p2, \p3, \p4, \p5
.endm

.macro PLAY2 reload, prescale, delay
    .byte 0x85, \reload, \prescale, \delay
.endm

.macro PATTERN pat
    .byte 0x86
    .short \pat
.endm

.macro RETURN
    .byte 0x87
.endm

.macro DEF_VOLENV nr, ptr
    .byte 0x88, \nr
    .short \ptr
.endm

.macro SET_VOLENV nr
    .byte 0x89, \nr
.endm

.macro DEF_FRQENV nr, ptr
    .byte 0x8a, \nr
    .short \ptr
.endm

.macro SET_FRQENV nr
    .byte 0x8b, \nr
.endm

.macro DEF_WAVEENV nr, ptr
    .byte 0x8c, \nr
    .short \ptr
.endm

.macro SET_WAVEENV nr
    .byte 0x8d, \nr
.endm

.macro SET_STEREO ste
    .byte 0x8e, \ste
.endm

.macro SET_ATT_ON att
    .byte 0x8f, \att
.endm

.macro SET_CH_ATT att
    .byte 0x90, \att
.endm

.macro SETPLAYERFREQ p1, p2
    .byte 0x91, \p1, \p2
.endm

.macro RETURNALL
    .byte 0x92
.endm
