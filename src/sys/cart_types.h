#ifndef CART_TYPES_H
#define CART_TYPES_H

#include "types.h"

constexpr u16 CART_BLOCK_SIZE = 2048;
constexpr u8 CART_BLOCK_SHIFT = 11;

struct __attribute__((packed)) CartAsset {
    u8 block;
    u16 pos;
};

#endif
