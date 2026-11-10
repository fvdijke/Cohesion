#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// ===========================================================================
//  Transient shaping via a differential envelope (SPL-style).
//  A fast and a slow envelope follower track the level; their difference marks
//  the attack (fast > slow) and sustain (fast < slow) phases.
// ===========================================================================
namespace trs
{
    // Peak-ish follower (own attack & release). Slow releases hold peaks so the
    // envelope does NOT ripple at audio rate -> no amplitude-modulation distortion.
    struct Env
    {
        float at = 0.0f, rel = 0.0f, y = 0.0f;
        static float coef (float ms, double sr) { return 1.0f - (float) std::exp (-1.0 / (sr * (double) ms * 0.001)); }
        void setTimes (float atMs, float relMs, double sr) { at = coef (atMs, sr); rel = coef (relMs, sr); }
        void reset() { y = 0.0f; }
        float process (float x)
        {
            const float c = (x > y) ? at : rel;
            y += (x - y) * c;
            return y;
        }
    };

    class Shaper
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            // attack detector: fast vs slow ATTACK time (marks the onset), peak-hold release
            aFast.setTimes (1.0f,  55.0f, sr);
            aSlow.setTimes (26.0f, 55.0f, sr);
            // sustain detector: fast vs slow RELEASE (marks the tail), instant attack
            sFast.setTimes (1.0f,  35.0f,  sr);
            sSlow.setTimes (1.0f,  260.0f, sr);
            aFast.reset(); aSlow.reset(); sFast.reset(); sSlow.reset();
            gSm = 1.0f;
            gCoef = 1.0f - (float) std::exp (-1.0 / (sr * 0.003));   // ~3 ms gain smoothing
        }

        void setParams (float attackDb_, float sustainDb_, float sensitivity01)
        {
            attackDb  = attackDb_;
            sustainDb = sustainDb_;
            sens      = 0.6f + sensitivity01 * 2.8f;
        }

        // Stereo-linked `level` (rectified). Returns smoothed linear gain.
        float processGain (float level)
        {
            const float af = aFast.process (level), as = aSlow.process (level);
            const float sf = sFast.process (level), ss = sSlow.process (level);

            // both >= 0: attack active only on onsets, sustain only on tails.
            // In steady state both are ~0 -> unity gain -> no ducking.
            const float attackSig  = juce::jmin (1.0f, juce::jmax (0.0f, (af - as) / (as + 1.0e-4f)) * sens);
            const float sustainSig = juce::jmin (1.0f, juce::jmax (0.0f, (ss - sf) / (ss + 1.0e-4f)) * sens);

            lastGainDb = attackDb * attackSig + sustainDb * sustainSig;
            const float g = juce::Decibels::decibelsToGain (lastGainDb);
            gSm += (g - gSm) * gCoef;
            return gSm;
        }

        float getGainDb() const { return lastGainDb; }

    private:
        double sampleRate = 44100.0;
        Env aFast, aSlow, sFast, sSlow;
        float attackDb = 0.0f, sustainDb = 0.0f, sens = 1.0f;
        float gSm = 1.0f, gCoef = 0.01f, lastGainDb = 0.0f;
    };
}
