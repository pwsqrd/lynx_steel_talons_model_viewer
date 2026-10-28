#ifndef CHIPPER_H
#define CHIPPER_H

#include "../types.h"

#define CHIPPER_MAX_INSTRUMENTS 64

// Bodies in sys/chipper.S; the asm labels bind to its symbols.
namespace sys::chipper {

void Init(void) __asm__("chipper_Init");
void Tick(void) __asm__("chipper_Tick");

void Play(u8 channel, const u8* musicData) __asm__("chipper_Play");
void Stop(u8 channel) __asm__("chipper_Stop");
void StopAll(void) __asm__("chipper_StopAll");
void Pause(void) __asm__("chipper_Pause");
void Continue(void) __asm__("chipper_Continue");

bool IsPlaying(u8 channel) __asm__("chipper_IsPlaying");
u8   GetActiveMask(void) __asm__("chipper_GetActiveMask");

}

#endif
