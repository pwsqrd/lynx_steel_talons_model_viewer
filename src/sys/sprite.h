#ifndef SPRITE_H
#define SPRITE_H

#include "types.h"
#include "suzy.h"

// Each draw call fills the one shared SCB and kicks Suzy, blocking until the
// sprite is drawn into the back buffer. Only one SCB is ever in flight, so the
// layouts share storage. The sys::sprite draws use .rehv, which reloads the
// penpal from g_spriteSCB.rehv.penpal; sys::StartFrame installs the identity
// penpal, and the per-sprite setters below override it.
//
// The penpal sits at a different offset in each layout: .rehvs.stretch and
// .rehvst.stretch/tilt overlap .rehv.penpal[0..3]. Code that kicks one of the
// other layouts must fill that layout's penpal itself, then call
// SetPenpalSprite or SetPenpal before the next REHV draw.
union SpriteSCB
{
    SCB_RENONE_PAL renone;
    SCB_REHV_PAL   rehv;
    SCB_REHVS_PAL  rehvs;
    SCB_REHVST_PAL rehvst;
};
extern SpriteSCB g_spriteSCB;

namespace sys::sprite {

// Sprite asset as loaded by sys::cart::Read: a 4-byte header written by
// tools/process_assets.py followed by the sprpck data. ctl0 holds the BPP and
// sprite type, ctl1 PACKED or LITERAL.
struct Sprite
{
    u8      width;
    u8      height;
    Sprctl0 ctl0;
    Sprctl1 ctl1;
    u8      data[];
};
static_assert(sizeof(Sprite) == 4, "Sprite header must match process_assets.py");

void InitPenpal(void);
void SetPenpalSprite(void);

// Set the 4-byte SCB penpal that 3BPP sprites consume (pens 0..7). Call
// before each 3BPP sprite kick if a non-identity mapping is required.
void SetPenpal(const u8* penpal);

__attribute__((always_inline)) inline void SetPenpal2BPP(const u8* penpal)
{
    g_spriteSCB.rehv.penpal[0] = penpal[0];
    g_spriteSCB.rehv.penpal[1] = penpal[1];
}

__attribute__((always_inline)) inline void SetPenpal4BPP(const u8* penpal)
{
    for (u8 i = 0; i < 8; i++) {
        g_spriteSCB.rehv.penpal[i] = penpal[i];
    }
}

void KickAndWait(u8* scb);

// Fill g_spriteSCB with an arbitrary sprctl0/sprctl1 and draw raw sprite data.
void DrawCore(s16 x, s16 y, u8* spriteData, Sprctl0 ctl0, Sprctl1 ctl1, u16 hsize, u16 vsize);

void Draw(s16 x, s16 y, const Sprite* sprite, u16 hsize = 0x100, u16 vsize = 0x100);
void DrawFlipped(s16 x, s16 y, const Sprite* sprite, Sprctl0 flip, u16 hsize = 0x100, u16 vsize = 0x100);

}

#endif
