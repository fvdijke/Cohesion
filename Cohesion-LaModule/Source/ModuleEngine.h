#pragma once

#include "MtMemory.h"

// Per-part output buffers handed to an engine for one block.
struct PartBuffers
{
    float* dryL[mt::kParts] {}; float* dryR[mt::kParts] {};
    // own engine: reverb *input* per part (pre-reverb send); Munt: reverb *wet* per part
    float* fxL[mt::kParts] {};  float* fxR[mt::kParts] {};
};

// A sound engine for the nine MT-32 parts. All calls come from the audio thread.
struct ModuleEngine
{
    virtual ~ModuleEngine() = default;
    virtual bool fxIsReverbWet() const = 0;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void allSoundOff() = 0;
    // a channel message for `part` (status byte's channel is ignored); offset in samples
    virtual void message (int part, const uint8_t* msg, int len, int offset) = 0;
    // an MT-32 SysEx already applied to the shared memory
    virtual void sysex (const uint8_t* data, int len, int offset) = 0;
    virtual void render (const PartBuffers& out, int n) = 0;
    virtual int activePartials() const = 0;
};
