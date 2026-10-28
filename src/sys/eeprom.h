#ifndef SYS_EEPROM_H
#define SYS_EEPROM_H

#include "types.h"

// 93C46 SPI EEPROM driver (128 bytes, organized as 64 16-bit words).
//
// Wiring (standard Lynx EEPROM cart per Bastian Schick's BLL driver,
// also the layout the RetroHQ GameDrive and GearLynx emulate):
//   CS    = A7 of the cart address shift register
//   CLK   = A1 of the cart address shift register
//   DI/DO = AUDIN pin (bidirectional, switched via MIKEY.iodir bit 4)
//
// The chip is declared in the LNX header EEPROM-type byte (byte 60 = 1
// for 93C46). pack_lynx_cart.py emits the .lyx, makelnx wraps it as a
// .lnx with byte 60 zero; a post-step patches byte 60 to 1.
//
// All routines disable IRQs internally and invalidate the cart-block
// cursor (g_cartCurrentBlock) on exit, since the EEPROM protocol shifts
// the cart address counter to arbitrary state. The next cart read
// re-seeks automatically.

constexpr u8 SYS_EEPROM_BYTES = 128;
constexpr u8 SYS_EEPROM_WORDS = 64;

// Bodies in sys/eeprom.S; the asm labels bind to its symbols.
namespace sys::eeprom {

// Read a 16-bit word from word address (0..63). Returns the data word.
u16  ReadWord(u8 wordAddr) __asm__("sys_EepromReadWord");

// Write a 16-bit word to word address (0..63). Handles EWEN before /
// EWDS after, and polls the busy line until the write completes.
void WriteWord(u8 wordAddr, u16 value) __asm__("sys_EepromWriteWord");

}

#endif
