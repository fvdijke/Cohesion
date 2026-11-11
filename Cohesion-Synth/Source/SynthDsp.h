#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <array>

// ===========================================================================
//  COHESION - SYNTH  DSP engine (foundation).
//  Band-limited (PolyBLEP) virtual-analog oscillators with unison, a sub and
//  noise source, FM / ring routing, a ZDF 4-pole ladder filter and ADSR amp &
//  filter envelopes. Polyphonic via juce::Synthesiser.
// ===========================================================================
namespace syn
{
    // ---- plain per-block parameter snapshot (filled from the APVTS) ----
    struct OscP
    {
        bool  on = false;  int mode = 0, wave = 0, oct = 0, semi = 0, unison = 1;
        float fine = 0, wtpos = 0, detune = 0, pan = 0, level = 0, phase = 0;
    };

    struct EnvP   { float a = 5, d = 200, s = 0.7f, r = 200; };
    struct LfoP   { int wave = 0; float rateHz = 2; bool sync = false; int div = 4;
                    float fade = 0, phase = 0, smooth = 0; bool mono = false; };
    struct ModSlot{ int src = 0, dst = 0; float amt = 0; };

    // mod sources:  0 Off,1 LFO1,2 LFO2,3 LFO3,4 LFO4,5 ENV1,6 ENV2,7 ENV3,8 ENV4,9 Vel,10 ModWheel,11 KeyTrack
    // mod dests:    0 Off,1 F1 Cutoff,2 F2 Cutoff,3 Pitch,4 Amp,5 Pan,6 LFO1 Rate
    enum { kNumMod = 8 };

    struct Snapshot
    {
        OscP osc[4];
        bool  subOn = false;   int subWave = 0, subOct = -1;  float subLevel = 0.5f, subPan = 0;
        bool  noiseOn = false; int noiseType = 0;             float noiseLevel = 0.3f, noisePan = 0, noiseTilt = 0;
        float fmAmt = 0, ringAmt = 0;

        int   fltType = 0;    float fltCutoff = 12000, fltRes = 0.1f, fltDrive = 0, fltKeyTrk = 0, fltEnvAmt = 0;
        int   flt1EnvSrc = 1;
        int   flt2Type = 0;   float flt2Cutoff = 20000, flt2Res = 0.1f, flt2Drive = 0, flt2KeyTrk = 0, flt2EnvAmt = 0;
        int   flt2Slope = 1,  flt2EnvSrc = 1;
        int   routing = 0;    float routeBalance = 0.5f;  // 0 = F1 only ... 1 = F2 only (parallel)

        EnvP  env[4];
        LfoP  lfo[4];
        ModSlot mod[kNumMod];
        float mainVol = 0.8f;
        float glideMs = 0.0f;   // portamento (arp SLIDE)
    };

