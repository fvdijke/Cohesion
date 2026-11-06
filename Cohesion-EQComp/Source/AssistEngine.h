#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>
#include <cmath>

// ===========================================================================
//  ASSIST analysis: compare a measured spectrum to a per-instrument target
//  balance and propose EQ + compressor moves.
// ===========================================================================
namespace assist
{
    static constexpr int kBands = 8;
    static const float   kFreq[kBands] = { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };

    struct Target
    {
        const char* name;
        float rel[kBands];     // desired relative balance (dB, roughly zero-mean)
        float hpf;             // suggested high-pass cleanup
        float ratio, thr, atk, rel_;   // suggested wideband comp
    };

    struct Style { const char* name; float delta[kBands]; float compScale; };

    inline const std::vector<Style>& styles()
    {
        static const std::vector<Style> s = {
            { "Modern / tight", {  0.f, 0.f,-1.f, 0.f, 0.f, 0.f, 1.f, 1.f }, 1.00f },
            { "Vintage warm",   {  1.f, 1.f, 0.f, 0.f, 0.f,-1.f,-2.f,-2.f }, 0.90f },
            { "Natural",        {  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }, 0.80f },
            { "Aggressive",     {  0.f,-1.f,-1.f, 0.f, 1.f, 1.f, 1.f, 0.f }, 1.35f },
            { "Smooth / dark",  {  1.f, 1.f, 1.f, 0.f,-1.f,-2.f,-3.f,-3.f }, 0.90f },
            { "Bright / air",   { -1.f,-1.f, 0.f, 0.f, 0.f, 1.f, 2.f, 3.f }, 1.00f },
            { "Lo-fi",          {  2.f, 0.f, 0.f, 1.f,-1.f,-3.f,-4.f,-5.f }, 1.10f },
            { "Open / airy",    {  0.f, 0.f,-1.f, 0.f, 0.f, 0.f, 1.f, 2.f }, 0.85f },
            { "Punchy",         {  1.f, 0.f,-1.f,-1.f, 0.f, 1.f, 1.f, 0.f }, 1.20f },
            { "Hi-fi master",   {  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f }, 0.75f },
        };
        return s;
    }

    inline const std::vector<Target>& targets()
    {
        static const std::vector<Target> t = {
            { "Bass (electric)", {  3,  2,  0, -1, -2, -3, -4, -5 }, 30.0f, 3.0f, -18.0f, 15.0f, 180.0f },
            { "Vocal",           { -4, -2, -1,  0,  1,  2,  2,  0 }, 90.0f, 2.5f, -16.0f, 8.0f,  120.0f },
            { "Drums",           {  2,  0, -1,  0,  1,  2,  1,  2 }, 40.0f, 4.0f, -14.0f, 1.0f,  100.0f },
            { "Guitar",          { -3, -1,  0,  1,  1,  1,  0, -1 }, 70.0f, 2.5f, -16.0f, 5.0f,  120.0f },
            { "Synth",           {  0,  0,  0,  0,  0,  0,  1,  1 }, 30.0f, 2.0f, -18.0f, 10.0f, 150.0f },
            { "Master",          {  1,  0,  0,  0,  0,  0,  1,  1 }, 20.0f, 2.0f, -20.0f, 30.0f, 200.0f },
        };
        return t;
    }

    struct EqMove
    {
        int   band;            // target EQ band index (0..)
        float freq, gain, q;
        int   type;            // 0 bell
        bool  on = true;
        juce::String text;
    };

    struct Proposal
    {
        std::vector<EqMove> eq;
        bool  hpfOn = true;
        float hpf = 30.0f;
        juce::String hpfText;
        float ratio = 2.0f, thr = -18.0f, atk = 15.0f, rel = 180.0f;
        juce::String compText;
        bool  limOn = true;
        float limCeil = -0.3f;
        float limPush = 6.0f;           // drive into the ceiling (dB) for loudness
        juce::String limText;
        float measured[kBands] { 0 };   // pink-relative balance (dB), for display
        float target[kBands]   { 0 };
        float measAbs[kBands]  { 0 };   // absolute measured level per band (dB)
        float strength = 0.7f;
        bool  valid = false;
    };

