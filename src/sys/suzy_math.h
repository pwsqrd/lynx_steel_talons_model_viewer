/*
===========================================================================
    suzy_math.h

    Type-safe Suzy math helper functions. Operations return typed handles
    that enforce correct register reads at compile time.

    Example usage:

        Multiplication:
        auto mul = sys::math::StartMultiply(static_cast<u16>(100), static_cast<u16>(50));
        u16 result = mul.Get16();

        Division:
        auto div = sys::math::StartDivide(static_cast<u24>(10000), static_cast<u16>(100));
        u8 result = div.Get8();

        Accumulate:
        auto acc = sys::math::StartAccumulate();
        acc.Mul(static_cast<u16>(10), static_cast<u16>(10));
        acc.Mul(static_cast<u16>(20), static_cast<u16>(5));
        u16 result = acc.Get16();
===========================================================================
*/
#ifndef __SUZY_MATH_H
#define __SUZY_MATH_H

#include "suzy.h"

namespace sys::math {

constexpr u8 SIGNED         = static_cast<u8>(SprsysW::SIGNMATH);
constexpr u8 ACCUMULATE = static_cast<u8>(SprsysW::ACCUMULATE);

/*
=================
Wait

Spins until the current multiply or divide is done. Inlined: the poll is a
2-instruction loop (lda SPRSYS / bmi), and out of line it would be a jsr
barrier at every Get, bloating every math-heavy function.
=================
*/
__attribute__((always_inline)) inline void Wait()
{
    while (SUZY.Sprsys() & SprsysR::MATHWORKING);
}
extern bool IsBusy();

struct SuMulResult
{
    bool neg;

    SuMulResult() = default;
    SuMulResult(bool n) : neg(n) {}

    /*
    =================
    SuMulResult::Get8

    Low byte of |a| x b. The sign is not applied.
    =================
    */
    __attribute__((always_inline)) inline u8 Get8()
    {
        Wait();
        return MATH_H;
    }

    /*
    =================
    SuMulResult::Get16

    Low 16 bits of |a| x b. The sign is not applied.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16()
    {
        Wait();
        return MATH_GH;
    }

    /*
    =================
    SuMulResult::Get16_RShift8

    Signed (a x b) >> 8, as 16 bits.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16_RShift8()
    {
        Wait();
        return neg ? -MATH_FG : MATH_FG;
    }

    /*
    =================
    SuMulResult::Get16_RShift7

    Signed (a x b) >> 7, as 16 bits. Reads bits 7..22 with byte loads and a
    one-bit shift, cheaper than Get16() >> 7. For shifts of 4 or 6 the plain
    shift is smaller.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16_RShift7()
    {
        Wait();
        u16 r = ((MATH_FG << 1) | (MATH_H >> 7));
        return neg ? static_cast<u16>(-static_cast<s16>(r)) : r;
    }

    /*
    =================
    SuMulResult::Get16_RShift9

    Signed (a x b) >> 9, as 16 bits. The unsigned product must fit in 24
    bits (MATH_E zero).
    =================
    */
    __attribute__((always_inline)) inline u16 Get16_RShift9()
    {
        Wait();
        u16 r = (MATH_FG >> 1);
        return neg ? static_cast<u16>(-static_cast<s16>(r)) : r;
    }

    /*
    =================
    SuMulResult::Get32_RShift6

    Signed (a x b) >> 6, as 32 bits.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32_RShift6()
    {
        Wait();
        s32 result = neg ? -static_cast<s32>(MATH_EFGH) : static_cast<s32>(MATH_EFGH);
        return static_cast<u32>(result >> 6);
    }

    /*
    =================
    SuMulResult::Get32_RShift7

    Signed (a x b) >> 7, as 32 bits.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32_RShift7()
    {
        Wait();
        s32 result = neg ? -static_cast<s32>(MATH_EFGH) : static_cast<s32>(MATH_EFGH);
        return static_cast<u32>(result >> 7);
    }

    /*
    =================
    SuMulResult::Get32_RShift8

    Signed (a x b) >> 8, as 32 bits.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32_RShift8()
    {
        Wait();
        s32 result = neg ? -static_cast<s32>(MATH_EFGH) : static_cast<s32>(MATH_EFGH);
        return static_cast<u32>(result >> 8);
    }
};

struct MulResult
{
    MulResult() = default;

    /*
    =================
    MulResult::Get8

    Low byte of the product.
    =================
    */
    __attribute__((always_inline)) inline u8 Get8() const
    {
        Wait();
        return MATH_H;
    }

    /*
    =================
    MulResult::Get16

    Low 16 bits of the product.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16() const
    {
        Wait();
        return MATH_GH;
    }

    /*
    =================
    MulResult::Get24

    Low 24 bits of the product.
    =================
    */
    __attribute__((always_inline)) inline u32 Get24() const
    {
        Wait();
        return (static_cast<u32>(MATH_F) << 16) | MATH_GH;
    }

