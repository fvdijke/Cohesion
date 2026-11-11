// Offline checks for the COHESION - SYNTH FX engine (not part of the plugin).
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/FxDsp.h"
#include <cstdio>
#include <vector>
#include <cmath>

static const double SR = 48000.0;
static int failures = 0;
static void check (bool ok, const char* what, double val)
{
    std::printf ("  [%s] %-58s %10.4f\n", ok ? " OK " : "FAIL", what, val);
    if (! ok) ++failures;
}

struct Buf { std::vector<float> L, R; explicit Buf (int n) : L ((size_t) n, 0.0f), R ((size_t) n, 0.0f) {} };

static void runChain (fx::FxChain& ch, Buf& b, const fx::FxParams& p, const int* order)
{
    const int blk = 256;
    for (int s = 0; s < (int) b.L.size(); s += blk)
        ch.process (b.L.data() + s, b.R.data() + s, juce::jmin (blk, (int) b.L.size() - s), p, order);
}

static double peakOf (const Buf& b, int from, int to)
{
    double pk = 0; for (int i = from; i < to; ++i) pk = std::max (pk, (double) std::max (std::abs (b.L[(size_t) i]), std::abs (b.R[(size_t) i])));
    return pk;
}
static double rmsOf (const Buf& b, int from, int to)
{
    double s = 0; for (int i = from; i < to; ++i) s += 0.5 * (b.L[(size_t) i] * b.L[(size_t) i] + b.R[(size_t) i] * b.R[(size_t) i]);
    return std::sqrt (s / juce::jmax (1, to - from));
}
static bool finite (const Buf& b) { for (size_t i = 0; i < b.L.size(); ++i) if (! std::isfinite (b.L[i]) || ! std::isfinite (b.R[i])) return false; return true; }

