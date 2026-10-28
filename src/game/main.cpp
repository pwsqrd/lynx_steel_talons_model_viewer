#include "sys/sys.h"
#include "sys/suzy_math.h"
#include "models_data.h"

// Steel Talons model viewer. LEFT/RIGHT pick the model, UP/DOWN tilt the
// camera, A toggles the spin.
//
// Every frame the model is rotated and projected, then its faces are drawn
// in the model's order, skipping those pointing away from the camera. Like
// the game (DRAWPOLYGONS@ in DRAWOBJ.ASM), there is no sort: the models are
// built so that order is right. The models are triangles (tools/gen_models.py). A triangle is one sprite, or two when
// it has to be split at its middle vertex: a literal pixel stretched to the
// span width, with TILT following the left edge and STRETCH the width per
// row. Each sprite is drawn as soon as it is set up, through one SCB.
//
// All multiplies run on the Suzy math unit in signed mode through the inline
// sys::math starts, which return at once so the CPU sets up the next
// operands while one runs. The functions that do this are NOINLINE and use
// no C '*': __mulhi3 (sys.cpp) also uses the math unit and the compiler
// would be free to call it mid-sequence.

namespace math = sys::math;

constexpr u8  SCREEN_CX     = SYS_SCREEN_WIDTH / 2;
constexpr u8  SCREEN_CY     = SYS_SCREEN_HEIGHT / 2;
constexpr u8  PITCH_MIN     = 0;
constexpr u8  PITCH_MAX     = 64;
constexpr u8  PITCH_DEFAULT = 22;
constexpr u8  NAME_Y        = 3;

static SCB_REHVST_PAL g_scb;

// One literal 4bpp pixel per pen, as in sys::ClearScreenRegion.
static u8 g_pixel[16][3];

// Screen points. gen_models.py sizes the projection so the model always fits.
static ZP u8 g_sx[MODEL_MAX_POINTS];
static ZP u8 g_sy[MODEL_MAX_POINTS];

// View matrix rows (256 = 1.0): screen right, screen up, depth away from the
// camera. The right row has no z term.
static ZP s16 g_mx[2];
static ZP s16 g_mu[3];
static ZP s16 g_md[3];

static u8   g_model;
static u8   g_yaw;
static u8   g_pitch;
static bool g_spin;
static u8   g_prevJoy;

/*
=================
Cos
=================
*/
static s8 Cos(u8 angle)
{
    return g_sin[(u8)(angle + 64)];
}

/*
=================
BuildMatrix

Yaw turns the model about its Z (up) axis, then the camera tilts down by
pitch. Matches the handedness of talons-build/tools/viewer.html.

The sines are 64 = 1.0, so shifted left 2 they are matrix entries, and the
product of two of them >> 8 is one too. The four products run on Suzy
while the plain entries are stored.
=================
*/
static NOINLINE void BuildMatrix()
{
    math::SetMode(math::SIGNED);
    s16 sy = g_sin[g_yaw] << 2;
    s16 sp = g_sin[g_pitch] << 2;
    math::SMulResult upX = math::StartSignedMultiply(-sy, sp);
    s16 cy = Cos(g_yaw) << 2;
    s16 cp = Cos(g_pitch) << 2;
    g_mu[0] = upX.GetS16_RShift8();

    math::SMulResult upY = math::StartSignedMultiply(-cy, sp);
    g_mx[0] = cy;
    g_mx[1] = -sy;
    g_mu[1] = upY.GetS16_RShift8();

    math::SMulResult depthX = math::StartSignedMultiply(-sy, cp);
    g_mu[2] = cp;
    g_md[0] = depthX.GetS16_RShift8();

    math::SMulResult depthY = math::StartSignedMultiply(-cy, cp);
    g_md[2] = -sp;
    g_md[1] = depthY.GetS16_RShift8();
}