    inline float polyBlep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0f;
        if (t < dt)          { t /= dt;             return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt)    { t = (t - 1.0) / dt;  return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    // ---- one band-limited oscillator core ----
    struct Core
    {
        double phase = 0.0, inc = 0.0;
        float  triState = 0.0f;

        void setInc (double i)         { inc = i; }
        void setPhase01 (double p)     { phase = p - std::floor (p); }

        float next (int wave, double pm = 0.0, double mult = 1.0)
        {
            const double step = inc * mult;
            double t = phase + pm;
            t -= std::floor (t);

            float v = 0.0f;
            switch (wave)
            {
                case 3: // sine
                    v = (float) std::sin (juce::MathConstants<double>::twoPi * t);
                    break;
                case 1: // square
                {
                    v = t < 0.5 ? 1.0f : -1.0f;
                    v += polyBlep (t, step);
                    double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
                    v -= polyBlep (t2, step);
                    break;
                }
                case 2: // triangle (integrated square)
                {
                    float sq = t < 0.5 ? 1.0f : -1.0f;
                    sq += polyBlep (t, step);
                    double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
                    sq -= polyBlep (t2, step);
                    triState = triState * 0.9995f + (float) (4.0 * step) * sq;
                    v = juce::jlimit (-1.0f, 1.0f, triState);
                    break;
                }
                default: // saw
                    v = (float) (2.0 * t - 1.0);
                    v -= polyBlep (t, step);
                    break;
            }
            phase += step; if (phase >= 1.0) phase -= 1.0;
            return v;
        }
    };

    // ---- oscillator with unison ----
    struct Osc
    {
        std::array<Core, 7> cores;
        std::array<float, 7> gL { }, gR { };
        int n = 1;
        float lastOut = 0.0f;

        void startNote (double freqHz, double sr, const OscP& p, juce::Random& rng)
        {
            n = juce::jlimit (1, 7, p.unison);
            configure (freqHz, sr, p);
            for (int k = 0; k < n; ++k)
            {
                if (p.phase <= 0.0f) cores[(size_t) k].setPhase01 (rng.nextDouble());
                else                 cores[(size_t) k].setPhase01 (p.phase / 360.0 + 0.001 * k);
            }
            lastOut = 0.0f;
        }

        void configure (double freqHz, double sr, const OscP& p)
        {
            n = juce::jlimit (1, 7, p.unison);
            const double det = p.detune / 100.0; // cents fraction of a semitone span
            for (int k = 0; k < n; ++k)
            {
                float spread = (n == 1) ? 0.0f : (2.0f * (float) k / (float) (n - 1) - 1.0f); // -1..1
                double cents = spread * det * 50.0;                 // up to +/-50 cents at det=1
                double mult  = std::pow (2.0, cents / 1200.0);
                cores[(size_t) k].setInc (freqHz * mult / sr);

                float panPos = juce::jlimit (-1.0f, 1.0f, p.pan + spread * 0.5f * (n > 1 ? 1.0f : 0.0f));
                float a = (panPos * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
                gL[(size_t) k] = std::cos (a);
                gR[(size_t) k] = std::sin (a);
            }
        }

        // fills stereo accumulators (scaled by gain). pm = phase-mod input,
        // mult = per-sample frequency multiplier (pitch modulation).
        // lastOut keeps the pre-gain mono signal (used as an FM/ring source).
        void render (int wave, double pm, float gain, double mult, float& outL, float& outR)
        {
            const float norm = 1.0f / std::sqrt ((float) n);
            float sumMono = 0.0f;
            for (int k = 0; k < n; ++k)
            {
                float s = cores[(size_t) k].next (wave, pm, mult);
                outL += s * gL[(size_t) k] * norm * gain;
                outR += s * gR[(size_t) k] * norm * gain;
                sumMono += s;
            }
            lastOut = sumMono * norm;
        }
    };

    // ---- sub oscillator ----
    struct Sub
    {
        Core core;
        void startNote (double f, double sr) { core.setInc (f / sr); core.setPhase01 (0.0); }
        void setFreq  (double f, double sr)  { core.setInc (f / sr); }
        float next (int wave, double mult = 1.0) { return core.next (wave == 0 ? 3 : (wave == 1 ? 2 : 1), 0.0, mult); }
    };

    // ---- noise (white / pink) ----
    struct Noise
    {
        juce::Random rng;
        float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
        float next (int type)
        {
            float w = rng.nextFloat() * 2.0f - 1.0f;
            if (type == 0) return w;
            // Paul Kellet pink filter
            b0 = 0.99886f*b0 + w*0.0555179f; b1 = 0.99332f*b1 + w*0.0750759f;
            b2 = 0.96900f*b2 + w*0.1538520f; b3 = 0.86650f*b3 + w*0.3104856f;
            b4 = 0.55000f*b4 + w*0.5329522f; b5 = -0.7616f*b5 - w*0.0168980f;
            float pink = b0+b1+b2+b3+b4+b5+b6+w*0.5362f;
            b6 = w*0.115926f;
            return pink * 0.11f;
        }
    };

    // ---- ZDF 4-pole ladder filter (one channel) ----
    struct Ladder
    {
        double sr = 44100.0;
        float z0=0,z1=0,z2=0,z3=0, G=0.5f, k=0.0f, drive=1.0f;
        int mode = 0;

        void setSR (double s)  { sr = s; }
        void reset()           { z0=z1=z2=z3=0.0f; }

        void setParams (float cutoffHz, float res, float driveAmt, int m)
        {
            cutoffHz = juce::jlimit (20.0f, (float) (sr * 0.45), cutoffHz);
            float g = std::tan (juce::MathConstants<float>::pi * cutoffHz / (float) sr);
            G = g / (1.0f + g);
            k = juce::jlimit (0.0f, 4.0f, res * 4.0f);
            drive = 1.0f + driveAmt * 3.0f;
            mode = m;
        }

        inline float onepole (float x, float& s)
        {
            float v = (x - s) * G;
            float y = v + s;
            s = y + v;
            return y;
        }

        float process (float x)
        {
            x *= drive;
            float in = std::tanh (x - k * z3);
            float y0 = onepole (in, z0);
            float y1 = onepole (y0, z1);
            float y2 = onepole (y1, z2);
            float y3 = onepole (y2, z3);
            float out;
            if      (mode == 0) out = y3;        // LP24
            else if (mode == 1) out = y1;        // LP12
            else                out = in - y3;   // HP (approx)
            return out / drive;
        }
    };

    // ---- TPT state-variable filter, multimode (Cytomic). One channel. ----
    struct SVF
    {
        double sr = 44100.0;
        float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, drive = 1;
        int mode = 0;   // 0 LP, 1 HP, 2 BP, 3 Notch
        int slope = 1;  // 0 = 12 dB, 1 = 24 dB (cascaded twice)
        float ic1b = 0, ic2b = 0;

        void setSR (double s) { sr = s; }
        void reset()          { ic1 = ic2 = ic1b = ic2b = 0.0f; }

        void setParams (float cut, float res, float driveAmt, int m, int sl)
        {
            cut = juce::jlimit (20.0f, (float) (sr * 0.45), cut);
            g  = std::tan (juce::MathConstants<float>::pi * cut / (float) sr);
            k  = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, res);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
            mode = m; slope = sl; drive = 1.0f + driveAmt * 3.0f;
        }

        inline float stage (float v0, float& s1, float& s2)
        {
            float v3 = v0 - s2;
            float v1 = a1 * s1 + a2 * v3;
            float v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            float lp = v2, bp = v1, hp = v0 - k * v1 - v2;
            switch (mode) { case 1: return hp; case 2: return bp; case 3: return v0 - k * v1; default: return lp; }
        }

        float process (float x)
        {
            x *= drive;
            float y = stage (x, ic1, ic2);
            if (slope == 1) y = stage (y, ic1b, ic2b);
            return y / drive;
        }
    };

    // ---- LFO generator (per voice) ----
    struct LfoGen
    {
        double phase = 0.0, inc = 0.0;
        float sm = 0.0f, sh = 0.0f;
        juce::Random rng;

        void setRate (float hz, double sr) { inc = (double) hz / sr; }
        void reset (double ph)             { phase = ph - std::floor (ph); sm = 0.0f; sh = rng.nextFloat() * 2.0f - 1.0f; }

        float next (int wave, float smooth)
        {
            double t = phase;
            float v;
            switch (wave)
            {
                case 1:  v = (float) (4.0 * std::abs (t - 0.5) - 1.0); break;   // triangle
                case 2:  v = (float) (2.0 * t - 1.0);                  break;   // saw
                case 3:  v = t < 0.5 ? 1.0f : -1.0f;                   break;   // square
                case 4:  v = sh;                                       break;   // sample & hold
                default: v = (float) std::sin (juce::MathConstants<double>::twoPi * t); break;
            }
            phase += inc;
            if (phase >= 1.0) { phase -= 1.0; sh = rng.nextFloat() * 2.0f - 1.0f; }
            const float coef = 1.0f - smooth * 0.985f;
            sm += (v - sm) * coef;
            return smooth > 0.001f ? sm : v;
        }
    };
}
