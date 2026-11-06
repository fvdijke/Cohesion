#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

// ===========================================================================
//  Simple look-ahead brick-wall limiter (stereo-linked). Catches peaks so the
//  signal can be pushed for maximum loudness without clipping.
// ===========================================================================
namespace ceqc
{
class Limiter
{
public:
    void prepare (double fs, int /*numCh*/)
    {
        sr = fs;
        lookahead = juce::jmax (1, (int) (fs * 0.0015));   // 1.5 ms look-ahead
        for (int c = 0; c < 2; ++c) delay[c].assign ((size_t) lookahead, 0.0f);
        writeIdx = 0; env = 1.0f;
        setRelease (relMs);
    }

    void setEnabled (bool e)  { enabled = e; }
    void setCeiling (float dB) { ceil = juce::Decibels::decibelsToGain (dB); }
    void setPush    (float dB) { push = juce::Decibels::decibelsToGain (juce::jmax (0.0f, dB)); }
    void setRelease (float ms) { relMs = juce::jmax (1.0f, ms); relCoef = (float) std::exp (-1.0 / (sr * relMs * 0.001)); }

    void process (float* const* data, int numCh, int numSamples)
    {
        if (! enabled) return;
        const int n = juce::jmin (numCh, 2);
        for (int i = 0; i < numSamples; ++i)
        {
            // drive the signal into the ceiling (push) for loudness
            float peak = 0.0f;
            for (int c = 0; c < n; ++c) peak = juce::jmax (peak, std::abs (data[c][i] * push));

            const float g = (peak > ceil && peak > 1.0e-9f) ? ceil / peak : 1.0f;
            if (g < env) env = g;                          // instant attack
            else         env = g + (env - g) * relCoef;    // smooth release

            for (int c = 0; c < n; ++c)
            {
                const float in  = data[c][i] * push;
                const float out = delay[c][(size_t) writeIdx];
                delay[c][(size_t) writeIdx] = in;
                data[c][i] = juce::jlimit (-ceil, ceil, out * env);
            }
            if (++writeIdx >= lookahead) writeIdx = 0;
        }
    }

private:
    double sr = 44100.0;
    int lookahead = 64, writeIdx = 0;
    float ceil = 0.966f, push = 1.0f, env = 1.0f, relCoef = 0.999f, relMs = 60.0f;
    bool enabled = false;
    std::vector<float> delay[2];
};
}
