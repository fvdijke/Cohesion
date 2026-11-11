#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "Params.h"
#include "ArpPatterns.h"
#include "PresetGen.h"
#include "PresetGains.h"
#include <unordered_map>

// ===========================================================================
//  Factory presets (foundation set). Each preset lists only the parameters it
//  changes; applying a preset first resets every parameter to its default.
//  Values are the real parameter values (converted to 0..1 on apply).
// ===========================================================================
struct SynthPreset
{
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> v;
};

inline std::vector<SynthPreset> buildSynthPresets()
{
    using V = std::vector<std::pair<juce::String, float>>;
    std::vector<SynthPreset> out;

    out.push_back ({ "Default", "Init", { } });

    out.push_back ({ "Sub Driver", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.8f }, { pid::oscUni (1), 3 }, { pid::oscDet (1), 12 },
        { pid::oscOn (2), 0 },
        { pid::subOn, 1 }, { pid::subWave, 0 }, { pid::subOct, -1 }, { pid::subLevel, 0.7f },
        { pid::fltCutoff, 800 }, { pid::fltRes, 0.2f }, { pid::fltEnvAmt, 0.55f }, { pid::fltKeyTrk, 0.3f }, { pid::flt1EnvSrc, 1 },
        { pid::ampA, 3 }, { pid::ampD, 300 }, { pid::ampS, 0.8f }, { pid::ampR, 150 },
        { pid::envA (2), 2 }, { pid::envD (2), 250 }, { pid::envS (2), 0.0f }, { pid::envR (2), 150 } } });

    out.push_back ({ "Reese", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 2 }, { pid::oscDet (1), 30 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.7f }, { pid::oscUni (2), 2 }, { pid::oscDet (2), 40 },
        { pid::subOn, 1 }, { pid::subLevel, 0.4f },
        { pid::fltCutoff, 520 }, { pid::fltRes, 0.35f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.4f }, { pid::lfoRate (1), 0.3f }, { pid::lfoWave (1), 0 },
        { pid::ampA, 5 }, { pid::ampD, 400 }, { pid::ampS, 0.85f }, { pid::ampR, 200 } } });

    out.push_back ({ "Acid", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.85f }, { pid::oscOn (2), 0 },
        { pid::fltCutoff, 600 }, { pid::fltRes, 0.6f }, { pid::fltEnvAmt, 0.6f }, { pid::fltDrive, 0.4f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 200 }, { pid::envS (2), 0.0f }, { pid::envR (2), 100 },
        { pid::ampA, 1 }, { pid::ampD, 300 }, { pid::ampS, 0.6f }, { pid::ampR, 100 },
        { pid::modSrc (1), 10 }, { pid::modDst (1), 1 }, { pid::modAmt (1), 0.5f } } });

    out.push_back ({ "Super Saw", "Lead", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 7 }, { pid::oscDet (1), 35 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.6f }, { pid::oscUni (2), 7 }, { pid::oscDet (2), 35 }, { pid::oscFine (2), 6 },
        { pid::fltCutoff, 14000 }, { pid::fltRes, 0.1f },
        { pid::ampA, 6 }, { pid::ampD, 300 }, { pid::ampS, 0.9f }, { pid::ampR, 220 } } });

    out.push_back ({ "Square Lead", "Lead", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 1 }, { pid::oscLevel (1), 0.8f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 1 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.45f },
        { pid::fltCutoff, 6000 }, { pid::fltRes, 0.2f }, { pid::fltEnvAmt, 0.3f }, { pid::flt1EnvSrc, 1 },
        { pid::ampA, 2 }, { pid::ampD, 200 }, { pid::ampS, 0.85f }, { pid::ampR, 150 } } });

    out.push_back ({ "Pluck", "Pluck", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.75f }, { pid::oscOn (2), 0 },
        { pid::fltCutoff, 3000 }, { pid::fltRes, 0.3f }, { pid::fltEnvAmt, 0.7f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 180 }, { pid::envS (2), 0.0f }, { pid::envR (2), 120 },
        { pid::ampA, 1 }, { pid::ampD, 220 }, { pid::ampS, 0.0f }, { pid::ampR, 130 } } });

    out.push_back ({ "Soft EP", "Keys", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.7f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.3f },
        { pid::fltCutoff, 8000 },
        { pid::ampA, 3 }, { pid::ampD, 500 }, { pid::ampS, 0.4f }, { pid::ampR, 300 } } });

    out.push_back ({ "Warm Pad", "Pad", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.6f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 20 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.6f }, { pid::oscUni (2), 5 }, { pid::oscDet (2), 25 }, { pid::oscFine (2), 6 },
        { pid::fltCutoff, 1800 }, { pid::fltRes, 0.1f },
        { pid::ampA, 800 }, { pid::ampD, 1000 }, { pid::ampS, 0.8f }, { pid::ampR, 1200 },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.25f }, { pid::lfoRate (1), 0.5f }, { pid::lfoSmooth (1), 0.3f } } });

    out.push_back ({ "Glass Pad", "Pad", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.6f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscSemi (2), 7 }, { pid::oscLevel (2), 0.4f },
        { pid::oscOn (3), 1 }, { pid::oscWave (3), 3 }, { pid::oscSemi (3), 12 }, { pid::oscLevel (3), 0.3f },
        { pid::fltCutoff, 12000 },
        { pid::ampA, 1200 }, { pid::ampD, 800 }, { pid::ampS, 0.7f }, { pid::ampR, 1500 } } });

    out.push_back ({ "Ensemble", "Strings", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.6f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 18 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.6f }, { pid::oscUni (2), 5 }, { pid::oscDet (2), 22 }, { pid::oscFine (2), 4 },
        { pid::fltCutoff, 6000 }, { pid::fltRes, 0.1f },
        { pid::ampA, 400 }, { pid::ampD, 600 }, { pid::ampS, 0.85f }, { pid::ampR, 500 },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.015f }, { pid::lfoRate (1), 5.0f } } });

    out.push_back ({ "Noise Sweep", "FX", V {
        { pid::oscOn (1), 0 }, { pid::oscOn (2), 0 },
        { pid::noiseOn, 1 }, { pid::noiseType, 1 }, { pid::noiseLevel, 0.6f },
        { pid::fltCutoff, 500 }, { pid::fltRes, 0.5f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.7f }, { pid::lfoRate (1), 0.2f }, { pid::lfoWave (1), 0 },
        { pid::ampA, 500 }, { pid::ampS, 1.0f }, { pid::ampR, 1000 } } });

    // ---- FX per preset ----
    auto addFx = [&] (const char* name, V extra)
    {
        for (auto& p : out) if (p.name == name) p.v.insert (p.v.end(), extra.begin(), extra.end());
    };
    addFx ("Sub Driver",  V { { pid::compOn, 1 }, { pid::compThresh, -20 }, { pid::compRatio, 3 }, { pid::compMakeup, 3 } });
    addFx ("Reese",       V { { pid::compOn, 1 }, { pid::compThresh, -18 }, { pid::compRatio, 4 }, { pid::compMakeup, 4 },
                              { pid::rvOn, 1 }, { pid::rvMix, 0.12f }, { pid::rvSize, 0.5f }, { pid::rvDecay, 0.4f } });
    addFx ("Acid",        V { { pid::compOn, 1 }, { pid::compThresh, -16 }, { pid::compRatio, 3 }, { pid::compMakeup, 2 },
                              { pid::ecOn, 1 }, { pid::ecSync, 5 }, { pid::ecFb, 0.35f }, { pid::ecMix, 0.2f }, { pid::ecDamp, 0.5f } });
    addFx ("Super Saw",   V { { pid::ecOn, 1 }, { pid::ecSync, 5 }, { pid::ecFb, 0.3f }, { pid::ecMix, 0.18f }, { pid::ecPing, 1 },
                              { pid::rvOn, 1 }, { pid::rvMix, 0.2f }, { pid::rvSize, 0.7f }, { pid::rvDecay, 0.55f } });
    addFx ("Square Lead", V { { pid::ecOn, 1 }, { pid::ecSync, 2 }, { pid::ecFb, 0.3f }, { pid::ecMix, 0.2f },
                              { pid::rvOn, 1 }, { pid::rvMix, 0.12f } });
    addFx ("Pluck",       V { { pid::ecOn, 1 }, { pid::ecSync, 4 }, { pid::ecFb, 0.35f }, { pid::ecMix, 0.22f }, { pid::ecPing, 1 },
                              { pid::rvOn, 1 }, { pid::rvMix, 0.15f } });
    addFx ("Soft EP",     V { { pid::phOn, 1 }, { pid::phRate, 0.25f }, { pid::phDepth, 0.5f }, { pid::phMix, 0.35f },
                              { pid::rvOn, 1 }, { pid::rvMix, 0.18f } });
    addFx ("Warm Pad",    V { { pid::rvOn, 1 }, { pid::rvMix, 0.32f }, { pid::rvSize, 0.75f }, { pid::rvDecay, 0.65f } });
    addFx ("Glass Pad",   V { { pid::rvOn, 1 }, { pid::rvMix, 0.38f }, { pid::rvSize, 0.85f }, { pid::rvDecay, 0.7f }, { pid::rvPre, 40 } });
    addFx ("Ensemble",    V { { pid::flOn, 1 }, { pid::flRate, 0.15f }, { pid::flDepth, 0.3f }, { pid::flFb, 0.1f }, { pid::flDelay, 4.0f }, { pid::flMix, 0.3f },
                              { pid::rvOn, 1 }, { pid::rvMix, 0.3f }, { pid::rvSize, 0.7f }, { pid::rvDecay, 0.6f } });
    addFx ("Noise Sweep", V { { pid::rvOn, 1 }, { pid::rvMix, 0.45f }, { pid::rvSize, 1.0f }, { pid::rvDecay, 0.8f } });


    // =====================================================================
    //  Extra patches: filter 2 / routing / LFOs / envelopes / mod matrix / FX
    // =====================================================================
    // mod sources: 1-4 LFO1-4, 5-8 ENV1-4, 9 Vel, 10 ModWheel, 11 KeyTrack
    // mod dests:   1 F1 cut, 2 F2 cut, 3 Pitch, 4 Amp, 5 Pan, 6 LFO1 rate
    auto P = [&] (const char* name, const char* cat, V v) { out.push_back ({ name, cat, std::move (v) }); };
    auto off2 = V { { pid::oscOn (2), 0 } };
    auto cat = [] (V a, const V& b) { a.insert (a.end(), b.begin(), b.end()); return a; };

    // ---------------- Bass ----------------
    P ("Wobble", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.8f }, { pid::oscUni (1), 3 }, { pid::oscDet (1), 15 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 1 }, { pid::oscOct (2), -1 }, { pid::oscLevel (2), 0.5f },
        { pid::subOn, 1 }, { pid::subLevel, 0.5f },
        { pid::fltCutoff, 300 }, { pid::fltRes, 0.5f }, { pid::fltDrive, 0.3f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.7f }, { pid::lfoRate (1), 3.0f }, { pid::lfoSmooth (1), 0.2f },
        { pid::ampA, 3 }, { pid::ampS, 1.0f }, { pid::ampR, 120 },
        { pid::compOn, 1 }, { pid::compThresh, -18 }, { pid::compRatio, 4 }, { pid::compMakeup, 4 } });

    P ("Growl", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.8f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscFine (2), 12 }, { pid::oscLevel (2), 0.7f }, { pid::fmAmt, 0.3f },
        { pid::fltCutoff, 900 }, { pid::fltRes, 0.3f },
        { pid::flt2Type, 2 }, { pid::flt2Cutoff, 700 }, { pid::flt2Res, 0.6f }, { pid::fltRouting, 1 }, { pid::fltBalance, 0.5f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 2 }, { pid::modAmt (0), 0.6f }, { pid::lfoRate (1), 1.5f }, { pid::lfoWave (1), 1 },
        { pid::ampA, 4 }, { pid::ampS, 1.0f }, { pid::ampR, 150 },
        { pid::compOn, 1 }, { pid::compThresh, -20 }, { pid::compRatio, 4 }, { pid::compMakeup, 4 } });

    // ---------------- Lead ----------------
    P ("Vibrato Lead", "Lead", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.75f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 1 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.3f },
        { pid::fltCutoff, 4500 }, { pid::fltRes, 0.25f }, { pid::fltEnvAmt, 0.3f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.02f }, { pid::lfoRate (1), 5.5f }, { pid::lfoFade (1), 600 },
        { pid::ampA, 8 }, { pid::ampD, 300 }, { pid::ampS, 0.85f }, { pid::ampR, 220 },
        { pid::ecOn, 1 }, { pid::ecSync, 5 }, { pid::ecFb, 0.3f }, { pid::ecMix, 0.2f }, { pid::rvOn, 1 }, { pid::rvMix, 0.15f } });

    P ("Hoover", "Synth", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 7 }, { pid::oscDet (1), 45 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscOct (2), -1 }, { pid::oscLevel (2), 0.5f }, { pid::oscUni (2), 5 }, { pid::oscDet (2), 40 },
        { pid::fltCutoff, 6000 },
        { pid::modSrc (0), 7 }, { pid::modDst (0), 3 }, { pid::modAmt (0), -0.05f }, { pid::envA (3), 1 }, { pid::envD (3), 250 }, { pid::envS (3), 0.0f },
        { pid::ampA, 5 }, { pid::ampS, 1.0f }, { pid::ampR, 300 },
        { pid::flOn, 1 }, { pid::flRate, 0.3f }, { pid::flDepth, 0.5f }, { pid::flFb, 0.4f }, { pid::flMix, 0.35f } });

    P ("Brass Stab", "Synth", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 3 }, { pid::oscDet (1), 12 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscOct (2), -1 }, { pid::oscLevel (2), 0.4f },
        { pid::fltCutoff, 1200 }, { pid::fltRes, 0.2f }, { pid::fltEnvAmt, 0.55f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 30 }, { pid::envD (2), 400 }, { pid::envS (2), 0.4f }, { pid::envR (2), 200 },
        { pid::ampA, 20 }, { pid::ampD, 400 }, { pid::ampS, 0.7f }, { pid::ampR, 200 },
        { pid::compOn, 1 }, { pid::compThresh, -16 }, { pid::compMakeup, 2 }, { pid::rvOn, 1 }, { pid::rvMix, 0.15f } });

    // ---------------- Keys ----------------
    P ("FM Bells", "Keys", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 3 }, { pid::oscLevel (1), 0.7f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscOct (2), 1 }, { pid::oscSemi (2), 7 }, { pid::oscLevel (2), 0.0f },
        { pid::fmAmt, 0.6f },
        { pid::ampA, 2 }, { pid::ampD, 1200 }, { pid::ampS, 0.0f }, { pid::ampR, 900 },
        { pid::ecOn, 1 }, { pid::ecSync, 4 }, { pid::ecPing, 1 }, { pid::ecMix, 0.2f }, { pid::rvOn, 1 }, { pid::rvMix, 0.3f } });

    P ("Clav", "Keys", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 1 }, { pid::oscLevel (1), 0.7f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.3f },
        { pid::fltCutoff, 5000 }, { pid::fltEnvAmt, 0.4f }, { pid::flt1EnvSrc, 1 },
        { pid::flt2Type, 1 }, { pid::flt2Cutoff, 400 },
        { pid::envA (2), 1 }, { pid::envD (2), 150 }, { pid::envS (2), 0.0f },
        { pid::ampA, 1 }, { pid::ampD, 250 }, { pid::ampS, 0.2f }, { pid::ampR, 80 },
        { pid::phOn, 1 }, { pid::phMix, 0.25f } });

    // ---------------- Pad ----------------
    P ("Evolving Pad", "Pad", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.55f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 25 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 2 }, { pid::oscSemi (2), 7 }, { pid::oscLevel (2), 0.45f }, { pid::oscUni (2), 3 },
        { pid::fltCutoff, 1500 }, { pid::fltRes, 0.2f },
        { pid::flt2Type, 2 }, { pid::flt2Cutoff, 1200 }, { pid::flt2Res, 0.4f }, { pid::fltRouting, 1 }, { pid::fltBalance, 0.4f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.35f }, { pid::lfoRate (1), 0.12f }, { pid::lfoSmooth (1), 0.4f },
        { pid::modSrc (1), 2 }, { pid::modDst (1), 2 }, { pid::modAmt (1), 0.5f }, { pid::lfoRate (2), 0.07f }, { pid::lfoWave (2), 1 },
        { pid::modSrc (2), 3 }, { pid::modDst (2), 5 }, { pid::modAmt (2), 0.4f }, { pid::lfoRate (3), 0.2f },
        { pid::ampA, 1500 }, { pid::ampD, 1500 }, { pid::ampS, 0.85f }, { pid::ampR, 2500 },
        { pid::flOn, 1 }, { pid::flRate, 0.1f }, { pid::flDepth, 0.25f }, { pid::flFb, 0.0f }, { pid::flDelay, 5.0f }, { pid::flMix, 0.25f },
        { pid::rvOn, 1 }, { pid::rvMix, 0.4f }, { pid::rvSize, 0.9f }, { pid::rvDecay, 0.75f } });

    P ("Dark Choir", "Pad", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.5f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 15 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscOct (2), -1 }, { pid::oscLevel (2), 0.4f }, { pid::oscUni (2), 5 },
        { pid::fltCutoff, 3500 },
        { pid::flt2Type, 2 }, { pid::flt2Cutoff, 800 }, { pid::flt2Res, 0.55f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 2 }, { pid::modAmt (0), 0.15f }, { pid::lfoRate (1), 0.3f },
        { pid::modSrc (1), 2 }, { pid::modDst (1), 3 }, { pid::modAmt (1), 0.008f }, { pid::lfoRate (2), 5.0f }, { pid::lfoFade (2), 800 },
        { pid::ampA, 700 }, { pid::ampS, 0.9f }, { pid::ampR, 1500 },
        { pid::rvOn, 1 }, { pid::rvMix, 0.4f }, { pid::rvDecay, 0.7f } });

    // ---------------- Strings ----------------
    P ("Slow Strings", "Strings", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.55f }, { pid::oscUni (1), 7 }, { pid::oscDet (1), 20 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.5f }, { pid::oscUni (2), 5 }, { pid::oscDet (2), 25 }, { pid::oscFine (2), -6 },
        { pid::fltCutoff, 2500 }, { pid::fltEnvAmt, 0.35f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 900 }, { pid::envD (2), 1500 }, { pid::envS (2), 0.6f }, { pid::envR (2), 900 },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.01f }, { pid::lfoRate (1), 5.0f }, { pid::lfoFade (1), 1000 },
        { pid::ampA, 900 }, { pid::ampD, 800 }, { pid::ampS, 0.9f }, { pid::ampR, 900 },
        { pid::rvOn, 1 }, { pid::rvMix, 0.35f }, { pid::rvSize, 0.8f } });

    P ("Pizzicato", "Strings", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 3 }, { pid::oscDet (1), 10 }, { pid::oscOn (2), 0 },
        { pid::fltCutoff, 2200 }, { pid::fltRes, 0.2f }, { pid::fltEnvAmt, 0.6f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 140 }, { pid::envS (2), 0.0f }, { pid::envR (2), 100 },
        { pid::ampA, 1 }, { pid::ampD, 260 }, { pid::ampS, 0.0f }, { pid::ampR, 200 },
        { pid::rvOn, 1 }, { pid::rvMix, 0.3f } });

    // ---------------- Flute ----------------
    P ("Breathy Flute", "Flute", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.7f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.15f },
        { pid::noiseOn, 1 }, { pid::noiseType, 1 }, { pid::noiseLevel, 0.12f },
        { pid::fltCutoff, 3500 }, { pid::fltRes, 0.1f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.012f }, { pid::lfoRate (1), 5.2f }, { pid::lfoFade (1), 500 },
        { pid::modSrc (1), 2 }, { pid::modDst (1), 4 }, { pid::modAmt (1), 0.08f }, { pid::lfoRate (2), 4.7f },
        { pid::ampA, 80 }, { pid::ampD, 300 }, { pid::ampS, 0.85f }, { pid::ampR, 250 },
        { pid::rvOn, 1 }, { pid::rvMix, 0.3f } });

    P ("Pan Flute", "Flute", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 3 }, { pid::oscLevel (1), 0.75f }, { pid::oscOn (2), 0 },
        { pid::noiseOn, 1 }, { pid::noiseLevel, 0.18f },
        { pid::fltCutoff, 6000 }, { pid::flt2Type, 2 }, { pid::flt2Cutoff, 350 }, { pid::flt2Res, 0.3f }, { pid::flt2KeyTrk, 1.0f }, { pid::fltRouting, 1 }, { pid::fltBalance, 0.4f }, { pid::flt2Slope, 0 },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.008f }, { pid::lfoRate (1), 5.0f }, { pid::lfoFade (1), 300 },
        { pid::ampA, 40 }, { pid::ampD, 200 }, { pid::ampS, 0.8f }, { pid::ampR, 300 },
        { pid::ecOn, 1 }, { pid::ecSync, 2 }, { pid::ecMix, 0.12f }, { pid::rvOn, 1 }, { pid::rvMix, 0.35f } });

    // ---------------- Drums ----------------
    P ("Kick", "Drums", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 3 }, { pid::oscOct (1), -1 }, { pid::oscLevel (1), 0.95f }, { pid::oscPhase (1), 90 }, { pid::oscOn (2), 0 },
        { pid::modSrc (0), 6 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.6f }, { pid::envA (2), 1 }, { pid::envD (2), 60 }, { pid::envS (2), 0.0f }, { pid::envR (2), 50 },
        { pid::ampA, 1 }, { pid::ampD, 400 }, { pid::ampS, 0.0f }, { pid::ampR, 100 },
        { pid::compOn, 1 }, { pid::compThresh, -12 }, { pid::compRatio, 4 }, { pid::compMakeup, 3 } });

    P ("Snare", "Drums", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.5f }, { pid::oscOn (2), 0 },
        { pid::modSrc (0), 6 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.25f }, { pid::envA (2), 1 }, { pid::envD (2), 40 }, { pid::envS (2), 0.0f },
        { pid::noiseOn, 1 }, { pid::noiseLevel, 0.7f },
        { pid::fltCutoff, 9000 }, { pid::flt2Type, 1 }, { pid::flt2Cutoff, 700 },
        { pid::ampA, 1 }, { pid::ampD, 180 }, { pid::ampS, 0.0f }, { pid::ampR, 120 },
        { pid::compOn, 1 }, { pid::compThresh, -14 }, { pid::compMakeup, 3 },
        { pid::rvOn, 1 }, { pid::rvMix, 0.18f }, { pid::rvSize, 0.4f }, { pid::rvDecay, 0.3f } });

    P ("Closed Hat", "Drums", V {
        { pid::oscOn (1), 0 }, { pid::oscOn (2), 0 }, { pid::noiseOn, 1 }, { pid::noiseLevel, 0.8f },
        { pid::fltCutoff, 18000 }, { pid::flt2Type, 1 }, { pid::flt2Cutoff, 7000 }, { pid::flt2Res, 0.2f },
        { pid::ampA, 1 }, { pid::ampD, 60 }, { pid::ampS, 0.0f }, { pid::ampR, 50 } });

    P ("Open Hat", "Drums", V {
        { pid::oscOn (1), 0 }, { pid::oscOn (2), 0 }, { pid::noiseOn, 1 }, { pid::noiseLevel, 0.75f },
        { pid::fltCutoff, 18000 }, { pid::flt2Type, 1 }, { pid::flt2Cutoff, 6500 }, { pid::flt2Res, 0.2f },
        { pid::ampA, 1 }, { pid::ampD, 350 }, { pid::ampS, 0.0f }, { pid::ampR, 250 } });

    P ("Tom", "Drums", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 3 }, { pid::oscLevel (1), 0.9f }, { pid::oscPhase (1), 90 }, { pid::oscOn (2), 0 },
        { pid::modSrc (0), 6 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.3f }, { pid::envA (2), 1 }, { pid::envD (2), 120 }, { pid::envS (2), 0.0f },
        { pid::noiseOn, 1 }, { pid::noiseLevel, 0.1f },
        { pid::ampA, 1 }, { pid::ampD, 500 }, { pid::ampS, 0.0f }, { pid::ampR, 200 },
        { pid::rvOn, 1 }, { pid::rvMix, 0.15f }, { pid::rvSize, 0.4f } });

    // ---------------- FX ----------------
    P ("Riser", "FX", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.35f }, { pid::oscUni (1), 7 }, { pid::oscDet (1), 40 }, { pid::oscOn (2), 0 },
        { pid::noiseOn, 1 }, { pid::noiseType, 1 }, { pid::noiseLevel, 0.5f },
        { pid::fltCutoff, 200 }, { pid::fltRes, 0.4f },
        { pid::modSrc (0), 7 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.9f },
        { pid::modSrc (1), 7 }, { pid::modDst (1), 3 }, { pid::modAmt (1), 0.25f },
        { pid::envA (3), 4000 }, { pid::envD (3), 1 }, { pid::envS (3), 1.0f }, { pid::envR (3), 1500 },
        { pid::ampA, 3000 }, { pid::ampS, 1.0f }, { pid::ampR, 1500 },
        { pid::ecOn, 1 }, { pid::ecSync, 4 }, { pid::ecPing, 1 }, { pid::ecMix, 0.25f }, { pid::rvOn, 1 }, { pid::rvMix, 0.4f } });

    P ("Laser Zap", "FX", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscOn (2), 0 },
        { pid::modSrc (0), 6 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.9f }, { pid::envA (2), 1 }, { pid::envD (2), 180 }, { pid::envS (2), 0.0f },
        { pid::fltCutoff, 8000 }, { pid::fltRes, 0.4f },
        { pid::ampA, 1 }, { pid::ampD, 300 }, { pid::ampS, 0.0f }, { pid::ampR, 200 },
        { pid::ecOn, 1 }, { pid::ecSync, 5 }, { pid::ecPing, 1 }, { pid::ecMix, 0.3f } });

    // =====================================================================
    //  ARP library presets (synth patch + ARP settings + step pattern)
    // =====================================================================
    auto pat = [] (const char* name)
    {
        V v;
        for (auto& pt : arpPatterns())
            if (pt.name == name)
            {
                v.push_back ({ pid::arpSteps, (float) pt.steps });
                for (int i = 0; i < 16; ++i)
                {
                    v.push_back ({ pid::arpStepOn (i), pt.on[i] == 'x' ? 1.0f : 0.0f });
                    v.push_back ({ pid::arpStepVel (i), pt.vel[i] > 0.0f ? pt.vel[i] : 0.5f });
                }
                if (pt.rate >= 0)     v.push_back ({ pid::arpRate, (float) pt.rate });
                if (pt.gate >= 0.0f)  v.push_back ({ pid::arpGate, pt.gate });
                if (pt.swing >= 0.0f) v.push_back ({ pid::arpSwing, pt.swing });
            }
        return v;
    };
    const V pluck = { { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 25 },
                      { pid::oscOn (2), 0 }, { pid::fltCutoff, 2500 }, { pid::fltRes, 0.3f }, { pid::fltEnvAmt, 0.6f }, { pid::flt1EnvSrc, 1 },
                      { pid::envA (2), 1 }, { pid::envD (2), 150 }, { pid::envS (2), 0.0f }, { pid::envR (2), 120 },
                      { pid::ampA, 1 }, { pid::ampD, 220 }, { pid::ampS, 0.0f }, { pid::ampR, 150 } };

    P ("Trance Pluck Arp", "Arp", cat (cat (pluck, pat ("Trance Gate 1/16")), V {
        { pid::arpOn, 1 }, { pid::arpMode, 0 }, { pid::arpOct, 2 },
        { pid::ecOn, 1 }, { pid::ecSync, 5 }, { pid::ecPing, 1 }, { pid::ecMix, 0.25f }, { pid::rvOn, 1 }, { pid::rvMix, 0.25f } }));

    P ("Acid Run", "Arp", cat (pat ("Accent 4"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.85f }, { pid::oscOn (2), 0 },
        { pid::fltCutoff, 500 }, { pid::fltRes, 0.65f }, { pid::fltEnvAmt, 0.55f }, { pid::fltDrive, 0.4f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 180 }, { pid::envS (2), 0.0f },
        { pid::modSrc (0), 9 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.4f },
        { pid::ampA, 1 }, { pid::ampD, 250 }, { pid::ampS, 0.6f }, { pid::ampR, 80 },
        { pid::arpOn, 1 }, { pid::arpMode, 2 }, { pid::arpOct, 2 }, { pid::arpSlide, 0.4f }, { pid::arpGate, 0.5f },
        { pid::compOn, 1 }, { pid::compThresh, -16 }, { pid::compMakeup, 2 }, { pid::ecOn, 1 }, { pid::ecSync, 5 }, { pid::ecMix, 0.18f } }));

    P ("Glass Sequence", "Arp", cat (pat ("Rising"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.6f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.35f },
        { pid::ampA, 2 }, { pid::ampD, 400 }, { pid::ampS, 0.2f }, { pid::ampR, 400 },
        { pid::arpOn, 1 }, { pid::arpMode, 2 }, { pid::arpOct, 3 },
        { pid::ecOn, 1 }, { pid::ecSync, 2 }, { pid::ecMix, 0.2f }, { pid::rvOn, 1 }, { pid::rvMix, 0.4f }, { pid::rvSize, 0.8f } }));

    P ("Octave Bass", "Arp", cat (pat ("Gallop"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.8f }, { pid::oscUni (1), 2 }, { pid::oscDet (1), 10 }, { pid::oscOn (2), 0 },
        { pid::subOn, 1 }, { pid::subLevel, 0.6f },
        { pid::fltCutoff, 900 }, { pid::fltRes, 0.25f }, { pid::fltEnvAmt, 0.4f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 160 }, { pid::envS (2), 0.1f },
        { pid::ampA, 1 }, { pid::ampD, 200 }, { pid::ampS, 0.7f }, { pid::ampR, 60 },
        { pid::arpOn, 1 }, { pid::arpMode, 0 }, { pid::arpOct, 2 }, { pid::arpOrder, 1 },
        { pid::compOn, 1 }, { pid::compThresh, -18 }, { pid::compMakeup, 3 } }));

    P ("Chord Stabs", "Arp", cat (pat ("Syncopated"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.6f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 20 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 1 }, { pid::oscLevel (2), 0.35f },
        { pid::fltCutoff, 2000 }, { pid::fltEnvAmt, 0.5f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 200 }, { pid::envS (2), 0.1f },
        { pid::ampA, 2 }, { pid::ampD, 250 }, { pid::ampS, 0.4f }, { pid::ampR, 120 },
        { pid::arpOn, 1 }, { pid::arpMode, 5 }, { pid::arpRate, 3 }, { pid::arpGate, 0.35f },
        { pid::rvOn, 1 }, { pid::rvMix, 0.25f } }));

    P ("Random Bells", "Arp", cat (pat ("Straight 16"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 3 }, { pid::oscLevel (1), 0.7f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscOct (2), 1 }, { pid::oscSemi (2), 7 }, { pid::oscLevel (2), 0.0f }, { pid::fmAmt, 0.5f },
        { pid::ampA, 2 }, { pid::ampD, 900 }, { pid::ampS, 0.0f }, { pid::ampR, 700 },
        { pid::arpOn, 1 }, { pid::arpMode, 4 }, { pid::arpOct, 2 }, { pid::arpRate, 2 }, { pid::arpHuman, 0.4f },
        { pid::ecOn, 1 }, { pid::ecSync, 4 }, { pid::ecPing, 1 }, { pid::ecMix, 0.3f }, { pid::rvOn, 1 }, { pid::rvMix, 0.35f } }));

    P ("Gated Pad", "Arp", cat (pat ("Trance Gate 1/16"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.6f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 22 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.55f }, { pid::oscUni (2), 5 }, { pid::oscFine (2), 6 },
        { pid::fltCutoff, 3000 }, { pid::fltRes, 0.15f },
        { pid::ampA, 5 }, { pid::ampD, 200 }, { pid::ampS, 0.9f }, { pid::ampR, 60 },
        { pid::arpOn, 1 }, { pid::arpMode, 5 }, { pid::arpGate, 0.6f },
        { pid::rvOn, 1 }, { pid::rvMix, 0.35f }, { pid::rvSize, 0.8f } }));

    P ("Shuffle Keys", "Arp", cat (pat ("Shuffle"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.7f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.3f },
        { pid::ampA, 3 }, { pid::ampD, 400 }, { pid::ampS, 0.3f }, { pid::ampR, 200 },
        { pid::arpOn, 1 }, { pid::arpMode, 0 }, { pid::arpOct, 1 },
        { pid::phOn, 1 }, { pid::phMix, 0.3f }, { pid::rvOn, 1 }, { pid::rvMix, 0.2f } }));

    P ("Euclid Pluck", "Arp", cat (cat (pluck, pat ("Euclid 7/16")), V {
        { pid::arpOn, 1 }, { pid::arpMode, 3 }, { pid::arpOct, 2 },
        { pid::ecOn, 1 }, { pid::ecSync, 4 }, { pid::ecMix, 0.25f }, { pid::rvOn, 1 }, { pid::rvMix, 0.2f } }));

    P ("Slide Lead Arp", "Arp", cat (pat ("Long Notes 1/8"), V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 1 }, { pid::oscLevel (1), 0.8f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 1 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.4f },
        { pid::fltCutoff, 5000 }, { pid::fltRes, 0.2f },
        { pid::ampA, 3 }, { pid::ampS, 0.9f }, { pid::ampR, 150 },
        { pid::arpOn, 1 }, { pid::arpMode, 0 }, { pid::arpOct, 2 }, { pid::arpSlide, 0.6f }, { pid::arpGate, 1.0f },
        { pid::ecOn, 1 }, { pid::ecSync, 5 }, { pid::ecMix, 0.2f } }));
    juce::ignoreUnused (off2);


    // =====================================================================
    //  Fill every category up to 100 presets with the procedural generator
    // =====================================================================
    for (const auto& cat : pgen::categories())
    {
        juce::StringArray taken;
        for (auto& p : out) if (p.category == cat.name) taken.add (p.name);
        if (taken.size() < 100)
            pgen::generate (cat, 100 - taken.size(), taken, out);
    }

    // ---- loudness matching (measured offline by tests/PresetRender) ----
    static const std::unordered_map<std::string, std::pair<float, float>> levels = []
    {
        std::unordered_map<std::string, std::pair<float, float>> m;
        for (auto& e : kPresetLevels) if (e.key[0] != 0) m[e.key] = { e.level, e.drive };
        return m;
    }();
    for (auto& p : out)
    {
        const auto it = levels.find ((p.category + "/" + p.name).toStdString());
        if (it == levels.end()) continue;
        p.v.push_back ({ pid::mainVol, it->second.first });
        if (it->second.second > 0.0f) p.v.push_back ({ pid::limDrive, it->second.second });
    }

    return out;
}

// Built once per process and shared by every plugin instance.
inline const std::vector<SynthPreset>& cohesionSynthPresets()
{
    static const std::vector<SynthPreset> all = buildSynthPresets();
    return all;
}

inline void applySynthPreset (juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts, const SynthPreset& preset)
{
    for (auto* param : proc.getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());

    for (auto& kv : preset.v)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}
