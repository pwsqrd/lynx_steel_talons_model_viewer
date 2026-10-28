/*****************************************************************************/
/*                                                                           */
/*                                _mikey.h                                   */
/*                                                                           */
/* Atari Lynx, Mikey chip register hardware structures                       */
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

#ifndef __MIKEY_H
#define __MIKEY_H

#include "types.h"

/* Forward declarations for timer control types */
enum class TimerCtrlA : u8;
enum class TimerCtrlB : u8;

/* Timer structure */
typedef struct MikeyTimer {
    u8 reload;
    TypedField<TimerCtrlA> control;
    u8 count;
    TypedField<TimerCtrlB> control2;
} MikeyTimer;

typedef struct MikeyAllTimers {
    MikeyTimer timer[8];
} MikeyAllTimers;

/* Audio channel structure */
typedef struct MikeyAudio {
    u8 volume;
    u8 feedback;
    u8 dac;
    u8 shiftlo;
    u8 reload;
    u8 control;
    u8 count;
    u8 other;
} MikeyAudio;

/* Define a structure with the mikey register offsets */
struct MikeyChip {
    MikeyTimer timer0;    /* 0xFD00 */
    MikeyTimer timer1;    /* 0xFD04 */
    MikeyTimer timer2;    /* 0xFD08 */
    MikeyTimer timer3;    /* 0xFD0C */
    MikeyTimer timer4;    /* 0xFD10 */
    MikeyTimer timer5;    /* 0xFD14 */
    MikeyTimer timer6;    /* 0xFD18 */
    MikeyTimer timer7;    /* 0xFD1C */
    MikeyAudio channel_a; /* 0xFD20 */
    MikeyAudio channel_b; /* 0xFD28 */
    MikeyAudio channel_c; /* 0xFD30 */
    MikeyAudio channel_d; /* 0xFD38 */
    u8 attena;          /* 0xFD40  ?? not yet allocated? */
    u8 attenb;          /* 0xFD41      | */
    u8 attenc;          /* 0xFD42      | */
    u8 attend;          /* 0xFD43      | */
    u8 panning;         /* 0xFD44      | */
    u8 unused0[11];     /* 0xFD45 - 0xFD4F  not used */
    u8 mstereo;         /* 0xFD50  stereo control bits */
    u8 unused1[47];     /* 0xFD51 - 0xFD7F  not used */
    WriteOnly<u8> intrst; /* 0xFD80  interrupt poll 0 (write-only: clears interrupts) */
    ReadOnly<u8> intset;  /* 0xFD81  interrupt poll 1 (read-only!) */
    u8 unused2[2];        /* 0xFD82 - 0xFD83  not used */
    u8 magrdy0;           /* 0xFD84  mag tape channel0 ready bit */
    u8 magrdy1;           /* 0xFD85  mag tape channel1 ready bit */
    u8 audin;             /* 0xFD86  audio in */
    WriteOnly<u8> sysctl1;/* 0xFD87  control bits (write-only!) */
    ReadOnly<u8> mikeyrev;/* 0xFD88  mikey hardware rev (read-only!) */
    u8 mikeysrev;         /* 0xFD89  mikey software rev */
    WriteOnly<u8> iodir;  /* 0xFD8A  parallel i/o data dir (write-only!) */
    u8 iodat;             /* 0xFD8B  parallel data (readable) */
    WriteOnly<u8> serctl; /* 0xFD8C  serial control register (write-only!) */
    u8 serdat;            /* 0xFD8D  serial data */
    u8 unused3[2];        /* 0xFD8E - 0xFD8F  not used */
    WriteOnly<u8> sdoneack;/* 0xFD90  suzy done acknowledge (write-only!) */
    WriteOnly<u8> cpusleep;/* 0xFD91  cpu bus request disable (write-only!) */
    WriteOnly<u8> dispctl;/* 0xFD92  video bus request enable (write-only!) */
    u8 pkbkup;          /* 0xFD93  magic 'P' count */
    u8* scrbase;        /* 0xFD94  start address of video display */
    u8 unused4[6];      /* 0xFD96 - 0xFD9B  not used */
    u8 mtest0;          /* 0xFD9C */
    u8 mtest1;          /* 0xFD9D */
    u8 mtest2;          /* 0xFD9E */
    u8 unused5;         /* 0xFD9F  not used */
    u8 palette[32];     /* 0xFDA0 - 0xFDBF  palette 32 bytes */
    u8 unused6[64];     /* 0xFDC0 - 0xFDFF  not used */
    u8 bootrom[504];    /* 0xFE00 - 0xFFD8  boot rom */
    u8 reserved;        /* 0xFFD8  reserved for future hardware */
    u8 mapctl;          /* 0xFFF9  map control register */
    struct {
        u8* nmi;          /* 0xFFFA-0xFFFB  NMI vector */
        u8* reset;        /* 0xFFFC-0xFFFD  RESET vector */
        u8* irq;          /* 0xFFFE-0xFFFF  IRQ vector */
    } vectors;
};

