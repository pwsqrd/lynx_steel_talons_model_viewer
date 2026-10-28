/*
 * CHIPPER Sound driver for the Atari Lynx (llvm-mos version)
 * Converted from cc65 lynx-snd.s by Karri Kaksonen and Bjoern Spruck
 *
 * All _IFEQ/_IFNE/_ELSE/_ENDIF macros manually expanded to branch labels
 */

/* Hardware register addresses */
.set STIMBKUP,  0xFD1C
.set STIMCTLA,  0xFD1D
.set MSTEREO,   0xFD50
.set AUD0VOL,   0xFD20
.set AUD1VOL,   0xFD28
.set AUD2VOL,   0xFD30
.set AUD3VOL,   0xFD38
.set AUD0CTLA,  0xFD25
.set AUD1CTLA,  0xFD2D
.set AUD2CTLA,  0xFD35
.set AUD3CTLA,  0xFD3D

.set MAX_INSTRUMENTS, 64

/* Zero page variables. Flags "aw" (allocatable, writable) so lld respects
 * the linker script's `> zp` region -- without "a" the section is marked
 * non-allocatable, lld silently ignores the memory region and places
 * SndSema..SndEnvPtr at $00, overlapping __rc0..__rc8, which the VBL tick
 * then corrupts on every interrupt. */
.section .zp.bss.chipper,"aw",@nobits
SndSema:        .space 1
SndPtrTmp:      .space 2
SndTmp:         .space 2
SndEnvPtr:      .space 2

/* BSS variables */
.section .bss.chipper,"aw",@nobits
SndRetAFlag2:       .space 1
SndRetAFlag:        .space 1
SndPtrLo:           .space 4
SndPtrHi:           .space 4
SndDelay:           .space 4
SndLoopCnt:         .space 4
SndLoopPtrLo:       .space 4
SndLoopPtrHi:       .space 4
SndVolume:          .space 4
SndMaxVolume:       .space 4
SndNotePlaying:     .space 4
SndRetAddr:         .space 8
SndActive:          .space 4
SndReqStop:         .space 4
SndEnvVol:          .space 4
SndEnvFrq:          .space 4
SndEnvWave:         .space 4
SndChannel:         .space 32
SndEnvVolCnt:       .space 4
SndEnvVolInc:       .space 4
SndEnvVolOff:       .space 4
SndEnvVolLoop:      .space 4
SndEnvVolParts:     .space 4
SndEnvVolParts2:    .space 4
SndEnvFrqCnt:       .space 4
SndEnvFrqInc:       .space 4
SndEnvFrqOff:       .space 4
SndEnvFrqLoop:      .space 4
SndEnvFrqParts:     .space 4
SndEnvFrqParts2:    .space 4
SndEnvWaveCnt:      .space 4
SndEnvWaveOff:      .space 4
SndEnvWaveLoop:     .space 4
SndEnvWaveParts:    .space 4
SndEnvWaveParts2:   .space 4

SndEnvVolPtrLo:     .space MAX_INSTRUMENTS
SndEnvVolPtrHi:     .space MAX_INSTRUMENTS
SndEnvFrqPtrLo:     .space MAX_INSTRUMENTS
SndEnvFrqPtrHi:     .space MAX_INSTRUMENTS
SndEnvWavePtrLo:    .space MAX_INSTRUMENTS
SndEnvWavePtrHi:    .space MAX_INSTRUMENTS

/* Saved pause state - NOT self-modifying */
SndPausedCtrl:      .space 1
SndPausedStereo:    .space 1

/* Read-only data */
.section .rodata,"a",@progbits

SndOffsets: .byte 0x00, 0x08, 0x10, 0x18
SndMask:    .byte 1, 2, 4, 8

SndPrescaler:
    .byte 0x00,0x06,0x06,0x06,0x06,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x04,0x04,0x04,0x04
    .byte 0x04,0x04,0x04,0x04,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x02,0x02
    .byte 0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x01,0x01,0x01,0x01,0x01
    .byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00
    .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00

