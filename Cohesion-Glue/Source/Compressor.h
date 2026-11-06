#pragma once

#include <cmath>
#include <algorithm>
#include <vector>

// ===========================================================================
//  Cohesion - ORIGINAL master-bus "glue" processing engine.
//
//  Building blocks (all from published DSP theory, nothing extracted from
//  any commercial plug-in):
//    - Biquad         : RBJ cookbook low-shelf / high-shelf EQ.
//    - OnePoleHighpass : sidechain detection high-pass.
//    - BusCompressor  : soft-knee, stereo-linked, auto/manual release.
//    - Limiter        : fast feed-forward peak limiter (safety + loudness).
// ===========================================================================

namespace cohesion
{
    static constexpr float kPi     = 3.14159265358979323846f;
    static constexpr float kMinLvl = 1.0e-6f;
    static constexpr float kKneeDb = 6.0f;

    inline float dbToLin (float dB) { return std::pow (10.0f, dB * 0.05f); }
    inline float linToDb (float x)  { return 20.0f * std::log10 (std::max (x, kMinLvl)); }

    // --- RBJ biquad, Transposed Direct Form II, up to 2 channels -----------
    class Biquad
    {
    public:
        void reset()
        {
            for (int c = 0; c < 2; ++c) { z1[c] = z2[c] = 0.0f; }
        }

        void setLowShelf (float f0, float gainDb, double fs)
        {
            const float A   = std::pow (10.0f, gainDb / 40.0f);
            const float w0  = 2.0f * kPi * f0 / static_cast<float> (fs);
            const float c   = std::cos (w0);
            const float s   = std::sin (w0);
            const float alp = s * 0.5f * std::sqrt (2.0f);          // S = 1
            const float sa  = 2.0f * std::sqrt (A) * alp;

            const float b0 =       A * ((A + 1) - (A - 1) * c + sa);
            const float b1 = 2.0f * A * ((A - 1) - (A + 1) * c);
            const float b2 =       A * ((A + 1) - (A - 1) * c - sa);
            const float a0 =            (A + 1) + (A - 1) * c + sa;
            const float a1 = -2.0f *   ((A - 1) + (A + 1) * c);
            const float a2 =            (A + 1) + (A - 1) * c - sa;
            normalise (b0, b1, b2, a0, a1, a2);
        }

        void setHighShelf (float f0, float gainDb, double fs)
        {
            const float A   = std::pow (10.0f, gainDb / 40.0f);
            const float w0  = 2.0f * kPi * f0 / static_cast<float> (fs);
            const float c   = std::cos (w0);
            const float s   = std::sin (w0);
            const float alp = s * 0.5f * std::sqrt (2.0f);          // S = 1
            const float sa  = 2.0f * std::sqrt (A) * alp;

            const float b0 =        A * ((A + 1) + (A - 1) * c + sa);
            const float b1 = -2.0f * A * ((A - 1) + (A + 1) * c);
            const float b2 =        A * ((A + 1) + (A - 1) * c - sa);
            const float a0 =             (A + 1) - (A - 1) * c + sa;
            const float a1 =  2.0f *    ((A - 1) - (A + 1) * c);
            const float a2 =             (A + 1) - (A - 1) * c - sa;
            normalise (b0, b1, b2, a0, a1, a2);
        }

        void setHighpass (float f0, double fs)
        {
            const float w0  = 2.0f * kPi * f0 / static_cast<float> (fs);
            const float c   = std::cos (w0);
            const float s   = std::sin (w0);
            const float alp = s / (2.0f * 0.70710678f);
            const float b0 =  (1.0f + c) * 0.5f;
            const float b1 = -(1.0f + c);
            const float b2 =  (1.0f + c) * 0.5f;
            const float a0 =   1.0f + alp;
            const float a1 =  -2.0f * c;
            const float a2 =   1.0f - alp;
            normalise (b0, b1, b2, a0, a1, a2);
        }

