#pragma once

#include <cmath>
#include <algorithm>
#include "EqDsp.h"

// ===========================================================================
//  COHESION - EQ/COMP : multiband dynamics engine.
//
//  Band split: Linkwitz-Riley 4th-order crossovers (two cascaded Butterworth
//  SVF sections per split) with 2nd-order all-pass phase alignment of the
//  already-separated lower bands -> the bands sum back to a flat (all-pass)
//  response. Up to 5 bands / 4 crossovers.
//
//  Per band: feed-forward, log-domain dynamics with an RMS detector and
//  soft knee. Mode = Compressor / Expander / Gate (downward).
// ===========================================================================

namespace ceqc
{
    inline float dbToLin (float dB) { return std::pow (10.0f, dB * 0.05f); }

    // --- one channel's band splitter --------------------------------------
    class Splitter
    {
    public:
        static constexpr int kMaxCross = 4;

        void setSampleRate (double fs)
        {
            for (int c = 0; c < kMaxCross; ++c)
            {
                lpA[c].setSampleRate (fs); lpB[c].setSampleRate (fs);
                hpA[c].setSampleRate (fs); hpB[c].setSampleRate (fs);
                for (int b = 0; b < kMaxCross; ++b) ap[b][c].setSampleRate (fs);
            }
        }

        void reset()
        {
            for (int c = 0; c < kMaxCross; ++c)
            {
                lpA[c].reset(); lpB[c].reset(); hpA[c].reset(); hpB[c].reset();
                for (int b = 0; b < kMaxCross; ++b) ap[b][c].reset();
            }
        }

        void setFreqs (const float* f, int nCross)
        {
            const float Q = 0.70710678f;
            for (int c = 0; c < nCross; ++c)
            {
                lpA[c].set (SVF::Lowpass,  f[c], Q, 0.0f);
                lpB[c].set (SVF::Lowpass,  f[c], Q, 0.0f);
                hpA[c].set (SVF::Highpass, f[c], Q, 0.0f);
                hpB[c].set (SVF::Highpass, f[c], Q, 0.0f);
                for (int b = 0; b < c; ++b)
                    ap[b][c].set (SVF::Allpass, f[c], Q, 0.0f);
            }
        }

        inline void process (float x, int nBands, float* out)
        {
            const int nCross = nBands - 1;
            float cur = x;
            for (int c = 0; c < nCross; ++c)
            {
                const float lo = lpB[c].process (lpA[c].process (cur));
                const float hi = hpB[c].process (hpA[c].process (cur));
                out[c] = lo;
                cur = hi;
            }
            out[nCross] = cur;

            // phase-align the lower bands through the later crossovers
            for (int b = 0; b < nCross; ++b)
                for (int c = b + 1; c < nCross; ++c)
                    out[b] = ap[b][c].process (out[b]);
        }

    private:
        SVF lpA[kMaxCross], lpB[kMaxCross], hpA[kMaxCross], hpB[kMaxCross];
        SVF ap[kMaxCross][kMaxCross];   // ap[band][crossover]
    };

    // --- stereo-linked per-band dynamics ----------------------------------
    class BandDynamics
    {
    public:
        void prepare (double fs) { sr = fs; rmsCoeff = onepole (0.005f); reset(); }
        void reset()             { env = 0.0f; rms = 0.0f; hold = 0.0f; }

        // mode: 0 = Comp, 1 = Expander, 2 = Gate
        // widthDb doubles as knee width (Comp) or max range/depth (Exp/Gate).
        void set (int mode_, float thrDb, float ratio_, float atkMs, float relMs,
                  float gainDb, float widthDb)
        {
            mode  = mode_;
            thr   = thrDb;
            ratio = std::max (1.0f, ratio_);
            atk   = onepole (atkMs * 0.001f);
            rel   = onepole (relMs * 0.001f);
            gain  = dbToLin (gainDb);
            width = std::max (0.0f, widthDb);
        }

