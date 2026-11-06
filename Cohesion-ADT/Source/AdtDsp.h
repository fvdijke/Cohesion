#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// ===========================================================================
//  Artificial Double Tracking DSP building blocks.
// ===========================================================================
namespace adt
{
    // Simple fractional-delay ring buffer with multiple read taps.
    struct DelayBuf
    {
        std::vector<float> buf;
        int size = 0, w = 0;

        void prepare (int maxSamples) { size = juce::jmax (8, maxSamples); buf.assign ((size_t) size, 0.0f); w = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x) { buf[(size_t) w] = x; w = (w + 1) % size; }

        // delay in samples (>= 0): 0 = most recent sample.
        float read (float d) const
        {
            d = juce::jlimit (0.0f, (float) (size - 2), d);
            const int k0 = (int) std::floor (d);
            const float fr = d - (float) k0;
            auto at = [&] (int k) { return buf[(size_t) (((w - 1 - k) % size + size) % size)]; };
            return at (k0) * (1.0f - fr) + at (k0 + 1) * fr;
        }
    };

    // LFO: sine or smoothed-random, phase 0..1.
    struct Lfo
    {
        double sr = 44100.0, phase = 0.0, inc = 0.0;
        float cur = 0.0f, target = 0.0f;
        int shape = 0;                     // 0 = sine, 1 = random
        juce::Random rng;

        void prepare (double s, float startPhase) { sr = s; phase = startPhase; cur = target = 0.0f; }
        void setRate (float hz) { inc = (double) hz / sr; }
        void setShape (int s)   { shape = s; }

        float next()
        {
            phase += inc;
            if (phase >= 1.0) { phase -= 1.0; target = rng.nextFloat() * 2.0f - 1.0f; }
            if (shape == 0)
                return (float) std::sin (phase * juce::MathConstants<double>::twoPi);
            cur += (target - cur) * 0.0015f;   // smoothed random
            return cur;
        }
    };

    // First-order all-pass for phase rotation.
    struct AllPass1
    {
        float a = 0.0f, z = 0.0f;
        void setCoef (float coef) { a = juce::jlimit (-0.98f, 0.98f, coef); }
        void reset() { z = 0.0f; }
        float process (float x) { const float y = -a * x + z; z = x + a * y; return y; }
    };

    // One-pole low-pass; high-pass = input - low-pass.
    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setFreq (double f, double sr) { a = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, sr * 0.49, f) / sr); }
        void reset() { z = 0.0f; }
        float lp (float in) { z = a * z + (1.0f - a) * in; return z; }
        float hp (float in) { return in - lp (in); }
    };
}
