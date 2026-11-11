#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <functional>
#include "Params.h"
#include "ArpPatterns.h"

// ===========================================================================
//  Procedural factory presets.
//  Every category has 10 sound archetypes; each archetype yields variations
//  within musically sensible ranges. Fully deterministic (fixed seeds), so
//  every build produces exactly the same library.
// ===========================================================================
namespace pgen
{
    using V = std::vector<std::pair<juce::String, float>>;

    // waveforms / indices used below
    enum { Saw = 0, Square = 1, Tri = 2, Sine = 3 };
    enum { LP = 0, HP = 1, BP = 2, Notch = 3 };                     // filter 2 types
    enum { sLFO1 = 1, sLFO2, sLFO3, sLFO4, sENV1, sENV2, sENV3, sENV4, sVel, sMW, sKey };
    enum { dF1 = 1, dF2, dPitch, dAmp, dPan, dLfo1Rate };

    // ---------------------------------------------------------------- builder
    struct G
    {
        juce::Random r;
        V v;
        int nextMod = 0;

        static juce::int64 scramble (juce::uint64 x)   // splitmix64: decorrelates neighbouring seeds
        {
            x += 0x9e3779b97f4a7c15ULL;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            return (juce::int64) ((x ^ (x >> 31)) & 0x7fffffffffffULL);
        }

        explicit G (juce::int64 seed) : r (scramble ((juce::uint64) seed))
        {
            for (int k = 0; k < 4; ++k) r.nextInt();   // warm up the generator
            set (pid::oscOn (1), 0); set (pid::oscOn (2), 0);   // archetypes switch on what they use
        }

        float f (float a, float b)          { return a + (b - a) * r.nextFloat(); }
        float lg (float a, float b)         { return a * std::pow (b / a, r.nextFloat()); }     // log-uniform
        int   i (int a, int b)              { return a + r.nextInt (b - a + 1); }
        bool  p (float prob)                { return r.nextFloat() < prob; }
        template <typename T> T pick (std::initializer_list<T> l) { return *(l.begin() + r.nextInt ((int) l.size())); }

        void set (const juce::String& id, float val) { v.push_back ({ id, val }); }

        // ---- building blocks ----
        void osc (int n, int wave, float level, int oct = 0, int semi = 0, float fine = 0, int uni = 1, float det = 0, float pan = 0)
        {
            set (pid::oscOn (n), 1); set (pid::oscWave (n), (float) wave); set (pid::oscLevel (n), level);
            set (pid::oscOct (n), (float) oct); set (pid::oscSemi (n), (float) semi); set (pid::oscFine (n), fine);
            set (pid::oscUni (n), (float) uni); set (pid::oscDet (n), det); set (pid::oscPan (n), pan);
        }
        void phase (int n, float deg)  { set (pid::oscPhase (n), deg); }
        void sub (float level, int wave = 0, int oct = -1) { set (pid::subOn, 1); set (pid::subLevel, level); set (pid::subWave, (float) wave); set (pid::subOct, (float) oct); }
        void noise (float level, int type = 0)             { set (pid::noiseOn, 1); set (pid::noiseLevel, level); set (pid::noiseType, (float) type); }
        void fm (float amt)   { set (pid::fmAmt, amt); }
        void ring (float amt) { set (pid::ringAmt, amt); }
        void f1 (float cut, float res, float envAmt = 0, float drive = 0, int type = 0, float key = 0, int envSrc = 1)
        {
            set (pid::fltCutoff, cut); set (pid::fltRes, res); set (pid::fltEnvAmt, envAmt); set (pid::fltDrive, drive);
            set (pid::fltType, (float) type); set (pid::fltKeyTrk, key); set (pid::flt1EnvSrc, (float) envSrc);
        }
        void f2 (int type, float cut, float res, int routing = 0, float balance = 0.5f, int slope = 1, float envAmt = 0, float key = 0)
        {
            set (pid::flt2Type, (float) type); set (pid::flt2Cutoff, cut); set (pid::flt2Res, res); set (pid::fltRouting, (float) routing);
            set (pid::fltBalance, balance); set (pid::flt2Slope, (float) slope); set (pid::flt2EnvAmt, envAmt); set (pid::flt2KeyTrk, key);
        }
        void amp (float a, float d, float s, float rel) { set (pid::ampA, a); set (pid::ampD, d); set (pid::ampS, s); set (pid::ampR, rel); }
        void env (int n, float a, float d, float s, float rel) { set (pid::envA (n), a); set (pid::envD (n), d); set (pid::envS (n), s); set (pid::envR (n), rel); }
        void lfo (int n, int wave, float rate, float fade = 0, float smooth = 0)
        { set (pid::lfoWave (n), (float) wave); set (pid::lfoRate (n), rate); set (pid::lfoFade (n), fade); set (pid::lfoSmooth (n), smooth); }
        void mod (int src, int dst, float amt)
        {
            if (nextMod >= 8) return;
            set (pid::modSrc (nextMod), (float) src); set (pid::modDst (nextMod), (float) dst); set (pid::modAmt (nextMod), amt); ++nextMod;
        }
        void vibrato (float amt, float rate, float fade = 400) { lfo (2, 0, rate, fade); mod (sLFO2, dPitch, amt); }

        // ---- FX ----
        void comp (float thr, float ratio, float makeup) { set (pid::compOn, 1); set (pid::compThresh, thr); set (pid::compRatio, ratio); set (pid::compMakeup, makeup); }
        void chorus (float mix, float rate = 0.2f)       { set (pid::flOn, 1); set (pid::flRate, rate); set (pid::flDepth, 0.3f); set (pid::flFb, 0.05f); set (pid::flDelay, 5.0f); set (pid::flMix, mix); set (pid::flWidth, 1.0f); }
        void flanger (float mix, float rate, float fb)   { set (pid::flOn, 1); set (pid::flRate, rate); set (pid::flDepth, 0.6f); set (pid::flFb, fb); set (pid::flMix, mix); }
        void phaser (float mix, float rate, float depth = 0.6f) { set (pid::phOn, 1); set (pid::phMix, mix); set (pid::phRate, rate); set (pid::phDepth, depth); }
        void echo (int sync, float fb, float mix, bool ping = false, float damp = 0.35f)
        { set (pid::ecOn, 1); set (pid::ecSync, (float) sync); set (pid::ecFb, fb); set (pid::ecMix, mix); set (pid::ecPing, ping ? 1.0f : 0.0f); set (pid::ecDamp, damp); }
        void reverb (float mix, float size, float decay, float pre = 20) { set (pid::rvOn, 1); set (pid::rvMix, mix); set (pid::rvSize, size); set (pid::rvDecay, decay); set (pid::rvPre, pre); }

        // tasteful random space for melodic sounds
        void space (float amount = 1.0f)
        {
            if (p (0.55f)) echo (pick ({ 2, 4, 5, 5, 6 }), f (0.2f, 0.4f), f (0.1f, 0.22f) * amount, p (0.5f));
            if (p (0.75f)) reverb (f (0.12f, 0.3f) * amount, f (0.4f, 0.85f), f (0.35f, 0.65f));
        }

        // ---- ARP ----
        void arpPattern (const juce::String& name)
        {
            for (auto& pt : arpPatterns())
                if (pt.name == name)
                {
                    set (pid::arpSteps, (float) pt.steps);
                    for (int s = 0; s < 16; ++s)
                    {
                        set (pid::arpStepOn (s), pt.on[s] == 'x' ? 1.0f : 0.0f);
                        set (pid::arpStepVel (s), pt.vel[s] > 0.0f ? pt.vel[s] : 0.5f);
                    }
                    if (pt.rate >= 0)     set (pid::arpRate, (float) pt.rate);
                    if (pt.gate >= 0.0f)  set (pid::arpGate, pt.gate);
                    if (pt.swing >= 0.0f) set (pid::arpSwing, pt.swing);
                }
        }
        void arp (int mode, int oct, const juce::String& pattern)
        {
            arpPattern (pattern);
            set (pid::arpOn, 1); set (pid::arpMode, (float) mode); set (pid::arpOct, (float) oct);
        }
    };

    struct Archetype
    {
        juce::StringArray nouns;
        std::function<void (G&)> build;
    };

    struct Category
    {
        juce::String name;
        juce::StringArray adjectives;
        std::vector<Archetype> types;
    };