SndReload:
    .byte 0x00,0x9A,0x96,0x8F,0x86,0xFA,0xE5,0xD1,0xBE,0xAC,0x9C,0x8D,0x00,0xE8,0xD3,0xC0
    .byte 0xAF,0xA0,0x93,0x87,0xFA,0xE7,0xD6,0xC6,0xB8,0xAC,0xA1,0x96,0x8D,0x84,0xFA,0xEB
    .byte 0xDE,0xD2,0xC7,0xBC,0xB3,0xAA,0xA1,0x9A,0x93,0x8C,0x86,0x00,0xF5,0xEB,0xE1,0xD8
    .byte 0xCF,0xC7,0xC0,0xB9,0xB2,0xAB,0xA5,0xA0,0x9A,0x95,0x90,0x8B,0x87,0x82,0xFD,0xF5
    .byte 0xEE,0xE7,0xE0,0xD9,0xD3,0xCD,0xC8,0xC2,0xBD,0xB8,0xB3,0xAE,0xAA,0xA5,0xA1,0x9D
    .byte 0x99,0x96,0x92,0x8F,0x8B,0x88,0x85,0x82,0x7F,0x7C,0x79,0x77,0x74,0x72,0x6F,0x6D
    .byte 0x6B,0x69,0x67,0x64,0x63,0x61,0x5F,0x5D,0x5B,0x59,0x58,0x56,0x55,0x53,0x51,0x50
    .byte 0x4F,0x4D,0x4C,0x4B,0x49,0x48,0x47,0x46,0x44,0x43,0x42,0x41,0x40,0x3F,0x3E,0x3D

/* Command dispatch tables - addresses minus 1 for RTS trick */
SndCmdsLo:
    .byte SndLoopM1@mos16lo
    .byte SndDoM1@mos16lo
    .byte SndPauseM1@mos16lo
    .byte SndNoteOffM1@mos16lo
    .byte SndSetInstrM1@mos16lo
    .byte SndNewNote2M1@mos16lo
    .byte SndCallPatternM1@mos16lo
    .byte SndRetToSongM1@mos16lo
    .byte SndDefEnvVolM1@mos16lo
    .byte SndSetEnvVolM1@mos16lo
    .byte SndDefEnvFrqM1@mos16lo
    .byte SndSetEnvFrqM1@mos16lo
    .byte SndDefEnvWaveM1@mos16lo
    .byte SndSetEnvWaveM1@mos16lo
    .byte SndSetStereoM1@mos16lo
    .byte SndSetAttenuationOnM1@mos16lo
    .byte SndSetChnAttenuationM1@mos16lo
    .byte SndPlayerFreqM1@mos16lo
    .byte SndReturnAllM1@mos16lo

SndCmdsHi:
    .byte SndLoopM1@mos16hi
    .byte SndDoM1@mos16hi
    .byte SndPauseM1@mos16hi
    .byte SndNoteOffM1@mos16hi
    .byte SndSetInstrM1@mos16hi
    .byte SndNewNote2M1@mos16hi
    .byte SndCallPatternM1@mos16hi
    .byte SndRetToSongM1@mos16hi
    .byte SndDefEnvVolM1@mos16hi
    .byte SndSetEnvVolM1@mos16hi
    .byte SndDefEnvFrqM1@mos16hi
    .byte SndSetEnvFrqM1@mos16hi
    .byte SndDefEnvWaveM1@mos16hi
    .byte SndSetEnvWaveM1@mos16hi
    .byte SndSetStereoM1@mos16hi
    .byte SndSetAttenuationOnM1@mos16hi
    .byte SndSetChnAttenuationM1@mos16hi
    .byte SndPlayerFreqM1@mos16hi
    .byte SndReturnAllM1@mos16hi


/* Code section */
.section .text.chipper,"ax",@progbits

