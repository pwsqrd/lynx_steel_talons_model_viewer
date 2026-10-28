#ifndef CHIPPER_SFX_H
#define CHIPPER_SFX_H

#include "../types.h"

/*
 * Sound effects using CHIPPER format
 *
 * Command bytes:
 *   Note (0x00-0x7F) followed by delay byte = play note
 *   0x82, delay = delay without note
 *   0x83 = note off / stop
 *   0x84, shiftlo, shifthi, feedback, volume, maxvol = set instrument
 *   0x85, reload, prescale, delay = PLAY2 (explicit pitch)
 *   0x00 = end of tune
 */

#define CMD_DELAY   0x82
#define CMD_NOTEOFF 0x83
#define CMD_INSTR   0x84
#define CMD_PLAY2   0x85

extern const u8 sfx_beep[];
extern const u8 sfx_pickup[];
extern const u8 sfx_jump[];
extern const u8 sfx_melody[];
extern const u8 sfx_explosion[];

#endif
