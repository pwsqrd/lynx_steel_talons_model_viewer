#include "sys.h"
#include "zx0.h"
#include "chipper/chipper.h"
#include "sprite.h"
#include "cart_asm.h"

// Referenced by name only from the naked-asm irqBody, so they need extern "C"
// linkage and `used` to survive LTO.
extern "C" __attribute__((used)) volatile u8* FRONTBUFFER_PTR  = reinterpret_cast<volatile u8*>(SYS_SCREEN_BUFFER_1);
extern "C" __attribute__((used)) volatile u8* BACKBUFFER_PTR   = reinterpret_cast<volatile u8*>(SYS_SCREEN_BUFFER_2);
extern "C" __attribute__((used)) volatile u8  g_waitingForSwap = 0;
extern "C" __attribute__((used)) u8           g_screenFlip     = 0;
static u8                                      g_flipComboPrev  = 0;

__attribute__((used)) volatile u16 g_sysTicks  = 0;

// Zero page - exported for cart.S / zx0.S
ZP u8    g_cartCurrentBlock;
ZP u16   g_cartBlockPosition;

extern "C" void     irqBody(void);

/*
=================
sys::Init
=================
*/
void sys::Init(void)
{
    __asm__("sei");

    g_waitingForSwap = 0;
    g_screenFlip     = 0;
    g_flipComboPrev  = 0;
    g_sysTicks       = 0;

    for (u8 i = 0; i < 128; i++) {
        (reinterpret_cast<volatile u8*>(&MIKEY))[i] = 0;
    }

    MIKEY.sysctl1        = Sysctl1::POWERON;
    MIKEY.iodir          = ASM_CART_IODIR_INIT;
    MIKEY.iodat          = static_cast<u8>(Iodat::RESTLESS) | static_cast<u8>(Iodat::CART_ADDR_DATA);
    MIKEY.dispctl        = Dispctl::DISABLE;
    MIKEY.mapctl         = static_cast<u8>(Mapctl::VECTOR_SPACE);

    MIKEY.vectors.irq    = reinterpret_cast<u8*>(irqHandler);

    MIKEY.timer0.reload  = HBL_RELOAD;
    MIKEY.timer0.control = HBL_CONTROL;
    MIKEY.timer2.reload  = VBL_RELOAD;
    MIKEY.timer2.control = VBL_CONTROL | TimerCtrlA::ENABLE_INT;

    sys::cart::Init();

    MIKEY.pkbkup         = PKBKUP_VALUE;
    MIKEY.dispctl        = Dispctl::DMA_ON | Dispctl::FOURBIT | Dispctl::COLOR;

    SUZY.tmpadr    = 0;
    SUZY.tiltacc   = 0;
    SUZY.hoff      = 0;
    SUZY.voff      = 0;
    SUZY.hsizeoff  = 0x7F;
    SUZY.vsizeoff  = 0x7F;
    SUZY.colbase   = 0;
    SUZY.coladr    = 0;
    SUZY.colloff   = 0;
    SUZY.scbnext   = 0;

    SUZY.mathd     = 0;
    SUZY.mathc     = 0;
    SUZY.mathb     = 0;
    SUZY.matha     = 0;
    SUZY.mathp     = 0;
    SUZY.mathn     = 0;
    SUZY.mathh     = 0;
    SUZY.mathg     = 0;
    SUZY.mathf     = 0;
    SUZY.mathe     = 0;
    SUZY.mathm     = 0;
    SUZY.mathl     = 0;
    SUZY.mathk     = 0;
    SUZY.mathj     = 0;

    SUZY.sprcoll   = 0;
    SUZY.SprsysSetMathMode(0);
    SUZY.sprinit   = SPRINIT_DEFAULT;
    SUZY.suzybusen = 1;

    MIKEY.scrbase  = const_cast<u8*>(FRONTBUFFER_PTR);
    SUZY.sprbase   = const_cast<u8*>(BACKBUFFER_PTR);
    SUZY.vidadr    = const_cast<u8*>(BACKBUFFER_PTR);

    __asm__("cli");

    sys::chipper::Init();
}