/* Define address-1 symbols for jump table (RTS trick) */
.set SndLoopM1, SndLoop - 1
.set SndDoM1, SndDo - 1
.set SndPauseM1, SndPause - 1
.set SndNoteOffM1, SndNoteOff - 1
.set SndSetInstrM1, SndSetInstr - 1
.set SndNewNote2M1, SndNewNote2 - 1
.set SndCallPatternM1, SndCallPattern - 1
.set SndRetToSongM1, SndRetToSong - 1
.set SndDefEnvVolM1, SndDefEnvVol - 1
.set SndSetEnvVolM1, SndSetEnvVol - 1
.set SndDefEnvFrqM1, SndDefEnvFrq - 1
.set SndSetEnvFrqM1, SndSetEnvFrq - 1
.set SndDefEnvWaveM1, SndDefEnvWave - 1
.set SndSetEnvWaveM1, SndSetEnvWave - 1
.set SndSetStereoM1, SndSetStereo - 1
.set SndSetAttenuationOnM1, SndSetAttenuationOn - 1
.set SndSetChnAttenuationM1, SndSetChnAttenuation - 1
.set SndPlayerFreqM1, SndPlayerFreq - 1
.set SndReturnAllM1, SndReturnAll - 1

/*
 * void chipper_Init(void)
 * Initialize the sound engine
 */
.global chipper_Init
chipper_Init:
    php
    sei

    stz AUD0VOL
    stz AUD1VOL
    stz AUD2VOL
    stz AUD3VOL

    stz 0xfd44          /* all channels full volume / no attenuation */
    lda #0xff
    stz MSTEREO

    lda #0
    sta AUD0CTLA
    sta AUD1CTLA
    sta AUD2CTLA
    sta AUD3CTLA

    ldx #3
    lda #0
.Linit0:
    stz SndActive,x
    stz SndReqStop,x
    stz SndEnvVol,x
    stz SndEnvFrq,x
    stz SndEnvWave,x
    ldy SndOffsets,x
    sta SndChannel+2,y
    dex
    bpl .Linit0
    stz SndRetAFlag
    stz SndRetAFlag2
    stz SndSema
    plp
    rts


/*
 * void chipper_Tick(void)
 * Called from Timer 0/5 interrupt at 240Hz
 */
.global chipper_Tick
chipper_Tick:
    lda #0xff
    tsb SndSema
    bne .Lendirq
    phy

    /* Set values from last interrupt */
    jsr SndSetValues

    /* Check return flag */
    lda SndRetAFlag
    beq .Lif01_else
        lda #0x0F
        sta SndRetAFlag2
        stz SndRetAFlag
.Lif01_else:

    ldx #3
.Lirq0:
    phx
    lda SndActive,x
    beq .Lif02_else
        lda SndEnvVol,x
        beq .Lif03_else
            phx
            jsr SndChangeVol
            plx
.Lif03_else:
        lda SndEnvFrq,x
        beq .Lif04_else
            phx
            jsr SndChangeFrq
            plx
.Lif04_else:
        lda SndEnvWave,x
        beq .Lif05_else
            phx
            jsr SndChangeWave
            plx
.Lif05_else:
        jsr SndGetCmd
.Lif02_else:
    plx
    dex
    bpl .Lirq0
    ply
    stz SndSema
.Lendirq:
    clc
    rts


/* Get next sound command from stream */
SndGetCmd:
    lda SndReqStop,x
    bne SndStop

    lda SndRetAFlag2
    and SndMask,x
    beq .Lif06_else
        eor SndRetAFlag2
        sta SndRetAFlag2
        lda SndRetAddr,x
        sta SndPtrLo,x
        lda SndRetAddr+4,x
        sta SndPtrHi,x
        bra .Lif06_endif
.Lif06_else:
        dec SndDelay,x
        bne .Lcmd991
.Lif06_endif:

    lda SndPtrLo,x
    sta SndPtrTmp
    lda SndPtrHi,x
    sta SndPtrTmp+1
.Lcmd0:
    lda (SndPtrTmp)
    beq SndStop
    bpl .Lif07_else
        and #0x7f
        tay
        jsr SndCallCmd
        bra .Lif07_endif
.Lif07_else:
        jsr SndNewNote
.Lif07_endif:
    clc
    tya
    and #0x7f
    adc SndPtrTmp
    sta SndPtrLo,x
    sta SndPtrTmp
    lda #0
    adc SndPtrTmp+1
    sta SndPtrHi,x
    sta SndPtrTmp+1

    tya
    bmi .Lcmd0