/*
=================
TransformPoints

Each row is a signed multiply-accumulate; with the matrix at 256 = 1.0 the
result is MATH_L. The projection multiplies by g_recip and keeps MATH_G.
=================
*/
static NOINLINE void TransformPoints(const Model& model)
{
    math::SetMode(math::SIGNED | math::ACCUMULATE);
    math::AccumContext acc;
    const s8* p = model.points;
    for (u8 i = 0; i < model.numPoints; i++, p += 3) {
        s16 x = p[0];
        s16 y = p[1];
        s16 z = p[2];

        acc.Clear();
        acc.Start(g_mx[0], x);
        acc.Start(g_mx[1], y);
        s8 vx = acc.GetS8_RShift8();

        acc.Clear();
        acc.Start(g_mu[0], x);
        acc.Start(g_mu[1], y);
        acc.Start(g_mu[2], z);
        s8 vu = acc.GetS8_RShift8();

        acc.Clear();
        acc.Start(g_md[0], x);
        acc.Start(g_md[1], y);
        acc.Start(g_md[2], z);
        s8 vd = acc.GetS8_RShift8();

        math::SMulResult projX = math::StartSignedMultiply(vx, g_recip[(u8)(vd + 128)]);
        g_sx[i] = (u8)(SCREEN_CX + projX.GetS8_RShift8());

        math::SMulResult projY = math::RestartSignedMultiply(vu);
        g_sy[i] = (u8)(SCREEN_CY - projY.GetS8_RShift8());
    }
}

/*
=================
FacesCamera

gen_models.py winds each triangle so (b - a) x (c - a) points along its
normal; through the view matrix and the y-down screen that makes a front
face's (b - a) x (c - a) positive. Two multiplies.
=================
*/
static NOINLINE bool FacesCamera(u8 a, u8 b, u8 c)
{
    math::AccumContext acc;
    u8 xa = g_sx[a];
    u8 ya = g_sy[a];
    acc.Clear();
    acc.Start((s16)g_sx[b] - xa, (s16)g_sy[c] - ya);
    s16 ex = (s16)g_sx[c] - xa;
    s16 ey = (s16)ya - g_sy[b];
    acc.Start(ey, ex);
    return acc.GetS16() > 0;
}

/*
=================
DrawSprite

Draws one sprite now: a single pixel h rows tall from (x, y), hsize (8.8)
growing by stretch and x moving by tilt each row.
=================
*/
static void DrawSprite(u8 pen, u8 x, u8 y, u8 h, u16 hsize, s16 stretch, s16 tilt)
{
    SCB_REHVST_PAL& scb = g_scb;
    scb.data    = g_pixel[pen];
    scb.hpos    = x;
    scb.vpos    = y;
    scb.hsize   = hsize;
    scb.vsize   = (u16)h << 8;
    scb.stretch = (u16)stretch;
    scb.tilt    = (u16)tilt;
    // Suzy reads the SCB from RAM: keep the stores before the kick and the
    // next sprite's stores after the wait.
    __asm__ volatile("" ::: "memory");
    sys::sprite::KickAndWait(reinterpret_cast<u8*>(&g_scb));
    __asm__ volatile("" ::: "memory");
}

