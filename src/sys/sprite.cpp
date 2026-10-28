#include "sprite.h"
#include "mikey.h"

SpriteSCB g_spriteSCB;

namespace sys::sprite {

/*
=================
SetPenpal
=================
*/
void SetPenpal(const u8* penpal)
{
    g_spriteSCB.rehv.penpal[0] = penpal[0];
    g_spriteSCB.rehv.penpal[1] = penpal[1];
    g_spriteSCB.rehv.penpal[2] = penpal[2];
    g_spriteSCB.rehv.penpal[3] = penpal[3];
}

/*
=================
KickAndWait
=================
*/
void KickAndWait(u8* scb)
{
    SUZY.vidadr    = SUZY.sprbase;
    SUZY.scbnext   = scb;
    MIKEY.sdoneack = 0;
    SUZY.suzybusen = 1;
    SUZY.sprgo     = Sprgo::GO;
    MIKEY.cpusleep = 0;

    while (SUZY.Sprsys() & SprsysR::SPRITEWORKING) {
        MIKEY.cpusleep = 0;
        __asm__("nop");
    }

    MIKEY.sdoneack = 0;
}

/*
=================
InitPenpal
=================
*/
void InitPenpal(void)
{
    g_spriteSCB.rehv.penpal[0] = 0x01;
    g_spriteSCB.rehv.penpal[1] = 0x23;
    g_spriteSCB.rehv.penpal[2] = 0x45;
    g_spriteSCB.rehv.penpal[3] = 0x67;
    g_spriteSCB.rehv.penpal[4] = 0x89;
    g_spriteSCB.rehv.penpal[5] = 0xAB;
    g_spriteSCB.rehv.penpal[6] = 0xCD;
    g_spriteSCB.rehv.penpal[7] = 0xEF;
}

/*
=================
SetPenpalSprite
=================
*/
void SetPenpalSprite(void)
{
    g_spriteSCB.rehv.penpal[0] = 0x01;
    g_spriteSCB.rehv.penpal[1] = 0x23;
    g_spriteSCB.rehv.penpal[2] = 0x45;
    g_spriteSCB.rehv.penpal[3] = 0x67;
}

/*
=================
DrawCore

Shared body for all Draw* variants. Caller supplies the
variant-specific sprctl0/sprctl1 (BPP, TYPE, PACKED/LITERAL) plus scale.
=================
*/
NOINLINE void DrawCore(s16 x, s16 y, u8* spriteData, Sprctl0 ctl0, Sprctl1 ctl1, u16 hsize, u16 vsize)
{
    g_spriteSCB.rehv.sprctl0 = ctl0;
    g_spriteSCB.rehv.sprctl1 = ctl1;
    g_spriteSCB.rehv.sprcoll = 0;
    g_spriteSCB.rehv.next    = 0;
    g_spriteSCB.rehv.data    = spriteData;
    g_spriteSCB.rehv.hpos    = x;
    g_spriteSCB.rehv.vpos    = y;
    g_spriteSCB.rehv.hsize   = hsize;
    g_spriteSCB.rehv.vsize   = vsize;
    __asm__ volatile("" ::: "memory");
    KickAndWait(reinterpret_cast<u8*>(&g_spriteSCB));
}

/*
=================
Draw

Sprite at x, y using the format from its header; hsize/vsize are 8.8
fixed-point scale (0x100 = 1:1).
=================
*/
void Draw(s16 x, s16 y, const Sprite* sprite, u16 hsize, u16 vsize)
{
    DrawCore(x, y, const_cast<u8*>(sprite->data), sprite->ctl0, sprite->ctl1 | Sprctl1::REHV, hsize, vsize);
}

/*
=================
DrawFlipped

Draw with Sprctl0::HFLIP and/or Sprctl0::VFLIP.
=================
*/
void DrawFlipped(s16 x, s16 y, const Sprite* sprite, Sprctl0 flip, u16 hsize, u16 vsize)
{
    DrawCore(x, y, const_cast<u8*>(sprite->data), sprite->ctl0 | flip, sprite->ctl1 | Sprctl1::REHV, hsize, vsize);
}

}
