#ifndef CART_ASM_H
#define CART_ASM_H

// Shared constants for the cart-bus assembler (sys/eeprom.S) and sys/sys.cpp
// (drift check). The static_assert block at the bottom verifies every value
// against the real headers so an upstream change breaks the build instead
// of silently desyncing the asm. sys/cart_asm.S (the read/skip inner loops)
// keeps its local RCART_0 definition.

#define ASM_RCART0          0xFCB2
#define ASM_MIKEY_SYSCTL1   0xFD87
#define ASM_MIKEY_IODIR     0xFD8A
#define ASM_MIKEY_IODAT     0xFD8B

#define ASM_CART_ADDR_DATA  0x02
// Iodat::AUDIN_BIT and its complement. sys/eeprom.S drives AUDIN as the 93C46
// DI/DO line and must leave every other bit of the port exactly as it found
// it, so it masks with these rather than with the reference driver's literals.
#define ASM_CART_AUDIN      0x10
#define ASM_CART_NOT_AUDIN  0xEF
// Sysctl1::POWERON | Sysctl1::CART_STROBE / Sysctl1::POWERON
#define ASM_CART_STROBE_ON  0x03
#define ASM_CART_STROBE_OFF 0x02
// Iodat::AUDIN_BIT | Iodat::RESTLESS | Iodat::CART_ADDR_DATA, the boot ROM /
// cc65 crt0 direction ($1A). RESTLESS MUST be an output driven high or Mikey's
// LCD rest signal never reaches the display and a Lynx I shows a black screen
// (a Lynx II does not care). NOEXP and EXTERNAL_POWER stay inputs.
#define ASM_CART_IODIR_INIT 0x1A

// CART_BLOCK_SIZE (2048) high byte; the block position only ever advances
// one byte at a time, so the boundary test is hi == 0x08 after a low-byte
// wrap.
#define ASM_CART_BLOCK_SIZE_HI 0x08

// struct CartAsset field offsets (packed: u8 block, u16 pos).
#define ASM_CARTASSET_BLOCK_OFF 0
#define ASM_CARTASSET_POS_OFF   1

#if !defined(__ASSEMBLER__)
#include <stddef.h>
#include "mikey.h"
#include "cart_types.h"

static_assert(ASM_CART_ADDR_DATA == static_cast<u8>(Iodat::CART_ADDR_DATA),
              "CART_ADDR_DATA drift");
static_assert(ASM_CART_AUDIN == static_cast<u8>(Iodat::AUDIN_BIT),
              "AUDIN bit drift");
static_assert(ASM_CART_NOT_AUDIN
              == static_cast<u8>(~static_cast<u8>(Iodat::AUDIN_BIT)),
              "AUDIN mask drift");
static_assert(ASM_CART_STROBE_ON
              == static_cast<u8>(Sysctl1::POWERON | Sysctl1::CART_STROBE),
              "sysctl1 strobe-on drift");
static_assert(ASM_CART_STROBE_OFF == static_cast<u8>(Sysctl1::POWERON),
              "sysctl1 strobe-off drift");
static_assert(ASM_CART_IODIR_INIT
              == (static_cast<u8>(Iodat::AUDIN_BIT)
                  | static_cast<u8>(Iodat::RESTLESS)
                  | static_cast<u8>(Iodat::CART_ADDR_DATA)),
              "iodir init drift");

static_assert((ASM_CART_BLOCK_SIZE_HI << 8) == CART_BLOCK_SIZE,
              "CART_BLOCK_SIZE drift");

static_assert(ASM_CARTASSET_BLOCK_OFF == offsetof(CartAsset, block),
              "CartAsset.block drift");
static_assert(ASM_CARTASSET_POS_OFF == offsetof(CartAsset, pos),
              "CartAsset.pos drift");
#endif

#endif