.Lcmd991:
    lda #1
    cmp SndDelay,x
    bne .Lcmd99

    lda SndPtrLo,x
    sta SndPtrTmp
    lda SndPtrHi,x
    sta SndPtrTmp+1

    lda (SndPtrTmp)
    cmp #0x92           /* Return all? */
    bne .Lcmd99
        sta SndRetAFlag
.Lcmd99:
    rts


/* Call function by table index Y */
SndCallCmd:
    lda SndCmdsHi,y
    pha
    lda SndCmdsLo,y
    pha
    ldy #1
    rts


/* Stop sound on channel X */
SndStop:
    stz SndReqStop,x
    stz SndActive,x
    ldy SndOffsets,x
    lda #0
    sta SndChannel,y
    inc a
    sta SndChannel+2,y
    tay
    rts


/* Play note from note table */
SndNewNote:
    phx
    sta SndNotePlaying,x
    pha
    ldy #1
    lda (SndPtrTmp),y
    sta SndDelay,x
    ldy SndOffsets,x
    lda SndVolume,x
    sta SndChannel,y
    plx
    lda SndPrescaler,x
    sta SndChannel+5,y
    lda SndReload,x
    sta SndChannel+4,y
    lda #0xFF
    sta SndChannel+2,y
    plx
    lda SndEnvVol,x
    beq .Lnn_skip_vol
        jsr SndSetEnvVol1
.Lnn_skip_vol:
    lda SndEnvFrq,x
    beq .Lnn_skip_frq
        jsr SndSetEnvFrq1
.Lnn_skip_frq:
    lda SndEnvWave,x
    beq .Lnn_skip_wave
        jsr SndSetEnvWave1
.Lnn_skip_wave:
    ldy #0x2
    rts


/* Loop with count */
SndLoop:
    lda (SndPtrTmp),y
    sta SndLoopCnt,x
    lda SndPtrTmp
    sta SndLoopPtrLo,x
    lda SndPtrTmp+1
    sta SndLoopPtrHi,x
    ldy #0x82
    rts

SndDo:
    dec SndLoopCnt,x
    beq .Ldo_else
        lda SndLoopPtrLo,x
        sta SndPtrTmp
        lda SndLoopPtrHi,x
        sta SndPtrTmp+1
        ldy #0x82
        rts
.Ldo_else:
        ldy #0x81
        rts


/* Volume envelope */
SndDefEnvVol:
    phx
    lda (SndPtrTmp),y
    tax
    iny
    lda (SndPtrTmp),y
    sta SndEnvVolPtrLo,x
    iny
    lda (SndPtrTmp),y
    sta SndEnvVolPtrHi,x
    ldy #0x84
    plx
    rts

SndSetEnvVol:
    lda (SndPtrTmp),y

SndSetEnvVol1:
    and #0x7f
    sta SndEnvVol,x
    bne .Lsev_continue
        ldy #0x82
        rts
.Lsev_continue:
    tay
    lda SndEnvVolPtrLo,y
    sta SndEnvPtr
    lda SndEnvVolPtrHi,y
    sta SndEnvPtr+1
    lda (SndEnvPtr)
    sta SndTmp
    asl
    sta SndEnvVolLoop,x
    ldy #1
    lda (SndEnvPtr),y
    sta SndEnvVolParts,x
    sec
    sbc SndTmp
    sta SndEnvVolParts2,x
    stz SndEnvVolCnt,x
    lda #2
    sta SndEnvVolOff,x
    ldy #0x82
    rts


/* Frequency envelope */
SndDefEnvFrq:
    phx
    lda (SndPtrTmp),y
    tax
    iny
    lda (SndPtrTmp),y
    sta SndEnvFrqPtrLo,x
    iny
    lda (SndPtrTmp),y
    sta SndEnvFrqPtrHi,x
    plx
    ldy #0x84
    rts

SndSetEnvFrq:
    lda (SndPtrTmp),y

SndSetEnvFrq1:
    and #0x7f
    sta SndEnvFrq,x
    bne .Lsef_continue
        ldy #0x82
        rts
