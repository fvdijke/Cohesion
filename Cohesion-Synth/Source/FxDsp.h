#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

// ===========================================================================
//  COHESION - SYNTH  FX engine.
//  Compressor, flanger, phaser, echo, FDN reverb and a lookahead limiter.
//  Every effect has a click-free on/off ramp; the chain order is free.
// ===========================================================================
namespace fx
{
    enum { kComp = 0, kFlanger, kPhaser, kEcho, kReverb, kNumFx };

    struct FxParams
    {
        bool  compOn = false; float compThresh = -18, compRatio = 4, compAtk = 10, compRel = 150, compMakeup = 0, compKnee = 6, compMix = 1;
        bool  flOn   = false; float flRate = 0.3f, flDepth = 0.6f, flFb = 0.5f, flDelay = 2.0f, flWidth = 0.8f; int flSync = 0; float flMix = 0.5f;
        bool  phOn   = false; float phRate = 0.4f, phDepth = 0.7f, phFb = 0.5f; int phStages = 1; float phSpread = 0.5f; int phSync = 0; float phMix = 0.5f;
        bool  ecOn   = false; float ecTime = 375;  int ecSync = 0; float ecFb = 0.4f, ecDamp = 0.3f, ecWidth = 0.7f; bool ecPing = false; float ecMix = 0.25f;
        bool  rvOn   = false; float rvSize = 0.6f, rvDecay = 0.5f, rvPre = 20, rvDamp = 0.4f, rvWidth = 1.0f, rvLowCut = 120, rvMix = 0.25f;
        bool  limOn  = true;  float limCeil = -0.3f, limRel = 80.0f, limDrive = 0.0f;
        double bpm = 120.0;
    };

    // --- tempo sync tables ---
    // modulation effects:  Free, 1/1, 1/2, 1/4, 1/8, 1/16   (beats per LFO cycle)
    inline float modSyncHz (int idx, float freeHz, double bpm)
    {
        static const float beats[] = { 0.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };
        if (idx <= 0 || idx > 5) return freeHz;
        return (float) (bpm / 60.0) / beats[idx];
    }
    // echo:  Free, 1/2, 1/4, 1/4 D, 1/8, 1/8 D, 1/8 T, 1/16   (beats)
    inline float echoSyncMs (int idx, float freeMs, double bpm)
    {
        static const float beats[] = { 0.0f, 2.0f, 1.0f, 1.5f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f };
        if (idx <= 0 || idx > 7) return freeMs;
        return (float) (beats[idx] * 60000.0 / bpm);
    }

    inline void xfade (float mix, float& dry, float& wet)
    {
        const float a = juce::jlimit (0.0f, 1.0f, mix) * juce::MathConstants<float>::halfPi;
        dry = std::cos (a); wet = std::sin (a);
    }

    inline float softSat (float x) { return std::tanh (x); }

