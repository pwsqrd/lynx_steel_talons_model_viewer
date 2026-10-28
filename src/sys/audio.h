#ifndef AUDIO_H
#define AUDIO_H

#include "types.h"

namespace sys::audio {

void Init(void);
void PlaySample(const u8* data, u16 length, u8 sampleRate);
void StopSample(void);
bool IsSamplePlaying(void);
void SampleTick(void);

}

#endif