/*
===========================================================================
Enum Definitions
===========================================================================
*/

/* TIM_CONTROLA control bit definitions */
enum class TimerCtrlA : u8 {
    ENABLE_INT    = 0x80,
    RESET_DONE    = 0x40,
    ENABLE_RELOAD = 0x10,
    ENABLE_COUNT  = 0x08,
    NONE
};

/* TIM_CONTROLB control bit definitions */
enum class TimerCtrlB : u8 {
    TIMER_DONE = 0x08,
    LAST_CLOCK = 0x04,
    BORROW_IN  = 0x02,
    BORROW_OUT = 0x01,
    NONE
};

/* Audio and timer clock settings for source period */
enum class ClockSelect : u8 {
    LINKING = 0x07,
    DIV_64  = 0x06,
    DIV_32  = 0x05,
    DIV_16  = 0x04,
    DIV_8   = 0x03,
    DIV_4   = 0x02,
    DIV_2   = 0x01,
    DIV_1   = 0x00
};

/* AUD_CONTROL control bit definitions */
enum class AudioCtrl : u8 {
    FEEDBACK_7       = 0x80,
    ENABLE_INTEGRATE = 0x20
};

/* MPAN and MSTEREO registers bit definitions */
enum class Stereo : u8 {
    LEFT3        = 0x80,
    LEFT2        = 0x40,
    LEFT1        = 0x20,
    LEFT0        = 0x10,
    RIGHT3       = 0x08,
    RIGHT2       = 0x04,
    RIGHT1       = 0x02,
    RIGHT0       = 0x01,
    LEFT_MASK    = 0xF0,
    RIGHT_MASK   = 0x0F
};

/* Interrupt Reset and Set bit definitions */
enum class Interrupt : u8 {
    TIMER7     = 0x80,
    TIMER6     = 0x40,
    TIMER5     = 0x20,
    TIMER4     = 0x10,
    TIMER3     = 0x08,
    TIMER2     = 0x04,
    TIMER1     = 0x02,
    TIMER0     = 0x01,
    SERIAL     = 0x10,
    VERTICAL   = 0x04,
    HORIZONTAL = 0x01
};

/* SYSCTL1 bit definitions */
enum class Sysctl1 : u8 {
    POWERON     = 0x02,
    CART_STROBE = 0x01
};

/* IODIR and IODAT bit definitions */
enum class Iodat : u8 {
    AUDIN_BIT      = 0x10,
    READ_ENABLE    = 0x10,
    RESTLESS       = 0x08,
    NOEXP          = 0x04,
    CART_ADDR_DATA = 0x02,
    CART_POWER_OFF = 0x02,
    EXTERNAL_POWER = 0x01
};

/* SERCTL bit definitions for write operations */
enum class SerctlW : u8 {
    TXINTEN  = 0x80,
    RXINTEN  = 0x40,
    PAREN    = 0x10,
    RESETERR = 0x08,
    TXOPEN   = 0x04,
    TXBRK    = 0x02,
    PAREVEN  = 0x01
};

/* SERCTL bit definitions for read operations */
enum class SerctlR : u8 {
    TXRDY   = 0x80,
    RXRDY   = 0x40,
    TXEMPTY = 0x20,
    PARERR  = 0x10,
    OVERRUN = 0x08,
    FRAMERR = 0x04,
    RXBRK   = 0x02,
    PARBIT  = 0x01
};

/* DISPCTL bit definitions */
enum class Dispctl : u8 {
    COLOR    = 0x08,
    FOURBIT  = 0x04,
    FLIP     = 0x02,
    DMA_ON   = 0x01,
    DISABLE  = 0x00
};