    // measBandDb = measured level per band (dB, any offset). instrument indexes targets(), style indexes styles().
    inline Proposal analyze (const float* measBandDb, int instrument, int style, float strength)
    {
        Proposal p;
        const auto& T = targets()[(size_t) juce::jlimit (0, (int) targets().size() - 1, instrument)];
        const auto& S = styles() [(size_t) juce::jlimit (0, (int) styles().size()  - 1, style)];

        // Compare the measured spectrum to the target balance RELATIVE to a pink
        // reference (music naturally falls ~3 dB/oct). This keeps each instrument's
        // and style's tonal character instead of flattening it, while not fighting
        // the natural downward tilt (which used to cut all lows / boost all highs).
        auto zeroMean = [] (float* v)
        {
            float m = 0.0f; for (int i = 0; i < kBands; ++i) m += v[i]; m /= (float) kBands;
            for (int i = 0; i < kBands; ++i) v[i] -= m;
        };
        // measured, pink-compensated (+3 dB/band removes the pink tilt), then zero-mean
        for (int i = 0; i < kBands; ++i) p.measured[i] = measBandDb[i] + 3.0f * (float) i;
        zeroMean (p.measured);
        // target balance = instrument profile + style, zero-mean
        for (int i = 0; i < kBands; ++i) p.target[i] = T.rel[i] + S.delta[i];
        zeroMean (p.target);

        // rank bands by deviation (target - measured)
        int order[kBands];
        for (int i = 0; i < kBands; ++i) order[i] = i;
        std::sort (order, order + kBands, [&] (int a, int b)
                   { return std::abs (p.target[a] - p.measured[a]) > std::abs (p.target[b] - p.measured[b]); });

        const float s = juce::jlimit (0.0f, 1.0f, strength);
        int used = 0;
        for (int k = 0; k < kBands && used < 3; ++k)
        {
            const int i = order[k];
            const float raw = p.target[i] - p.measured[i];
            if (std::abs (raw) < 1.5f) break;
            EqMove m;
            m.band = used;
            m.freq = kFreq[i];
            m.gain = juce::jlimit (-8.0f, 8.0f, raw * 0.9f * s);   // clearly audible
            m.q    = 1.2f;
            m.type = 0;
            m.text = "EQ " + juce::String (m.gain >= 0 ? "+" : "") + juce::String (m.gain, 1)
                   + " dB @ " + (m.freq >= 1000.0f ? juce::String (m.freq / 1000.0f, 1) + " kHz"
                                                   : juce::String ((int) m.freq) + " Hz");
            p.eq.push_back (m);
            ++used;
        }

        p.hpfOn = true; p.hpf = T.hpf;
        p.hpfText = "HPF " + juce::String ((int) T.hpf) + " Hz (sub opruimen)";
        p.ratio = 1.0f + (T.ratio - 1.0f) * S.compScale;                        // style scales the compression
        p.thr   = juce::jlimit (-48.0f, -6.0f, T.thr - (S.compScale - 1.0f) * 8.0f);  // harder styles -> lower threshold
        p.atk   = juce::jlimit (0.5f, 100.0f, T.atk * (2.0f - S.compScale));    // harder styles -> faster attack
        p.rel   = T.rel_;
        p.compText = "Comp " + juce::String (p.ratio, 1) + ":1 - Thr " + juce::String ((int) p.thr) + " dB\n"
                   + "Att " + juce::String ((int) p.atk) + " ms - Rel " + juce::String ((int) p.rel) + " ms";

        // Brick-wall limiter for maximum loudness: higher strength -> harder ceiling + more push.
        p.limOn   = true;
        p.limCeil = juce::jmap (s, 0.0f, 1.0f, -1.5f, -0.2f);
        p.limPush = juce::jmap (s, 0.0f, 1.0f, 3.0f, 12.0f);
        p.limText = "Push +" + juce::String ((int) p.limPush) + " dB -> " + juce::String (p.limCeil, 1) + " dB";

        for (int i = 0; i < kBands; ++i) p.measAbs[i] = measBandDb[i];
        p.strength = s;
        p.valid = true;
        return p;
    }
}
