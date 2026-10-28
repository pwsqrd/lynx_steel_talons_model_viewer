#include "audio.h"
#include "mikey.h"

static const u8* g_sampleData = 0;
static u16       g_sampleLength = 0;
static u16       g_samplePos = 0;
static u8        g_sampleSkip = 0;
static u8        g_sampleSkipCounter = 0;

namespace sys::audio {

/*
=================
Init

Initialize audio subsystem for sample playback.
=================
*/
void Init(void)
{
    MIKEY.mstereo = 0xFF;
    MIKEY.attend = 0xFF;
    MIKEY.channel_d.volume = 0;
    MIKEY.channel_d.control = 0;
    MIKEY.channel_d.dac = 0x80;
}

/*
=================
PlaySample

Starts playing an 8-bit unsigned PCM sample.
sampleRate: 1=fast, higher=slower playback
=================
*/
void PlaySample(const u8* data, u16 length, u8 sampleRate)
{
    g_sampleData = data;
    g_sampleLength = length;
    g_samplePos = 0;
    g_sampleSkip = sampleRate;
    g_sampleSkipCounter = 0;

    MIKEY.channel_d.volume = 0x7F;
    MIKEY.channel_d.control = 0x00;
}

/*
=================
StopSample

Stops sample playback.
=================
*/
void StopSample(void)
{
    g_sampleData = 0;
    g_sampleLength = 0;
    g_samplePos = 0;

    MIKEY.channel_d.volume = 0;
    MIKEY.channel_d.dac = 0x80;
}

/*
=================
IsSamplePlaying

Returns true if a sample is currently playing.
=================
*/
bool IsSamplePlaying(void)
{
    return g_samplePos < g_sampleLength;
}

/*
=================
SampleTick

Call from main loop to advance sample playback.
=================
*/
void SampleTick(void)
{
    if (g_sampleData == 0 || g_samplePos >= g_sampleLength) {
        return;
    }

    for (u8 i = 0; i < 128 && g_samplePos < g_sampleLength; i++) {
        g_sampleSkipCounter++;
        if (g_sampleSkipCounter >= g_sampleSkip) {
            g_sampleSkipCounter = 0;
            MIKEY.channel_d.dac = g_sampleData[g_samplePos++];
        }
    }

    if (g_samplePos >= g_sampleLength) {
        StopSample();
    }
}

}