/*
=================
DrawTriangle

Corners sorted by y: a top, b middle, c bottom. A flat top or flat bottom is
one sprite; otherwise the triangle is split at b's row into two, the bottom
one starting where the long edge a-c crosses that row.

A sprite starts at the left edge (HPOS, TILT follows its slope) and its
width grows by the right slope minus the left one (STRETCH). A slope dx / dy
in 8.8 is (dx * 4) x (16384 / dy) >> 8; dy = 0 gives 0, and with the points
within 49 pixels of the centre it fits 16 bits. The slopes and the long
edge's crossing are multiplies overlapped with the setup; none is running
when a sprite is drawn.
=================
*/
static NOINLINE void DrawTriangle(u8 pen, u8 a, u8 b, u8 c)
{
    if (g_sy[b] < g_sy[a]) { u8 t = a; a = b; b = t; }
    if (g_sy[c] < g_sy[b]) { u8 t = b; b = c; c = t; }
    if (g_sy[b] < g_sy[a]) { u8 t = a; a = b; b = t; }

    u8 xa = g_sx[a], ya = g_sy[a];
    u8 xb = g_sx[b], yb = g_sy[b];
    u8 xc = g_sx[c], yc = g_sy[c];

    if (ya == yc) {
        return;
    }
    s16 acDx    = (xc - xa) << 2;
    s16 acRecip = g_slopeRecip[yc - ya];
    math::SMulResult acSlope = math::StartSignedMultiply(acDx, acRecip);
    u8 h = yc - ya + 1;

    if (ya == yb) {
        // Flat top: a-c and b-c down to c.
        s16 bcDx    = (xc - xb) << 2;
        s16 bcRecip = g_slopeRecip[yc - yb];
        s16 sac     = acSlope.GetS16_RShift8();

        math::SMulResult bcSlope = math::StartSignedMultiply(bcDx, bcRecip);
        bool aLeft = xa < xb;
        u8   x     = aLeft ? xa : xb;
        u16  w     = (u16)(aLeft ? xb - xa + 1 : xa - xb + 1) << 8;
        s16  sbc   = bcSlope.GetS16_RShift8();
        if (aLeft) {
            DrawSprite(pen, x, ya, h, w, sbc - sac, sac);
        } else {
            DrawSprite(pen, x, ya, h, w, sac - sbc, sbc);
        }
        return;
    }

    s16 abDx    = (xb - xa) << 2;
    s16 abRecip = g_slopeRecip[yb - ya];
    s16 sac     = acSlope.GetS16_RShift8();

    math::SMulResult abSlope = math::StartSignedMultiply(abDx, abRecip);
    u8  hTop = yb - ya;
    s16 sab  = abSlope.GetS16_RShift8();
    bool bLeft = sab < sac;
    s16  sl    = bLeft ? sab : sac;
    s16  sr    = bLeft ? sac : sab;

    if (yb == yc) {
        // Flat bottom: one sprite from a down to b and c.
        DrawSprite(pen, xa, ya, h, 0x100, sr - sl, sl);
        return;
    }

    // Bottom: b's row down to c, from b to where a-c crosses it (rounded).
    math::SMulResult crossing = math::StartSignedMultiply(sac, hTop);
    u8  hBottom = yc - yb + 1;
    s16 bcDx    = (xc - xb) << 2;
    s16 bcRecip = g_slopeRecip[yc - yb];
    u8  xm      = (u8)(xa + crossing.GetU8_RShift8_Round());

    math::SMulResult bcSlope = math::StartSignedMultiply(bcDx, bcRecip);
    u8  x = bLeft ? xb : xm;
    s16 w = bLeft ? xm - xb + 1 : xb - xm + 1;
    if (w < 1) {
        w = 1;
    }
    s16 sbc = bcSlope.GetS16_RShift8();

    // Top: a down to the row above b.
    DrawSprite(pen, xa, ya, hTop, 0x100, sr - sl, sl);
    if (bLeft) {
        DrawSprite(pen, x, yb, hBottom, (u16)w << 8, sac - sbc, sbc);
    } else {
        DrawSprite(pen, x, yb, hBottom, (u16)w << 8, sbc - sac, sac);
    }
}

/*
=================
DrawLine

A one-pixel line: a column one pixel wide tilted along it.
=================
*/
static NOINLINE void DrawLine(u8 pen, u8 a, u8 b)
{
    if (g_sy[b] < g_sy[a]) { u8 t = a; a = b; b = t; }
    u8 xa = g_sx[a], ya = g_sy[a];
    u8 xb = g_sx[b], yb = g_sy[b];
    if (ya == yb) {
        u8 x = xa < xb ? xa : xb;
        u8 w = xa < xb ? xb - xa : xa - xb;
        DrawSprite(pen, x, ya, 1, (u16)(w + 1) << 8, 0, 0);
        return;
    }
    s16 slope = math::StartSignedMultiply((xb - xa) << 2, g_slopeRecip[yb - ya]).GetS16_RShift8();
    DrawSprite(pen, xa, ya, yb - ya + 1, 0x100, 0, slope);
}

/*
=================
DrawModel
=================
*/
static void DrawModel(const Model& model)
{
    BuildMatrix();
    TransformPoints(model);

    sys::ClearScreen(BACKGROUND_PEN);
    math::SetMode(math::SIGNED | math::ACCUMULATE);
    const u8* face = model.faces;
    for (u8 i = 0; i < model.numFaces; i++) {
        u8 count = face[1];
        if (count == 2) {
            DrawLine(face[0], face[2], face[3]);
        } else if (model.twoSided || FacesCamera(face[2], face[3], face[4])) {
            DrawTriangle(face[0], face[2], face[3], face[4]);
        }
        face += 2 + count;
    }
}

