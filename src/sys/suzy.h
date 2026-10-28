/*****************************************************************************/
/*                                                                           */
/*                                _suzy.h                                    */
/*                                                                           */
/* Atari Lynx, Suzy chip register hardware structures                        */
/*                                                                           */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.                                    */
/*                                                                           */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:                            */
/*                                                                           */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.                                       */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.                      */
/* 3. This notice may not be removed or altered from any source              */
/*    distribution.                                                          */
/*                                                                           */
/*****************************************************************************/

#ifndef __SUZY_H
#define __SUZY_H

#include "types.h"

/*
===========================================================================
Enum Definitions
===========================================================================
*/

/* JOYSTICK bit definitions */
enum class Joystick : u8 {
    RIGHT   = 0x10,
    LEFT    = 0x20,
    DOWN    = 0x40,
    UP      = 0x80,
    OPTION1 = 0x08,
    OPTION2 = 0x04,
    INNER   = 0x02,
    OUTER   = 0x01
};

/* SWITCHES bit definitions */
enum class Switches : u8 {
    CART1_IO_INACTIVE = 0x04,
    CART0_IO_INACTIVE = 0x02,
    PAUSE             = 0x01
};

/* SPRCTL0 bit definitions */
enum class Sprctl0 : u8 {
    BPP_4            = 0xC0,
    BPP_3            = 0x80,
    BPP_2            = 0x40,
    BPP_1            = 0x00,
    HFLIP            = 0x20,
    VFLIP            = 0x10,
    TYPE_SHADOW      = 0x07,
    TYPE_XOR         = 0x06,
    TYPE_NONCOLL     = 0x05,
    TYPE_NORMAL      = 0x04,
    TYPE_BOUNDARY    = 0x03,
    TYPE_BSHADOW     = 0x02,
    TYPE_BACKNONCOLL = 0x01,
    TYPE_BACKGROUND  = 0x00
};

/* SPRCTL1 bit definitions */
enum class Sprctl1 : u8 {
    LITERAL  = 0x80,
    PACKED   = 0x00,
    ALGO3    = 0x40,
    RENONE   = 0x00,
    REHV     = 0x10,
    REHVS    = 0x20,
    REHVST   = 0x30,
    REUSEPAL = 0x08,
    SKIP     = 0x04,
    DRAWUP   = 0x02,
    DRAWLEFT = 0x01
};

/* SPRGO bit definitions */
enum class Sprgo : u8 {
    GO      = 0x01,
    EVER_ON = 0x04
};

/* SPRSYS bit definitions for write operations */
enum class SprsysW : u8 {
    SIGNMATH   = 0x80,
    ACCUMULATE = 0x40,
    NO_COLLIDE = 0x20,
    VSTRETCH   = 0x10,
    LEFTHAND   = 0x08,
    CLR_UNSAFE = 0x04,
    STOP       = 0x02
};

/* SPRSYS bit definitions for read operations */
enum class SprsysR : u8 {
    MATHWORKING   = 0x80,
    MATHWARNING   = 0x40,
    MATHCARRY     = 0x20,
    VSTRETCHING   = 0x10,
    LEFTHANDED    = 0x08,
    UNSAFE_ACCESS = 0x04,
    SPRITETOSTOP  = 0x02,
    SPRITEWORKING = 0x01
};

/*
===========================================================================
Enum Operators
===========================================================================
*/