.Lsef_continue:
    tay
    lda SndEnvFrqPtrLo,y
    sta SndEnvPtr
    lda SndEnvFrqPtrHi,y
    sta SndEnvPtr+1
    lda (SndEnvPtr)
    sta SndTmp
    asl
    sta SndEnvFrqLoop,x
    ldy #1
    lda (SndEnvPtr),y
    sta SndEnvFrqParts,x
    sec
    sbc SndTmp
    sta SndEnvFrqParts2,x
    stz SndEnvFrqCnt,x
    lda #2
    sta SndEnvFrqOff,x
    ldy #0x82
    rts


/* Wave envelope */
SndDefEnvWave:
    phx
    lda (SndPtrTmp),y
    tax
    iny
    lda (SndPtrTmp),y
    sta SndEnvWavePtrLo,x
    iny
    lda (SndPtrTmp),y
    sta SndEnvWavePtrHi,x
    plx
    ldy #0x84
    rts

SndSetEnvWave:
    lda (SndPtrTmp),y

SndSetEnvWave1:
    and #0x7f
    sta SndEnvWave,x
    bne .Lsew_continue
        ldy #0x82
        rts
.Lsew_continue:
    tay
    lda SndEnvWavePtrLo,y
    sta SndEnvPtr
    lda SndEnvWavePtrHi,y
    sta SndEnvPtr+1
    lda (SndEnvPtr)
    sta SndTmp
    asl
    dec a
    asl
    sta SndEnvWaveLoop,x
    ldy #1
    lda (SndEnvPtr),y
    sta SndEnvWaveParts,x
    sec
    sbc SndTmp
    sta SndEnvWaveParts2,x
    stz SndEnvWaveCnt,x
    lda #2
    sta SndEnvWaveOff,x
    ldy #0x82
    rts


/* Pause/delay */
SndPause:
    lda (SndPtrTmp),y
    sta SndDelay,x
    iny
    rts


/* Set player frequency */
SndPlayerFreq:
    iny
    lda (SndPtrTmp),y       /* Get backup value first */
    sta STIMBKUP
    dey
    lda (SndPtrTmp),y       /* Then control */
    sta STIMCTLA
    ldy #0x83
    rts


/* Note off */
SndNoteOff:
    ldy SndOffsets,x
    stz SndNotePlaying,x
    lda SndEnvVol,x
    ora #0x80
    sta SndEnvVol,x
    lda SndEnvFrq,x
    ora #0x80
    sta SndEnvFrq,x
    lda SndEnvWave,x
    ora #0x80
    sta SndEnvWave,x
    lda #0
    sta SndChannel,y
    sta SndChannel+4,y
    sta SndChannel+5,y
    dec a
    sta SndChannel+2,y
    ldy #0x81
    rts


/* Set instrument */
SndSetInstr:
    phx
    lda SndOffsets,x
    tax
    lda (SndPtrTmp),y
    sta SndChannel+3,x
    iny
    lda (SndPtrTmp),y
    sta SndChannel+7,x
    iny
    lda (SndPtrTmp),y
    sta SndChannel+1,x
    plx
    iny
    lda (SndPtrTmp),y
    sta SndVolume,x
    iny
    lda (SndPtrTmp),y
    sta SndMaxVolume,x
    ldy #0x86
    rts


/* Call pattern */
SndCallPattern:
    clc
    lda SndPtrTmp
    adc #3
    sta SndRetAddr,x
    lda SndPtrTmp+1
    adc #0
    sta SndRetAddr+4,x
    ldy #1
    lda (SndPtrTmp),y
    pha
    iny
    lda (SndPtrTmp),y
    sta SndPtrTmp+1
    pla
    sta SndPtrTmp
    ldy #0x80
    rts


/* Return to song */
SndRetToSong:
    lda SndRetAddr,x
    sta SndPtrTmp
    lda SndRetAddr+4,x
    sta SndPtrTmp+1
    ldy #0x80
    rts


/* Return all channels */
SndReturnAll:
    lda #1
    sta SndRetAFlag
    sta SndDelay,x
    ldy #0x0
    rts