        inline void processFrame (float* s, int nc)
        {
            const float m = (nc > 1) ? 0.5f * (s[0] + s[1]) : s[0];
            rms += rmsCoeff * (m * m - rms);
            const float lvl = 10.0f * std::log10 (std::max (rms, 1.0e-12f));

            const float desired = computeGR (lvl);
            const float c = (desired > env) ? atk : rel;
            env += c * (desired - env);
            if (env > hold) hold = env;

            const float g = gain * dbToLin (-env);
            for (int ch = 0; ch < nc; ++ch) s[ch] *= g;
        }

        float fetchGR() { const float v = hold; hold = 0.0f; return v; }

    private:
        inline float onepole (float t) const
        {
            if (t <= 0.0f) return 1.0f;
            return 1.0f - std::exp (-1.0f / (t * (float) sr));
        }

        inline float computeGR (float lvl) const
        {
            if (mode == 0)   // compressor (upward threshold, soft knee = width)
            {
                const float over = lvl - thr;
                const float slope = 1.0f - 1.0f / ratio;
                if (width <= 0.0f) return over <= 0.0f ? 0.0f : slope * over;
                const float half = width * 0.5f;
                if (over <= -half) return 0.0f;
                if (over >=  half) return slope * over;
                const float x = over + half;
                return slope * (x * x) / (2.0f * width);
            }
            // expander / gate (downward below threshold)
            const float under = thr - lvl;
            if (under <= 0.0f) return 0.0f;
            const float r = (mode == 2) ? std::max (ratio, 10.0f) : ratio;
            return std::min (under * (r - 1.0f), width);
        }

        double sr = 44100.0;
        int    mode = 0;
        float  thr = 0.0f, ratio = 2.0f, atk = 0.0f, rel = 0.0f, gain = 1.0f, width = 6.0f;
        float  env = 0.0f, rms = 0.0f, hold = 0.0f, rmsCoeff = 0.0f;
    };

    // --- full multiband processor -----------------------------------------
    class Multiband
    {
    public:
        static constexpr int kMaxBands = 5;

        void prepare (double fs)
        {
            for (int ch = 0; ch < 2; ++ch) { split[ch].setSampleRate (fs); split[ch].reset(); }
            for (int b = 0; b < kMaxBands; ++b) dyn[b].prepare (fs);
        }

        void setBands (int n) { numBands = std::min (kMaxBands, std::max (2, n)); }
        int  getBands() const { return numBands; }

        void setFreqs (const float* f)
        {
            float ff[4];
            for (int i = 0; i < 4; ++i) ff[i] = f[i];
            for (int i = 1; i < 4; ++i) ff[i] = std::max (ff[i], ff[i - 1] + 1.0f);
            for (int ch = 0; ch < 2; ++ch) split[ch].setFreqs (ff, numBands - 1);
        }

        void setBand (int b, int mode, float thr, float ratio, float atkMs, float relMs,
                      float gainDb, float widthDb, bool soloB, bool muteB, bool bypB)
        {
            dyn[b].set (mode, thr, ratio, atkMs, relMs, gainDb, widthDb);
            solo[b] = soloB; mute[b] = muteB; byp[b] = bypB;
        }

        inline void process (float* frame, int nc)
        {
            float bands[2][kMaxBands];
            for (int ch = 0; ch < nc; ++ch) split[ch].process (frame[ch], numBands, bands[ch]);

            bool anySolo = false;
            for (int b = 0; b < numBands; ++b) if (solo[b]) anySolo = true;

            for (int ch = 0; ch < nc; ++ch) frame[ch] = 0.0f;

            for (int b = 0; b < numBands; ++b)
            {
                float bf[2] = { bands[0][b], (nc > 1 ? bands[1][b] : 0.0f) };
                if (! byp[b]) dyn[b].processFrame (bf, nc);

                const bool audible = mute[b] ? false : (anySolo ? solo[b] : true);
                if (audible)
                    for (int ch = 0; ch < nc; ++ch) frame[ch] += bf[ch];
            }
        }

        float fetchGR (int b) { return dyn[b].fetchGR(); }

    private:
        int numBands = 4;
        Splitter split[2];
        BandDynamics dyn[kMaxBands];
        bool solo[kMaxBands] { false, false, false, false, false };
        bool mute[kMaxBands] { false, false, false, false, false };
        bool byp[kMaxBands]  { false, false, false, false, false };
    };
}
