#include "suzy_math.h"

namespace sys::math {

/*
=================
IsBusy

True while a multiply or divide is still running.
=================
*/
bool IsBusy()
{
    return (SUZY.Sprsys() & SprsysR::MATHWORKING) != 0;
}

/*
=================
StartMultiply

Unsigned 8 x 8 multiply.
=================
*/
MulResult StartMultiply(u8 a, u8 b)
{
    // The Lynx multiplier reads MATH_AB × MATH_CD as 16-bit pairs and FIRES
    // on the store to MATH_A ($FC55). All four operand bytes must be valid
    // before that store, so MATH_A must be written LAST. Writing MATH_A=0
    // first would latch (0, stale_B) × (stale_C, stale_D) and never
    // re-trigger when B/C/D are written afterward.
    SUZY.SprsysSetMathMode(0);
    MATH_C = 0;
    MATH_D = b;
    MATH_B = a;
    MATH_A = 0;
    return MulResult{};
}

/*
=================
StartMultiply

Unsigned 16 x 16 multiply, 32-bit product.
=================
*/
MulResult StartMultiply(u16 a, u16 b)
{
    SUZY.SprsysSetMathMode(0);
    MATH_CD = b;
    MATH_AB = a;
    return MulResult{};
}

/*
=================
StartMultiply

Signed 16 x 16 multiply in hardware. The hardware treats $8000 as positive.
=================
*/
MulResult StartMultiply(s16 a, s16 b)
{
    SUZY.SprsysSetMathMode(SIGNED);
    MATH_CD = static_cast<u16>(b);
    MATH_AB = static_cast<u16>(a);
    return MulResult{};
}

/*
=================
StartDivide

Unsigned 24 / 16 divide. Dividing by zero gives $FFFFFFFF.
=================
*/
DivResult StartDivide(u24 dividend, u16 divisor)
{
    SUZY.SprsysSetMathMode(0);
    MATH_NP = divisor;
    MATH_EFGH = static_cast<u32>(dividend);
    return DivResult{};
}

/*
=================
StartDivide

Unsigned 16 / 16 divide. Dividing by zero gives $FFFFFFFF.
=================
*/
DivResult StartDivide(u16 dividend, u16 divisor)
{
    SUZY.SprsysSetMathMode(0);
    MATH_NP = divisor;
    MATH_GH = dividend;
    MATH_EF = 0;
    return DivResult{};
}

/*
=================
StartMultiply

Signed x unsigned multiply. Multiplies |a| by b unsigned; the result
re-applies the sign.
=================
*/
SuMulResult StartMultiply(s16 a, u16 b)
{
    SUZY.SprsysSetMathMode(0);
    bool neg = (a < 0);
    u16 abs_a = neg ? static_cast<u16>(-a) : static_cast<u16>(a);
    MATH_CD = b;
    MATH_AB = abs_a;
    return SuMulResult{neg};
}

/*
=================
StartDivide

Signed 24 / 16 divide. Divides the magnitudes unsigned; the result
re-applies the sign.
=================
*/
SDivResult StartDivide(s24 dividend, s16 divisor)
{
    SUZY.SprsysSetMathMode(0);
    bool neg = (dividend < 0) != (divisor < 0);
    u24 abs_dividend = dividend < 0 ? static_cast<u24>(-dividend) : static_cast<u24>(dividend);
    u16 abs_divisor = divisor < 0 ? static_cast<u16>(-divisor) : static_cast<u16>(divisor);
    MATH_NP = abs_divisor;
    MATH_EFGH = static_cast<u32>(abs_dividend);
    return SDivResult{neg};
}

/*
=================
StartDivide

Signed / unsigned 16-bit divide. Divides the magnitudes unsigned; the
result re-applies the sign.
=================
*/
SDivResult StartDivide(s16 dividend, u16 divisor)
{
    SUZY.SprsysSetMathMode(0);
    bool neg = (dividend < 0);
    u16 abs_dividend = neg ? static_cast<u16>(-dividend) : static_cast<u16>(dividend);
    MATH_NP = divisor;
    MATH_GH = abs_dividend;
    MATH_EF = 0;
    return SDivResult{neg};
}

/*
=================
StartDivide

Signed 16 / 16 divide. Divides the magnitudes unsigned; the result
re-applies the sign.
=================
*/
SDivResult StartDivide(s16 dividend, s16 divisor)
{
    SUZY.SprsysSetMathMode(0);
    bool neg = (dividend < 0) != (divisor < 0);
    u16 abs_dividend = dividend < 0 ? static_cast<u16>(-dividend) : static_cast<u16>(dividend);
    u16 abs_divisor = divisor < 0 ? static_cast<u16>(-divisor) : static_cast<u16>(divisor);
    MATH_NP = abs_divisor;
    MATH_GH = abs_dividend;
    MATH_EF = 0;
    return SDivResult{neg};
}

/*
=================
StartDivide

Unsigned / signed 16-bit divide. Divides the magnitudes unsigned; the
result re-applies the sign.
=================
*/
SDivResult StartDivide(u16 dividend, s16 divisor)
{
    SUZY.SprsysSetMathMode(0);
    bool neg = (divisor < 0);
    u16 abs_divisor = neg ? static_cast<u16>(-divisor) : static_cast<u16>(divisor);
    MATH_NP = abs_divisor;
    MATH_GH = dividend;
    MATH_EF = 0;
    return SDivResult{neg};
}

/*
=================
StartAccumulate

Clears the accumulator and switches the multiplier to accumulate mode.
Pass SIGNED for signed products.
=================
*/
AccumContext StartAccumulate(u8 flags)
{
    SUZY.SprsysSetMathMode(ACCUMULATE | (flags & SIGNED));
    MATH_K = 0;
    MATH_M = 0;
    return AccumContext{};
}

}