        void setLowpass (float f0, double fs)
        {
            const float w0  = 2.0f * kPi * f0 / static_cast<float> (fs);
            const float c   = std::cos (w0);
            const float s   = std::sin (w0);
            const float alp = s / (2.0f * 0.70710678f);
            const float b0 = (1.0f - c) * 0.5f;
            const float b1 =  1.0f - c;
            const float b2 = (1.0f - c) * 0.5f;
            const float a0 =  1.0f + alp;
            const float a1 = -2.0f * c;
            const float a2 =  1.0f - alp;
            normalise (b0, b1, b2, a0, a1, a2);
        }

        inline float process (float x, int ch)
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }

    private:
        void normalise (float b0, float b1, float b2, float a0, float a1, float a2)
        {
            this->b0 = b0 / a0; this->b1 = b1 / a0; this->b2 = b2 / a0;
            this->a1 = a1 / a0; this->a2 = a2 / a0;
        }

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1[2] { 0.0f, 0.0f }, z2[2] { 0.0f, 0.0f };
    };

    // --- One-pole high-pass for the sidechain detection path ---------------
    class OnePoleHighpass
    {
    public:
        void prepare (double fs) { sampleRate = fs; reset(); update(); }
        void reset()             { xPrev = 0.0f; yPrev = 0.0f; }
        void setFrequency (float f) { freq = f; update(); }

        inline float process (float x)
        {
            const float y = a * (yPrev + x - xPrev);
            xPrev = x; yPrev = y;
            return y;
        }

    private:
        void update() { a = std::exp (-2.0f * kPi * freq / static_cast<float> (sampleRate)); }
        double sampleRate = 44100.0;
        float  freq = 20.0f, a = 0.0f, xPrev = 0.0f, yPrev = 0.0f;
    };

    // --- Stereo-linked soft-knee bus compressor ----------------------------
    class BusCompressor
    {
    public:
        void prepare (double fs) { sampleRate = fs; rmsCoeff = coeff (0.005f); hpf.prepare (fs); reset(); }
        void reset()             { grEnv = grFast = grSlow = 0.0f; meterHold = 0.0f; rmsEnv = 0.0f; hpf.reset(); }

        void setRatio (float r)  { ratio = r; slope = 1.0f - 1.0f / ratio; }
        void setSidechain (float freqHz, bool on) { scOn = on; if (on) hpf.setFrequency (freqHz); }
        void setKnee (float kneeWidthDb) { kneeDb = std::max (0.0f, kneeWidthDb); }

        void setTimes (float attackMs, float releaseSeconds, bool autoRelease)
        {
            autoRel  = autoRelease;
            attCoeff = coeff (attackMs * 0.001f);
            if (autoRelease) { relFastCoeff = coeff (0.08f); relSlowCoeff = coeff (1.50f); }
            else             { relCoeff     = coeff (releaseSeconds); }
        }

        inline void processFrame (float* samples, int numCh,
                                  float thresholdDb, float makeupLin, float mix)
        {
            float scSig = samples[0];
            if (numCh > 1) scSig = 0.5f * (samples[0] + samples[1]);

            const float det = scOn ? hpf.process (scSig) : scSig;
            rmsEnv += rmsCoeff * (det * det - rmsEnv);          // RMS detector = smoother "glue"
            const float levelDb = linToDb (std::sqrt (rmsEnv));
            const float overDb  = levelDb - thresholdDb;
            const float desired = computeGainReduction (overDb);

            float gr;
            if (autoRel)
            {
                grFast += ((desired > grFast) ? attCoeff : relFastCoeff) * (desired - grFast);
                grSlow += ((desired > grSlow) ? attCoeff : relSlowCoeff) * (desired - grSlow);
                gr = std::max (grFast, grSlow);
            }
            else
            {
                grEnv += ((desired > grEnv) ? attCoeff : relCoeff) * (desired - grEnv);
                gr = grEnv;
            }

            if (gr > meterHold) meterHold = gr;

            const float gain = makeupLin * dbToLin (-gr);
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float dry = samples[ch];
                const float wet = dry * gain;
                samples[ch] = dry * (1.0f - mix) + wet * mix;
            }
        }

        float fetchGainReductionDb() { const float v = meterHold; meterHold = 0.0f; return v; }

    private:
        inline float coeff (float t) const
        {
            if (t <= 0.0f) return 1.0f;
            return 1.0f - std::exp (-1.0f / (t * static_cast<float> (sampleRate)));
        }

        inline float computeGainReduction (float overDb) const
        {
            const float half = kneeDb * 0.5f;
            if (overDb <= -half) return 0.0f;
            if (overDb >=  half) return slope * overDb;
            const float x = overDb + half;
            return slope * (x * x) / (2.0f * kneeDb);
        }

        double sampleRate = 44100.0;
        OnePoleHighpass hpf;
        bool  scOn = false;
        float kneeDb = kKneeDb;
        float ratio = 4.0f, slope = 0.75f;
        bool  autoRel = false;
        float attCoeff = 0.0f, relCoeff = 0.0f, relFastCoeff = 0.0f, relSlowCoeff = 0.0f;
        float grEnv = 0.0f, grFast = 0.0f, grSlow = 0.0f, meterHold = 0.0f;
        float rmsEnv = 0.0f, rmsCoeff = 0.0f;
    };

    // --- Look-ahead peak limiter (clean ceiling, no overshoot) -------------
    //  A short look-ahead delay lets the gain drop BEFORE a peak arrives, so
    //  transients never slip through. Running inside 2x oversampling keeps it
    //  well under the ceiling even for most inter-sample peaks. A per-sample
    //  safety clamp on the delayed output guarantees a hard ceiling.
    class Limiter
    {
    public:
        void prepare (double fs)
        {
            sampleRate = fs;
            envRel = coeff (0.080f);                             // ~80 ms release hold
            look   = std::max (1, (int) std::lround (fs * 0.0015));   // ~1.5 ms look-ahead
            for (int ch = 0; ch < 2; ++ch)
            {
                delay[ch].assign ((size_t) look, 0.0f);
            }
            reset();
        }

        void reset()
        {
            env = 0.0f; meterHold = 0.0f; pos = 0;
            for (int ch = 0; ch < 2; ++ch)
                std::fill (delay[ch].begin(), delay[ch].end(), 0.0f);
        }

        void setCeiling (float dB) { ceilingLin = dbToLin (dB); }
        int  getLatencySamples() const { return look; }          // at the (oversampled) rate

        inline void processFrame (float* samples, int numCh)
        {
            // peak of the INCOMING (future) frame drives the envelope...
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = std::max (peak, std::fabs (samples[ch]));

            env = std::max (peak, env * envRel);                 // instant attack, slow release
            float g = (env > ceilingLin && env > kMinLvl) ? ceilingLin / env : 1.0f;

            // ...applied to the DELAYED output (so it is already down on arrival)
            float outPeak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float in = samples[ch];
                const float d  = delay[ch][(size_t) pos];
                delay[ch][(size_t) pos] = in;
                samples[ch] = d;
                outPeak = std::max (outPeak, std::fabs (d));
            }
            pos = (pos + 1) % look;

            // hard safety clamp so no output sample exceeds the ceiling
            if (outPeak * g > ceilingLin && outPeak > kMinLvl)
                g = ceilingLin / outPeak;

            for (int ch = 0; ch < numCh; ++ch)
                samples[ch] *= g;

            const float grDb = -linToDb (g);
            if (grDb > meterHold) meterHold = grDb;
        }

        float fetchGainReductionDb() { const float v = meterHold; meterHold = 0.0f; return v; }

    private:
        inline float coeff (float t) const
        {
            if (t <= 0.0f) return 1.0f;
            return 1.0f - std::exp (-1.0f / (t * static_cast<float> (sampleRate)));
        }

        double sampleRate = 44100.0;
        float  ceilingLin = 0.966f;   // ~ -0.3 dBFS
        float  env = 0.0f, envRel = 0.0f, meterHold = 0.0f;
        int    look = 1, pos = 0;
        std::vector<float> delay[2];
    };
} // namespace cohesion