/* New note with explicit pitch */
SndNewNote2:
    phx
    sta SndNotePlaying,x
    ldy #1
    lda (SndPtrTmp),y       /* reload */
    pha
    iny
    lda (SndPtrTmp),y       /* prescale */
    pha
    iny
    lda (SndPtrTmp),y       /* delay */
    sta SndDelay,x
    ldy SndOffsets,x
    lda SndVolume,x
    sta SndChannel,y
    pla
    sta SndChannel+5,y
    pla
    sta SndChannel+4,y
    lda #0xFF
    sta SndChannel+2,y
    plx
    lda SndEnvVol,x
    beq .Lnn2_skip_vol
        jsr SndSetEnvVol1
.Lnn2_skip_vol:
    lda SndEnvFrq,x
    beq .Lnn2_skip_frq
        jsr SndSetEnvFrq1
.Lnn2_skip_frq:
    lda SndEnvWave,x
    beq .Lnn2_skip_wave
        jsr SndSetEnvWave1
.Lnn2_skip_wave:
    ldy #4
    rts


/* Set stereo */
SndSetStereo:
    ldy #1
    lda (SndPtrTmp),y
    sta MSTEREO
    ldy #0x82
    rts


/* Set attenuation on */
SndSetAttenuationOn:
    ldy #1
    lda (SndPtrTmp),y
    sta 0xFD44
    ldy #0x82
    rts


/* Set channel attenuation */
SndSetChnAttenuation:
    ldy #1
    lda (SndPtrTmp),y
    sta 0xFD40,x
    ldy #0x82
    rts


/* Volume envelope processing */
SndChangeVol:
    tay
    bpl .Lvol_continue
.Lvol99:
    rts
.Lvol_continue:
    lda SndNotePlaying,x
    beq .Lvol99

    lda SndEnvVolPtrLo,y
    sta SndEnvPtr
    lda SndEnvVolPtrHi,y
    sta SndEnvPtr+1

    dec SndEnvVolCnt,x
    bpl .Lvol_apply

    dec SndEnvVolParts,x
    bpl .Lvol_next_part

    lda SndEnvVolLoop,x
    beq .Lvol_end_env
        tay
        lda SndEnvVolParts2,x
        sta SndEnvVolParts,x
        bra .Lvol1v
.Lvol_end_env:
        tya
        ora #0x80
        sta SndEnvVol,x
        rts

.Lvol_next_part:
    ldy SndEnvVolOff,x
.Lvol1v:
    lda (SndEnvPtr),y
    sta SndEnvVolCnt,x
    iny
    lda (SndEnvPtr),y
    sta SndEnvVolInc,x
    iny
    tya
    sta SndEnvVolOff,x
    rts

.Lvol_apply:
    ldy SndOffsets,x
    clc
    lda SndEnvVolInc,x
    beq .Lvol99
    bmi .Lvol_dec

    /* Increase volume */
    adc SndChannel,y
    cmp SndMaxVolume,x
    bcc .Lvol_check_max
        lda SndMaxVolume,x
        stz SndEnvVolInc,x
.Lvol_check_max:
    cmp #0x80
    bcc .Lvol_store
        lda #0x7f
        stz SndEnvVolInc,x
        bra .Lvol_store

.Lvol_dec:
    /* Decrease volume */
    adc SndChannel,y
    bcs .Lvol_check_neg
        lda #0
        sta SndEnvVolInc,x
        sta SndNotePlaying,x
        bra .Lvol_store
.Lvol_check_neg:
    cmp #0x80
    bcc .Lvol_store
        lda #0
        sta SndEnvVolInc,x
        sta SndNotePlaying,x

.Lvol_store:
    sta SndChannel,y
    lda SndChannel+2,y
    ora #1
    sta SndChannel+2,y
    rts


/* Frequency envelope processing */
SndChangeFrq:
    tay
    bpl .Lfrq_continue
.Lfrq99:
    rts
.Lfrq_continue:
    lda SndNotePlaying,x
    beq .Lfrq99

    lda SndEnvFrqPtrLo,y
    sta SndEnvPtr
    lda SndEnvFrqPtrHi,y
    sta SndEnvPtr+1

    dec SndEnvFrqCnt,x
    bpl .Lfrq_apply

    dec SndEnvFrqParts,x
    bpl .Lfrq_next_part

    lda SndEnvFrqLoop,x
    beq .Lfrq_end_env
        tay
        lda SndEnvFrqParts2,x
        sta SndEnvFrqParts,x
        bra .Lfrq1f