int main()
{
    const int order[5] = { 0, 1, 2, 3, 4 };
    juce::Random rng (42);

    // ---------- 1. stability with extreme settings ----------
    std::printf ("1. Stability (2 s noise + 6 s silence, extreme settings)\n");
    const char* names[5] = { "COMP", "FLANGER", "PHASER", "ECHO", "REVERB" };
    for (int id = 0; id < 5; ++id)
    {
        fx::FxChain ch; ch.prepare (SR, 512);
        fx::FxParams p;
        p.compOn = id == 0; p.compThresh = -40; p.compRatio = 20; p.compAtk = 0.1f; p.compMakeup = 24;
        p.flOn = id == 1;   p.flFb = 0.95f; p.flDepth = 1; p.flMix = 0.5f; p.flRate = 8;
        p.phOn = id == 2;   p.phFb = 0.95f; p.phStages = 4; p.phDepth = 1; p.phRate = 6;
        p.ecOn = id == 3;   p.ecFb = 0.95f; p.ecTime = 40; p.ecDamp = 0; p.ecMix = 1; p.ecPing = true;
        p.rvOn = id == 4;   p.rvDecay = 0.45f; p.rvSize = 1; p.rvDamp = 0; p.rvMix = 1;
        Buf b ((int) (SR * 8));
        for (int i = 0; i < (int) (SR * 2); ++i) { b.L[(size_t) i] = rng.nextFloat() * 0.6f - 0.3f; b.R[(size_t) i] = rng.nextFloat() * 0.6f - 0.3f; }
        runChain (ch, b, p, order);
        char t[96];
        std::snprintf (t, sizeof t, "%s finite output", names[id]);           check (finite (b), t, 0);
        std::snprintf (t, sizeof t, "%s peak bounded (< 8)", names[id]);      check (peakOf (b, 0, (int) b.L.size()) < 8.0, t, peakOf (b, 0, (int) b.L.size()));
        if (id != 0)
        {
            std::snprintf (t, sizeof t, "%s tail dies away (last 0.5 s RMS < 0.01)", names[id]);
            const double r = rmsOf (b, (int) (SR * 7.5), (int) b.L.size());
            check (r < 0.01, t, r);
        }
    }

    // ---------- 2. compressor static curve ----------
    std::printf ("2. Compressor static curve (-6 dBFS sine, T -18, R 4, hard knee -> -15 dBFS)\n");
    {
        fx::FxChain ch; ch.prepare (SR, 512); fx::FxParams p; p.compOn = true; p.compThresh = -18; p.compRatio = 4; p.compKnee = 0; p.compAtk = 1; p.compRel = 50;
        Buf b ((int) SR);
        const float a = juce::Decibels::decibelsToGain (-6.0f);
        for (int i = 0; i < (int) SR; ++i) b.L[(size_t) i] = b.R[(size_t) i] = a * (float) std::sin (2 * juce::MathConstants<double>::pi * 200 * i / SR);
        runChain (ch, b, p, order);
        const double outDb = juce::Decibels::gainToDecibels (peakOf (b, (int) (SR * 0.8), (int) SR));
        check (std::abs (outDb + 15.0) < 0.75, "steady-state peak level (dBFS)", outDb);
    }

    // ---------- 3. echo timing ----------
    std::printf ("3. Echo timing (100 ms, impulse)\n");
    {
        fx::FxChain ch; ch.prepare (SR, 512); fx::FxParams p; p.ecOn = true; p.ecTime = 100; p.ecFb = 0.5f; p.ecMix = 1; p.ecDamp = 0; p.ecWidth = 1;
        // let the on-ramp settle first
        Buf warm ((int) (SR * 0.1)); runChain (ch, warm, p, order);
        Buf b ((int) (SR * 0.5)); b.L[0] = b.R[0] = 1.0f;
        runChain (ch, b, p, order);
        int at = 0; double best = 0;
        for (int i = 10; i < (int) (SR * 0.2); ++i) if (std::abs (b.L[(size_t) i]) > best) { best = std::abs (b.L[(size_t) i]); at = i; }
        const double ms = at * 1000.0 / SR;
        check (std::abs (ms - 100.0) < 0.5, "first echo position (ms)", ms);
    }

    // ---------- 4. reverb RT60 ----------
    std::printf ("4. Reverb RT60 (decay 0.5 -> target 1.55 s)\n");
    {
        fx::FxChain ch; ch.prepare (SR, 512); fx::FxParams p; p.rvOn = true; p.rvDecay = 0.5f; p.rvSize = 0.6f; p.rvDamp = 0; p.rvMix = 1; p.rvPre = 1; p.rvLowCut = 20;
        Buf warm ((int) (SR * 0.1)); runChain (ch, warm, p, order);
        const int n = (int) (SR * 5);
        Buf b (n);
        for (int i = 0; i < 64; ++i) b.L[(size_t) i] = b.R[(size_t) i] = rng.nextFloat() - 0.5f;
        runChain (ch, b, p, order);
        // Schroeder backward integration
        std::vector<double> e ((size_t) n);
        double acc = 0; for (int i = n - 1; i >= 0; --i) { acc += b.L[(size_t) i] * b.L[(size_t) i] + b.R[(size_t) i] * b.R[(size_t) i]; e[(size_t) i] = acc; }
        auto tAt = [&] (double db) { const double thr = e[0] * std::pow (10.0, db / 10.0); for (int i = 0; i < n; ++i) if (e[(size_t) i] < thr) return i / SR; return n / SR; };
        const double rt = (tAt (-35.0) - tAt (-5.0)) * 2.0;   // T30 extrapolated to 60 dB
        check (std::abs (rt - 1.55) / 1.55 < 0.25, "measured RT60 (s)", rt);
        check (finite (b), "finite output", 0);
    }

    // ---------- 5. limiter ceiling + latency ----------
    std::printf ("5. Limiter (+12 dB sine, ceiling -0.3 dB)\n");
    {
        fx::Limiter lim; lim.prepare (SR); fx::FxParams p; p.limOn = true; p.limCeil = -0.3f;
        Buf b ((int) SR);
        for (int i = 0; i < (int) SR; ++i) b.L[(size_t) i] = b.R[(size_t) i] = 4.0f * (float) std::sin (2 * juce::MathConstants<double>::pi * 100 * i / SR);
        lim.process (b.L.data(), b.R.data(), (int) SR, p);
        const double pk = juce::Decibels::gainToDecibels (peakOf (b, 0, (int) SR));
        check (pk <= -0.29, "output peak (dBFS)", pk);
        check (lim.latency() == (int) (SR * 0.0015), "latency (samples)", lim.latency());
        // off = pure delay
        fx::Limiter l2; l2.prepare (SR); p.limOn = false;
        Buf d (200); d.L[0] = d.R[0] = 1.0f; l2.process (d.L.data(), d.R.data(), 200, p);
        check (std::abs (d.L[(size_t) l2.latency()] - 1.0f) < 1e-6, "bypassed limiter = pure delay", d.L[(size_t) l2.latency()]);
    }

    // ---------- 6. click-free on/off ----------
    std::printf ("6. Click-free toggling (100 Hz sine, toggle each effect on and off)\n");
    for (int id = 0; id < 5; ++id)
    {
        fx::FxChain ch; ch.prepare (SR, 512);
        const int n = (int) (SR * 1.5);
        Buf b (n);
        for (int i = 0; i < n; ++i) b.L[(size_t) i] = b.R[(size_t) i] = 0.5f * (float) std::sin (2 * juce::MathConstants<double>::pi * 100 * i / SR);
        fx::FxParams p; p.compThresh = -30; p.compMakeup = 6; p.flMix = 0.5f; p.phMix = 0.5f; p.ecMix = 0.4f; p.rvMix = 0.4f;
        const int blk = 256;
        for (int s = 0; s < n; s += blk)
        {
            const bool on = s >= (int) (SR * 0.5) && s < (int) (SR * 1.0);
            p.compOn = id == 0 && on; p.flOn = id == 1 && on; p.phOn = id == 2 && on; p.ecOn = id == 3 && on; p.rvOn = id == 4 && on;
            ch.process (b.L.data() + s, b.R.data() + s, juce::jmin (blk, n - s), p, order);
        }
        // the largest sample-to-sample jump right around the switch points
        double jump = 0;
        for (int sw : { (int) (SR * 0.5), (int) (SR * 1.0) })
            for (int i = sw - 64; i < sw + 2048; ++i)
                jump = std::max (jump, (double) std::abs (b.L[(size_t) i] - b.L[(size_t) i - 1]));
        char t[96]; std::snprintf (t, sizeof t, "%s max step at switch (sine step = 0.0065)", names[id]);
        check (jump < 0.05, t, jump);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