    /*
    =================
    MulResult::Get32

    Full 32-bit product.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32() const
    {
        Wait();
        return MATH_EFGH;
    }

    /*
    =================
    MulResult::Get32_RShift6

    Product >> 6.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32_RShift6() const
    {
        Wait();
        return MATH_EFGH >> 6;
    }

    /*
    =================
    MulResult::Get32_RShift7

    Product >> 7.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32_RShift7() const
    {
        Wait();
        return MATH_EFGH >> 7;
    }

    /*
    =================
    MulResult::Get32_RShift8

    Product >> 8, as 32 bits (bits 8..23).
    =================
    */
    __attribute__((always_inline)) inline u32 Get32_RShift8() const
    {
        Wait();
        return MATH_FG;
    }

    /*
    =================
    MulResult::Get16_RShift8

    Product >> 8, as 16 bits.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16_RShift8() const
    {
        Wait();
        return MATH_FG;
    }

    /*
    =================
    MulResult::Get16_RShift7

    Product >> 7, as 16 bits. Reads bits 7..22 with byte loads and a
    one-bit shift, cheaper than Get16() >> 7. For shifts of 4 or 6 the plain
    shift is smaller.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16_RShift7() const
    {
        Wait();
        return ((MATH_FG << 1) | (MATH_H >> 7));
    }

    __attribute__((always_inline)) inline struct DivResult StartDivide(u16 divisor) const;
};

struct DivResult
{
    DivResult() = default;

    /*
    =================
    DivResult::Get8

    Low byte of the quotient.
    =================
    */
    __attribute__((always_inline)) inline u8 Get8()
    {
        Wait();
        return MATH_D;
    }

    /*
    =================
    DivResult::Get16

    Low 16 bits of the quotient.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16()
    {
        Wait();
        return MATH_CD;
    }

    /*
    =================
    DivResult::Get24

    Low 24 bits of the quotient.
    =================
    */
    __attribute__((always_inline)) inline u32 Get24()
    {
        Wait();
        return (static_cast<u32>(MATH_B) << 16) | MATH_CD;
    }

    /*
    =================
    DivResult::Get32

    Full 32-bit quotient.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32()
    {
        Wait();
        return MATH_ABCD;
    }
};

/*
=================
MulResult::StartDivide

Divides the product by divisor without reading it back. Waits for the
multiply, writes only the divisor to NP and rewrites MATH_E to start the
divider on the product already in EFGH (lynx9 "QbertRoot").
=================
*/
__attribute__((always_inline)) inline DivResult MulResult::StartDivide(u16 divisor) const
{
    Wait();
    MATH_NP = divisor;
    MATH_E = MATH_E;
    return DivResult{};
}

struct SDivResult
{
    bool neg;

    SDivResult() = default;
    SDivResult(bool n) : neg(n) {}

    /*
    =================
    SDivResult::Get16

    Signed 16-bit quotient.
    =================
    */
    __attribute__((always_inline)) inline s16 Get16()
    {
        Wait();
        u16 result = MATH_CD;
        return neg ? -static_cast<s16>(result) : static_cast<s16>(result);
    }

    /*
    =================
    SDivResult::Get32

    Signed 32-bit quotient.
    =================
    */
    __attribute__((always_inline)) inline s32 Get32()
    {
        Wait();
        u32 result = MATH_ABCD;
        return neg ? -static_cast<s32>(result) : static_cast<s32>(result);
    }
};

struct AccumContext
{
    /*
    =================
    AccumContext::Mul

    Adds a x b to the accumulator and waits for it.
    =================
    */
    __attribute__((always_inline)) inline void Mul(u8 a, u8 b)
    {
        MATH_D = b;
        MATH_B = a;
        Wait();
    }

    /*
    =================
    AccumContext::Mul

    Adds a x b to the accumulator and waits for it.
    =================
    */
    __attribute__((always_inline)) inline void Mul(u16 a, u16 b)
    {
        MATH_CD = b;
        MATH_AB = a;
        Wait();
    }

    /*
    =================
    AccumContext::Mul

    Adds signed a x b to the accumulator and waits for it. Needs
    StartAccumulate(SIGNED).
    =================
    */
    __attribute__((always_inline)) inline void Mul(s16 a, s16 b)
    {
        MATH_CD = static_cast<u16>(b);
        MATH_AB = static_cast<u16>(a);
        Wait();
    }

    /*
    =================
    AccumContext::Start

    Waits for the previous multiply, then starts adding signed a x b to the
    accumulator and returns at once, so the CPU can work while it runs.
    Needs signed accumulate mode.
    =================
    */
    __attribute__((always_inline)) inline void Start(s16 a, s16 b)
    {
        Wait();
        MATH_CD = static_cast<u16>(b);
        MATH_AB = static_cast<u16>(a);
    }

