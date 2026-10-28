#include "chipper_sfx.h"


// Use Furnace or CHipper to author music and sound effects.

const u8 sfx_beep[] = {
    CMD_INSTR, 0x16, 0x00, 0x0D, 100, 127,
    60, 20,
    0x00
};

const u8 sfx_pickup[] = {
    CMD_INSTR, 0x16, 0x00, 0x0D, 100, 127,
    CMD_PLAY2, 150, 0x1A, 3,
    CMD_PLAY2, 100, 0x1A, 3,
    CMD_PLAY2, 75, 0x1A, 6,
    0x00
};

const u8 sfx_jump[] = {
    CMD_INSTR, 0x16, 0x00, 0x0D, 80, 127,
    CMD_PLAY2, 200, 0x1A, 2,
    CMD_PLAY2, 180, 0x1A, 2,
    CMD_PLAY2, 160, 0x1A, 2,
    CMD_PLAY2, 140, 0x1A, 2,
    CMD_PLAY2, 120, 0x1A, 2,
    CMD_PLAY2, 100, 0x1A, 2,
    CMD_PLAY2, 80, 0x1A, 2,
    0x00
};

const u8 sfx_melody[] = {
    CMD_INSTR, 0x16, 0x00, 0x0D, 100, 127,
    60, 8,
    62, 8,
    64, 8,
    65, 8,
    67, 8,
    69, 8,
    71, 8,
    72, 12,
    60, 8,
    62, 8,
    64, 8,
    65, 8,
    67, 8,
    69, 8,
    71, 8,
    72, 12,
    0x00
};

const u8 sfx_explosion[] = {
    CMD_INSTR, 0xFF, 0xFF, 0xFF, 127, 127,
    CMD_PLAY2, 50, 0x18, 2,
    CMD_PLAY2, 80, 0x18, 2,
    CMD_PLAY2, 120, 0x19, 3,
    CMD_PLAY2, 160, 0x1A, 4,
    CMD_PLAY2, 200, 0x1B, 6,
    CMD_PLAY2, 230, 0x1C, 8,
    0x00
};