__attribute__((always_inline)) constexpr Joystick operator|(Joystick a, Joystick b) {
    return static_cast<Joystick>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr u8 operator&(u8 a, Joystick b) {
    return a & static_cast<u8>(b);
}
__attribute__((always_inline)) constexpr u8 operator&(u8 a, Switches b) {
    return a & static_cast<u8>(b);
}
__attribute__((always_inline)) constexpr Sprctl0 operator|(Sprctl0 a, Sprctl0 b) {
    return static_cast<Sprctl0>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Sprctl1 operator|(Sprctl1 a, Sprctl1 b) {
    return static_cast<Sprctl1>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Sprgo operator|(Sprgo a, Sprgo b) {
    return static_cast<Sprgo>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr SprsysW operator|(SprsysW a, SprsysW b) {
    return static_cast<SprsysW>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr u8 operator&(u8 a, SprsysR b) {
    return a & static_cast<u8>(b);
}

/*
===========================================================================
Sprite Control Block (SCB) Structures
===========================================================================
*/

/* SCB with all attributes */
typedef struct SCB_REHVST_PAL {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
    u16 hsize;
    u16 vsize;
    u16 stretch;
    u16 tilt;
    u8 penpal[8];
} SCB_REHVST_PAL;

/* SCB without pallette */
typedef struct SCB_REHVST {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
    u16 hsize;
    u16 vsize;
    u16 stretch;
    u16 tilt;
} SCB_REHVST;

/* SCB without stretch/tilt */
typedef struct SCB_REHV {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
    u16 hsize;
    u16 vsize;
} SCB_REHV;

/* SCB without stretch/tilt, with penpal */
typedef struct SCB_REHV_PAL {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
    u16 hsize;
    u16 vsize;
    u8 penpal[8];
} SCB_REHV_PAL;

/* SCB without tilt/penpal */
typedef struct SCB_REHVS {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
    u16 hsize;
    u16 vsize;
    u16 stretch;
} SCB_REHVS;

/* SCB without tilt, with penpal */
typedef struct SCB_REHVS_PAL {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
    u16 hsize;
    u16 vsize;
    u16 stretch;
    u8 penpal[8];
} SCB_REHVS_PAL;

/* SCB without size/stretch/tilt/penpal */
typedef struct SCB_RENONE {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
} SCB_RENONE;

/* SCB without size/str/tilt, with penpal */
typedef struct SCB_RENONE_PAL {
    TypedField<Sprctl0> sprctl0;
    TypedField<Sprctl1> sprctl1;
    u8 sprcoll;
    char *next;
    u8 *data;
    s16 hpos;
    s16 vpos;
    u8 penpal[8];
} SCB_RENONE_PAL;

typedef struct PENPAL_4 {
    u8 penpal[8];
} PENPAL_4;

typedef struct PENPAL_3 {
    u8 penpal[4];
} PENPAL_3;

typedef struct PENPAL_2 {
    u8 penpal[2];
} PENPAL_2;

typedef struct PENPAL_1 {
    u8 penpal[1];
} PENPAL_1;

/*
===========================================================================
Suzy Hardware Registers
===========================================================================
*/

// SPRSYS sticky configuration: bits we always want set in SPRSYS regardless
// of what math mode the current op needs. Only NO_COLLIDE is set here (sprite
// collisions disabled); VSTRETCH/LEFTHAND/CLR_UNSAFE/STOP are
// never used. Every SPRSYS write goes through SprsysSetMathMode(), which ORs
// in SPRSYS_STICKY — no software shadow needed.
constexpr u8 SPRSYS_STICKY = static_cast<u8>(SprsysW::NO_COLLIDE);

struct SuzyChip {
    u8* tmpadr;        /* 0xFC00  Temporary address */
    u16 tiltacc;       /* 0xFC02  Tilt accumulator */
    u16 hoff;          /* 0xFC04  Offset to H edge of screen */
    u16 voff;          /* 0xFC06  Offset to V edge of screen */
    u8* sprbase;       /* 0xFC08  Base address of sprite */
    u8* colbase;       /* 0xFC0A  Base address of collision buffer */
    u8* vidadr;        /* 0xFC0C  Current vid buffer address */
    u8* coladr;        /* 0xFC0E  Current col buffer address */
    u8* scbnext;       /* 0xFC10  Address of next SCB */
    u8* sprdline;      /* 0xFC12  start of sprite data line address */
    u16 hposstrt;      /* 0xFC14  start hpos */
    u16 vposstrt;      /* 0xFC16  start vpos */
    u16 sprhsize;      /* 0xFC18  sprite h size */
    u16 sprvsize;      /* 0xFC1A  sprite v size */
    u16 stretchl;      /* 0xFC1C  H size adder */
    u16 tilt;          /* 0xFC1E  H pos adder */
    u16 sprdoff;       /* 0xFC20  offset to next sprite data line */
    u16 sprvpos;       /* 0xFC22  current vpos */
    u16 colloff;       /* 0xFC24  offset to collision depository */
    u16 vsizeacc;      /* 0xFC26  vertical size accumulator */
    u16 hsizeoff;      /* 0xFC28  horizontal size offset */
    u16 vsizeoff;      /* 0xFC2A  vertical size offset */
    u8* scbaddr;       /* 0xFC2C  address of current SCB */
    u8* procaddr;      /* 0xFC2E  address of current spr data proc */
    u8 unused0[32];    /* 0xFC30 - 0xFC4F  reserved/unused */
    u8 unused1[2];     /* 0xFC50 - 0xFC51  do not use */
    u8 mathd;          /* 0xFC52 */
    u8 mathc;          /* 0xFC53 */
    u8 mathb;          /* 0xFC54 */
    u8 matha;          /* 0xFC55  write starts a multiply operation */
    u8 mathp;          /* 0xFC56 */
    u8 mathn;          /* 0xFC57 */
    u8 unused2[8];     /* 0xFC58 - 0xFC5F  do not use */
    u8 mathh;          /* 0xFC60 */
    u8 mathg;          /* 0xFC61 */
    u8 mathf;          /* 0xFC62 */
    u8 mathe;          /* 0xFC63  write starts a divide operation */
    u8 unused3[8];     /* 0xFC64 - 0xFC6B  do not use */
    u8 mathm;          /* 0xFC6C */
    u8 mathl;          /* 0xFC6D */
    u8 mathk;          /* 0xFC6E */
    u8 mathj;          /* 0xFC6F */
    u8 unused4[16];    /* 0xFC70 - 0xFC7F  do not use */
    TypedField<Sprctl0> sprctl0;  /* 0xFC80  sprite control bits 0 */
    TypedField<Sprctl1> sprctl1;  /* 0xFC81  sprite control bits 1 */
    u8 sprcoll;        /* 0xFC82  sprite collision number */
    u8 sprinit;        /* 0xFC83  sprite initialization bits */
    u8 unused5[4];     /* 0xFC84 - 0xFC87  unused */
    ReadOnly<u8> suzyhrev; /* 0xFC88  suzy hardware rev (read-only!) */
    ReadOnly<u8> suzysrev; /* 0xFC89  suzy software rev (read-only!) */
    u8 unused6[6];         /* 0xFC8A - 0xFC8F  unused */
    WriteOnly<u8> suzybusen;/* 0xFC90  suzy bus enable (write-only!) */
    WriteOnly<u8> sprgo;   /* 0xFC91  sprite process start bit (write-only!) */
private:
    u8 sprsys;            /* 0xFC92  sprite system control bits (r/w but different meanings!) */
public:
    u8 unused7[29];        /* 0xFC93 - 0xFCAF  unused */
    ReadOnly<u8> joystick; /* 0xFCB0  joystick and buttons (read-only!) */
    ReadOnly<u8> switches; /* 0xFCB1  other switches (read-only!) */
    u8 cart0;          /* 0xFCB2  cart0 r/w */
    u8 cart1;          /* 0xFCB3  cart1 r/w */
    u8 unused8[8];     /* 0xFCB4 - 0xFCBF  unused */
    u8 leds;           /* 0xFCC0  leds */
    u8 unused9;        /* 0xFCC1  unused */
    u8 parstat;        /* 0xFCC2  parallel port status */
    u8 pardata;        /* 0xFCC3  parallel port data */
    u8 howie;          /* 0xFCC4  howie (?) */
    /* 0xFCC5 - 0xFCFF  unused */

    __attribute__((always_inline)) inline u8 Sprsys() volatile
    {
        return sprsys;
    }

    // Single SPRSYS write entry point. Called by every Suzy math op at entry
    // with the math-mode bits it needs (0 / SIGNMATH / ACCUMULATE / both).
    // SPRSYS_STICKY is OR'd in so the sprite-engine sticky config (NO_COLLIDE)
    // is preserved across every write. No read-modify-write, no shadow, no
    // branch — one constant OR + one store.
    __attribute__((always_inline)) inline void SprsysSetMathMode(u8 mathBits) volatile
    {
        constexpr u8 MATH_MASK = static_cast<u8>(SprsysW::SIGNMATH)
                               | static_cast<u8>(SprsysW::ACCUMULATE);
        sprsys = SPRSYS_STICKY | (mathBits & MATH_MASK);
    }
};

/*
===========================================================================
Macros and Defines
===========================================================================
*/

/* Hardware math registers - byte access */
#define MATH_A  (*reinterpret_cast<volatile u8*>(0xFC55))
#define MATH_B  (*reinterpret_cast<volatile u8*>(0xFC54))
#define MATH_C  (*reinterpret_cast<volatile u8*>(0xFC53))
#define MATH_D  (*reinterpret_cast<volatile u8*>(0xFC52))
#define MATH_E  (*reinterpret_cast<volatile u8*>(0xFC63))
#define MATH_F  (*reinterpret_cast<volatile u8*>(0xFC62))
#define MATH_G  (*reinterpret_cast<volatile u8*>(0xFC61))
#define MATH_H  (*reinterpret_cast<volatile u8*>(0xFC60))
#define MATH_J  (*reinterpret_cast<volatile u8*>(0xFC6F))
#define MATH_K  (*reinterpret_cast<volatile u8*>(0xFC6E))
#define MATH_L  (*reinterpret_cast<volatile u8*>(0xFC6D))
#define MATH_M  (*reinterpret_cast<volatile u8*>(0xFC6C))
#define MATH_N  (*reinterpret_cast<volatile u8*>(0xFC57))
#define MATH_P  (*reinterpret_cast<volatile u8*>(0xFC56))

/* Hardware math registers - word/dword access */
#define MATH_AB      (*reinterpret_cast<volatile u16*>(0xFC54))
#define MATH_CD      (*reinterpret_cast<volatile u16*>(0xFC52))
#define MATH_ABCD    (*reinterpret_cast<volatile u32*>(0xFC52))
#define MATH_EF      (*reinterpret_cast<volatile u16*>(0xFC62))
#define MATH_FG      (*reinterpret_cast<volatile u16*>(0xFC61))
#define MATH_GH      (*reinterpret_cast<volatile u16*>(0xFC60))
#define MATH_EFGH    (*reinterpret_cast<volatile u32*>(0xFC60))
#define MATH_FGH     (*reinterpret_cast<volatile u32*>(0xFC60))
#define MATH_LM      (*reinterpret_cast<volatile u16*>(0xFC6C))
#define MATH_JKLM    (*reinterpret_cast<volatile u32*>(0xFC6C))
#define MATH_NP      (*reinterpret_cast<volatile u16*>(0xFC56))

/* Legacy hardware math defines */
#define FACTOR_A *(unsigned int *) 0xFC54
#define FACTOR_B *(unsigned int *) 0xFC52
#define PRODUCT0 *(unsigned int *) 0xFC60
#define PRODUCT1 *(unsigned int *) 0xFC62
#define PRODUCT *(long *) 0xFC60

#define DIVIDEND0 *(unsigned int *) 0xFC60
#define DIVIDEND1 *(unsigned int *) 0xFC62
#define DIVIDEND *(long *) 0xFC60
#define DIVISOR *(unsigned int *) 0xFC56
#define QUOTIENT0 *(unsigned int *) 0xFC52
#define QUOTIENT1 *(unsigned int *) 0xFC54
#define QUOTIENT *(long *) 0xFC52
#define REMAINDER0 *(unsigned int *) 0xFC6C
#define REMAINDER1 *(unsigned int *) 0xFC6E
#define REMAINDER *(long *) 0xFC6C

/* Deprecated definitions */

/* MAPCTL $FFF9 */
#define HIGHSPEED       0x80
#define VECTORSPACE     0x08
#define ROMSPACE        0x04
#define MIKEYSPACE      0x02
#define SUZYSPACE       0x01


/* Standard sprite engine initialization */
#define SPRINIT_DEFAULT  0xF3
#define SUZY_START 0xFC00

#define SUZY  (*reinterpret_cast<volatile SuzyChip*>(SUZY_START))

#endif
