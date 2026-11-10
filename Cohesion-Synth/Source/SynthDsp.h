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

    struct Snapshot
    {
        OscP osc[4];
        bool  subOn = false;   int subWave = 0, subOct = -1;  float subLevel = 0.5f, subPan = 0;
        bool  noiseOn = false; int noiseType = 0;             float noiseLevel = 0.3f, noisePan = 0, noiseTilt = 0;
        float fmAmt = 0, ringAmt = 0;
        int   fltType = 0;     float fltCutoff = 12000, fltRes = 0.1f, fltDrive = 0, fltKeyTrk = 0, fltEnvAmt = 0;
        float ampA = 5, ampD = 400, ampS = 0.8f, ampR = 250;
        float fenA = 5, fenD = 300, fenS = 0.4f, fenR = 250;
        float mainVol = 0.8f;
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

        float next (int wave, double pm = 0.0)
        {
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
                    v += polyBlep (t, inc);
                    double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
                    v -= polyBlep (t2, inc);
                    break;
                }
                case 2: // triangle (integrated square)
                {
                    float sq = t < 0.5 ? 1.0f : -1.0f;
                    sq += polyBlep (t, inc);
                    double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
                    sq -= polyBlep (t2, inc);
                    triState += (float) (4.0 * inc) * (sq - triState * 0.0f); // leaky integrate
                    triState = triState * 0.9995f + (float) (4.0 * inc) * sq;
                    v = juce::jlimit (-1.0f, 1.0f, triState);
                    break;
                }
                default: // saw
                    v = (float) (2.0 * t - 1.0);
                    v -= polyBlep (t, inc);
                    break;
            }
            phase += inc; if (phase >= 1.0) phase -= 1.0;
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

        // fills stereo accumulators (scaled by gain). pm = phase-mod input.
        // lastOut keeps the pre-gain mono signal (used as an FM/ring source).
        void render (int wave, double pm, float gain, float& outL, float& outR)
        {
            const float norm = 1.0f / std::sqrt ((float) n);
            float sumMono = 0.0f;
            for (int k = 0; k < n; ++k)
            {
                float s = cores[(size_t) k].next (wave, pm);
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
        float next (int wave) { return core.next (wave == 0 ? 3 : (wave == 1 ? 2 : 1)); }
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
}
