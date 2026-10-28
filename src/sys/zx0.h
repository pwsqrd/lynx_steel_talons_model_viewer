#ifndef ZX0_H
#define ZX0_H

#include "types.h"

namespace sys::zx0 {

void Decompress(u8* dst) __asm__("zx0_Decompress");

}

#endif