    // ========================================================================
    //  Category definitions
    // ========================================================================
    inline std::vector<Category> categories()
    {
        std::vector<Category> c;

        // ------------------------------------------------------------ BASS
        c.push_back ({ "Bass",
            { "Deep", "Dark", "Heavy", "Rubber", "Liquid", "Gritty", "Fat", "Low", "Round", "Punchy", "Warm", "Dirty", "Thick",
              "Hollow", "Analog", "Muted", "Bouncy", "Solid", "Driving", "Wide", "Tight", "Smooth", "Raw", "Velvet", "Iron", "Midnight",
              "Concrete", "Basement", "Pressure", "Steady" },
        {
            { { "Sub", "Bottom", "Floor", "Depth", "Foundation" }, [] (G& g)
              { g.osc (1, g.pick ({ Sine, Tri }), 0.85f); g.sub (g.f (0.4f, 0.7f), 0, -1);
                g.f1 (g.lg (250, 900), 0.1f, g.f (0.1f, 0.3f), 0.0f); g.env (2, 1, 200, 0.3f, 150);
                g.amp (g.f (1, 6), g.f (300, 800), g.f (0.7f, 1.0f), g.f (60, 200)); } },
            { { "Reese", "Rumble", "Mover", "Engine", "Growler" }, [] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, 0, g.i (2, 3), g.f (25, 45)); g.osc (2, Saw, 0.65f, 0, 0, g.f (6, 14), g.i (1, 3), g.f (20, 40));
                g.f1 (g.lg (350, 1200), g.f (0.1f, 0.35f)); g.lfo (1, 0, g.f (0.1f, 0.5f), 0, 0.3f); g.mod (sLFO1, dF1, g.f (0.15f, 0.4f));
                g.amp (5, 400, 0.9f, 180); g.comp (g.f (-20, -14), 4, 4); } },
            { { "Acid", "Squelch", "303", "Acid Line", "Bubble" }, [] (G& g)
              { g.osc (1, g.pick ({ Saw, Square }), 0.85f); g.f1 (g.lg (300, 900), g.f (0.55f, 0.8f), g.f (0.4f, 0.7f), g.f (0.2f, 0.5f));
                g.env (2, 1, g.f (120, 320), 0.0f, 100); g.mod (sVel, dF1, g.f (0.2f, 0.45f));
                g.amp (1, 300, g.f (0.5f, 0.8f), 80); g.comp (-16, 3, 2); } },
            { { "Wobble", "Wub", "Womp", "Yoyo", "Swing" }, [] (G& g)
              { g.osc (1, Saw, 0.8f, 0, 0, 0, 3, 15); g.osc (2, Square, 0.5f, -1); g.sub (0.45f);
                g.f1 (g.lg (220, 600), g.f (0.4f, 0.6f), 0.0f, g.f (0.2f, 0.4f)); g.lfo (1, g.pick ({ 0, 1, 2 }), g.f (1.5f, 6.0f), 0, 0.15f);
                g.mod (sLFO1, dF1, g.f (0.5f, 0.8f)); g.amp (3, 300, 1.0f, 120); g.comp (-18, 4, 4); } },
            { { "Pluck Bass", "Thump", "Pop", "Bounce", "Knock" }, [] (G& g)
              { g.osc (1, Saw, 0.8f, 0, 0, 0, 2, 10); g.sub (0.4f); g.f1 (g.lg (500, 1500), 0.25f, g.f (0.5f, 0.7f));
                g.env (2, 1, g.f (100, 220), 0.0f, 80); g.amp (1, g.f (250, 500), g.f (0.1f, 0.3f), 80); g.comp (-16, 3, 3); } },
            { { "FM Bass", "Tine Bass", "Metal", "Bell Bass", "Glass Bass" }, [] (G& g)
              { g.osc (1, Sine, 0.85f); g.osc (2, Sine, 0.0f, g.pick ({ 0, 1 }), 0); g.fm (g.f (0.3f, 0.7f)); g.sub (0.3f);
                g.f1 (g.lg (2000, 5000), 0.1f); g.amp (1, g.f (300, 600), g.f (0.5f, 0.8f), 100); } },
            { { "Growl", "Snarl", "Beast", "Roar", "Grind" }, [] (G& g)
              { g.osc (1, Saw, 0.8f); g.osc (2, Saw, 0.7f, 0, 0, g.f (8, 15)); g.fm (g.f (0.15f, 0.35f));
                g.f1 (g.lg (600, 1200), 0.3f); g.f2 (BP, g.lg (500, 1000), g.f (0.5f, 0.7f), 1, 0.5f);
                g.lfo (1, 1, g.f (0.8f, 3.0f)); g.mod (sLFO1, dF2, g.f (0.4f, 0.7f)); g.amp (4, 300, 1.0f, 150); g.comp (-20, 4, 4); } },
            { { "Square", "Pulse", "Hollow Bass", "Chip Bass", "Box" }, [] (G& g)
              { g.osc (1, Square, 0.75f); g.sub (0.5f, g.pick ({ 0, 2 })); g.f1 (g.lg (800, 2000), 0.2f, 0.3f);
                g.env (2, 1, 250, 0.2f, 100); g.amp (2, 400, 0.8f, 100); } },
            { { "Saw Stack", "Wall", "Unison Bass", "Supersaw Bass", "Stack" }, [] (G& g)
              { g.osc (1, Saw, 0.65f, 0, 0, 0, g.i (4, 6), g.f (15, 30)); g.osc (2, Saw, 0.5f, -1); g.f1 (g.lg (900, 2500), 0.15f, 0.4f);
                g.env (2, 2, 300, 0.3f, 150); g.amp (3, 400, 0.85f, 150); g.comp (-18, 3, 3); } },
            { { "Drive", "Fuzz", "Distortion", "Overdrive", "Crunch" }, [] (G& g)
              { g.osc (1, Saw, 0.8f); g.osc (2, Square, 0.5f, 0, 0, g.f (4, 9)); g.f1 (g.lg (500, 1400), g.f (0.25f, 0.45f), 0.3f, g.f (0.6f, 0.9f));
                g.env (2, 1, 300, 0.4f, 100); g.amp (2, 400, 0.9f, 120); g.comp (-22, 6, 6); } },
        } });

        // ------------------------------------------------------------ LEAD
        c.push_back ({ "Lead",
            { "Bright", "Screaming", "Soaring", "Silver", "Neon", "Laser", "Electric", "Sharp", "Singing", "Burning", "Crystal",
              "Golden", "Hyper", "Glowing", "Fierce", "Sweet", "Cutting", "Vivid", "Stellar", "Turbo", "Chrome", "Lucid", "Rising",
              "Wild", "Proud", "Hero", "Night", "Velvet", "Arcade", "Retro" },
        {
            { { "Supersaw", "Anthem", "Trance Lead", "Festival", "Stack Lead" }, [] (G& g)
              { g.osc (1, Saw, 0.65f, 0, 0, 0, g.i (6, 7), g.f (25, 45)); g.osc (2, Saw, 0.55f, 0, 0, g.f (4, 9), g.i (5, 7), g.f (25, 40));
                g.f1 (g.lg (7000, 15000), 0.1f); g.amp (6, 300, 0.9f, 220); g.echo (5, 0.3f, 0.2f, true); g.reverb (0.2f, 0.7f, 0.55f); } },
            { { "Square Lead", "Chip", "8-Bit", "Pulse Lead", "Arcade" }, [] (G& g)
              { g.osc (1, Square, 0.75f); g.osc (2, Square, g.f (0.3f, 0.5f), g.pick ({ 1, 0 }), g.pick ({ 0, 7, 12 }));
                g.f1 (g.lg (3000, 7000), 0.2f, 0.25f); g.amp (2, 200, 0.85f, 150); g.space (0.8f); } },
            { { "FM Lead", "Sync Lead", "Metal Lead", "Bite", "Edge" }, [] (G& g)
              { g.osc (1, Saw, 0.75f); g.osc (2, Sine, 0.0f, g.pick ({ 0, 1 }), g.pick ({ 0, 7 })); g.fm (g.f (0.2f, 0.5f));
                g.f1 (g.lg (3000, 8000), 0.2f, 0.3f); g.env (2, 1, 300, 0.4f, 200); g.amp (3, 300, 0.85f, 180); g.space(); } },
            { { "Vibrato Lead", "Singer", "Voice", "Wail", "Cry" }, [] (G& g)
              { g.osc (1, Saw, 0.75f); g.osc (2, g.pick ({ Square, Tri }), 0.3f, 1); g.f1 (g.lg (3500, 6000), 0.25f, 0.3f);
                g.vibrato (g.f (0.012f, 0.025f), g.f (4.8f, 6.0f), g.f (300, 800)); g.amp (8, 300, 0.85f, 220); g.space(); } },
            { { "Soft Lead", "Whisper", "Breath", "Mellow", "Dream" }, [] (G& g)
              { g.osc (1, g.pick ({ Tri, Sine }), 0.8f); g.osc (2, Sine, 0.25f, 1); g.noise (g.f (0.03f, 0.08f), 1);
                g.f1 (g.lg (2500, 5000), 0.1f); g.vibrato (0.01f, 5.0f, 600); g.amp (g.f (20, 80), 300, 0.85f, 300); g.reverb (0.3f, 0.7f, 0.6f); } },
            { { "Hard Lead", "Razor", "Saw Lead", "Distort", "Blade" }, [] (G& g)
              { g.osc (1, Saw, 0.8f, 0, 0, 0, 2, 12); g.osc (2, Saw, 0.5f, 0, g.pick ({ 7, 12 })); g.f1 (g.lg (2500, 6000), g.f (0.35f, 0.5f), 0.3f, g.f (0.4f, 0.7f));
                g.env (2, 1, 250, 0.4f, 150); g.amp (2, 300, 0.9f, 150); g.comp (-18, 4, 4); g.space (0.7f); } },
            { { "Detuned Lead", "Duo", "Twin", "Pair", "Drift" }, [] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, -g.f (6, 14)); g.osc (2, Saw, 0.7f, 0, 0, g.f (6, 14)); g.f1 (g.lg (4000, 9000), 0.15f);
                g.amp (5, 300, 0.85f, 200); g.space(); } },
            { { "Whistle", "Theremin", "Sine Lead", "Ghost", "Spirit" }, [] (G& g)
              { g.osc (1, Sine, 0.85f); g.osc (2, Sine, 0.15f, 1); g.vibrato (g.f (0.015f, 0.03f), g.f (5, 6.5f), g.f (200, 700));
                g.amp (g.f (30, 120), 200, 0.95f, 300); g.reverb (0.3f, 0.75f, 0.6f); } },
            { { "Resonant Lead", "Wah", "Talker", "Quack", "Formant" }, [] (G& g)
              { g.osc (1, Saw, 0.8f); g.f1 (g.lg (1500, 3500), g.f (0.55f, 0.75f), g.f (0.35f, 0.6f)); g.env (2, g.f (5, 60), g.f (200, 500), 0.2f, 200);
                g.amp (3, 300, 0.85f, 200); g.space (0.8f); } },
            { { "Phase Lead", "Jet", "Swirl", "Flange Lead", "Orbit" }, [] (G& g)
              { g.osc (1, g.pick ({ Square, Saw }), 0.75f); g.osc (2, Saw, 0.35f, 0, 0, 8); g.f1 (g.lg (3500, 7000), 0.2f);
                if (g.p (0.5f)) g.phaser (g.f (0.3f, 0.5f), g.f (0.2f, 0.8f)); else g.flanger (g.f (0.3f, 0.45f), g.f (0.1f, 0.5f), g.f (0.3f, 0.6f));
                g.amp (4, 300, 0.85f, 200); g.reverb (0.18f, 0.6f, 0.5f); } },
        } });

        // ------------------------------------------------------------ PAD
        c.push_back ({ "Pad",
            { "Warm", "Floating", "Endless", "Distant", "Soft", "Glacial", "Velvet", "Ambient", "Ethereal", "Hazy", "Dusk", "Dawn",
              "Ocean", "Cloud", "Celestial", "Slow", "Lunar", "Misty", "Silk", "Gentle", "Aurora", "Frozen", "Golden", "Deep", "Wide",
              "Nordic", "Sacred", "Calm", "Starlit", "Dreamy" },
        {
            { { "Pad", "Blanket", "Wash", "Bed", "Haze" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 5, g.f (15, 25)); g.osc (2, Saw, 0.55f, 0, 0, g.f (4, 8), 5, g.f (20, 30));
                g.f1 (g.lg (1200, 3000), 0.1f); g.lfo (1, 0, g.f (0.1f, 0.6f), 0, 0.3f); g.mod (sLFO1, dF1, g.f (0.15f, 0.3f));
                g.amp (g.f (600, 1500), 1000, 0.85f, g.f (1000, 2500)); g.reverb (g.f (0.3f, 0.4f), 0.8f, 0.65f); } },
            { { "Glass", "Crystal", "Ice", "Prism", "Halo" }, [] (G& g)
              { g.osc (1, Tri, 0.6f); g.osc (2, Sine, 0.4f, 0, g.pick ({ 7, 12, 5 })); g.set (pid::oscOn (3), 1); g.set (pid::oscWave (3), Sine);
                g.set (pid::oscLevel (3), 0.25f); g.set (pid::oscOct (3), 1); g.f1 (12000, 0.1f);
                g.amp (g.f (800, 1800), 800, 0.7f, g.f (1500, 3000)); g.reverb (g.f (0.35f, 0.45f), 0.9f, 0.7f, 40); } },
            { { "Evolver", "Motion", "Morph", "Drift", "Tide" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 5, 25); g.osc (2, Tri, 0.45f, 0, 7, 0, 3);
                g.f1 (g.lg (1200, 2500), 0.2f); g.f2 (BP, g.lg (800, 2000), 0.4f, 1, 0.4f);
                g.lfo (1, 0, g.f (0.05f, 0.2f), 0, 0.4f); g.mod (sLFO1, dF1, 0.35f);
                g.lfo (3, 1, g.f (0.03f, 0.1f)); g.mod (sLFO3, dF2, 0.5f); g.lfo (4, 0, 0.2f); g.mod (sLFO4, dPan, 0.4f);
                g.amp (1500, 1500, 0.85f, 2500); g.chorus (0.25f, 0.1f); g.reverb (0.4f, 0.9f, 0.75f); } },
            { { "Choir", "Voices", "Aah", "Chant", "Hymn" }, [] (G& g)
              { g.osc (1, Saw, 0.5f, 0, 0, 0, 5, 15); g.osc (2, Saw, 0.4f, -1, 0, 0, 5, 12); g.f1 (3500, 0.1f);
                g.f2 (BP, g.lg (650, 1100), g.f (0.45f, 0.6f)); g.vibrato (0.008f, 5.0f, 800);
                g.amp (g.f (500, 1000), 800, 0.9f, 1500); g.reverb (0.4f, 0.85f, 0.7f); } },
            { { "Dark Pad", "Shadow", "Abyss", "Cave", "Undertow" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, -1, 0, 0, 5, 20); g.osc (2, Saw, 0.5f, 0, 0, 7, 3, 25); g.f1 (g.lg (500, 1100), 0.25f);
                g.lfo (1, 1, 0.08f); g.mod (sLFO1, dF1, 0.3f); g.amp (1200, 1000, 0.9f, 2500); g.reverb (0.35f, 0.9f, 0.8f); } },
            { { "Shimmer", "Sparkle", "Glow", "Light", "Heaven" }, [] (G& g)
              { g.osc (1, Tri, 0.55f, 0, 0, 0, 3, 12); g.osc (2, Saw, 0.3f, 1, 0, 0, 3, 18); g.f1 (g.lg (5000, 12000), 0.1f);
                g.amp (1000, 1000, 0.8f, 2500); g.echo (5, 0.45f, 0.25f, true); g.reverb (0.45f, 1.0f, 0.8f, 60); } },
            { { "String Pad", "Silk Pad", "Section", "Orchestra Pad", "Cinema" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 7, 20); g.osc (2, Saw, 0.5f, 0, 0, -5, 5, 25); g.f1 (g.lg (2000, 4000), 0.1f);
                g.amp (g.f (600, 1200), 800, 0.9f, 1200); g.chorus (0.3f, 0.15f); g.reverb (0.35f, 0.8f, 0.65f); } },
            { { "Air", "Breath Pad", "Wind Pad", "Atmosphere", "Vapor" }, [] (G& g)
              { g.osc (1, Saw, 0.45f, 0, 0, 0, 5, 20); g.noise (g.f (0.15f, 0.3f), 1); g.f1 (3000, 0.1f);
                g.f2 (BP, g.lg (800, 2500), 0.5f, 1, 0.5f); g.lfo (1, 0, g.f (0.05f, 0.15f)); g.mod (sLFO1, dF2, 0.6f);
                g.amp (1500, 1000, 0.9f, 2500); g.reverb (0.45f, 1.0f, 0.8f); } },
            { { "PWM Pad", "Pulse Pad", "Analog Pad", "Vintage", "Juno" }, [] (G& g)
              { g.osc (1, Square, 0.5f, 0, 0, 0, 5, 18); g.osc (2, Saw, 0.4f, 0, 0, 5, 3, 15); g.f1 (g.lg (1500, 3500), 0.15f);
                g.amp (800, 800, 0.85f, 1500); g.phaser (0.35f, 0.15f); g.chorus (0.25f); g.reverb (0.3f, 0.7f, 0.6f); } },
            { { "Sweep", "Bloom", "Opening", "Sunrise", "Unfold" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 5, 22); g.osc (2, Saw, 0.5f, 0, 0, 7, 5, 22); g.f1 (g.lg (250, 600), 0.3f);
                g.env (3, g.f (2000, 5000), 1000, 1.0f, 2000); g.mod (sENV3, dF1, g.f (0.6f, 0.85f));
                g.amp (800, 1000, 0.9f, 2500); g.reverb (0.4f, 0.9f, 0.7f); } },
        } });

        // ------------------------------------------------------------ PLUCK
        c.push_back ({ "Pluck",
            { "Bright", "Short", "Crisp", "Tiny", "Sparkling", "Clean", "Happy", "Bouncy", "Shiny", "Pop", "Sharp", "Wooden",
              "Digital", "Funky", "Glassy", "Little", "Quick", "Snappy", "Candy", "Island", "Sunny", "Pixel", "Morning", "Tropical",
              "Pearl", "Mint", "Neon", "Paper", "Clockwork", "Zen" },
        {
            { { "Saw Pluck", "Pluck", "Stab", "Pick", "Pling" }, [] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, 0, g.i (1, 5), g.f (10, 25)); g.f1 (g.lg (1500, 3500), 0.3f, g.f (0.5f, 0.7f));
                g.env (2, 1, g.f (100, 200), 0.0f, 100); g.amp (1, g.f (200, 350), 0.0f, 180); g.space(); } },
            { { "Square Pluck", "Bleep", "Blip", "Chip Pluck", "Tick" }, [] (G& g)
              { g.osc (1, Square, 0.7f); g.f1 (g.lg (2000, 5000), 0.25f, 0.5f); g.env (2, 1, 150, 0.0f, 80);
                g.amp (1, g.f (150, 300), 0.0f, 120); g.space(); } },
            { { "Bell Pluck", "Chime", "Ding", "Tine", "Bell" }, [] (G& g)
              { g.osc (1, Sine, 0.75f); g.osc (2, Sine, 0.0f, 1, g.pick ({ 7, 0, 4 })); g.fm (g.f (0.35f, 0.6f));
                g.amp (1, g.f (500, 1100), 0.0f, 600); g.echo (4, 0.3f, 0.18f, true); g.reverb (0.25f, 0.7f, 0.55f); } },
            { { "Echo Pluck", "Ping", "Delay Pluck", "Bounce", "Ripple" }, [] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, 0, 3, 15); g.f1 (2500, 0.3f, 0.6f); g.env (2, 1, 140, 0.0f, 100);
                g.amp (1, 220, 0.0f, 150); g.echo (g.pick ({ 4, 5, 6 }), g.f (0.35f, 0.5f), g.f (0.25f, 0.35f), true); g.reverb (0.18f, 0.6f, 0.5f); } },
            { { "Soft Pluck", "Harp", "Mellow Pluck", "Feather", "Drop" }, [] (G& g)
              { g.osc (1, Tri, 0.8f); g.osc (2, Sine, 0.3f, 1); g.f1 (g.lg (3000, 6000), 0.1f, 0.3f); g.env (2, 1, 250, 0.0f, 150);
                g.amp (1, g.f (400, 800), 0.0f, 400); g.reverb (0.25f, 0.7f, 0.55f); } },
            { { "Resonant Pluck", "Zap Pluck", "Twang", "Boing", "Sproing" }, [] (G& g)
              { g.osc (1, Saw, 0.75f); g.f1 (g.lg (800, 2000), g.f (0.55f, 0.75f), g.f (0.5f, 0.75f)); g.env (2, 1, g.f (120, 250), 0.0f, 100);
                g.amp (1, 250, 0.0f, 150); g.space (0.8f); } },
            { { "Chord Pluck", "House Pluck", "Stab Pluck", "Hook", "Riff" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 25); g.osc (2, Square, 0.35f, 0, 0, 0, 3, 15); g.f1 (g.lg (1500, 3000), 0.25f, 0.6f);
                g.env (2, 1, 160, 0.0f, 100); g.amp (1, 260, 0.0f, 150); g.echo (5, 0.3f, 0.18f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Noise Pluck", "Click", "Snap", "Crackle", "Spark" }, [] (G& g)
              { g.osc (1, Saw, 0.6f); g.noise (0.35f, 0); g.f1 (g.lg (2000, 5000), 0.3f, 0.6f); g.env (2, 1, 80, 0.0f, 60);
                g.amp (1, 180, 0.0f, 100); g.space (0.8f); } },
            { { "Phase Pluck", "Swirl Pluck", "Liquid", "Ripple Pluck", "Wave" }, [] (G& g)
              { g.osc (1, g.pick ({ Saw, Square }), 0.7f, 0, 0, 0, 2, 12); g.f1 (2500, 0.3f, 0.6f); g.env (2, 1, 160, 0.0f, 100);
                g.amp (1, 280, 0.0f, 160); g.phaser (0.4f, g.f (0.2f, 0.8f)); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Marimba", "Mallet", "Kalimba", "Wood", "Log" }, [] (G& g)
              { g.osc (1, Sine, 0.85f); g.osc (2, Sine, 0.2f, 2); g.amp (1, g.f (250, 500), 0.0f, 250); g.env (2, 1, 60, 0, 50);
                g.mod (sENV2, dPitch, 0.02f); g.reverb (0.2f, 0.5f, 0.45f); } },
        } });

        // ------------------------------------------------------------ KEYS
        c.push_back ({ "Keys",
            { "Classic", "Vintage", "Warm", "Smooth", "Mellow", "Bright", "Studio", "Dusty", "Lounge", "Jazz", "Soul", "Late Night",
              "Sunday", "Tape", "Velvet", "Club", "Bar", "Chapel", "Gospel", "Electric", "Home", "Attic", "Basement", "Cafe",
              "Rainy", "Sepia", "Boogie", "Smoky", "Gentle", "Silver" },
        {
            { { "EP", "Rhodes", "E-Piano", "Tines", "Suitcase" }, [] (G& g)
              { g.osc (1, Tri, 0.7f); g.osc (2, Sine, 0.3f, 1); g.fm (g.f (0.0f, 0.15f)); g.f1 (g.lg (5000, 9000), 0.1f);
                g.amp (3, g.f (500, 900), g.f (0.3f, 0.5f), 300); if (g.p (0.5f)) g.phaser (0.3f, 0.3f); g.reverb (0.18f, 0.6f, 0.5f); } },
            { { "FM Bells", "Bells", "DX Keys", "Tubular", "Chimes" }, [] (G& g)
              { g.osc (1, Sine, 0.7f); g.osc (2, Sine, 0.0f, 1, g.pick ({ 7, 0, 4 })); g.fm (g.f (0.4f, 0.7f));
                g.amp (2, g.f (900, 1600), 0.0f, 900); g.echo (4, 0.3f, 0.18f, true); g.reverb (0.28f, 0.7f, 0.6f); } },
            { { "Clav", "Funk Keys", "Clavi", "Stringer", "Picker" }, [] (G& g)
              { g.osc (1, Square, 0.7f); g.osc (2, Saw, 0.3f, 0, 12); g.f1 (g.lg (3500, 6000), 0.2f, 0.4f); g.f2 (HP, g.lg (300, 600), 0.1f);
                g.env (2, 1, 150, 0.0f, 80); g.amp (1, 250, 0.2f, 80); if (g.p (0.5f)) g.phaser (0.25f, 0.4f); } },
            { { "Organ", "Drawbar", "Tonewheel", "Church", "B3" }, [] (G& g)
              { g.osc (1, Sine, 0.6f); g.osc (2, Sine, 0.4f, 1); g.set (pid::oscOn (3), 1); g.set (pid::oscWave (3), Sine);
                g.set (pid::oscOct (3), 1); g.set (pid::oscSemi (3), 7); g.set (pid::oscLevel (3), g.f (0.2f, 0.4f));
                g.amp (5, 100, 1.0f, 60); g.chorus (g.f (0.3f, 0.45f), g.f (0.6f, 1.2f)); g.reverb (0.15f, 0.6f, 0.45f); } },
            { { "Vibes", "Vibraphone", "Mallet Keys", "Lounge Vibes", "Glock" }, [] (G& g)
              { g.osc (1, Sine, 0.8f); g.osc (2, Sine, 0.15f, 2); g.lfo (3, 0, g.f (4, 6)); g.mod (sLFO3, dAmp, g.f (0.15f, 0.3f));
                g.amp (1, g.f (1200, 2000), 0.0f, 1200); g.reverb (0.25f, 0.7f, 0.55f); } },
            { { "Celesta", "Toy Piano", "Music Box", "Lullaby", "Twinkle" }, [] (G& g)
              { g.osc (1, Sine, 0.7f); g.osc (2, Square, 0.15f, 1); g.amp (1, g.f (500, 900), 0.0f, 500);
                g.echo (4, 0.3f, 0.15f, true); g.reverb (0.3f, 0.7f, 0.6f); } },
            { { "Synth Piano", "Digital Piano", "Poly Keys", "House Piano", "Keys" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 2, 8); g.osc (2, Tri, 0.4f, 1); g.f1 (g.lg (2000, 4000), 0.15f, 0.5f);
                g.env (2, 1, 400, 0.1f, 200); g.amp (1, 700, 0.2f, 300); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Harpsichord", "Baroque", "Cembalo", "Court", "Plucked Keys" }, [] (G& g)
              { g.osc (1, Square, 0.55f); g.osc (2, Saw, 0.4f, 1); g.f1 (g.lg (5000, 9000), 0.15f, 0.3f); g.f2 (HP, 350, 0.1f);
                g.env (2, 1, 300, 0.0f, 150); g.amp (1, g.f (600, 900), 0.0f, 300); g.reverb (0.18f, 0.5f, 0.45f); } },
            { { "Mallet", "Woodblock", "Xylo", "Plonk", "Tap" }, [] (G& g)
              { g.osc (1, Sine, 0.8f); g.osc (2, Sine, 0.0f, 2, g.pick ({ 0, 7 })); g.fm (g.f (0.15f, 0.35f));
                g.amp (1, g.f (300, 600), 0.0f, 300); g.reverb (0.2f, 0.5f, 0.45f); } },
            { { "Wurli", "Reed Keys", "Barroom", "Soul Keys", "Tremolo Keys" }, [] (G& g)
              { g.osc (1, Tri, 0.7f); g.osc (2, Square, 0.2f, 1); g.f1 (g.lg (3000, 5000), 0.15f, 0.2f, g.f (0.2f, 0.4f));
                g.lfo (3, 0, g.f (4, 6)); g.mod (sLFO3, dAmp, g.f (0.15f, 0.3f)); g.amp (2, 700, 0.4f, 300); g.reverb (0.15f, 0.5f, 0.45f); } },
        } });

        // ------------------------------------------------------------ STRINGS
        c.push_back ({ "Strings",
            { "Lush", "Grand", "Epic", "Tender", "Intimate", "Cinematic", "Silk", "Velvet", "Romantic", "Noble", "Dark", "Bright",
              "Vienna", "Royal", "Autumn", "Winter", "Spring", "Summer", "Golden", "Old", "Hall", "Chamber", "Soft", "Wide",
              "Dramatic", "Heroic", "Gentle", "Nocturne", "Sonata", "Elegy" },
        {
            { { "Ensemble", "Section", "Orchestra", "Strings", "Tutti" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 18); g.osc (2, Saw, 0.6f, 0, 0, 4, 5, 22); g.f1 (g.lg (4000, 7000), 0.1f);
                g.vibrato (0.006f, 5.0f, 800); g.amp (g.f (250, 500), 600, 0.85f, 500); g.reverb (0.3f, 0.75f, 0.6f); } },
            { { "Slow Strings", "Legato", "Adagio", "Swell", "Long Bow" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 7, 20); g.osc (2, Saw, 0.5f, 0, 0, -6, 5, 25); g.f1 (g.lg (2000, 3500), 0.1f, 0.35f);
                g.env (2, 900, 1500, 0.6f, 900); g.vibrato (0.01f, 5.0f, 1000); g.amp (g.f (700, 1200), 800, 0.9f, 900); g.reverb (0.35f, 0.8f, 0.65f); } },
            { { "Pizzicato", "Pizz", "Plucked Strings", "Pizz Section", "Pluck Bow" }, [] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, 0, 3, 10); g.f1 (g.lg (1800, 3000), 0.2f, 0.6f); g.env (2, 1, 140, 0.0f, 100);
                g.amp (1, g.f (220, 320), 0.0f, 200); g.reverb (0.3f, 0.7f, 0.55f); } },
            { { "Cello", "Solo Cello", "Viola", "Low Strings", "Bass Bow" }, [] (G& g)
              { g.osc (1, Saw, 0.75f, -1, 0, 0, 2, 8); g.f1 (g.lg (1500, 2800), 0.2f, 0.25f); g.env (2, 150, 600, 0.6f, 400);
                g.vibrato (0.012f, 5.2f, 500); g.amp (g.f (120, 250), 500, 0.9f, 400); g.reverb (0.25f, 0.7f, 0.55f); } },
            { { "Tremolo", "Tremolando", "Shiver", "Flutter", "Nervous" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 18); g.osc (2, Saw, 0.5f, 0, 0, 5, 5, 20); g.f1 (4000, 0.1f);
                g.lfo (3, 1, g.f (7, 11)); g.mod (sLFO3, dAmp, g.f (0.4f, 0.6f)); g.amp (200, 600, 0.9f, 500); g.reverb (0.3f, 0.75f, 0.6f); } },
            { { "Violins", "High Strings", "Bright Bow", "Soprano", "Air Strings" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 5, 15); g.osc (2, Saw, 0.4f, 1, 0, 0, 3, 18); g.f1 (g.lg (6000, 10000), 0.1f);
                g.f2 (HP, 250, 0.1f); g.vibrato (0.008f, 5.5f, 600); g.amp (300, 600, 0.85f, 600); g.reverb (0.35f, 0.8f, 0.65f); } },
            { { "Dark Strings", "Basses", "Low Section", "Requiem", "Shadow Bow" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, -1, 0, 0, 5, 18); g.osc (2, Saw, 0.5f, 0, 0, 0, 5, 20); g.f1 (g.lg (900, 1600), 0.15f);
                g.amp (500, 800, 0.9f, 800); g.reverb (0.35f, 0.85f, 0.7f); } },
            { { "Staccato", "Spiccato", "Marcato", "Short Bow", "Chop" }, [] (G& g)
              { g.osc (1, Saw, 0.65f, 0, 0, 0, 5, 18); g.osc (2, Saw, 0.5f, 0, 0, 5, 3, 18); g.f1 (g.lg (3000, 5000), 0.15f, 0.3f);
                g.env (2, 5, 150, 0.2f, 100); g.amp (8, g.f (150, 250), 0.0f, 120); g.reverb (0.25f, 0.65f, 0.5f); } },
            { { "Chamber", "Quartet", "Small Strings", "Salon", "Trio" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 2, 10); g.osc (2, Saw, 0.5f, 0, 0, 6, 2, 12); g.f1 (g.lg (3000, 5000), 0.15f);
                g.vibrato (0.01f, 5.2f, 500); g.amp (180, 600, 0.85f, 500); g.reverb (0.25f, 0.6f, 0.5f); } },
            { { "Synth Strings", "Solina", "String Machine", "Analog Strings", "Retro Strings" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 20); g.osc (2, Square, 0.3f, 1, 0, 0, 3, 15); g.f1 (g.lg (3000, 6000), 0.1f);
                g.amp (400, 600, 0.85f, 700); g.chorus (0.4f, 0.25f); g.reverb (0.25f, 0.7f, 0.55f); } },
        } });

        // ------------------------------------------------------------ FLUTE
        c.push_back ({ "Flute",
            { "Breathy", "Airy", "Wooden", "Bamboo", "Forest", "Mountain", "Morning", "Shepherd", "Pastoral", "Silver", "Soft",
              "Gentle", "Misty", "Highland", "Temple", "Distant", "Folk", "Nordic", "Andean", "Celtic", "Eastern", "Garden",
              "River", "Meadow", "Echoing", "Sacred", "Wind", "Cloud", "Lake", "Valley" },
        {
            { { "Flute", "Concert Flute", "Breath Flute", "Air Flute", "Solo Flute" }, [] (G& g)
              { g.osc (1, Tri, 0.7f); g.osc (2, Sine, 0.15f, 1); g.noise (g.f (0.08f, 0.16f), 1); g.f1 (g.lg (3000, 4500), 0.1f);
                g.vibrato (0.012f, 5.2f, 500); g.lfo (3, 0, 4.7f); g.mod (sLFO3, dAmp, 0.08f); g.amp (80, 300, 0.85f, 250); g.reverb (0.3f, 0.7f, 0.55f); } },
            { { "Pan Flute", "Pan Pipes", "Pipes", "Syrinx", "Reed Pipes" }, [] (G& g)
              { g.osc (1, Sine, 0.75f); g.noise (g.f (0.15f, 0.22f)); g.f1 (6000, 0.1f); g.f2 (BP, g.lg (280, 420), 0.3f, 1, 0.4f, 0, 0, 1.0f);
                g.vibrato (0.008f, 5.0f, 300); g.amp (40, 200, 0.8f, 300); g.echo (2, 0.25f, 0.12f); g.reverb (0.35f, 0.75f, 0.6f); } },
            { { "Shakuhachi", "Bamboo Flute", "Zen Flute", "Temple Flute", "Dizi" }, [] (G& g)
              { g.osc (1, Sine, 0.7f); g.noise (g.f (0.2f, 0.3f), 1); g.f1 (4000, 0.1f); g.f2 (BP, g.lg (280, 420), 0.35f, 1, 0.35f, 0, 0, 1.0f);
                g.vibrato (0.015f, 4.5f, 700); g.amp (120, 400, 0.8f, 400); g.reverb (0.4f, 0.85f, 0.7f); } },
            { { "Ocarina", "Clay Flute", "Vessel", "Pocket Flute", "Round Flute" }, [] (G& g)
              { g.osc (1, Sine, 0.8f); g.osc (2, Tri, 0.15f); g.noise (0.05f, 1); g.vibrato (0.01f, 5.5f, 400);
                g.amp (50, 300, 0.9f, 200); g.reverb (0.3f, 0.6f, 0.5f); } },
            { { "Recorder", "Alto Recorder", "Consort", "School Flute", "Fipple" }, [] (G& g)
              { g.osc (1, Tri, 0.75f); g.osc (2, Sine, 0.2f, 1); g.noise (0.06f, 1); g.f1 (g.lg (3500, 5000), 0.1f);
                g.amp (30, 200, 0.9f, 150); g.reverb (0.2f, 0.5f, 0.45f); } },
            { { "Whistle", "Tin Whistle", "Penny Whistle", "Irish Whistle", "Bird" }, [] (G& g)
              { g.osc (1, Sine, 0.8f, 1); g.noise (0.06f, 1); g.vibrato (g.f (0.01f, 0.02f), g.f (5.5f, 6.5f), 300);
                g.amp (20, 200, 0.9f, 150); g.reverb (0.25f, 0.6f, 0.5f); } },
            { { "Fife", "Piccolo", "March Fife", "Bright Flute", "Military" }, [] (G& g)
              { g.osc (1, Tri, 0.7f, 1); g.osc (2, Square, 0.08f, 1); g.noise (0.05f, 1); g.f1 (g.lg (5000, 8000), 0.1f);
                g.amp (20, 150, 0.85f, 120); g.reverb (0.2f, 0.5f, 0.45f); } },
            { { "Synth Flute", "Square Flute", "Digital Flute", "Chip Flute", "Retro Flute" }, [] (G& g)
              { g.osc (1, Square, 0.6f); g.f1 (g.lg (1500, 3000), 0.15f); g.noise (0.05f, 1); g.vibrato (0.01f, 5.0f, 500);
                g.amp (40, 300, 0.85f, 250); g.echo (4, 0.25f, 0.15f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Air", "Breath", "Sigh", "Ghost Flute", "Wind Flute" }, [] (G& g)
              { g.osc (1, Sine, 0.6f); g.noise (g.f (0.35f, 0.5f), 1); g.f1 (5000, 0.1f); g.f2 (BP, g.lg (280, 500), 0.35f, 1, 0.45f, 0, 0, 1.0f);
                g.amp (150, 400, 0.85f, 400); g.reverb (0.4f, 0.85f, 0.7f); } },
            { { "Overblown", "Jazz Flute", "Growl Flute", "Flutter Tongue", "Edge Flute" }, [] (G& g)
              { g.osc (1, Tri, 0.7f); g.osc (2, Sine, 0.0f, 1); g.fm (g.f (0.05f, 0.15f)); g.noise (0.15f, 1);
                g.lfo (3, 4, g.f (12, 20)); g.mod (sLFO3, dAmp, g.f (0.05f, 0.15f)); g.vibrato (0.012f, 5.2f, 400);
                g.amp (60, 300, 0.85f, 250); g.reverb (0.25f, 0.6f, 0.5f); } },
        } });

        // ------------------------------------------------------------ DRUMS
        c.push_back ({ "Drums",
            { "Punchy", "Deep", "Tight", "Fat", "Dry", "Room", "Vintage", "808", "909", "Lo-Fi", "Studio", "Crisp", "Dusty", "Hard",
              "Soft", "Boomy", "Snappy", "Dirty", "Clean", "Analog", "Garage", "Club", "Tape", "Short", "Long", "Heavy", "Light",
              "Bright", "Dark", "Big" },
        {
            { { "Kick", "Bass Drum", "Boom", "Thud", "Kick Drum" }, [] (G& g)
              { g.osc (1, Sine, 0.95f, -1); g.phase (1, 90); g.env (2, 1, g.f (40, 90), 0.0f, 50); g.mod (sENV2, dPitch, g.f (0.45f, 0.75f));
                if (g.p (0.4f)) g.noise (0.08f, 0); g.amp (1, g.f (250, 700), 0.0f, 100); g.comp (-12, 4, 3); } },
            { { "Snare", "Snare Drum", "Crack", "Backbeat", "Rimshot Snare" }, [] (G& g)
              { g.osc (1, Tri, 0.5f); g.env (2, 1, 40, 0.0f, 30); g.mod (sENV2, dPitch, 0.25f); g.noise (g.f (0.6f, 0.8f));
                g.f1 (g.lg (7000, 12000), 0.1f); g.f2 (HP, g.lg (400, 900), 0.15f); g.amp (1, g.f (140, 260), 0.0f, 120);
                g.comp (-14, 3, 3); if (g.p (0.6f)) g.reverb (0.18f, 0.4f, 0.3f); } },
            { { "Clap", "Handclap", "Clapper", "Slap", "Smack" }, [] (G& g)
              { g.noise (0.95f); g.f2 (BP, g.lg (1000, 1800), 0.2f, 0, 0.5f, 0); g.f1 (9000, 0.1f); g.lfo (3, 3, g.f (25, 38)); g.mod (sLFO3, dAmp, 0.4f);
                g.amp (1, g.f (150, 250), 0.0f, 150); g.reverb (g.f (0.15f, 0.25f), 0.4f, 0.3f); } },
            { { "Closed Hat", "Hi-Hat", "Tick Hat", "Chick", "Tight Hat" }, [] (G& g)
              { g.noise (0.8f); g.f1 (18000, 0.1f); g.f2 (HP, g.lg (6000, 9000), 0.2f); g.amp (1, g.f (40, 90), 0.0f, 50); } },
            { { "Open Hat", "Open Hi-Hat", "Sizzle", "Hiss Hat", "Wash Hat" }, [] (G& g)
              { g.noise (0.75f); g.f1 (18000, 0.1f); g.f2 (HP, g.lg (5500, 8000), 0.2f); g.amp (1, g.f (250, 500), 0.0f, 250); } },
            { { "Tom", "Floor Tom", "Rack Tom", "Low Tom", "Hi Tom" }, [] (G& g)
              { g.osc (1, Sine, 0.9f, g.pick ({ -1, 0 })); g.phase (1, 90); g.env (2, 1, g.f (90, 160), 0.0f, 50); g.mod (sENV2, dPitch, g.f (0.2f, 0.35f));
                g.noise (0.1f); g.amp (1, g.f (350, 600), 0.0f, 200); g.reverb (0.15f, 0.4f, 0.35f); } },
            { { "Rim", "Rimshot", "Sidestick", "Click", "Woodblock" }, [] (G& g)
              { g.osc (1, Sine, 0.8f, 1); g.env (2, 1, 15, 0.0f, 10); g.mod (sENV2, dPitch, 0.3f); g.noise (0.3f);
                g.f2 (BP, g.lg (1500, 3000), 0.4f, 1, 0.5f); g.amp (1, g.f (40, 80), 0.0f, 40); } },
            { { "Cymbal", "Crash", "Ride", "Splash", "China" }, [] (G& g)
              { g.noise (0.6f); g.set (pid::oscOn (3), 1); g.set (pid::oscWave (3), Square); g.set (pid::oscLevel (3), 0.0f); g.set (pid::oscOct (3), 2);
                g.set (pid::oscOn (4), 1); g.set (pid::oscWave (4), Square); g.set (pid::oscLevel (4), 0.0f); g.set (pid::oscOct (4), 2); g.set (pid::oscSemi (4), 7);
                g.ring (g.f (0.3f, 0.5f)); g.f2 (HP, g.lg (4000, 7000), 0.2f); g.amp (1, g.f (800, 1800), 0.0f, 800); g.reverb (0.2f, 0.5f, 0.4f); } },
            { { "Cowbell", "Bell Perc", "Agogo", "Clank", "Ping Perc" }, [] (G& g)
              { g.osc (1, Square, 0.5f, 1); g.osc (2, Square, 0.45f, 1, g.pick ({ 6, 7 })); g.f2 (BP, g.lg (800, 1400), 0.5f);
                g.amp (1, g.f (150, 300), 0.0f, 120); } },
            { { "Perc", "Blip", "Zap Perc", "Pop Perc", "Laser Perc" }, [] (G& g)
              { g.osc (1, Sine, 0.85f, g.pick ({ 0, 1 })); g.osc (2, Sine, 0.0f, 1); g.fm (g.f (0.2f, 0.5f));
                g.env (2, 1, g.f (20, 60), 0.0f, 30); g.mod (sENV2, dPitch, g.f (0.3f, 0.7f)); g.amp (1, g.f (100, 250), 0.0f, 100); } },
        } });

        // ------------------------------------------------------------ FX
        c.push_back ({ "FX",
            { "Cosmic", "Alien", "Haunted", "Sci-Fi", "Industrial", "Galactic", "Broken", "Mutant", "Electric", "Toxic", "Orbital",
              "Quantum", "Rusty", "Robotic", "Distant", "Dark", "Glitchy", "Atomic", "Lunar", "Stellar", "Nebula", "Radio",
              "Cyber", "Machine", "Phantom", "Storm", "Signal", "Static", "Warp", "Void" },
        {
            { { "Riser", "Uplifter", "Build", "Ascend", "Rocket" }, [] (G& g)
              { g.osc (1, Saw, 0.35f, 0, 0, 0, 7, 40); g.noise (0.5f, 1); g.f1 (g.lg (150, 300), 0.4f);
                g.env (3, g.f (3000, 6000), 1, 1.0f, 1500); g.mod (sENV3, dF1, 0.9f); g.mod (sENV3, dPitch, g.f (0.15f, 0.3f));
                g.amp (g.f (2000, 4000), 1, 1.0f, 1500); g.echo (4, 0.35f, 0.25f, true); g.reverb (0.4f, 0.9f, 0.7f); } },
            { { "Downlifter", "Fall", "Drop", "Descent", "Dive" }, [] (G& g)
              { g.osc (1, Saw, 0.5f, 1, 0, 0, 5, 30); g.noise (0.3f, 1); g.f1 (6000, 0.3f); g.env (3, 1, g.f (2000, 4000), 0.0f, 1500);
                g.mod (sENV3, dPitch, g.f (0.3f, 0.6f)); g.mod (sENV3, dF1, 0.5f); g.amp (1, 4000, 0.0f, 1500); g.reverb (0.4f, 0.9f, 0.7f); } },
            { { "Laser", "Zap", "Pew", "Blaster", "Ray" }, [] (G& g)
              { g.osc (1, g.pick ({ Saw, Square }), 0.7f); g.env (2, 1, g.f (120, 300), 0.0f, 100); g.mod (sENV2, dPitch, g.f (0.6f, 0.95f));
                g.f1 (8000, 0.4f); g.amp (1, 300, 0.0f, 200); g.echo (5, 0.4f, 0.3f, true); } },
            { { "Noise Sweep", "Whoosh", "Sweep", "Swoosh", "Jet" }, [] (G& g)
              { g.noise (0.95f, g.pick ({ 0, 1 })); g.f1 (g.lg (500, 1500), 0.4f); g.lfo (1, g.pick ({ 0, 1 }), g.f (0.1f, 0.4f));
                g.mod (sLFO1, dF1, g.f (0.6f, 0.85f)); g.amp (400, 500, 1.0f, 1000); g.reverb (0.45f, 1.0f, 0.8f); } },
            { { "Computer", "Data", "Bleeps", "Mainframe", "Terminal" }, [] (G& g)
              { g.osc (1, Square, 0.6f, 1); g.lfo (1, 4, g.f (6, 14)); g.mod (sLFO1, dPitch, g.f (0.2f, 0.5f)); g.mod (sLFO1, dF1, 0.3f);
                g.f1 (4000, 0.4f); g.amp (5, 300, 1.0f, 300); g.echo (6, 0.4f, 0.25f, true); } },
            { { "Drone", "Hum", "Engine", "Monolith", "Generator" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, -1, 0, 0, 5, 30); g.osc (2, Square, 0.4f, -1, 7, 0, 3, 20); g.f1 (g.lg (300, 900), 0.4f);
                g.lfo (1, 0, g.f (0.03f, 0.12f)); g.mod (sLFO1, dF1, 0.5f); g.amp (2000, 1000, 1.0f, 3000); g.reverb (0.4f, 1.0f, 0.85f); } },
            { { "Impact", "Hit", "Boom FX", "Slam", "Thunder" }, [] (G& g)
              { g.noise (0.6f, 1); g.sub (0.8f, 0, -2); g.osc (1, Sine, 0.6f, -1); g.env (2, 1, 300, 0.0f, 200); g.mod (sENV2, dPitch, 0.5f);
                g.f1 (g.lg (1500, 4000), 0.2f); g.amp (1, 2500, 0.0f, 2000); g.reverb (0.5f, 1.0f, 0.85f); } },
            { { "Alarm", "Siren", "Warning", "Klaxon", "Beacon" }, [] (G& g)
              { g.osc (1, Square, 0.6f); g.lfo (1, g.pick ({ 3, 1 }), g.f (1, 4)); g.mod (sLFO1, dPitch, g.f (0.1f, 0.25f));
                g.f1 (3000, 0.3f); g.amp (5, 200, 1.0f, 200); g.reverb (0.25f, 0.7f, 0.5f); } },
            { { "Wind", "Gust", "Storm", "Breeze", "Howl" }, [] (G& g)
              { g.noise (0.9f, 1); g.f1 (3000, 0.1f); g.f2 (BP, g.lg (600, 1500), g.f (0.3f, 0.45f), 1, 0.6f, 0); g.lfo (1, 0, g.f (0.05f, 0.2f), 0, 0.5f);
                g.mod (sLFO1, dF2, 0.6f); g.lfo (3, 4, 0.5f); g.mod (sLFO3, dAmp, 0.3f); g.amp (1500, 1000, 1.0f, 2500); g.reverb (0.4f, 1.0f, 0.8f); } },
            { { "Glitch", "Stutter", "Crash FX", "Error", "Bitstream" }, [] (G& g)
              { g.osc (1, g.pick ({ Square, Saw }), 0.6f); g.noise (0.2f); g.lfo (1, 4, g.f (8, 20)); g.mod (sLFO1, dF1, 0.7f);
                g.lfo (3, 3, g.f (6, 16)); g.mod (sLFO3, dAmp, 0.6f); g.f1 (2000, 0.5f); g.amp (1, 400, 0.8f, 300); g.echo (7, 0.5f, 0.3f, true); } },
        } });

        // ------------------------------------------------------------ SYNTH
        c.push_back ({ "Synth",
            { "Retro", "Modern", "Vintage", "Analog", "Digital", "Poly", "Classic", "Eighties", "Synthwave", "Outrun", "Funky",
              "Disco", "Cosmic", "Bold", "Warm", "Cold", "Shiny", "Rich", "Fat", "Lush", "Electric", "Chrome", "Sunset", "Miami",
              "Tokyo", "Berlin", "Detroit", "Studio", "Night", "Velvet" },
        {
            { { "Brass", "Brass Stab", "Horns", "Fanfare", "Section" }, [] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, 0, 3, 12); g.osc (2, Saw, 0.4f, -1); g.f1 (g.lg (900, 1600), 0.2f, g.f (0.45f, 0.65f));
                g.env (2, g.f (20, 60), 400, 0.4f, 200); g.amp (20, 400, 0.7f, 200); g.comp (-16, 3, 2); g.reverb (0.15f, 0.6f, 0.5f); } },
            { { "Hoover", "Mentasm", "Rave", "Dark Hoover", "Alarm Synth" }, [] (G& g)
              { g.osc (1, Saw, 0.65f, 0, 0, 0, 7, 45); g.osc (2, Saw, 0.5f, -1, 0, 0, 5, 40); g.f1 (6000, 0.1f);
                g.env (3, 1, 250, 0.0f, 100); g.mod (sENV3, dPitch, -0.05f); g.amp (5, 300, 1.0f, 300); g.flanger (0.35f, 0.3f, 0.4f); } },
            { { "Saw Stab", "Stab", "Hit", "Rave Stab", "Chord Hit" }, [] (G& g)
              { g.osc (1, Saw, 0.65f, 0, 0, 0, 5, 25); g.osc (2, Saw, 0.5f, 0, 0, 7, 3, 20); g.f1 (g.lg (1500, 3500), 0.2f, 0.5f);
                g.env (2, 1, 180, 0.0f, 120); g.amp (1, 280, 0.0f, 200); g.space(); } },
            { { "Poly", "Polysynth", "Poly Lead", "Chords", "Poly Keys" }, [] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 3, 15); g.osc (2, Saw, 0.5f, 0, 0, 7, 3, 15); g.f1 (g.lg (2000, 4500), 0.15f, 0.35f);
                g.env (2, 5, 500, 0.3f, 300); g.amp (5, 500, 0.7f, 400); g.chorus (0.25f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "PWM", "Pulse Synth", "Square Poly", "Hollow", "Reed Synth" }, [] (G& g)
              { g.osc (1, Square, 0.55f, 0, 0, 0, 5, 18); g.osc (2, Square, 0.4f, 1, 0, 0, 3, 12); g.f1 (g.lg (2000, 4000), 0.2f, 0.3f);
                g.amp (10, 500, 0.75f, 300); g.phaser (0.35f, g.f (0.2f, 0.6f)); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Sync Brass", "FM Brass", "Bite Synth", "Edge Brass", "Metal Brass" }, [] (G& g)
              { g.osc (1, Saw, 0.7f); g.osc (2, Sine, 0.0f, 1); g.fm (g.f (0.2f, 0.45f)); g.f1 (g.lg (1500, 3000), 0.25f, 0.5f);
                g.env (2, 30, 400, 0.4f, 200); g.amp (20, 400, 0.7f, 200); g.reverb (0.15f, 0.6f, 0.5f); } },
            { { "Chord Stab", "House Stab", "Organ Stab", "Club Chord", "Detroit Chord" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 3, 18); g.osc (2, Square, 0.4f, 0, g.pick ({ 7, 12, 3, 4 })); g.f1 (g.lg (1500, 3000), 0.25f, 0.45f);
                g.env (2, 1, 160, 0.0f, 100); g.amp (1, 250, 0.0f, 150); g.echo (5, 0.3f, 0.2f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Soft Synth", "Mellow Synth", "Velvet Synth", "Cotton", "Pillow" }, [] (G& g)
              { g.osc (1, Tri, 0.6f, 0, 0, 0, 3, 12); g.osc (2, Saw, 0.3f, 0, 0, 5, 3, 15); g.f1 (g.lg (1500, 3000), 0.1f);
                g.amp (g.f (40, 150), 500, 0.8f, 500); g.chorus (0.3f); g.reverb (0.25f, 0.7f, 0.55f); } },
            { { "Res Sweep", "Filter Sweep", "Opener", "Sweeper", "Rising Synth" }, [] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, 0, 3, 15); g.f1 (g.lg (300, 700), g.f (0.45f, 0.65f)); g.env (3, g.f (800, 2000), 1500, 0.6f, 800);
                g.mod (sENV3, dF1, g.f (0.6f, 0.8f)); g.amp (10, 800, 0.85f, 500); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Unison", "Wall Synth", "Massive", "Stack Synth", "Supersynth" }, [] (G& g)
              { g.osc (1, Saw, 0.55f, 0, 0, 0, 7, 30); g.osc (2, Saw, 0.5f, -1, 0, 0, 5, 30); g.sub (0.3f); g.f1 (g.lg (3000, 7000), 0.15f);
                g.amp (5, 500, 0.85f, 400); g.comp (-18, 3, 3); g.space(); } },
        } });

        // ------------------------------------------------------------ ARP
        static const juce::StringArray pats { "Straight 16", "Accent 4", "Trance Gate 1/16", "Offbeat 8", "Gallop", "Syncopated",
                                              "Euclid 5/16", "Euclid 7/16", "Pulse 8", "Rising", "Falling", "Shuffle", "Triplet 12",
                                              "Sparse", "Broken", "Long Notes 1/8" };
        auto arpSettings = [] (G& g, std::initializer_list<int> modes)
        {
            g.arp (g.pick (modes), g.i (1, 3), pats[g.r.nextInt (pats.size())]);
            if (g.p (0.25f)) g.set (pid::arpSwing, g.f (0.1f, 0.35f));
            if (g.p (0.2f))  g.set (pid::arpHuman, g.f (0.1f, 0.4f));
        };
        c.push_back ({ "Arp",
            { "Running", "Spinning", "Cascading", "Rolling", "Pulsing", "Racing", "Dancing", "Looping", "Chasing", "Climbing",
              "Falling", "Ticking", "Rushing", "Tumbling", "Bubbling", "Sparkling", "Stepping", "Hopping", "Rippling", "Whirling",
              "Flowing", "Marching", "Galloping", "Zigzag", "Orbiting", "Circling", "Bouncing", "Drifting", "Swirling", "Endless" },
        {
            { { "Pluck Arp", "Trance Arp", "Sequence", "Pattern", "Motion" }, [arpSettings] (G& g)
              { g.osc (1, Saw, 0.7f, 0, 0, 0, 5, 25); g.f1 (g.lg (1800, 3000), 0.3f, 0.6f); g.env (2, 1, 150, 0.0f, 120);
                g.amp (1, 220, 0.0f, 150); arpSettings (g, { 0, 2, 3 }); g.echo (5, 0.3f, 0.22f, true); g.reverb (0.22f, 0.6f, 0.5f); } },
            { { "Acid Arp", "303 Run", "Squelch Run", "Acid Loop", "Bubble Run" }, [arpSettings] (G& g)
              { g.osc (1, Saw, 0.85f); g.f1 (g.lg (400, 800), g.f (0.6f, 0.75f), 0.55f, 0.4f); g.env (2, 1, 180, 0.0f, 80);
                g.mod (sVel, dF1, 0.4f); g.amp (1, 250, 0.6f, 80); arpSettings (g, { 0, 2, 4 }); g.set (pid::arpSlide, g.f (0.2f, 0.5f));
                g.comp (-16, 3, 2); g.echo (5, 0.3f, 0.15f); } },
            { { "Bell Arp", "Chime Run", "Music Box", "Bell Sequence", "Ting" }, [arpSettings] (G& g)
              { g.osc (1, Sine, 0.7f); g.osc (2, Sine, 0.0f, 1, 7); g.fm (g.f (0.35f, 0.6f)); g.amp (2, 800, 0.0f, 600);
                arpSettings (g, { 0, 2, 4 }); g.echo (4, 0.35f, 0.25f, true); g.reverb (0.3f, 0.7f, 0.6f); } },
            { { "Glass Arp", "Crystal Run", "Ice Steps", "Prism Run", "Frost" }, [arpSettings] (G& g)
              { g.osc (1, Tri, 0.6f); g.osc (2, Sine, 0.35f, 1); g.amp (2, 400, 0.2f, 400); arpSettings (g, { 0, 2, 3 });
                g.echo (2, 0.3f, 0.2f); g.reverb (0.4f, 0.8f, 0.65f); } },
            { { "Bass Arp", "Octave Bass", "Bass Run", "Bassline", "Low Run" }, [arpSettings] (G& g)
              { g.osc (1, Saw, 0.8f, -1, 0, 0, 2, 10); g.sub (0.5f); g.f1 (g.lg (700, 1200), 0.25f, 0.4f); g.env (2, 1, 160, 0.1f, 80);
                g.amp (1, 200, 0.7f, 60); arpSettings (g, { 0, 1, 2 }); g.comp (-18, 3, 3); } },
            { { "Chord Arp", "Stab Run", "Chord Gate", "Stabber", "Chord Loop" }, [arpSettings] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 20); g.osc (2, Square, 0.35f); g.f1 (g.lg (1500, 2500), 0.2f, 0.5f); g.env (2, 1, 200, 0.1f, 100);
                g.amp (2, 250, 0.4f, 120); arpSettings (g, { 5 }); g.reverb (0.25f, 0.6f, 0.5f); } },
            { { "Gated Pad", "Trance Gate", "Pad Gate", "Chopper", "Gate" }, [arpSettings] (G& g)
              { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 22); g.osc (2, Saw, 0.55f, 0, 0, 6, 5, 22); g.f1 (g.lg (2000, 4000), 0.15f);
                g.amp (5, 200, 0.9f, 60); arpSettings (g, { 5 }); g.reverb (0.35f, 0.8f, 0.6f); } },
            { { "Keys Arp", "Piano Run", "EP Run", "Broken Chord", "Arpeggio" }, [arpSettings] (G& g)
              { g.osc (1, Tri, 0.7f); g.osc (2, Sine, 0.3f, 1); g.amp (3, 400, 0.3f, 200); arpSettings (g, { 0, 2, 3, 4 });
                if (g.p (0.5f)) g.phaser (0.3f, 0.3f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { { "Lead Arp", "Slide Run", "Lead Line", "Melody Run", "Hook Arp" }, [arpSettings] (G& g)
              { g.osc (1, Square, 0.75f); g.osc (2, Square, 0.4f, 1); g.f1 (g.lg (3000, 6000), 0.2f); g.amp (3, 300, 0.9f, 150);
                arpSettings (g, { 0, 2, 4 }); g.set (pid::arpSlide, g.f (0.0f, 0.6f)); g.echo (5, 0.3f, 0.2f); } },
            { { "Bleep Arp", "Computer Run", "Chip Arp", "Blip Run", "Robot" }, [arpSettings] (G& g)
              { g.osc (1, Square, 0.6f, 1); g.f1 (g.lg (2500, 5000), 0.3f, 0.5f); g.env (2, 1, 80, 0.0f, 50); g.amp (1, 120, 0.0f, 80);
                arpSettings (g, { 0, 4, 2 }); g.echo (6, 0.4f, 0.25f, true); } },
        } });

        return c;
    }

    // ------------------------------------------------------------------------
    //  Generate `count` presets for one category (names unique vs `taken`).
    // ------------------------------------------------------------------------
    template <typename Preset>
    inline void generate (const Category& cat, int count, juce::StringArray& taken, std::vector<Preset>& out)
    {
        const int nTypes = (int) cat.types.size();
        juce::int64 catSeed = 0;
        for (auto ch : cat.name) catSeed = catSeed * 131 + (juce::int64) ch;

        for (int k = 0; k < count; ++k)
        {
            const auto& type = cat.types[(size_t) (k % nTypes)];
            G g (catSeed * 7919 + k * 104729 + 17);
            type.build (g);

            // name: adjective + archetype noun, unique within the category
            juce::String name;
            for (int tries = 0; tries < 200 && (name.isEmpty() || taken.contains (name)); ++tries)
                name = cat.adjectives[g.r.nextInt (cat.adjectives.size())] + " " + type.nouns[g.r.nextInt (type.nouns.size())];
            const juce::String base = name;
            for (int n = 2; taken.contains (name); ++n) name = base + " " + juce::String (n);
            taken.add (name);
            out.push_back ({ name, cat.name, std::move (g.v) });
        }
    }
}