    // ---- fractional delay line, cubic (Catmull-Rom) interpolation ----
    struct DelayLine
    {
        std::vector<float> buf; int size = 0, w = 0;
        void prepare (int n) { size = juce::jmax (8, n); buf.assign ((size_t) size, 0.0f); w = 0; }
        void reset()         { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x)  { buf[(size_t) w] = x; if (++w >= size) w = 0; }
        inline float at (int k) const { int i = w - 1 - k; while (i < 0) i += size; return buf[(size_t) i]; }
        float read (float d) const
        {
            d = juce::jlimit (1.0f, (float) (size - 3), d);
            const int k0 = (int) d; const float fr = d - (float) k0;
            const float ym1 = at (k0 - 1), y0 = at (k0), y1 = at (k0 + 1), y2 = at (k0 + 2);
            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * fr + c2) * fr + c1) * fr + y0;
        }
    };

    // ======================================================================
    //  Compressor: feed-forward, stereo-linked, soft knee, log-domain detector.
    // ======================================================================
    struct Compressor
    {
        double sr = 44100.0; float env = 0.0f;
        std::atomic<float> grDb { 0.0f };
        void prepare (double s) { sr = s; reset(); }
        void reset()            { env = 0.0f; grDb.store (0.0f); }

        void process (float* L, float* R, int n, const FxParams& p)
        {
            const float aC = std::exp (-1.0f / (float) (sr * juce::jmax (0.05f, p.compAtk) * 0.001));
            const float rC = std::exp (-1.0f / (float) (sr * juce::jmax (5.0f,  p.compRel) * 0.001));
            const float T = p.compThresh, Rt = juce::jmax (1.0f, p.compRatio), W = juce::jmax (0.0f, p.compKnee);
            const float slope = 1.0f / Rt - 1.0f;
            const float makeup = juce::Decibels::decibelsToGain (p.compMakeup);
            const float mix = juce::jlimit (0.0f, 1.0f, p.compMix);
            float peakGr = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float lvl = juce::jmax (std::abs (L[i]), std::abs (R[i]), 1.0e-6f);
                const float xdB = 20.0f * std::log10 (lvl);
                const float over = xdB - T;
                float gc;
                if (W > 0.01f && 2.0f * std::abs (over) <= W) gc = slope * (over + 0.5f * W) * (over + 0.5f * W) / (2.0f * W);
                else gc = over > 0.0f ? slope * over : 0.0f;
                const float target = -gc;   // positive dB of reduction
                env = target > env ? aC * env + (1.0f - aC) * target : rC * env + (1.0f - rC) * target;
                const float g = juce::Decibels::decibelsToGain (-env) * makeup;
                const float wet = 1.0f + (g - 1.0f) * mix;   // parallel blend in the gain domain
                L[i] *= wet; R[i] *= wet;
                peakGr = juce::jmax (peakGr, env);
            }
            grDb.store (peakGr);
        }
    };

    // ======================================================================
    //  Flanger: modulated short delay with feedback, stereo LFO offset.
    // ======================================================================
    struct Flanger
    {
        double sr = 44100.0, ph = 0.0; DelayLine dL, dR; float fbL = 0, fbR = 0;
        void prepare (double s) { sr = s; dL.prepare ((int) (sr * 0.03) + 16); dR.prepare ((int) (sr * 0.03) + 16); dL.reset(); dR.reset(); reset(); }
        void reset()            { fbL = fbR = 0.0f; }     // keep the (continuously fed) delay lines
        // while bypassed: keep the delay lines filled so switching on is seamless
        void feed (const float* L, const float* R, int n) { for (int i = 0; i < n; ++i) { dL.push (L[i]); dR.push (R[i]); } }

        void process (float* L, float* R, int n, const FxParams& p)
        {
            const double inc = (double) modSyncHz (p.flSync, p.flRate, p.bpm) / sr;
            const float base = juce::jmax (0.2f, p.flDelay) * 0.001f * (float) sr;
            const float depth = juce::jlimit (0.0f, 1.0f, p.flDepth) * 0.92f;
            const float fb = juce::jlimit (-0.95f, 0.95f, p.flFb);
            const double offR = juce::jlimit (0.0f, 1.0f, p.flWidth) * 0.25;
            float dG, wG; xfade (p.flMix, dG, wG);
            for (int i = 0; i < n; ++i)
            {
                const float lfoL = (float) std::sin (juce::MathConstants<double>::twoPi * ph);
                const float lfoR = (float) std::sin (juce::MathConstants<double>::twoPi * (ph + offR));
                const float dl = juce::jmax (1.5f, base * (1.0f + depth * lfoL));
                const float dr = juce::jmax (1.5f, base * (1.0f + depth * lfoR));
                dL.push (L[i] + softSat (fbL * fb));
                dR.push (R[i] + softSat (fbR * fb));
                const float wl = dL.read (dl), wr = dR.read (dr);
                fbL = wl; fbR = wr;
                L[i] = L[i] * dG + wl * wG;
                R[i] = R[i] * dG + wr * wG;
                ph += inc; if (ph >= 1.0) ph -= 1.0;
            }
        }
    };

    // ======================================================================
    //  Phaser: cascaded first-order all-passes, exponential sweep, feedback.
    // ======================================================================
    struct Phaser
    {
        double sr = 44100.0, ph = 0.0;
        float zx[2][12] {}, zy[2][12] {}, fb[2] {};
        void prepare (double s) { sr = s; reset(); }
        void reset()            { for (int c = 0; c < 2; ++c) { fb[c] = 0; for (int k = 0; k < 12; ++k) zx[c][k] = zy[c][k] = 0; } }

        void process (float* L, float* R, int n, const FxParams& p)
        {
            static const int stagesTab[] = { 2, 4, 6, 8, 12 };
            const int stages = stagesTab[juce::jlimit (0, 4, p.phStages)];
            const double inc = (double) modSyncHz (p.phSync, p.phRate, p.bpm) / sr;
            const float octaves = 1.0f + juce::jlimit (0.0f, 1.0f, p.phDepth) * 5.0f;
            const float fbAmt = juce::jlimit (-0.95f, 0.95f, p.phFb);
            const double spread = juce::jlimit (0.0f, 1.0f, p.phSpread) * 0.5;
            const float nyq = (float) (sr * 0.45);
            float dG, wG; xfade (p.phMix, dG, wG);
            float* io[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const double pc = ph + (c == 1 ? spread : 0.0);
                    const float lfo = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * pc);
                    const float f = juce::jmin (nyq, 140.0f * std::pow (2.0f, octaves * lfo));
                    const float t = std::tan (juce::MathConstants<float>::pi * f / (float) sr);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    const float in = io[c][i];
                    float x = in + fb[c] * fbAmt;
                    for (int s = 0; s < stages; ++s)
                    {
                        const float y = a * x + zx[c][s] - a * zy[c][s];
                        zx[c][s] = x; zy[c][s] = y; x = y;
                    }
                    fb[c] = softSat (x);
                    io[c][i] = in * dG + x * wG;
                }
                ph += inc; if (ph >= 1.0) ph -= 1.0;
            }
        }
    };

    // ======================================================================
    //  Echo: stereo delay, damped feedback loop, ping-pong, wet width.
    // ======================================================================
    struct Echo
    {
        double sr = 44100.0; DelayLine dL, dR; float lpL = 0, lpR = 0, tSm = 0;
        void prepare (double s) { sr = s; dL.prepare ((int) (sr * 2.6) + 16); dR.prepare ((int) (sr * 2.6) + 16); reset(); }
        void reset()            { dL.reset(); dR.reset(); lpL = lpR = 0; tSm = 0; }

        void process (float* L, float* R, int n, const FxParams& p)
        {
            const float ms = echoSyncMs (p.ecSync, p.ecTime, p.bpm);
            const float target = juce::jlimit (2.0f, (float) dL.size - 4.0f, ms * 0.001f * (float) sr);
            if (tSm <= 0.0f) tSm = target;
            const float tC = 1.0f - std::exp (-1.0f / (float) (sr * 0.06));
            const float fc = 18000.0f * std::pow (0.05f, juce::jlimit (0.0f, 1.0f, p.ecDamp));
            const float a = std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (fc, (float) sr * 0.45f) / (float) sr);
            const float fb = juce::jlimit (0.0f, 0.95f, p.ecFb);
            const float width = juce::jlimit (0.0f, 1.0f, p.ecWidth);
            float dG, wG; xfade (p.ecMix, dG, wG);
            for (int i = 0; i < n; ++i)
            {
                tSm += (target - tSm) * tC;
                const float wl = dL.read (tSm), wr = dR.read (tSm);
                lpL = (1.0f - a) * wl + a * lpL;
                lpR = (1.0f - a) * wr + a * lpR;
                float inL, inR;
                if (p.ecPing) { inL = 0.5f * (L[i] + R[i]) + lpR * fb; inR = lpL * fb; }
                else          { inL = L[i] + lpL * fb;                 inR = R[i] + lpR * fb; }
                dL.push (softSat (inL)); dR.push (softSat (inR));
                const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
                L[i] = L[i] * dG + (mid + side) * wG;
                R[i] = R[i] * dG + (mid - side) * wG;
            }
        }
    };

    // ======================================================================
    //  Reverb: 8-line FDN (Householder), input diffusion, damping, modulation.
    // ======================================================================
    struct Reverb
    {
        static constexpr int N = 8;
        struct AP
        {
            std::vector<float> b; int i = 0; float g = 0.62f;
            void prepare (int n) { b.assign ((size_t) juce::jmax (2, n), 0.0f); i = 0; }
            void reset()         { std::fill (b.begin(), b.end(), 0.0f); i = 0; }
            float process (float x) { const float d = b[(size_t) i]; const float y = -g * x + d; b[(size_t) i] = x + g * y; if (++i >= (int) b.size()) i = 0; return y; }
        };

        double sr = 44100.0, modPh = 0.0;
        DelayLine preL, preR, line[N];
        AP apL[2], apR[2];
        float baseLen[N] {}, lenSm[N] {}, lp[N] {};
        float hpYL = 0, hpXL = 0, hpYR = 0, hpXR = 0;

        void prepare (double s)
        {
            sr = s;
            static const float ms[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.3f };
            for (int k = 0; k < N; ++k)
            {
                baseLen[k] = ms[k] * 0.001f * (float) sr;
                line[k].prepare ((int) (baseLen[k] * 2.1f) + 64);
            }
            preL.prepare ((int) (sr * 0.26) + 16); preR.prepare ((int) (sr * 0.26) + 16);
            const float sc = (float) (sr / 48000.0);
            apL[0].prepare ((int) (142 * sc)); apL[1].prepare ((int) (379 * sc));
            apR[0].prepare ((int) (151 * sc)); apR[1].prepare ((int) (397 * sc));
            reset();
        }

        void reset()
        {
            preL.reset(); preR.reset();
            for (auto& l : line) l.reset();
            for (auto& a : apL) a.reset();
            for (auto& a : apR) a.reset();
            for (int k = 0; k < N; ++k) { lp[k] = 0; lenSm[k] = 0; }
            hpYL = hpXL = hpYR = hpXR = 0; modPh = 0;
        }

        void process (float* L, float* R, int n, const FxParams& p)
        {
            const float sizeMul = 0.4f + juce::jlimit (0.0f, 1.0f, p.rvSize) * 1.6f;
            const float rt60 = 0.2f * std::pow (60.0f, juce::jlimit (0.0f, 1.0f, p.rvDecay));   // 0.2 .. 12 s
            const float damp = juce::jlimit (0.0f, 1.0f, p.rvDamp) * 0.85f;
            const float pre = juce::jlimit (1.0f, (float) preL.size - 4.0f, p.rvPre * 0.001f * (float) sr);
            const float rc = 1.0f / (juce::MathConstants<float>::twoPi * juce::jlimit (20.0f, 2000.0f, p.rvLowCut));
            const float hpA = rc / (rc + 1.0f / (float) sr);
            const float width = juce::jlimit (0.0f, 1.0f, p.rvWidth);
            const double modInc = 0.6 / sr;
            float target[N], g[N];
            for (int k = 0; k < N; ++k)
            {
                target[k] = baseLen[k] * sizeMul;
                if (lenSm[k] <= 0.0f) lenSm[k] = target[k];
                g[k] = std::pow (10.0f, -3.0f * target[k] / ((float) sr * rt60));
            }
            const float lenC = 1.0f - std::exp (-1.0f / (float) (sr * 0.15));
            float dG, wG; xfade (p.rvMix, dG, wG);

            for (int i = 0; i < n; ++i)
            {
                preL.push (L[i]); preR.push (R[i]);
                float xl = preL.read (pre), xr = preR.read (pre);
                // low cut (one-pole high-pass)
                hpYL = hpA * (hpYL + xl - hpXL); hpXL = xl; xl = hpYL;
                hpYR = hpA * (hpYR + xr - hpXR); hpXR = xr; xr = hpYR;
                xl = apL[1].process (apL[0].process (xl));
                xr = apR[1].process (apR[0].process (xr));

                float o[N], sum = 0.0f;
                for (int k = 0; k < N; ++k)
                {
                    lenSm[k] += (target[k] - lenSm[k]) * lenC;
                    float d = lenSm[k];
                    if (k < 4) d += 6.0f * (float) std::sin (juce::MathConstants<double>::twoPi * (modPh + 0.25 * k));
                    const float v = line[k].read (d);
                    lp[k] = (1.0f - damp) * v + damp * lp[k];
                    o[k] = lp[k] * g[k];
                    sum += o[k];
                }
                const float hh = sum * (2.0f / (float) N);
                for (int k = 0; k < N; ++k)
                    line[k].push (o[k] - hh + ((k & 1) ? xr : xl) * 0.35f);

                const float wl = (o[0] - o[2] + o[4] - o[6]) * 0.55f;
                const float wr = (o[1] - o[3] + o[5] - o[7]) * 0.55f;
                const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
                L[i] = L[i] * dG + (mid + side) * wG;
                R[i] = R[i] * dG + (mid - side) * wG;
                modPh += modInc; if (modPh >= 1.0) modPh -= 1.0;
            }
        }
    };

    // ======================================================================
    //  Lookahead peak limiter (1.5 ms). Constant latency, also when off.
    // ======================================================================
    struct Limiter
    {
        int la = 64; std::vector<float> bL, bR, req; int w = 0, rw = 0; float gSm = 1.0f, attC = 0.1f;
        double sr = 44100.0;
        std::atomic<float> grDb { 0.0f }, outPk { 0.0f };
        void prepare (double s)
        {
            sr = s;
            la = juce::jmax (8, (int) (sr * 0.0015));
            bL.assign ((size_t) la, 0.0f); bR.assign ((size_t) la, 0.0f); req.assign ((size_t) la + 1, 1.0f);
            attC = 1.0f - std::exp (-4.6f / (float) la);
            reset();
        }
        void reset() { std::fill (bL.begin(), bL.end(), 0.0f); std::fill (bR.begin(), bR.end(), 0.0f); std::fill (req.begin(), req.end(), 1.0f); w = rw = 0; gSm = 1.0f; }
        int latency() const { return la; }

        void process (float* L, float* R, int n, const FxParams& p)
        {
            const float ceil  = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 0.0f, p.limCeil));
            const float drive = p.limOn ? juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 24.0f, p.limDrive)) : 1.0f;
            const float relC  = 1.0f - std::exp (-1.0f / (float) (sr * juce::jlimit (5.0f, 2000.0f, p.limRel) * 0.001));
            float minG = 1.0f, pkOut = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i] * drive, inR = R[i] * drive;
                const float pk = juce::jmax (std::abs (inL), std::abs (inR));
                req[(size_t) rw] = (p.limOn && pk > ceil) ? ceil / pk : 1.0f;
                if (++rw > la) rw = 0;
                float mn = 1.0f;   // window = the last la+1 inputs, incl. the sample leaving now
                for (int k = 0; k <= la; ++k) mn = juce::jmin (mn, req[(size_t) k]);
                gSm += (mn - gSm) * (mn < gSm ? attC : relC);
                const float oL = bL[(size_t) w], oR = bR[(size_t) w];   // delayed by `la` samples
                bL[(size_t) w] = inL; bR[(size_t) w] = inR;
                if (++w >= la) w = 0;
                if (p.limOn)
                {
                    L[i] = juce::jlimit (-ceil, ceil, oL * gSm);
                    R[i] = juce::jlimit (-ceil, ceil, oR * gSm);
                }
                else { L[i] = oL; R[i] = oR; }
                minG  = juce::jmin (minG, p.limOn ? gSm : 1.0f);
                pkOut = juce::jmax (pkOut, std::abs (L[i]), std::abs (R[i]));
            }
            grDb.store (-juce::Decibels::gainToDecibels (minG, -60.0f));
            outPk.store (pkOut);
        }
    };

    // ======================================================================
    //  The chain: free order, per-effect click-free on/off ramps.
    // ======================================================================
    struct FxChain
    {
        Compressor comp; Flanger fl; Phaser ph; Echo ec; Reverb rv; Limiter lim;
        float ramp[kNumFx] {};
        std::vector<float> tL, tR;
        int maxBlock = 512; double sr = 44100.0;

        void prepare (double s, int block)
        {
            sr = s; maxBlock = juce::jmax (64, block);
            tL.assign ((size_t) maxBlock, 0.0f); tR.assign ((size_t) maxBlock, 0.0f);
            comp.prepare (s); fl.prepare (s); ph.prepare (s); ec.prepare (s); rv.prepare (s); lim.prepare (s);
            for (auto& r : ramp) r = 0.0f;
        }

        bool isOn (int fxId, const FxParams& p) const
        {
            switch (fxId) { case kComp: return p.compOn; case kFlanger: return p.flOn; case kPhaser: return p.phOn;
                            case kEcho: return p.ecOn; case kReverb: return p.rvOn; default: return false; }
        }

        void runFx (int fxId, float* L, float* R, int n, const FxParams& p)
        {
            switch (fxId) { case kComp: comp.process (L, R, n, p); break; case kFlanger: fl.process (L, R, n, p); break;
                            case kPhaser: ph.process (L, R, n, p); break; case kEcho: ec.process (L, R, n, p); break;
                            case kReverb: rv.process (L, R, n, p); break; default: break; }
        }

        void resetFx (int fxId)
        {
            switch (fxId) { case kComp: comp.reset(); break; case kFlanger: fl.reset(); break; case kPhaser: ph.reset(); break;
                            case kEcho: ec.reset(); break; case kReverb: rv.reset(); break; default: break; }
        }

        void process (float* L, float* R, int total, const FxParams& p, const int* order)
        {
            const float rc = 1.0f - std::exp (-1.0f / (float) (sr * 0.01));   // 10 ms on/off ramp
            for (int start = 0; start < total; start += maxBlock)
            {
                const int n = juce::jmin (maxBlock, total - start);
                float* l = L + start; float* r = R + start;
                for (int slot = 0; slot < kNumFx; ++slot)
                {
                    const int id = order[slot];
                    if (id < 0 || id >= kNumFx) continue;
                    const float tgt = isOn (id, p) ? 1.0f : 0.0f;
                    if (tgt == 0.0f && ramp[id] <= 0.0f) { if (id == kFlanger) fl.feed (l, r, n); continue; }
                    if (tgt == 1.0f && ramp[id] >= 1.0f) { runFx (id, l, r, n, p); continue; }
                    // ramping: process a copy and blend sample by sample
                    std::copy (l, l + n, tL.begin()); std::copy (r, r + n, tR.begin());
                    runFx (id, tL.data(), tR.data(), n, p);
                    float g = ramp[id];
                    for (int i = 0; i < n; ++i)
                    {
                        g += (tgt - g) * rc;
                        l[i] += (tL[(size_t) i] - l[i]) * g;
                        r[i] += (tR[(size_t) i] - r[i]) * g;
                    }
                    if (tgt == 1.0f && g > 0.999f) g = 1.0f;
                    if (tgt == 0.0f && g < 0.001f) { g = 0.0f; resetFx (id); }
                    ramp[id] = g;
                }
            }
        }
    };
}