    /*
    =================
    AccumContext::Clear

    Zeroes the accumulator. A write to the low byte of a register pair
    clears the high byte, so K and M clear all of JKLM.
    =================
    */
    __attribute__((always_inline)) inline void Clear()
    {
        Wait();
        MATH_K = 0;
        MATH_M = 0;
    }

    /*
    =================
    AccumContext::GetS8_RShift8

    Signed accumulator >> 8, as 8 bits (MATH_L). The result must fit s8.
    =================
    */
    __attribute__((always_inline)) inline s8 GetS8_RShift8()
    {
        Wait();
        return static_cast<s8>(MATH_L);
    }

    /*
    =================
    AccumContext::GetS16

    Low 16 bits of the accumulator, signed.
    =================
    */
    __attribute__((always_inline)) inline s16 GetS16()
    {
        Wait();
        return static_cast<s16>(MATH_LM);
    }

    /*
    =================
    AccumContext::Get8

    Low byte of the accumulator.
    =================
    */
    __attribute__((always_inline)) inline u8 Get8()
    {
        Wait();
        return MATH_M;
    }

    /*
    =================
    AccumContext::Get16

    Low 16 bits of the accumulator.
    =================
    */
    __attribute__((always_inline)) inline u16 Get16()
    {
        Wait();
        return MATH_LM;
    }

    /*
    =================
    AccumContext::Get24

    Low 24 bits of the accumulator.
    =================
    */
    __attribute__((always_inline)) inline u32 Get24()
    {
        Wait();
        return (static_cast<u32>(MATH_K) << 16) | MATH_LM;
    }

    /*
    =================
    AccumContext::Get32

    Full 32-bit accumulator.
    =================
    */
    __attribute__((always_inline)) inline u32 Get32()
    {
        Wait();
        return MATH_JKLM;
    }
};

/*
=================
SetMode

Selects SIGNED and/or ACCUMULATE for the inline multiplies below, which
leave the mode alone so a run of them costs no SPRSYS writes.
=================
*/
__attribute__((always_inline)) inline void SetMode(u8 flags)
{
    SUZY.SprsysSetMathMode(flags);
}

struct SMulResult
{
    /*
    =================
    SMulResult::GetS8_RShift8

    Signed (a x b) >> 8, as 8 bits (MATH_G). The result must fit s8.
    =================
    */
    __attribute__((always_inline)) inline s8 GetS8_RShift8() const
    {
        Wait();
        return static_cast<s8>(MATH_G);
    }

    /*
    =================
    SMulResult::GetS16_RShift8

    Signed (a x b) >> 8, as 16 bits (MATH_FG).
    =================
    */
    __attribute__((always_inline)) inline s16 GetS16_RShift8() const
    {
        Wait();
        return static_cast<s16>(MATH_FG);
    }

    /*
    =================
    SMulResult::GetU8_RShift8_Round

    Low byte of (a x b + 128) >> 8: MATH_G plus the top bit of MATH_H.
    =================
    */
    __attribute__((always_inline)) inline u8 GetU8_RShift8_Round() const
    {
        Wait();
        return static_cast<u8>(MATH_G + (MATH_H >> 7));
    }
};

/*
=================
StartSignedMultiply

Waits for the previous multiply, then starts a x b and returns at once.
Needs SetMode(SIGNED): the hardware applies the sign, so EFGH holds the
two's complement product. Signed-mode quirks: $8000 counts as +32768.
=================
*/
__attribute__((always_inline)) inline SMulResult StartSignedMultiply(s16 a, s16 b)
{
    Wait();
    MATH_CD = static_cast<u16>(b);
    MATH_AB = static_cast<u16>(a);
    return SMulResult{};
}

/*
=================
RestartSignedMultiply

Starts a x the b of the previous signed multiply, rewriting only AB.
=================
*/
__attribute__((always_inline)) inline SMulResult RestartSignedMultiply(s16 a)
{
    Wait();
    MATH_AB = static_cast<u16>(a);
    return SMulResult{};
}

extern MulResult StartMultiply(u8 a, u8 b);
extern MulResult StartMultiply(u16 a, u16 b);
extern MulResult StartMultiply(s16 a, s16 b);
extern SuMulResult StartMultiply(s16 a, u16 b);
extern DivResult StartDivide(u24 dividend, u16 divisor);
extern DivResult StartDivide(u16 dividend, u16 divisor);
extern SDivResult StartDivide(s24 dividend, s16 divisor);
extern SDivResult StartDivide(s16 dividend, u16 divisor);
extern SDivResult StartDivide(s16 dividend, s16 divisor);
extern SDivResult StartDivide(u16 dividend, s16 divisor);
extern AccumContext StartAccumulate(u8 flags = 0);

}

#endif // __SUZY_MATH_H