/*
=================
Interrupt handler

irqHandler is a naked wrapper that saves A/X/Y only and JSRs the naked
irqBody. LLVM-MOS adds a 256-byte soft-stack adjustment to every
`interrupt` function, which would overrun the 256-byte soft stack.

INVARIANT: irqBody and chipper_Tick touch only A/X/Y, the hardware stack and
their own globals/zero page; never an imaginary register ($00-$1F) or the
soft stack. Anything added to the IRQ path must keep that true or save what
it clobbers in irqHandler.
=================
*/
static_assert(static_cast<u8>(Interrupt::TIMER2) == 0x04, "irqBody hardcodes #$04 for TIMER2");
static_assert(SYS_SCREEN_SIZE_BYTES - 1 == 0x1FDF, "irqBody hardcodes flip bias $1FDF");

extern "C" __attribute__((naked, used)) void irqBody(void)
{
    __asm__ volatile(
        "lda $FD81\n"                       // A = INTSET
        "sta $FD80\n"                       // INTRST = INTSET (ack what we read)
        "and #$04\n"                        // Timer 2 (VBL)?
        "beq 2f\n"
        "inc g_sysTicks\n"
        "bne 3f\n"
        "inc g_sysTicks+1\n"
        "3:\n"
        "lda g_waitingForSwap\n"
        "beq 5f\n"
        "lda FRONTBUFFER_PTR\n"   "pha\n"
        "lda FRONTBUFFER_PTR+1\n" "pha\n"
        "ldx BACKBUFFER_PTR\n"
        "ldy BACKBUFFER_PTR+1\n"
        "stx FRONTBUFFER_PTR\n"
        "sty FRONTBUFFER_PTR+1\n"
        "lda g_screenFlip\n"                // flipped: scan from the last byte
        "beq 4f\n"
        "clc\n"
        "txa\n" "adc #$DF\n" "tax\n"
        "tya\n" "adc #$1F\n" "tay\n"
        "4:\n"
        "stx $FD94\n" "sty $FD95\n"         // MIKEY.scrbase
        "pla\n" "tay\n"
        "pla\n" "tax\n"
        "stx BACKBUFFER_PTR\n"
        "sty BACKBUFFER_PTR+1\n"
        "stx $FC08\n" "sty $FC09\n"         // SUZY.sprbase
        "stx $FC0C\n" "sty $FC0D\n"         // SUZY.vidadr
        "stz g_waitingForSwap\n"
        "5:\n"
        "jsr chipper_Tick\n"                // ~4x VBL rate, close to 240 Hz
        "jsr chipper_Tick\n"
        "jsr chipper_Tick\n"
        "jsr chipper_Tick\n"
        "2:\n"
        "rts\n"
    );
}

extern "C" __attribute__((naked)) void irqHandler(void)
{
    __asm__ volatile(
        "cld\n"
        "pha\n"
        "phx\n"
        "phy\n"
        "jsr irqBody\n"
        "ply\n"
        "plx\n"
        "pla\n"
        "rti\n"
    );
}

/*
=================
sys::SetPalette

Palette data is MIKEY-native: 16 green bytes then 16 blue/red bytes.
=================
*/
NO_INLINE void sys::SetPalette(const u8* palette)
{
    memcpy(const_cast<u8*>(&MIKEY.palette[0]), palette, 32);
}

NO_INLINE void sys::SetPalette(const CartAsset& palette)
{
    sys::cart::Read(const_cast<u8*>(&MIKEY.palette[0]), palette.block, palette.pos, 32);
}

void sys::SetPaletteColor(u8 index, u8 r, u8 g, u8 b)
{
    MIKEY.palette[index]      = g;
    MIKEY.palette[16 + index] = static_cast<u8>((b << 4) | r);
}

/*
=================
sys::cart::Seek
=================
*/
NO_INLINE void sys::cart::Seek(u24 offset)
{
    sys::cart::SeekBlockPos(static_cast<u8>(offset >> CART_BLOCK_SHIFT), static_cast<u16>(offset & (CART_BLOCK_SIZE - 1)));
}