.Lfrq_end_env:
        tya
        ora #0x80
        sta SndEnvFrq,x
        rts

.Lfrq_next_part:
    ldy SndEnvFrqOff,x
.Lfrq1f:
    lda (SndEnvPtr),y
    sta SndEnvFrqCnt,x
    iny
    lda (SndEnvPtr),y
    eor #0xff
    inc a
    sta SndEnvFrqInc,x
    iny
    tya
    sta SndEnvFrqOff,x
    rts

.Lfrq_apply:
    ldy SndOffsets,x
    clc
    lda SndEnvFrqInc,x
    beq .Lfrq99
    bmi .Lfrq_dec

    /* Increase frequency (add to reload) */
    adc SndChannel+4,y
    bmi .Lfrq_inc_wrapped
        pha
        lda SndChannel+5,y
        cmp #6
        beq .Lfrq_inc_max
            inc a
            sta SndChannel+5,y
            pla
            eor #128
            bra .Lfrq_store
.Lfrq_inc_max:
        lda SndChannel+4,y
        bpl .Lfrq_inc_return
            pla
            bra .Lfrq_store
.Lfrq_inc_return:
        pla
        bra .Lfrq_store
.Lfrq_inc_wrapped:
    pha
    lda SndChannel+5,y
    cmp #6
    beq .Lfrq_inc_limit
        pla
        bra .Lfrq_store
.Lfrq_inc_limit:
    lda SndChannel+4,y
    bmi .Lfrq_inc_ret2
        pla
        rts
.Lfrq_inc_ret2:
    pla
    bra .Lfrq_store

.Lfrq_dec:
    /* Decrease frequency */
    adc SndChannel+4,y
    bmi .Lfrq_dec_ok
        pha
        lda SndChannel+5,y
        beq .Lfrq_dec_min
            dec a
            sta SndChannel+5,y
            pla
            eor #128
            bra .Lfrq_store
.Lfrq_dec_min:
        pla
        pha
        clc
        adc SndEnvFrqInc,x
        pla
        bra .Lfrq_store
.Lfrq_dec_ok:
    pha
    lda SndChannel+5,y
    beq .Lfrq_dec_store_min
        pla
        bra .Lfrq_store
.Lfrq_dec_store_min:
    pla
    sta SndChannel+4,y
    rts

.Lfrq_store:
    sta SndChannel+4,y
    ora #1
    rts


/* Wave envelope processing */
SndChangeWave:
    tay
    bpl .Lwav_continue
.Lwav99:
    rts
.Lwav_continue:
    lda SndNotePlaying,x
    beq .Lwav99

    lda SndEnvWavePtrLo,y
    sta SndEnvPtr
    lda SndEnvWavePtrHi,y
    sta SndEnvPtr+1

    dec SndEnvWaveCnt,x
    bpl .Lwav_done

    dec SndEnvWaveParts,x
    bpl .Lwav_next_part

    lda SndEnvWaveLoop,x
    beq .Lwav_end_env
        tay
        lda SndEnvWaveParts2,x
        sta SndEnvWaveParts,x
        bra .Lwav1v
.Lwav_end_env:
        tya
        ora #0x80
        sta SndEnvWave,x
        rts

.Lwav_next_part:
    ldy SndEnvWaveOff,x
.Lwav1v:
    lda (SndEnvPtr),y
    sta SndEnvWaveCnt,x
    phx
    lda SndOffsets,x
    tax

    iny
    lda (SndEnvPtr),y
    sta SndChannel+3,x      /* Shift LO */
    iny
    lda (SndEnvPtr),y
    sta SndChannel+7,x      /* Shift HI */
    iny
    lda (SndEnvPtr),y
    sta SndChannel+1,x      /* Feedback */
    iny
    tya
    ply
    sta SndEnvWaveOff,y
    lda #0xFF
    sta SndChannel+2,x
.Lwav_done:
    rts


/* Apply pending values to hardware */
SndSetValues:
    ldx #3