/*
=================
DrawText

The game's own font, 1:1, pen 1 mapped to the given palette index.
=================
*/
static void DrawText(s16 x, s16 y, const char* text, u8 pen)
{
    g_spriteSCB.rehv.penpal[0] = pen;
    g_spriteSCB.rehv.penpal[1] = 0x00;
    for (; *text; text++, x += FONT_ADVANCE) {
        char c = *text;
        u8   glyph;
        if (c >= '0' && c <= '9') {
            glyph = (u8)(c - '0');
        } else if (c >= 'A' && c <= 'Z') {
            glyph = (u8)(c - 'A' + 10);
        } else {
            continue;
        }
        sys::sprite::DrawCore(x, y, const_cast<u8*>(g_font + g_fontOffset[glyph]),
                              Sprctl0::BPP_2 | Sprctl0::TYPE_NORMAL, Sprctl1::PACKED | Sprctl1::REHV, 0x100, 0x100);
    }
    sys::sprite::InitPenpal();
}

/*
=================
DrawName
=================
*/
static void DrawName(const char* name)
{
    u8 len = (u8)strlen(name);
    s16 x  = (SYS_SCREEN_WIDTH - len * FONT_ADVANCE) / 2;
    DrawText(x + 1, NAME_Y + 1, name, TEXT_SHADOW_PEN);
    DrawText(x, NAME_Y, name, TEXT_PEN);
}

/*
=================
HandleInput
=================
*/
static void HandleInput()
{
    u8 joy     = (u8)SUZY.joystick;
    u8 pressed = joy & ~g_prevJoy;
    g_prevJoy  = joy;

    u8 left  = (u8)Joystick::LEFT;
    u8 right = (u8)Joystick::RIGHT;
    u8 up    = (u8)Joystick::UP;
    u8 down  = (u8)Joystick::DOWN;
    if (g_screenFlip) {
        u8 t = left;
        left  = right;
        right = t;
        t     = up;
        up    = down;
        down  = t;
    }

    if (pressed & left) {
        g_model = g_model ? g_model - 1 : MODEL_COUNT - 1;
    }
    if (pressed & right) {
        g_model = g_model == MODEL_COUNT - 1 ? 0 : g_model + 1;
    }
    if ((joy & up) && g_pitch < PITCH_MAX) {
        g_pitch++;
    }
    if ((joy & down) && g_pitch > PITCH_MIN) {
        g_pitch--;
    }
    if (pressed & (u8)Joystick::INNER) {
        g_spin = !g_spin;
    }
}

/*
=================
InitScbs
=================
*/
static void InitScbs()
{
    for (u8 pen = 0; pen < 16; pen++) {
        g_pixel[pen][0] = 0x02;
        g_pixel[pen][1] = (u8)((pen << 4) | pen);
        g_pixel[pen][2] = 0x00;
    }

    g_scb.sprctl0 = Sprctl0::BPP_4 | Sprctl0::TYPE_BACKNONCOLL;
    g_scb.sprctl1 = Sprctl1::LITERAL | Sprctl1::REHVST;
    g_scb.sprcoll = 0;
    g_scb.next    = 0;
    static const u8 IDENTITY[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    for (u8 i = 0; i < 8; i++) {
        g_scb.penpal[i] = IDENTITY[i];
    }
}

/*
=================
main
=================
*/
int main(void)
{
    sys::Init();
    sys::SetPalette(g_palette);
    InitScbs();

    g_model   = 0;
    g_yaw     = 0;
    g_pitch   = PITCH_DEFAULT;
    g_spin    = true;
    g_prevJoy = (u8)SUZY.joystick;

    u16 last = sys::GetTicks();
    while (1) {
        sys::StartFrame();
        HandleInput();

        u16 now = sys::GetTicks();
        if (g_spin) {
            g_yaw += (u8)(now - last);
        }
        last = now;

        const Model& model = g_models[g_model];
        DrawModel(model);
        DrawName(model.name);

        sys::EndFrame();
    }
}