/*
=================
sys::cart::AssetSize

Uncompressed size of a ZX02 data asset (its 2-byte header).
=================
*/
u16 sys::cart::AssetSize(const CartAsset& asset)
{
    sys::cart::SeekBlockPos(asset.block, asset.pos);
    return sys::cart::ReadWord();
}

/*
=================
sys::StartFrame
=================
*/
NO_INLINE void sys::StartFrame()
{
    g_waitingForSwap = 0;
    sys::sprite::InitPenpal();
}

/*
=================
sys::PollScreenFlip

Edge-triggered OPTION2+PAUSE gesture. Returns true on the frame it toggled.
=================
*/
bool sys::PollScreenFlip()
{
    if ((SUZY.joystick & Joystick::OPTION2) && (SUZY.switches & Switches::PAUSE)) {
        if (!g_flipComboPrev) {
            g_flipComboPrev = 1;
            g_screenFlip ^= 1;
            MIKEY.dispctl = static_cast<u8>(0x0D | (g_screenFlip << 1));
            return true;
        }
    } else {
        g_flipComboPrev = 0;
    }
    return false;
}

/*
=================
sys::ArmBufferSwap / sys::WaitBufferSwap / sys::EndFrame
=================
*/
NO_INLINE void sys::ArmBufferSwap()
{
    sys::PollScreenFlip();
    g_waitingForSwap = 1;
}

NO_INLINE void sys::WaitBufferSwap()
{
    while (g_waitingForSwap == 1) {
        __asm__("nop");
    }
}

NO_INLINE void sys::EndFrame()
{
    sys::ArmBufferSwap();
    sys::WaitBufferSwap();
}

/*
=================
sys::ClearScreen
=================
*/
NO_INLINE void sys::ClearScreen(u8 colorIndex)
{
    sys::ClearScreenRegion(0, 0, SYS_SCREEN_WIDTH, SYS_SCREEN_HEIGHT, colorIndex);
}

/*
=================
sys::ClearScreenRegion

One LITERAL pixel scaled to width x height. The line offset must stay 0x02;
a larger value makes Suzy emit a multiple of the intended span.
=================
*/
NO_INLINE void sys::ClearScreenRegion(u8 x, u8 y, u8 width, u8 height, u8 colorIndex)
{
    static u8 singlepixel_data[3] = { 0x02, 0x00, 0x00 };

    singlepixel_data[1] = (colorIndex << 4) | colorIndex;

    sys::sprite::InitPenpal();

    g_spriteSCB.rehv.sprctl0 = Sprctl0::BPP_4 | Sprctl0::TYPE_BACKGROUND;
    g_spriteSCB.rehv.sprctl1 = Sprctl1::LITERAL | Sprctl1::REHV;
    g_spriteSCB.rehv.sprcoll = 0;
    g_spriteSCB.rehv.next    = 0;
    g_spriteSCB.rehv.data    = singlepixel_data;
    g_spriteSCB.rehv.hpos    = x;
    g_spriteSCB.rehv.vpos    = y;
    g_spriteSCB.rehv.hsize   = static_cast<u16>(width) << 8;
    g_spriteSCB.rehv.vsize   = static_cast<u16>(height) << 8;

    SUZY.sprbase = const_cast<u8*>(BACKBUFFER_PTR);
    sys::sprite::KickAndWait(reinterpret_cast<u8*>(&g_spriteSCB));
}

/*
=================
__mulhi3

Hardware 16-bit multiply on the Suzy math unit, replacing libcrt's
software version.
=================
*/
extern "C" __attribute__((used)) NO_INLINE u16 __mulhi3(u16 a, u16 b)
{
    SUZY.SprsysSetMathMode(0);
    SUZY.mathd = static_cast<u8>(b);
    SUZY.mathc = static_cast<u8>(b >> 8);
    SUZY.mathb = static_cast<u8>(a);
    SUZY.matha = static_cast<u8>(a >> 8);
    while (SUZY.Sprsys() & SprsysR::MATHWORKING);
    return (static_cast<u16>(SUZY.mathg) << 8) | SUZY.mathh;
}
