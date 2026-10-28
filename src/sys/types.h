#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/*
=================
    Basic Type Definitions
=================
*/
typedef int8_t    s8;
typedef uint8_t   u8;
typedef int16_t   s16;
typedef uint16_t  u16;
typedef signed   _BitInt(24) s24;
typedef unsigned _BitInt(24) u24;
typedef int32_t   s32;
typedef uint32_t  u32;

/*
=================
    Zeropage Attribute Definition
    Usage: ZP u8 myVar;
=================
*/
#define ZP __attribute__((section(".zp.bss")))

/*
=================
    Extra BSS placement in the reclaimed stage2-loader region (0x200-0x2FF,
    256 B). Use for small statics that would otherwise take space in the main
    .bss region.
    Usage: EXTRABSS u16 myVar;
=================
*/
#define EXTRABSS __attribute__((section(".extrabss")))

/*
=================
    Force static const arrays out of llvm-mos's automatic .zp.data
    placement and into regular .rodata. Use for cold-path tables that
    would otherwise waste scarce ZP budget.
    Usage: static const u8 myTable[N] NO_ZP = { ... };
=================
*/
#define NO_ZP __attribute__((section(".rodata")))

#define NOINLINE __attribute__((noinline))

/*
=================
    WriteOnly template

    Variables of this type can only be written to, not read from.
    Attempting to read will result in a compile-time error.
    Useful for hardware registers that are write-only.

    Usage: WriteOnly<u8> myReg;
=================
*/
template<typename T>
struct WriteOnly
{
    volatile T value;

    __attribute__((always_inline)) void operator=(T v) volatile { value = v; }
    template<typename U>
    __attribute__((always_inline)) void operator=(U v) volatile { value = static_cast<T>(v); }

    operator T() const volatile = delete;
    T operator|=(T) volatile = delete;
    T operator&=(T) volatile = delete;
    T operator^=(T) volatile = delete;
    T operator+=(T) volatile = delete;
    T operator-=(T) volatile = delete;
};

/*
=================
    ReadOnly template

    Variables of this type can only be read from, not written to.
    Attempting to write will result in a compile-time error.
    Useful for hardware registers that are read-only.

    Usage: ReadOnly<u8> myReg;
================
*/
template<typename T>
struct ReadOnly
{
    volatile T value;

    __attribute__((always_inline)) operator T() const volatile { return value; }

    void operator=(T) volatile = delete;
    T operator|=(T) volatile = delete;
    T operator&=(T) volatile = delete;
    T operator^=(T) volatile = delete;
    T operator+=(T) volatile = delete;
    T operator-=(T) volatile = delete;
};

/*
=================
    TypedField template

    A field that uses a specific enum type for its value.
    This prevents you using flags of the wrong type.

    For example, tring to use a Sprctl0 flag on a Sprctl1 field will
    result in a compile-time error instead or weird Lynx behavior.

    Usage: 
           TypedField<Sprctl0> sprctl_0_field; 
           TypedField<Sprctl1> sprctl_1_field;

           sprctl_0_field = Sprctl0::ENABLE;                    // OK
           sprctl_0_field = Sprctl0::BPP_4 | Sprctl0::HFLIP;    // OK

           sprctl_1_field = Sprctl0::BPP_4 ;  // Compile-time error
           sprtcl_1_field = Sprctl1::LITERAL | Sprctl0::BPP_4;   // Compile-time error
           sprtcl_1_field = sprctl_0_field;    // Compile-time error

=================
*/
template<typename FlagType>
struct TypedField
{
    FlagType value;

    __attribute__((always_inline)) void operator=(FlagType v) { value = v; }
    __attribute__((always_inline)) void operator=(FlagType v) volatile { value = v; }
    __attribute__((always_inline)) operator FlagType() const { return value; }
    __attribute__((always_inline)) operator FlagType() const volatile { return value; }
};

#endif