/* MTEST0 bit definitions */
enum class Mtest0 : u8 {
    AT_CNT16   = 0x80,
    AT_TEST    = 0x40,
    XCLKEN     = 0x20,
    UART_TURBO = 0x10,
    ROM_SEL    = 0x08,
    ROM_TEST   = 0x04,
    M_TEST     = 0x02,
    CPU_TEST   = 0x01
};

/* MTEST1 bit definitions */
enum class Mtest1 : u8 {
    P_CNT16     = 0x40,
    REF_CNT16   = 0x20,
    VID_TRIG    = 0x10,
    REF_TRIG    = 0x08,
    VID_DMA_DIS = 0x04,
    REF_FAST    = 0x02,
    REF_DIS     = 0x01
};

/* MTEST2 bit definitions */
enum class Mtest2 : u8 {
    V_STROBE  = 0x10,
    V_ZERO    = 0x08,
    H_120     = 0x04,
    H_ZERO    = 0x02,
    V_BLANKEF = 0x01
};

/* MAPCTL bit definitions */
enum class Mapctl : u8 {
    TURBO_DISABLE = 0x80,
    VECTOR_SPACE  = 0x08,
    ROM_SPACE     = 0x04,
    MIKEY_SPACE   = 0x02,
    SUZY_SPACE    = 0x01
};

/*
===========================================================================
Enum Operators
===========================================================================
*/

__attribute__((always_inline)) constexpr TimerCtrlA operator|(TimerCtrlA a, TimerCtrlA b) {
    return static_cast<TimerCtrlA>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr TimerCtrlA operator|(TimerCtrlA a, ClockSelect b) {
    return static_cast<TimerCtrlA>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr u8 operator&(u8 a, TimerCtrlB b) {
    return a & static_cast<u8>(b);
}
__attribute__((always_inline)) constexpr AudioCtrl operator|(AudioCtrl a, AudioCtrl b) {
    return static_cast<AudioCtrl>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Stereo operator|(Stereo a, Stereo b) {
    return static_cast<Stereo>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Interrupt operator|(Interrupt a, Interrupt b) {
    return static_cast<Interrupt>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr u8 operator&(u8 a, Interrupt b) {
    return a & static_cast<u8>(b);
}
__attribute__((always_inline)) constexpr Sysctl1 operator|(Sysctl1 a, Sysctl1 b) {
    return static_cast<Sysctl1>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Iodat operator|(Iodat a, Iodat b) {
    return static_cast<Iodat>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr SerctlW operator|(SerctlW a, SerctlW b) {
    return static_cast<SerctlW>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr u8 operator&(u8 a, SerctlR b) {
    return a & static_cast<u8>(b);
}
__attribute__((always_inline)) constexpr Dispctl operator|(Dispctl a, Dispctl b) {
    return static_cast<Dispctl>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Mtest0 operator|(Mtest0 a, Mtest0 b) {
    return static_cast<Mtest0>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Mtest1 operator|(Mtest1 a, Mtest1 b) {
    return static_cast<Mtest1>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Mtest2 operator|(Mtest2 a, Mtest2 b) {
    return static_cast<Mtest2>(static_cast<u8>(a) | static_cast<u8>(b));
}
__attribute__((always_inline)) constexpr Mapctl operator|(Mapctl a, Mapctl b) {
    return static_cast<Mapctl>(static_cast<u8>(a) | static_cast<u8>(b));
}

/*
===========================================================================
Macros and Defines
===========================================================================
*/

/* Standard Lynx timing values */
#define HBL_RELOAD     0x9E
#define VBL_RELOAD     0x68
#define PKBKUP_VALUE   0x29

/* Timer control combinations */
#define HBL_CONTROL    (TimerCtrlA::ENABLE_COUNT | TimerCtrlA::ENABLE_RELOAD)
#define VBL_CONTROL    (TimerCtrlA::ENABLE_COUNT | TimerCtrlA::ENABLE_RELOAD | ClockSelect::LINKING)

#define MIKEY_START 0xFD00

#define MIKEY (*reinterpret_cast<volatile MikeyChip*>(MIKEY_START))
#define MIKEY_TIMERS (*reinterpret_cast<volatile MikeyAllTimers*>(0xFD00))
#define HBL_TIMER (*reinterpret_cast<volatile MikeyTimer*>(0xFD00))
#define VBL_TIMER (*reinterpret_cast<volatile MikeyTimer*>(0xFD08))
#define UART_TIMER (*reinterpret_cast<volatile MikeyTimer*>(0xFD14))

#endif
