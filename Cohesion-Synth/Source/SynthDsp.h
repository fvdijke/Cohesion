#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <array>
#include <complex>

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
    // mod dests:    0 Off,1 F1 Cutoff,2 F2 Cutoff,3 Pitch,4 Amp,5 Pan,6 LFO1 Rate,7 WT Pos
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
        bool  dxOn = false; float dxLevel = 0.8f, dxBright = 0.0f, dxTime = 1.0f, dxVel = 1.0f;
        bool  laOn = false; float laLevel = 0.8f, laCutoff = 0.0f, laTime = 1.0f;
        bool  vaFiltMod = true;                        // VA part through its filters + mod matrix (off: dry)
        double bpm = 120.0;                            // for tempo-synced LFOs
        int   voiceMode = 0;                           // 0 poly, 1 mono, 2 legato
        float velSens = 0.7f;                          // 0 = velocity ignored, 1 = full range
        const float* monoLfo[4] { nullptr, nullptr, nullptr, nullptr };   // shared (MONO) LFOs, this block

        // MIXER: one part per engine (0 VA, 1 FM, 2 LA)
        struct PartP
        {
            float gain = 1.0f, pan = 0.0f, sendEcho = 1.0f, sendReverb = 1.0f;
            bool  mute = false, solo = false, arp = true, viaFilter = false;
            int   keyLo = 0, keyHi = 127;
        };
        PartP part[3];
        bool  arpOn = false;   // notes on MIDI channel 2 come from the ARP, channel 1 = played
    };

    // LFO rate in Hz: free, or synced to the host tempo (1/1 ... 1/32 note per cycle)
    inline float lfoHz (const LfoP& l, double bpm)
    {
        if (! l.sync) return l.rateHz;
        static const float den[6] = { 1, 2, 4, 8, 16, 32 };
        return (float) (bpm / 60.0) * den[juce::jlimit (0, 5, l.div)] / 4.0f;
    }

    inline float polyBlep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0f;
        if (t < dt)          { t /= dt;             return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt)    { t = (t - 1.0) / dt;  return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    // ---- built-in wavetables (WT mode) ----
    // 12 banks of 8 frames each; WT POS morphs through the frames. Every frame exists as 10
    // band-limited mip levels (1024 ... 2 harmonics), built once at start-up: each frame is a
    // harmonic spectrum (given directly, or measured by FFT from a 4x oversampled cycle) and
    // every mip level is that spectrum cut off at its highest harmonic, inverse-FFT'd.
    //   0 ANALOG   sine -> triangle -> saw -> square
    //   1 PWM      pulse width 50 % -> 4 %
    //   2 HARMONIC a resonant peak sweeping up the harmonics
    //   3 VOCAL    saw through vowel formants a - e - i - o - u
    //   4 SYNC     hard-synced saw, slave ratio 1 -> 8
    //   5 FM       sine phase-modulated by its 2nd harmonic, index 0 -> 6
    //   6 FOLD     sine through a wavefolder, gain 1 -> 8
    //   7 ORGAN    drawbar registrations, flute 8' -> full organ
    //   8 ODD/EVEN saw -> square (odd only) -> even-heavy (hollow octave)
    //   9 COMB     saw through a sweeping comb filter (flanged)
    //  10 CHIP     8-bit: pulse 50 / 25 / 12.5 %, 4-bit triangle and saw, stepped sines
    //  11 DIGITAL  fixed random spectra (PPG-like, every frame different)
    //  (the bank names live in Params.h: pid::wtBankNames)
    struct Wavetables
    {
        static constexpr int N = 2048, F = 8, M = 10, B = 12, H = 1024;
        static constexpr double kRms = 0.6;   // frame loudness (a sine peaks at 0.85, a saw at ~1.05)
        std::vector<float> data;

        const float* table (int b, int f, int m) const
        {
            b = juce::jlimit (0, B - 1, b);
            return data.data() + (((size_t) b * F + (size_t) f) * M + (size_t) m) * (N + 1);
        }

        using Cx = std::complex<double>;

        // in-place radix-2 FFT; sign -1 forward, +1 inverse (unscaled)
        static void fft (std::vector<Cx>& a, int sign)
        {
            const size_t n = a.size();
            for (size_t i = 1, j = 0; i < n; ++i)
            {
                size_t bit = n >> 1;
                for (; j & bit; bit >>= 1) j ^= bit;
                j ^= bit;
                if (i < j) std::swap (a[i], a[j]);
            }
            for (size_t len = 2; len <= n; len <<= 1)
            {
                const double ang = sign * juce::MathConstants<double>::twoPi / (double) len;
                const Cx wl (std::cos (ang), std::sin (ang));
                for (size_t i = 0; i < n; i += len)
                {
                    Cx w (1.0, 0.0);
                    for (size_t k = 0; k < len / 2; ++k)
                    {
                        const Cx u = a[i + k], v = a[i + k + len / 2] * w;
                        a[i + k] = u + v; a[i + k + len / 2] = u - v;
                        w *= wl;
                    }
                }
            }
        }

        Wavetables()
        {
            data.assign ((size_t) B * F * M * (N + 1), 0.0f);
            // spec[h] = c_h with  x(t) = sum Re (c_h e^{i 2 pi h t})  (c_h = cos amp - i * sin amp)
            std::vector<Cx> spec ((size_t) H + 1), buf ((size_t) N), cyc ((size_t) N * 4);
            std::vector<float> amp ((size_t) H + 1);
            for (int b = 0; b < B; ++b)
                for (int f = 0; f < F; ++f)
                {
                    const float x = (float) f / (F - 1);
                    std::fill (spec.begin(), spec.end(), Cx());
                    if (isTimeDomain (b))
                    {
                        const size_t L = cyc.size();
                        for (size_t i = 0; i < L; ++i) cyc[i] = Cx (cycle (b, f, x, (double) i / (double) L), 0.0);
                        fft (cyc, -1);
                        for (int h = 1; h <= H; ++h) spec[(size_t) h] = cyc[(size_t) h] * (2.0 / (double) L);
                    }
                    else
                    {
                        spectrum (b, x, amp);
                        for (int h = 1; h <= H; ++h) spec[(size_t) h] = Cx (0.0, -(double) amp[(size_t) h]);
                    }

                    float scale = 0.0f;
                    for (int m = 0; m < M; ++m)
                    {
                        const int hMax = H >> m;
                        std::fill (buf.begin(), buf.end(), Cx());
                        for (int h = 1; h <= hMax && h < N / 2; ++h) buf[(size_t) h] = spec[(size_t) h];
                        fft (buf, +1);
                        if (m == 0)   // one gain per frame (all mips alike): equal RMS, peak at most 2
                        {
                            double pk = 1.0e-9, sq = 0.0;
                            for (auto& c : buf) { pk = juce::jmax (pk, std::abs (c.real())); sq += c.real() * c.real(); }
                            const double rmsV = std::sqrt (sq / N) + 1.0e-9;
                            scale = (float) juce::jmin (kRms / rmsV, 2.0 / pk);
                        }
                        float* t = const_cast<float*> (table (b, f, m));
                        for (int i = 0; i < N; ++i) t[i] = (float) buf[(size_t) i].real() * scale;
                        t[N] = t[0];
                    }
                }
        }

        static bool isTimeDomain (int bank) { return bank == 4 || bank == 5 || bank == 6 || bank == 10; }

        // one cycle of a time-domain bank at phase p (0..1), frame f, position x
        static double cycle (int bank, int f, float x, double p)
        {
            const double tp = juce::MathConstants<double>::twoPi;
            switch (bank)
            {
                case 4:   // hard sync: saw slave reset by the master every cycle
                {
                    const double r = 1.0 + x * 7.0; const double s = p * r;
                    return 2.0 * (s - std::floor (s)) - 1.0;
                }
                case 5:   // FM: carrier 1, modulator 2
                    return std::sin (tp * p + x * 6.0 * std::sin (tp * 2.0 * p));
                case 6:   // wavefolder
                {
                    double v = std::sin (tp * p) * (1.0 + x * 7.0);
                    for (int k = 0; k < 16 && std::abs (v) > 1.0; ++k) v = v > 1.0 ? 2.0 - v : -2.0 - v;
                    return v;
                }
                default:  // chip
                {
                    auto q = [] (double v, double steps) { return std::round (v * steps) / steps; };
                    const double tri = 1.0 - 4.0 * std::abs (p - 0.5), saw = 2.0 * p - 1.0, sn = std::sin (tp * p);
                    switch (f)
                    {
                        case 0:  return p < 0.5   ? 1.0 : -1.0;
                        case 1:  return p < 0.25  ? 1.0 : -1.0;
                        case 2:  return p < 0.125 ? 1.0 : -1.0;
                        case 3:  return q (tri, 7.5);
                        case 4:  return q (saw, 7.5);
                        case 5:  return q (sn, 4.0);
                        case 6:  return q (sn, 2.0);
                        default: return q (tri, 1.5);
                    }
                }
            }
        }

        // sine amplitudes per harmonic for the spectral banks
        static void spectrum (int bank, float x, std::vector<float>& a)
        {
            std::fill (a.begin(), a.end(), 0.0f);
            const float pi = juce::MathConstants<float>::pi;
            // organ registrations: drawbars at harmonics 1 2 3 4 5 6 8 (8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1')
            static const int bars[8][7] = { { 8,0,0,0,0,0,0 }, { 8,4,0,0,0,0,0 }, { 8,6,4,0,0,0,0 }, { 8,8,8,0,0,0,0 },
                                            { 8,8,8,6,0,0,0 }, { 8,8,8,8,4,4,0 }, { 8,8,8,8,8,8,6 }, { 8,8,8,8,8,8,8 } };
            static const int barH[7] = { 1, 2, 3, 4, 5, 6, 8 };
            if (bank == 7)
            {
                const int r = juce::jlimit (0, 7, (int) std::lround (x * 7.0f));
                for (int k = 0; k < 7; ++k)
                    if (bars[r][k] > 0) a[(size_t) barH[k]] = std::pow (10.0f, (float) (bars[r][k] - 8) * 3.0f / 20.0f);
                return;
            }
            if (bank == 11)
            {
                uint32_t s = 0x9e3779b9u * (uint32_t) (1 + (int) std::lround (x * 7.0f));
                auto rnd = [&s] { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.0f; };
                const int top = 8 + (int) (rnd() * 56.0f);      // band edge 8 .. 64 harmonics
                for (int h = 1; h <= top; ++h)
                {
                    const float r = rnd();
                    a[(size_t) h] = (r * r) * (rnd() < 0.5f ? -1.0f : 1.0f) / std::pow ((float) h, 0.6f);
                }
                a[1] = juce::jmax (a[1], 0.5f);
                return;
            }
            for (int h = 1; h <= H; ++h)
            {
                const float fh = (float) h;
                float v = 0.0f;
                switch (bank)
                {
                    case 8:   // odd / even
                    {
                        const float saw = 2.0f / (pi * fh);
                        const float ev = x < 0.5f ? 1.0f - 2.0f * x : 2.0f * x - 1.0f;
                        const float od = (h == 1 || x < 0.5f) ? 1.0f : 2.0f - 2.0f * x;
                        v = saw * ((h & 1) ? od : ev);
                        break;
                    }
                    case 9:   // comb-filtered saw
                    {
                        const float d = 0.02f + x * 0.23f;
                        v = 2.0f / (pi * fh) * 0.5f * (1.0f + std::cos (2.0f * pi * fh * d));
                        break;
                    }
                    case 0:
                    {
                        const float sine = h == 1 ? 1.0f : 0.0f;
                        const float tri  = (h & 1) ? ((h / 2) % 2 ? -1.0f : 1.0f) * 8.0f / (pi * pi * fh * fh) : 0.0f;
                        const float saw  = 2.0f / (pi * fh) * ((h & 1) ? 1.0f : -1.0f);
                        const float sq   = (h & 1) ? 4.0f / (pi * fh) : 0.0f;
                        const float p = x * 3.0f; const int seg = juce::jmin (2, (int) p); const float t = p - (float) seg;
                        const float k[4] = { sine, tri, saw, sq };
                        v = k[seg] + (k[seg + 1] - k[seg]) * t;
                        break;
                    }
                    case 1:
                    {
                        const float w = 0.5f - x * 0.46f;
                        v = 2.0f / (pi * fh) * std::sin (pi * fh * w);
                        break;
                    }
                    case 2:
                    {
                        const float c = 1.0f + x * 30.0f;                       // peak at harmonic 1 .. 31
                        v = 0.35f / fh + std::exp (-0.5f * (fh - c) * (fh - c) / 4.0f);
                        break;
                    }
                    default:
                    {
                        // vowel formants (F1, F2) in harmonics of a ~130 Hz voice
                        static const float F1[5] = { 730, 530, 270, 570, 300 }, F2[5] = { 1090, 1840, 2290, 840, 870 };
                        const float p = x * 4.0f; const int seg = juce::jmin (3, (int) p); const float t = p - (float) seg;
                        const float f1 = (F1[seg] + (F1[seg + 1] - F1[seg]) * t) / 130.0f, f2 = (F2[seg] + (F2[seg + 1] - F2[seg]) * t) / 130.0f;
                        auto res = [] (float hh, float c, float bw) { return 1.0f / (1.0f + (hh - c) * (hh - c) / (bw * bw)); };
                        v = (1.0f / fh) * (0.15f + res (fh, f1, 0.9f) + 0.7f * res (fh, f2, 1.4f));
                        break;
                    }
                }
                a[(size_t) h] = v;
            }
        }
    };
    inline const Wavetables& wavetables() { static const Wavetables w; return w; }

    // ---- one band-limited oscillator core ----
    struct Core
    {
        double phase = 0.0, inc = 0.0;
        float  triState = 0.0f;

        void setInc (double i)         { inc = i; }
        // the triangle integrator starts on the waveform itself (-1 at phase 0, +1 at 0.5):
        // no DC offset that has to leak away after the note starts
        void setPhase01 (double p)
        {
            phase = p - std::floor (p);
            triState = (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
        }

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
                    triState = triState * 0.99995f + (float) (4.0 * step) * sq;   // tiny leak: only against drift
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

        // wavetable read: bank `b`, position 0..1 (morph between frames), mip level by pitch
        float nextWT (int b, float pos, double pm, double mult)
        {
            const auto& wt = wavetables();
            const double step = inc * mult;
            // highest harmonic that stays below ~0.45 fs -> mip level (each level halves the harmonics)
            const double maxH = 0.45 / juce::jmax (1.0e-9, step);
            int m = 0; while (m < Wavetables::M - 1 && (double) (Wavetables::H >> m) > maxH) ++m;
            const float fp = juce::jlimit (0.0f, 1.0f, pos) * (Wavetables::F - 1);
            const int f0 = juce::jmin (Wavetables::F - 2, (int) fp); const float ft = fp - (float) f0;
            const float* ta = wt.table (b, f0, m);
            const float* tb = wt.table (b, f0 + 1, m);
            double t = phase + pm; t -= std::floor (t);
            const double x = t * Wavetables::N; const int i = (int) x; const float fr = (float) (x - i);
            const float va = ta[i] + (ta[i + 1] - ta[i]) * fr;
            const float vb = tb[i] + (tb[i + 1] - tb[i]) * fr;
            phase += step; if (phase >= 1.0) phase -= 1.0;
            return va + (vb - va) * ft;
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
        // wtPos < 0: VA waveform `wave`; wtPos >= 0: wavetable bank `wave` at that position
        void render (int wave, double pm, float gain, double mult, float& outL, float& outR, float wtPos = -1.0f)
        {
            const float norm = 1.0f / std::sqrt ((float) n);
            float sumMono = 0.0f;
            for (int k = 0; k < n; ++k)
            {
                float s = wtPos < 0.0f ? cores[(size_t) k].next (wave, pm, mult)
                                       : cores[(size_t) k].nextWT (wave, wtPos, pm, mult);
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
        // starts at zero crossing: sine at phase 0, triangle (wave 1) at phase 1/4
        void startNote (double f, double sr, int wave = 0) { core.setInc (f / sr); core.setPhase01 (wave == 1 ? 0.25 : 0.0); }
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
    // Four TPT one-poles; the feedback is solved without a unit delay (Zavalishin): the linear
    // estimate of this sample's output sets the input of the tanh stage. The resonance stays on
    // the cutoff at every frequency and self-oscillates at RES 100 % (k = 4).
    struct Ladder
    {
        double sr = 44100.0;
        float z0=0,z1=0,z2=0,z3=0, G=0.5f, k=0.0f, drive=1.0f, G4 = 0.0625f, fbDen = 1.0f;
        int mode = 0;

        void setSR (double s)  { sr = s; }
        void reset()           { z0=z1=z2=z3=0.0f; }

        void setParams (float cutoffHz, float res, float driveAmt, int m)
        {
            cutoffHz = juce::jlimit (20.0f, (float) (sr * 0.45), cutoffHz);
            float g = std::tan (juce::MathConstants<float>::pi * cutoffHz / (float) sr);
            G = g / (1.0f + g);
            k = juce::jlimit (0.0f, 4.0f, res * 4.0f);
            G4 = G * G * G * G;
            fbDen = 1.0f / (1.0f + k * G4);
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

        // same cutoff / resonance for the other channel without recomputing tan()
        void copyCoefs (const Ladder& o) { G = o.G; k = o.k; drive = o.drive; mode = o.mode; G4 = o.G4; fbDen = o.fbDen; }

        float process (float x)
        {
            x *= drive;
            // one-pole: y = G x + (1 - G) s, so the cascade gives y3 = G^4 u + S
            const float h = 1.0f - G;
            const float S = G * G * G * h * z0 + G * G * h * z1 + G * h * z2 + h * z3;
            const float y3est = (G4 * x + S) * fbDen;          // u = x - k y3, solved linearly
            float in = std::tanh (x - k * y3est);
            float y0 = onepole (in, z0);
            float y1 = onepole (y0, z1);
            float y2 = onepole (y1, z2);
            float y3 = onepole (y2, z3);
            float out;
            // the feedback lowers the pass band by 1 / (1 + k); give half of it back so a
            // resonant low-pass keeps its bass (as on most analog ladders with makeup)
            if      (mode == 0) out = y3 * (1.0f + 0.5f * k);                          // LP 24
            else if (mode == 1) out = y1 * (1.0f + 0.25f * k);                         // LP 12
            else                out = in - 4.0f * y0 + 6.0f * y1 - 4.0f * y2 + y3;     // HP 24 (4-pole binomial mix)
            return out / drive;
        }
    };

    // ---- envelope with analog (RC) curves ----
    // Attack charges towards 1.3 and stops at 1 (the classic convex analog attack); decay and
    // release fall exponentially, reaching -40 dB of the distance after the set time.
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        Stage stage = Idle;
        float level = 0.0f, sus = 0.7f, aC = 0, dC = 0, rC = 0, sC = 0.005f;
        double sr = 44100.0;

        void setSampleRate (double s) { sr = s; sC = 1.0f - std::exp (-1.0f / (float) (s * 0.005)); }
        void setParameters (float aMs, float dMs, float s, float rMs)
        {
            auto coef = [this] (float ms, float k) { return 1.0f - std::exp (-k / (float) (sr * juce::jmax (0.5f, ms) * 0.001)); };
            aC = coef (aMs, 1.47f);   // 1.3 target: reaches 1.0 after ~aMs
            dC = coef (dMs, 4.6f);
            rC = coef (rMs, 4.6f);
            sus = juce::jlimit (0.0f, 1.0f, s);   // while held, the level follows the knob (5 ms glide)
        }
        void noteOn()  { stage = Attack; }
        void noteOff() { if (stage != Idle) stage = Release; }
        void reset()   { stage = Idle; level = 0.0f; }
        bool isActive() const { return stage != Idle; }

        float getNextSample()
        {
            switch (stage)
            {
                case Attack:
                    level += (1.3f - level) * aC;
                    if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                    break;
                case Decay:
                    level += (sus - level) * dC;
                    if (std::abs (level - sus) < 1.0e-4f) { level = sus; stage = Sustain; }
                    break;
                case Sustain: level += (sus - level) * sC; break;
                case Release:
                    level -= level * rC;
                    if (level < 1.0e-4f) { level = 0.0f; stage = Idle; }
                    break;
                default: break;
            }
            return level;
        }
    };

    // ---- one-pole tilt for the noise source: < 0 low-pass (dark), > 0 high-pass (thin) ----
    struct Tilt
    {
        float a = 0.0f, z = 0.0f; int mode = 0;   // 0 bypass, 1 LP, 2 HP
        void set (float t, double sr)
        {
            if (std::abs (t) < 0.01f) { mode = 0; return; }
            const float hz = t < 0.0f ? 18000.0f * std::pow (0.011f, -t)    // 18 kHz .. 200 Hz
                                      : 30.0f * std::pow (200.0f, t);       // 30 Hz .. 6 kHz
            a = std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (hz, (float) sr * 0.45f) / (float) sr);
            mode = t < 0.0f ? 1 : 2;
        }
        float process (float x)
        {
            if (mode == 0) return x;
            z = (1.0f - a) * x + a * z;
            return mode == 1 ? z : x - z;
        }
    };

    // ---- TPT state-variable filter, multimode (Cytomic). One channel. ----
    // 24 dB = two stages; the second one is damped harder (k2 = 1 + k/2), so the resonance peak
    // stays at the 12 dB level instead of doubling in dB. DRIVE saturates the input (tanh, gain
    // neutral for small signals); at DRIVE 0 the filter is linear.
    struct SVF
    {
        double sr = 44100.0;
        float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, drive = 1, sat = 0;
        float kb = 1, b1 = 0, b2 = 0, b3 = 0;   // second stage (24 dB)
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
            kb = 1.0f + 0.5f * k;
            b1 = 1.0f / (1.0f + g * (g + kb));
            b2 = g * b1;
            b3 = g * b2;
            mode = m; slope = sl; drive = 1.0f + driveAmt * 3.0f;
            sat = juce::jlimit (0.0f, 1.0f, driveAmt * 10.0f);   // fades the saturation in over DRIVE 0 .. 10 %
        }
        void copyCoefs (const SVF& o)
        {
            g = o.g; k = o.k; a1 = o.a1; a2 = o.a2; a3 = o.a3; mode = o.mode; slope = o.slope; drive = o.drive; sat = o.sat;
            kb = o.kb; b1 = o.b1; b2 = o.b2; b3 = o.b3;
        }

        inline float stage (float v0, float& s1, float& s2, float kk, float c1, float c2, float c3) const
        {
            float v3 = v0 - s2;
            float v1 = c1 * s1 + c2 * v3;
            float v2 = s2 + c2 * s1 + c3 * v3;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            float lp = v2, bp = v1, hp = v0 - kk * v1 - v2;
            switch (mode) { case 1: return hp; case 2: return bp; case 3: return v0 - kk * v1; default: return lp; }
        }

        float process (float x)
        {
            if (sat > 0.0f)
            {
                const float xd = x * drive;
                x = (xd + (std::tanh (xd) - xd) * sat) / drive;
            }
            float y = stage (x, ic1, ic2, k, a1, a2, a3);
            if (slope == 1) y = stage (y, ic1b, ic2b, kb, b1, b2, b3);
            return y;
        }
    };

    // ---- LFO generator (per voice) ----
    struct LfoGen
    {
        double phase = 0.0, inc = 0.0;
        float sm = 0.0f, sh = 0.0f;
        float smCoef = 1.0f, smFor = -1.0f; double smInc = -1.0;
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
            if (smooth <= 0.001f) { sm = v; return v; }
            // SMOOTH = a one-pole whose time constant is a share of the LFO period (100 % = 1/8
            // period): the same rounding at every rate (square -> rounded, S&H -> smooth random)
            if (smooth != smFor || inc != smInc)
            {
                smFor = smooth; smInc = inc;
                smCoef = 1.0f - (float) std::exp (-8.0 * juce::jmax (1.0e-9, inc) / (double) smooth);
            }
            sm += (v - sm) * smCoef;
            return sm;
        }
    };
}
