#ifndef SYS_H
#define SYS_H

#define NO_INLINE __attribute__((noinline, nonreentrant))
#define INLINE __attribute__((always_inline))

#include <string.h>
#include "types.h"
#include "cart_types.h"
#include "suzy.h"
#include "mikey.h"
#include "sprite.h"

// Screen:
// Both framebuffers live under the Suzy/Mikey/ROM hardware shadow. Suzy and
// Mikey's video DMA see all 64K as RAM, so the CPU must NEVER read or write
// framebuffer bytes; clear and draw them with Suzy only. The front buffer ends
// below the CPU vector page, and the display base must be 4-byte aligned.
constexpr u8  SYS_SCREEN_WIDTH      = 160;
constexpr u8  SYS_SCREEN_HEIGHT     = 102;
constexpr u16 SYS_SCREEN_SIZE_BYTES = SYS_SCREEN_WIDTH * SYS_SCREEN_HEIGHT / 2;
constexpr u16 SYS_VECTOR_PAGE       = 0xFFFA;
constexpr u16 SYS_SCREEN_BUFFER_1   = static_cast<u16>((SYS_VECTOR_PAGE - SYS_SCREEN_SIZE_BYTES) & ~0x3u); // 0xE018
constexpr u16 SYS_SCREEN_BUFFER_2   = SYS_SCREEN_BUFFER_1 - SYS_SCREEN_SIZE_BYTES;              // 0xC038

extern "C" void     irqHandler(void);

// Referenced by name from the naked-asm irqBody.
extern "C" u8       g_screenFlip;
extern volatile u16 g_sysTicks;

namespace sys {

void                Init(void);

// Display:
void                SetPalette(const u8* palette);
void                SetPalette(const CartAsset& palette);
// r, g, b are 4-bit (0..15).
void                SetPaletteColor(u8 index, u8 r, u8 g, u8 b);
void                ClearScreen(u8 colorIndex);
void                ClearScreenRegion(u8 x, u8 y, u8 width, u8 height, u8 colorIndex);
void                StartFrame();
void                EndFrame();

// EndFrame split in two: arm the VBL swap, run logic, then wait. No Suzy
// blit may be in flight while the swap is armed.
void                ArmBufferSwap();
void                WaitBufferSwap();

// 180-degree hardware screen flip (left-handed play), toggled by OPTION2+PAUSE
// in EndFrame. Games should swap the d-pad axes while g_screenFlip is set.
bool                PollScreenFlip();

// Timing:
// VBL counter advanced by the IRQ; 1 tick = 1 frame (~60 Hz, ~18 min range).
INLINE inline u16 GetTicks()
{
    return g_sysTicks;
}

namespace cart {

// Bodies in sys/cart.S and sys/zx0.S; the asm labels bind to their symbols.
void                SelectBlock(u8 block) __asm__("sys_CartSelectBlock");
void                Init(void) __asm__("sys_CartInit");
void                SeekBlockPos(u8 block, u16 pos) __asm__("sys_CartSeekBlockPos");
u8                  ReadByte(void) __asm__("sys_CartReadByte");
u16                 ReadWord(void) __asm__("sys_CartReadWord");
void                Decompress(u8* dest) __asm__("sys_CartDecompress");
void                Read(void* dest, u8 block, u16 pos, u16 length) __asm__("sys_CartReadRaw");
void                Read(u8* dest, const CartAsset& asset) __asm__("sys_CartReadAsset");

void                Seek(u24 offset);
u16                 AssetSize(const CartAsset& asset);

}

}

#endif