.Lset0:
    ldy SndOffsets,x
    lda SndChannel+2,y
    beq .Lset_skip              /* flag == 0 => don't set */

    bit #0x80
    beq .Lset_vol_only

    /* Full update: stop timer, set shifter/feedback */
    lda #0
    sta 0xfd25,y                /* stop sound timer */
    lda SndChannel+3,y
    sta 0xfd23,y                /* shifter 1 */
    lda SndChannel+7,y
    sta 0xfd27,y                /* shifter 2 */
    lda SndChannel+1,y
    sta 0xfd21,y                /* feedback */

.Lset_vol_only:
    lda SndChannel,y
    sta 0xfd20,y                /* volume */
    lda SndChannel+2,y
    bit #0x80
    beq .Lset_clear_flag

    lda SndChannel+4,y
    sta 0xfd24,y                /* reload */
    lda SndChannel+5,y
    ora #0b00011000
    sta 0xfd25,y                /* re-enable timer */

.Lset_clear_flag:
    lda #0
    sta SndChannel+2,y          /* clear flag */

.Lset_skip:
    dex
    bpl .Lset0
    rts


/*
 * void chipper_Play(u8 channel, const u8* musicData)
 * Start playing on channel (A=channel, X:Y=ptr low:high)
 */
.global chipper_Play
chipper_Play:
    tax                         /* X = channel */
    lda __rc2                   /* ptr low from ABI */
    ldy __rc3                   /* ptr high from ABI */

    php
    pha
    lda SndActive,x
    beq .Lstart1
        dec SndReqStop,x
        lda #1
        sta SndDelay,x
.Lstart0:
        lda SndActive,x
        bne .Lstart0
.Lstart1:
    sei
    pla
    sta SndPtrLo,x
    tya
    sta SndPtrHi,x
    lda #1
    sta SndDelay,x
    stz SndEnvVol,x
    stz SndEnvFrq,x
    stz SndEnvWave,x
    sta SndActive,x
    stz SndReqStop,x
    plp
    rts


/*
 * void chipper_Stop(u8 channel)
 */
.global chipper_Stop
chipper_Stop:
    tax
    lda SndActive,x
    beq .Lstopc_done
        dec SndReqStop,x
        lda #1
        sta SndDelay,x
.Lstopc1:
        lda SndActive,x
        bne .Lstopc1
.Lstopc_done:
    rts


/*
 * void chipper_StopAll(void)
 */
.global chipper_StopAll
chipper_StopAll:
    ldx #3
.Lstop0:
    lda SndActive,x
    beq .Lstop_next
        dec SndReqStop,x
        lda #1
        sta SndDelay,x
.Lstop1:
        lda SndActive,x
        bne .Lstop1
.Lstop_next:
    dex
    bpl .Lstop0
    rts


/*
 * u8 chipper_GetActiveMask(void)
 * Returns bitmask of active channels
 */
.global chipper_GetActiveMask
chipper_GetActiveMask:
    ldx #3
    lda #0
.Lact0:
    ldy SndActive,x
    beq .Lact_skip
        ora SndMask,x
.Lact_skip:
    dex
    bpl .Lact0
    rts


/*
 * bool chipper_IsPlaying(u8 channel)
 */
.global chipper_IsPlaying
chipper_IsPlaying:
    tax
    lda SndActive,x
    rts


/*
 * void chipper_Pause(void)
 */
.global chipper_Pause
chipper_Pause:
    php
    sei
    lda STIMCTLA
    sta SndPausedCtrl
    stz STIMCTLA
    lda MSTEREO
    sta SndPausedStereo
    lda #0xff
    sta MSTEREO
    lda #0x18
    trb AUD0CTLA
    trb AUD1CTLA
    trb AUD2CTLA
    trb AUD3CTLA
    plp
    rts


/*
 * void chipper_Continue(void)
 */
.global chipper_Continue
chipper_Continue:
    php
    sei
    lda SndPausedCtrl
    sta STIMCTLA
    lda SndPausedStereo
    sta MSTEREO
    lda #0x18
    tsb AUD0CTLA
    tsb AUD1CTLA
    tsb AUD2CTLA
    tsb AUD3CTLA
    plp
    rts
